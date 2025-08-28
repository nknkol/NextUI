// heavily modified from the Onion original: https://github.com/OnionUI/Onion/blob/main/src/playActivity/playActivityUI.c
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <msettings.h>
#include <sqlite3.h>
#include <math.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "gametimedb.h"
#include "plugin.h" // 插件接口
#include "sysui.h"  // SysUI 接口

// --- 静态变量和结构体定义 ---

struct ListLayout
{
    int list_display_size_x;
    int list_display_size_y;
    int list_display_start_x;
    int list_display_start_y;
    SDL_Rect list_display_rect;

    int sub_title_x;
    int sub_title_y;

    int items_per_page;
    int num_pages;
} layout = {0};

#define BIG_PILL_SIZE 48
#define IMG_MARGIN 8
#define IMG_MAX_WIDTH BIG_PILL_SIZE - IMG_MARGIN
#define IMG_MAX_HEIGHT BIG_PILL_SIZE - IMG_MARGIN

// 插件作用域内的静态变量
static SDL_Surface *screen;
static bool quit_plugin = false;
static SDL_Surface **romImages;
static PlayActivities *play_activities;
static int selected;
static int start;
static int end;


// --- 核心改动：将原文件的私有渲染函数迁移至此 ---
static int _renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect, bool right_align)
{
    int text_width = 0;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (textSurface != NULL) {
        text_width = textSurface->w;
        if (right_align)
            SDL_BlitSurface(textSurface, NULL, screen, &(SDL_Rect){rect->w - textSurface->w, rect->y, rect->w, rect->h});
        else
            SDL_BlitSurface(textSurface, NULL, screen, rect);
        SDL_FreeSurface(textSurface);
    }
    return text_width;
}

static int renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, false);
}
// --- 核心改动结束 ---


static inline SDL_Color colorFromUint(uint32_t colour)
{
	SDL_Color tempcol;
	tempcol.a = 255;
	tempcol.r = (colour >> 16) & 0xFF;
	tempcol.g = (colour >> 8) & 0xFF;
	tempcol.b = colour & 0xFF;
	return tempcol;
}

void _setPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) {
        return;
    }
    SDL_LockSurface(surface);
    Uint8 *pixelPtr = (Uint8 *)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;
    switch (surface->format->BytesPerPixel) {
        case 1: *pixelPtr = color; break;
        case 2: *(Uint16 *)pixelPtr = color; break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                pixelPtr[0] = (color >> 16) & 0xFF;
                pixelPtr[1] = (color >> 8) & 0xFF;
                pixelPtr[2] = color & 0xFF;
            } else {
                pixelPtr[0] = color & 0xFF;
                pixelPtr[1] = (color >> 8) & 0xFF;
                pixelPtr[2] = (color >> 16) & 0xFF;
            }
            break;
        case 4: *(Uint32 *)pixelPtr = color; break;
    }
    SDL_UnlockSurface(surface);
}

void _drawFilledCircle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                _setPixel(surface, cx + x, cy + y, color);
            }
        }
    }
}

void renderRoundedRectangle(SDL_Rect rect, Uint32 color, int radius) {
    SDL_Rect fillRect = {rect.x + radius, rect.y, rect.w - 2 * radius, rect.h};
    SDL_FillRect(screen, &fillRect, color);
    fillRect.x = rect.x;
    fillRect.y = rect.y + radius;
    fillRect.w = rect.w;
    fillRect.h = rect.h - 2 * radius;
    SDL_FillRect(screen, &fillRect, color);
    _drawFilledCircle(screen, rect.x + radius, rect.y + radius, radius, color);
    _drawFilledCircle(screen, rect.x + rect.w - radius - 1, rect.y + radius, radius, color);
    _drawFilledCircle(screen, rect.x + radius, rect.y + rect.h - radius - 1, radius, color);
    _drawFilledCircle(screen, rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, radius, color);
}

SDL_Surface *loadRomImage(char *image_path)
{
    if(!exists(image_path)) return NULL;
    SDL_Surface *img = IMG_Load(image_path);
    if(!img) return NULL;
    if(img->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface *optimized = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(img); 
        img = optimized;
    }
    SDL_PixelFormat *ft = img->format;
    SDL_Surface *dst = SDL_CreateRGBSurface(0, SCALE1(IMG_MAX_WIDTH), SCALE1(IMG_MAX_HEIGHT), ft->BitsPerPixel, ft->Rmask, ft->Gmask, ft->Bmask, ft->Amask);
    SDL_Rect imgRect = GFX_blitScaled(GFX_SCALE_FILL, img, dst);
    GFX_ApplyRoundedCorners(dst, &imgRect, SCALE1(18));
    SDL_FreeSurface(img);
    return dst;
}

