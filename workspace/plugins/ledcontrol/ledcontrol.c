#include <stdbool.h>
#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"
#include "plugin.h" // 插件接口
#include "sysui.h"  // SysUI 接口

// --- 从 ledcontrol.c 迁移过来的静态变量和结构体定义 ---

#define NUM_MAIN_OPTIONS 5
#define NROF_TRIGGERS 14

static const char *lightnames[4];
static const char *triggernames[] = {
    "B", "A", "Y", "X", "L", "R", "FN1", "FN2", "MENU", "SELECT", "START", "ALL", "LR", "DPAD"};

static const char *effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive"};
static const char *topbar_effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive", "Topbar Rainbow", "Topbar night"};
static const char *lr_effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive", "LR Rainbow", "LR Reactive"};

// --- 插件内部状态变量 ---
static SDL_Surface* screen;
static int quit_plugin;
static int selected_light;
static int selected_setting;
// static bool is_brick;
static int num_of_lights;

// --- 从 ledcontrol.c 迁移过来的函数 ---

static void save_settings() {
    LOG_info("saving settings plat");
    char diskfilename[256];
    is_brick = exactMatch("brick", getenv("DEVICE"));
    int maxlights = 4;
    if(is_brick) {
        snprintf(diskfilename, sizeof(diskfilename), SHARED_USERDATA_PATH "/ledsettings_brick.txt");
    }
    else {
        maxlights = 3;
        snprintf(diskfilename, sizeof(diskfilename), SHARED_USERDATA_PATH "/ledsettings.txt");
    }

    FILE *file = fopen(diskfilename, "w");

    if (file == NULL)
    {
        perror("Unable to open settings file for writing");
    } else {
        LOG_info("saving leds!");
        for (int i = 0; i < maxlights; ++i)
        {
            fprintf(file, "[%s]\n", lightsDefault[i].name);
            fprintf(file, "effect=%d\n", lightsDefault[i].effect);
            fprintf(file, "color1=0x%06X\n", lightsDefault[i].color1);
            fprintf(file, "color2=0x%06X\n", lightsDefault[i].color2);
            fprintf(file, "speed=%d\n", lightsDefault[i].speed);
            fprintf(file, "brightness=%d\n", lightsDefault[i].brightness);
            fprintf(file, "trigger=%d\n", lightsDefault[i].trigger);
            fprintf(file, "filename=%s\n", lightsDefault[i].filename);
            fprintf(file, "inbrightness=%i\n", lightsDefault[i].inbrightness);
            fprintf(file, "\n");
        }
        fclose(file);
    }
}

