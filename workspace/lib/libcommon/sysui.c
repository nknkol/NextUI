#include "sysui.h"
#include "lang.h"
#include "api.h"
#include <stdbool.h>
#include <msettings.h>

// 全局唯一的 SysUI 上下文实例
static SysUI_Context g_sysui_ctx;
#define OVERLAY_TIMEOUT_MS 500
// #undef PAD_isPressed
// #undef PAD_justRepeated
// #define PAD_isPressed(p, btn)		(((p)->is_pressed) & (btn))
// #define PAD_justRepeated(p, btn)	(((p)->just_repeated) & (btn))


// 内部函数声明
static void SysUI_RenderTopBar(void);
static void SysUI_RenderBottomBar(void);

// --- 公共 API 实现 ---

void SysUI_Init(SDL_Surface* screen, GFX_Fonts* fonts) {
    memset(&g_sysui_ctx, 0, sizeof(SysUI_Context));
    g_sysui_ctx.screen_surface = screen;
    g_sysui_ctx.fonts = fonts;
    strncpy(g_sysui_ctx.title, "System", sizeof(g_sysui_ctx.title) - 1);
    g_sysui_ctx.show_bottom_bar = true;
}
void SysUI_ShowBottomBar(bool show) {
    g_sysui_ctx.show_bottom_bar = show;
}
void SysUI_Quit(void) {
    // 目前没有动态分配的内存需要释放
}

bool SysUI_Update(void) {
    LOG_note(LOG_REALTIME, "[SysUI] SysUI_Update() CALLED.\n");
    uint32_t now = SDL_GetTicks();
    SysUI_OverlayType last_overlay_state = g_sysui_ctx.active_overlay;

    // 优先处理静音状态变化，因为它是一个独立的事件
    static int was_muted = -1;
    if (was_muted == -1 && InitializedSettings()) was_muted = GetMute();
    if (InitializedSettings()) {
        int muted = GetMute();
        if (muted != was_muted) {
            was_muted = muted;
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_VOLUME;
            g_sysui_ctx.overlay_display_start_time = now;
            // 因为这是一个瞬时事件，我们直接返回，下一帧再处理按键
            return true;
        }
    }

    // --- 全新的、正确的核心逻辑 ---

    // 1. 检查当前是否有功能键被按住
    bool brightness_mod_pressed = PAD_isPressed(BTN_MOD_BRIGHTNESS);
    bool colortemp_mod_pressed = PAD_isPressed(BTN_MOD_COLORTEMP);
    
    // 2. 检查是否有调节键被按下
    bool setting_adjusted = PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS);

    // 3. 决定是否要显示或保持显示UI浮层
    //    条件：一个功能键正被按住，或者一个调节键刚刚被按下
    if (brightness_mod_pressed || colortemp_mod_pressed || setting_adjusted) {
        
        // 只要有相关操作，就重置UI消失的计时器
        g_sysui_ctx.overlay_display_start_time = now;

        // 根据被按下的功能键，确定要显示的浮层类型
        if (brightness_mod_pressed) {
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_BRIGHTNESS;
        } else if (colortemp_mod_pressed) {
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_COLORTEMP;
        } else {
            // 如果只有调节键被按下（没有特定的功能键），则默认为是音量调节
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_VOLUME;
        }

    } else {
        // 如果没有任何相关按键操作，根据浮层类型决定隐藏策略
        if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
            // 对于亮度和色温（组合键操作），立即隐藏
            if (g_sysui_ctx.active_overlay == SYSUI_OVERLAY_BRIGHTNESS || g_sysui_ctx.active_overlay == SYSUI_OVERLAY_COLORTEMP) {
                g_sysui_ctx.active_overlay = SYSUI_OVERLAY_NONE;
            }
            // 对于其他浮层（如音量），使用超时延迟隐藏
            else {
                if (now - g_sysui_ctx.overlay_display_start_time > OVERLAY_TIMEOUT_MS) {
                    g_sysui_ctx.active_overlay = SYSUI_OVERLAY_NONE;
                }
            }
        }
    }
    // --- 调试探针 #B：SysUI_Update 函数出口状态 ---
    if (g_sysui_ctx.active_overlay != last_overlay_state) {
        LOG_note(LOG_REALTIME, "[SysUI] Overlay state CHANGED from %d to %d\n", last_overlay_state, g_sysui_ctx.active_overlay);
    }
    
    bool is_interacting_with_mods = PAD_isPressed(BTN_MOD_BRIGHTNESS) || PAD_isPressed(BTN_MOD_COLORTEMP);
    bool state_just_changed = (last_overlay_state != g_sysui_ctx.active_overlay);

    // `setting_adjusted` 在函数开头已经计算好了，这里直接使用。
    // 它代表了用户是否正在按 +/- 来调整数值。
    return is_interacting_with_mods || setting_adjusted || state_just_changed;
}

void SysUI_Render(void) {
    if (g_sysui_ctx.is_fullscreen) {
        return;
    }
    SysUI_RenderTopBar();
    SysUI_RenderBottomBar();
}

// --- 参数设置 API 实现 ---

void SysUI_SetTitle(const char* title) {
    if (title) {
        strncpy(g_sysui_ctx.title, title, sizeof(g_sysui_ctx.title) - 1);
    } else {
        g_sysui_ctx.title[0] = '\0';
    }
}

void SysUI_SetFullscreen(bool fullscreen) {
    g_sysui_ctx.is_fullscreen = fullscreen;
}