void preloadRomImages()
{
    romImages = malloc(sizeof(SDL_Surface *) * play_activities->count);
    for (int i = 0; i < play_activities->count; i++) {
        PlayActivity *entry = play_activities->play_activity[i];
        ROM *rom = entry->rom;
        romImages[i] = loadRomImage(rom->image_path);
    }
}

void freeRomImages()
{
    if (!romImages) return;
    for (int i = 0; i < play_activities->count; i++) {
        if(romImages[i]) SDL_FreeSurface(romImages[i]);
    }
    free(romImages);
    romImages = NULL;
}

void renderList(int count, int start_index, int end_index, int selected_index)
{
    char rom_name[255];
    char total[25];
    char average[25];
    char plays[25];

    const int elemHeight = SCALE1(BIG_PILL_SIZE);
    const int thumbMargin = SCALE1(IMG_MARGIN);
    const int textHeight = (elemHeight - thumbMargin) / 2;

    int selected_row = selected_index - start_index;
    for (int index=start_index,row=0; index<end_index; index++,row++) {
        bool isSelected = selected_row == row;

        PlayActivity *entry = play_activities->play_activity[index];
        ROM *rom = entry->rom;

        renderRoundedRectangle((SDL_Rect){
            layout.list_display_start_x, 
            layout.list_display_start_y + row * elemHeight, 
            layout.list_display_size_x, 
            elemHeight
        }, isSelected ? RGB_WHITE : RGB_BLACK, SCALE1(24));

        SDL_Surface *romImage = romImages[index];
        if (romImage) {
            SDL_Rect rectRomImage = {
                layout.list_display_start_x + thumbMargin / 2 + (SCALE1(IMG_MAX_WIDTH) - romImage->w) / 2, 
                layout.list_display_start_y + elemHeight * row + thumbMargin / 2, 
                SCALE1(IMG_MAX_WIDTH), 
                SCALE1(IMG_MAX_HEIGHT)
            };
            SDL_BlitSurface(romImage, NULL, screen, &rectRomImage);
        } else {
            SDL_Rect rectRomImage = {
                layout.list_display_start_x + thumbMargin / 2, 
                layout.list_display_start_y + elemHeight * row + thumbMargin / 2, 
                SCALE1(IMG_MAX_WIDTH), 
                SCALE1(IMG_MAX_HEIGHT)
            };
            renderRoundedRectangle(rectRomImage, RGB_DARK_GRAY, SCALE1(18));
            SDL_Rect rect = {SCALE4(92,51,18,10)};
            int x = rectRomImage.x + (SCALE1(IMG_MAX_WIDTH) - rect.w) / 2;
            int y = rectRomImage.y + (SCALE1(IMG_MAX_HEIGHT) - rect.h) / 2;
            GFX_blitAssetColor(ASSET_GAMEPAD, NULL, screen, &(SDL_Rect){x,y}, THEME_COLOR1_255);
        }

        cleanName(rom_name, rom->name);
        SDL_Color textColor = isSelected ? COLOR_BLACK : COLOR_WHITE;
        
        // 使用本地的 renderText
        renderText(rom_name, font.medium, textColor, &(SDL_Rect){
            layout.list_display_start_x + thumbMargin + SCALE1(IMG_MAX_WIDTH), 
            layout.list_display_start_y + thumbMargin / 2 + elemHeight * row, 
            layout.list_display_size_x, 
            textHeight});

        serializeTime(total, entry->play_time_total);
        serializeTime(average, entry->play_time_average);
        snprintf(plays, 24, "%d", entry->play_count);

        const char *details[] = {"TOTAL ", total, "  AVERAGE ", average, "  # PLAYS ", plays};
        SDL_Rect detailsRect = {
            layout.list_display_start_x + thumbMargin + SCALE1(IMG_MAX_WIDTH), 
            layout.list_display_start_y + thumbMargin + textHeight + elemHeight * row, 
            layout.list_display_size_x, 
            textHeight
        };

        // 恢复使用本地 renderText 的原始逻辑
        for (int i = 0; i < 6; i++) {
            SDL_Color detailCol = i % 2 == 0 ? COLOR_DARK_TEXT : colorFromUint(THEME_COLOR2_255);
            detailsRect.x += renderText(details[i], font.small, detailCol, &detailsRect);
        }
    }

    if (count > layout.items_per_page) {
        #define SCROLL_WIDTH 24
        #define SCROLL_HEIGHT 4
        int ox = (screen->w - SCALE1(SCROLL_WIDTH)) / 2;
        int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
        if (start_index > 0) 
            GFX_blitAsset(ASSET_SCROLL_UP, NULL, screen, &(SDL_Rect){ox, SCALE1(PADDING + PILL_SIZE)});
        if (end_index < count) 
            GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen, &(SDL_Rect){ox, screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + oy});
    }
}