static void handle_light_input(LightSettings *light, int selected_setting)
{
    const uint32_t bright_colors[] = {
        0x000011, 0x000022, 0x000033, 0x000044, 0x000055, 0x000066, 0x000077, 0x000088, 0x000099, 0x0000AA, 0x0000BB, 0x0000CC, 0x3366FF, 0x4D7AFF, 0x6699FF, 0x80B3FF, 0x99CCFF, 0xB3D9FF, 0x0000FF,
        0x001111, 0x002222, 0x003333, 0x004444, 0x005555, 0x006666, 0x007777, 0x008888, 0x009999, 0x00AAAA, 0x00BBBB, 0x00CCCC, 0x33FFFF, 0x4DFFFF, 0x66FFFF, 0x80FFFF, 0x99FFFF, 0xB3FFFF, 0x00FFFF,
        0x001100, 0x002200, 0x003300, 0x004400, 0x005500, 0x006600, 0x007700, 0x008800, 0x009900, 0x00AA00, 0x00BB00, 0x00CC00, 0x33FF33, 0x4DFF4D, 0x66FF66, 0x80FF80, 0x99FF99, 0xB3FFB3, 0x00FF00,
        0x110011, 0x220022, 0x330033, 0x440044, 0x550055, 0x660066, 0x770077, 0x880088, 0x990099, 0xAA00AA, 0xBB00BB, 0xCC00CC, 0xFF33FF, 0xFF4DFF, 0xFF66FF, 0xFF80FF, 0xFF99FF, 0xFFB3FF, 0xFF00FF,
        0x220044, 0x330066, 0x440088, 0x5500AA, 0x6600CC, 0x7700DD, 0x8800EE, 0x9900FF, 0xAA00FF, 0xBB00FF, 0xCC00FF, 0x8833FF, 0x994DFF, 0xAA66FF, 0xBB80FF, 0xCC99FF, 0xDDB3FF,
        0x220000, 0x440000, 0x660000, 0x880000, 0xAA0000, 0xCC0000, 0xFF3333, 0xFF4D4D, 0xFF6666, 0xFF8080, 0xFF9999, 0xFFB3B3, 0xFF0000,
        0x222200, 0x444400, 0x666600, 0x888800, 0xAAAA00, 0xCCCC00, 0xFFFF33, 0xFFFF4D, 0xFFFF66, 0xFFFF80, 0xFFFF99, 0xFFFFB3, 0xFFFF00,
        0x331100, 0x662200, 0x993300, 0xCC4400, 0xFF5500, 0xFF6600, 0xFF7711, 0xFF8822, 0xFF9933, 0xFFAA44, 0xFFBB55, 0xFFCC66, 0xFFDD77, 0xFFEE88,
        0x000000, 0x111111, 0x222222, 0x333333, 0x444444, 0x555555, 0x666666, 0x777777, 0x888888, 0x999999, 0xAAAAAA, 0xBBBBBB, 0xCCCCCC, 0xDDDDDD, 0xFFFFFF
    };
    const int num_bright_colors = sizeof(bright_colors) / sizeof(bright_colors[0]);

    switch (selected_setting) {
    case 0: // Effect
        if (PAD_justPressed(BTN_RIGHT)) light->effect = (light->effect % 6) + 1;
        else if (PAD_justPressed(BTN_LEFT)) light->effect = (light->effect - 2 + 6) % 6 + 1;
        break;
    case 1: // Color
        if (PAD_justPressed(BTN_RIGHT)) {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++) if (bright_colors[i] == light->color1) { current_index = i; break; }
            light->color1 = bright_colors[(current_index + 1) % num_bright_colors];
        } else if (PAD_justPressed(BTN_LEFT)) {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++) if (bright_colors[i] == light->color1) { current_index = i; break; }
            light->color1 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    case 2: // Speed
        if (PAD_justPressed(BTN_RIGHT)) light->speed = (light->speed + 100) % 5000;
        else if (PAD_justPressed(BTN_LEFT)) light->speed = (light->speed - 100 + 5000) % 5000;
        break;
    case 3: // Brightness
        if (PAD_justPressed(BTN_RIGHT)) light->brightness = (light->brightness + 5) % 105;
        else if (PAD_justPressed(BTN_LEFT)) light->brightness = (light->brightness - 5 + 105) % 105;
        break;
    case 4: // Info Brightness
        if (PAD_justPressed(BTN_RIGHT)) light->inbrightness = (light->inbrightness + 5) % 105;
        else if (PAD_justPressed(BTN_LEFT)) light->inbrightness = (light->inbrightness - 5 + 105) % 105;
        break;
    }
    LEDS_updateLeds();
    save_settings();
}