void SysUI_SetBottomHints(const char* l_btn, const char* l_hint, const char* r_btn1, const char* r_hint1, const char* r_btn2, const char* r_hint2) {
    strncpy(g_sysui_ctx.bottom_hints.left_button,  l_btn  ? L(l_btn)  : "", sizeof(g_sysui_ctx.bottom_hints.left_button) - 1);
    strncpy(g_sysui_ctx.bottom_hints.left_hint,    l_hint ? L(l_hint) : "", sizeof(g_sysui_ctx.bottom_hints.left_hint) - 1);
    
    strncpy(g_sysui_ctx.bottom_hints.right_button1, r_btn1 ? L(r_btn1) : "", sizeof(g_sysui_ctx.bottom_hints.right_button1) - 1);
    strncpy(g_sysui_ctx.bottom_hints.right_hint1,   r_hint1 ? L(r_hint1) : "", sizeof(g_sysui_ctx.bottom_hints.right_hint1) - 1);
    
    strncpy(g_sysui_ctx.bottom_hints.right_button2, r_btn2 ? L(r_btn2) : "", sizeof(g_sysui_ctx.bottom_hints.right_button2) - 1);
    strncpy(g_sysui_ctx.bottom_hints.right_hint2,   r_hint2 ? L(r_hint2) : "", sizeof(g_sysui_ctx.bottom_hints.right_hint2) - 1);
}

void SysUI_ShowOverlay(SysUI_OverlayType type, int value, int min, int max) {
    g_sysui_ctx.active_overlay = type;
    g_sysui_ctx.overlay_display_start_time = SDL_GetTicks();
}

// --- 内部渲染函数实现 ---
static void SysUI_RenderTopBar(void) {
    SDL_Surface* screen = g_sysui_ctx.screen_surface;
    int ow = 0;
    int show_setting_flag = 0;

    // --- 调试探针 #C：SysUI_RenderTopBar 函数入口状态 ---
    LOG_note(LOG_REALTIME, "[SysUI] SysUI_RenderTopBar() CALLED. Active overlay is %d\n", g_sysui_ctx.active_overlay);

    if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
        switch(g_sysui_ctx.active_overlay) {
            case SYSUI_OVERLAY_BRIGHTNESS: show_setting_flag = 1; break;
            case SYSUI_OVERLAY_VOLUME:     show_setting_flag = 2; break;
            case SYSUI_OVERLAY_COLORTEMP:  show_setting_flag = 3; break;
            default: break;
        }
        // --- 调试探针 #D：准备调用 GFX_blitHardwareGroup ---
        LOG_note(LOG_REALTIME, "[SysUI] Preparing to call GFX_blitHardwareGroup with show_setting_flag = %d\n", show_setting_flag);
        ow = GFX_blitHardwareGroup(screen, show_setting_flag);
    } else {
        ow = GFX_blitHardwareGroup(screen, 0); 
    }

    if (strlen(g_sysui_ctx.title) > 0) {
        int title_max_width = screen->w - SCALE1(PADDING * 2) - ow;
        char display_name_title[256];
        int text_width = GFX_truncateText(g_sysui_ctx.fonts->large, g_sysui_ctx.title, display_name_title, title_max_width, SCALE1(BUTTON_PADDING * 2));
        title_max_width = MIN(title_max_width, text_width);

        SDL_Color textColor = uintToColour(THEME_COLOR6_255);
        SDL_Surface* text_title = TTF_RenderUTF8_Blended(g_sysui_ctx.fonts->large, display_name_title, textColor);
        
        GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
            SCALE1(PADDING),
            SCALE1(PADDING),
            title_max_width,
            SCALE1(PILL_SIZE)
        });
        
        SDL_BlitSurface(text_title, &(SDL_Rect){
            0, 0,
            title_max_width - SCALE1(BUTTON_PADDING * 2), text_title->h
        }, screen, &(SDL_Rect){
            SCALE1(PADDING + BUTTON_PADDING),
            SCALE1(PADDING + 4)
        });
        SDL_FreeSurface(text_title);
    }
}

static void SysUI_RenderBottomBar(void) {
    if (!g_sysui_ctx.show_bottom_bar) {
        return;
    }
    
    SDL_Surface* screen = g_sysui_ctx.screen_surface;

    char* hints_right[5] = {NULL, NULL, NULL, NULL, NULL};
    int right_hint_count = 0;
    if (strlen(g_sysui_ctx.bottom_hints.right_button1) > 0) {
        hints_right[right_hint_count++] = g_sysui_ctx.bottom_hints.right_button1;
        hints_right[right_hint_count++] = g_sysui_ctx.bottom_hints.right_hint1;
    }
    if (strlen(g_sysui_ctx.bottom_hints.right_button2) > 0) {
        hints_right[right_hint_count++] = g_sysui_ctx.bottom_hints.right_button2;
        hints_right[right_hint_count++] = g_sysui_ctx.bottom_hints.right_hint2;
    }
    if (right_hint_count > 0) {
        GFX_blitButtonGroup(hints_right, 1, screen, 1);
    }

    if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
        int show_setting_flag = 0;
        switch(g_sysui_ctx.active_overlay) {
            case SYSUI_OVERLAY_BRIGHTNESS: show_setting_flag = 1; break;
            case SYSUI_OVERLAY_VOLUME:     show_setting_flag = 2; break;
            case SYSUI_OVERLAY_COLORTEMP:  show_setting_flag = 3; break;
            default: break;
        }
        GFX_blitHardwareHints(screen, show_setting_flag);

    } else {
        if (strlen(g_sysui_ctx.bottom_hints.left_button) > 0) {
            char* hints_left[] = { g_sysui_ctx.bottom_hints.left_button, g_sysui_ctx.bottom_hints.left_hint, NULL };
            GFX_blitButtonGroup(hints_left, 0, screen, 0);
        }
    }
}