void initLayout()
{
    int hw = screen->w;
    int hh = screen->h;

    layout.list_display_start_x = SCALE1(PADDING);
    layout.list_display_start_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
    layout.list_display_size_x = hw - SCALE1(PADDING * 2);
    layout.list_display_size_y = hh - SCALE1(PADDING * 2 + PILL_SIZE * 2 + BUTTON_MARGIN);

    layout.list_display_rect = (SDL_Rect){
        layout.list_display_start_x, 
        layout.list_display_start_y, 
        layout.list_display_size_x, 
        layout.list_display_size_y
    };

    layout.items_per_page = layout.list_display_size_y / SCALE1(BIG_PILL_SIZE);
    if (play_activities) {
        layout.num_pages = (int)ceil((double)play_activities->count / (double)layout.items_per_page);
    } else {
        layout.num_pages = 0;
    }
}


// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = false;

    SysUI_Init(screen, &font);
    SysUI_SetFullscreen(false);
    
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    play_activities = play_activity_find_all();
    if (play_activities) {
        LOG_debug("found %d roms\n", play_activities->count);
    }

    initLayout();
    preloadRomImages();

    int count = play_activities ? play_activities->count : 0;
    selected = 0;
    start = 0;
    end = MIN(count, layout.items_per_page);

    SysUI_SetBottomHints("U/D", "SCROLL", "B", "BACK", NULL, NULL);

    return 0;
}

static int plugin_run() {
    int dirty = 1;
    bool input_handled_by_sysui = false;

    while(!quit_plugin) {
        PAD_poll();

        input_handled_by_sysui = SysUI_Update();
        if (input_handled_by_sysui) {
            dirty = 1;
        }

        if (!input_handled_by_sysui) {
            int count = play_activities ? play_activities->count : 0;
            if (count > 0 && PAD_justRepeated(BTN_UP)) {
                selected -= 1;
                if (selected < 0) {
                    selected = count - 1;
                    start = MAX(0, count - layout.items_per_page);
                    end = count;
                } else if (selected < start) {
                    start -= 1;
                    end -= 1;
                }
                dirty = 1;
            } else if (count > 0 && PAD_justRepeated(BTN_DOWN)) {
                selected += 1;
                if (selected >= count) {
                    selected = 0;
                    start = 0;
                    end = MIN(count, layout.items_per_page);
                } else if (selected >= end) {
                    start += 1;
                    end += 1;
                }
                dirty = 1;
            } else if (PAD_justPressed(BTN_B)) {
                quit_plugin = 1;
            }
        }

        if (dirty) {
            GFX_clear(screen);

            if (play_activities) {
                int play_time_total = play_activities->play_time_total;
                char play_time_total_formatted[255];
                serializeTime(play_time_total_formatted, play_time_total);
                char display_name[256];
                snprintf(display_name, sizeof(display_name), "Time spent having fun: %s", play_time_total_formatted);
                SysUI_SetTitle(display_name);

                renderList(play_activities->count, start, end, selected);
            } else {
                 SysUI_SetTitle("No Play Activity");
                 GFX_blitMessage(font.large, "No play activity found.", screen, NULL);
            }
            
            SysUI_Render();

            GFX_flip(screen);
            dirty = 0;
        } else {
            GFX_sync();
        }
    }
    return 0;
}

static void plugin_quit(void) {
    freeRomImages();
    if(play_activities) {
        free_play_activities(play_activities);
        play_activities = NULL;
    }
    SysUI_Quit();
}


// --- 插件导出 ---

static NextUI_Plugin gametime_plugin_export = {
    .name = "Game Time", // 插件的显示名称
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

NextUI_Plugin* GetPlugin(void) {
    return &gametime_plugin_export;
}