static void render_plugin_page() {
    const char *settings_labels[5];
    if (is_brick) {
        const char *brick_labels[] = {"Effect", "Color", "Speed", "Brightness", "Info brightness"};
        memcpy(settings_labels, brick_labels, sizeof(brick_labels));
    } else {
        const char *non_brick_labels[] = {"Effect", "Color", "Speed", "Brightness (All Leds)", "Info brightness (All Leds)"};
        memcpy(settings_labels, non_brick_labels, sizeof(non_brick_labels));
    }
    int settings_values[5] = {
        lightsDefault[selected_light].effect,
        lightsDefault[selected_light].color1,
        lightsDefault[selected_light].speed,
        lightsDefault[selected_light].brightness,
        lightsDefault[selected_light].inbrightness
    };

    // --- 垂直居中计算 (从 nextui.c 借鉴) ---
    int safe_area_top = SCALE1(PADDING + PILL_SIZE);
    int safe_area_bottom = screen->h - SCALE1(PADDING + PILL_SIZE);
    int available_height = safe_area_bottom - safe_area_top;
    int list_block_height = NUM_MAIN_OPTIONS * SCALE1(PILL_SIZE);
    int list_oy = safe_area_top + ((available_height - list_block_height) / 2);


    for (int j = 0; j < 5; ++j) {
        char setting_text[256];
        bool selected = (j == selected_setting);
        SDL_Color current_color = selected ? COLOR_BLACK : COLOR_WHITE;
        
        int y = list_oy + (j * SCALE1(PILL_SIZE));

        if (j == 0) {
            snprintf(setting_text, sizeof(setting_text), "%s: %s", settings_labels[j], selected_light == 3 ? lr_effect_names[settings_values[j] - 1] : selected_light == 2 ? topbar_effect_names[settings_values[j] - 1] : effect_names[settings_values[j] - 1]);
            // --- 修改: 使用 font.large ---
            SDL_Surface *text = TTF_RenderUTF8_Blended(font.large, setting_text, current_color);
            int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
            GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
            SDL_BlitSurface(text, &(SDL_Rect){0, 0, text->w, text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
            SDL_FreeSurface(text);
        } else if (j == 1) {
            snprintf(setting_text, sizeof(setting_text), "%s", settings_labels[j]);
            // --- 修改: 使用 font.large ---
            SDL_Surface *text = TTF_RenderUTF8_Blended(font.large, setting_text, current_color);
            int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
            GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), y, text_width + SCALE1(BUTTON_MARGIN + BUTTON_SIZE), SCALE1(PILL_SIZE)});
            SDL_BlitSurface(text, &(SDL_Rect){0, 0, text->w, text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
            SDL_FreeSurface(text);
            GFX_blitAssetColor(ASSET_BUTTON, NULL, screen, &(SDL_Rect){SCALE1(PADDING) + text_width, y + SCALE1(BUTTON_MARGIN)}, settings_values[j]);
        } else {
            snprintf(setting_text, sizeof(setting_text), "%s: %d", settings_labels[j], settings_values[j]);
            // --- 修改: 使用 font.large ---
            SDL_Surface *text = TTF_RenderUTF8_Blended(font.large, setting_text, current_color);
            int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
            GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
            SDL_BlitSurface(text, &(SDL_Rect){0, 0, text->w, text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
            SDL_FreeSurface(text);
        }
    }
}


// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0;
    selected_light = 0;
    selected_setting = 0;

    SysUI_Init(screen, &font);
    SysUI_SetFullscreen(false);
    SysUI_SetBottomHints("L1/R1", "Select light", "B", "Back", NULL, NULL);

    char* device = getenv("DEVICE");
    is_brick = exactMatch("brick", device);
    
    if (is_brick) {
        const char *brick_names[] = {"F1 key", "F2 key", "Top bar", "L&R triggers"};
        memcpy((void*)lightnames, brick_names, sizeof(brick_names));
        num_of_lights = 4;
    } else {
        const char *default_names[] = {"Joystick L","Joystick R", "Logo"};
        memcpy((void*)lightnames, default_names, sizeof(default_names));
        num_of_lights = 3;
    }

    PLAT_initLeds(lightsDefault);
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    return 0;
}

static int plugin_run() {
    int dirty = 1;
    bool input_handled_by_sysui = false;
    int show_setting_dummy = 0;

    while(!quit_plugin) {
        GFX_startFrame();
        PAD_poll();

        input_handled_by_sysui = SysUI_Update();
        if (input_handled_by_sysui) {
            dirty = 1;
        }

        PWR_update(&dirty, &show_setting_dummy, NULL, NULL);

        if (!input_handled_by_sysui) {
            if (PAD_justPressed(BTN_B)) {
                quit_plugin = 1;
            }
            else if(PAD_justPressed(BTN_DOWN)) {
                selected_setting = (selected_setting + 1) % NUM_MAIN_OPTIONS;
                dirty = 1;
            }
            else if(PAD_justPressed(BTN_UP)) {
                selected_setting = (selected_setting - 1 + NUM_MAIN_OPTIONS) % NUM_MAIN_OPTIONS;
                dirty = 1;
            }
            else if(PAD_justPressed(BTN_L1)) {
                selected_light = (selected_light - 1 + num_of_lights) % num_of_lights;
                dirty = 1;
            }
            else if(PAD_justPressed(BTN_R1)) {
                selected_light = (selected_light + 1) % num_of_lights;
                dirty = 1;
            }
            else if(PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT)) {
                handle_light_input(&lightsDefault[selected_light], selected_setting);
                dirty = 1;
            }
        }
        
        if (dirty) {
            GFX_clear(screen);

            char title_buffer[256];
            snprintf(title_buffer, sizeof(title_buffer), "LED Control: %s", lightnames[selected_light]);
            SysUI_SetTitle(title_buffer);
            
            render_plugin_page();
            
            SysUI_Render();

            GFX_flip(screen);
            dirty = 0;
        } else {
            GFX_delay();
        }
    }

    return 0;
}

static void plugin_quit(void) {
    SysUI_Quit();
}

// --- 插件导出 ---

static NextUI_Plugin led_plugin_Export = {
    .name = "LED Control",
    .display_path = SDCARD_PATH "/Tools/Settings",
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

NextUI_Plugin* GetPlugin(void) {
    return &led_plugin_Export;
}