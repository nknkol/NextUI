#include "sysui.h"
#include "lang.h"
#include "api.h"
#include <stdbool.h>
#include <msettings.h>

// 全局唯一的 SysUI 上下文实例
static SysUI_Context g_sysui_ctx;
#define OVERLAY_TIMEOUT_MS 1500
#undef PAD_isPressed
#undef PAD_justRepeated
#define PAD_isPressed(p, btn)		(((p)->is_pressed) & (btn))
#define PAD_justRepeated(p, btn)	(((p)->just_repeated) & (btn))
#define OVERLAY_TIMEOUT_MS 1500

// 内部函数声明
static void SysUI_RenderTopBar(void);
static void SysUI_RenderBottomBar(void);

// --- 公共 API 实现 ---

void SysUI_Init(SDL_Surface* screen, GFX_Fonts* fonts) {
    memset(&g_sysui_ctx, 0, sizeof(SysUI_Context));
    g_sysui_ctx.screen_surface = screen;
    g_sysui_ctx.fonts = fonts;
    // 设置一个默认标题，避免空值
    strncpy(g_sysui_ctx.title, "System", sizeof(g_sysui_ctx.title) - 1);
}

void SysUI_Quit(void) {
    // 目前没有动态分配的内存需要释放
}

bool SysUI_Update(PAD_Context* pad) {
    uint32_t now = SDL_GetTicks();
    bool state_changed = false;
    
    // <<< 关键修正：保存调用前的状态，用于后续比较
    SysUI_OverlayType last_overlay_state = g_sysui_ctx.active_overlay;
    int last_overlay_value = g_sysui_ctx.overlay_value;

    bool setting_adjusted = false;

    // 1. 处理需要修饰键的快捷键（亮度和色温）
    if (PAD_isPressed(pad, BTN_MOD_BRIGHTNESS) || PAD_isPressed(pad, BTN_MOD_COLORTEMP)) {
        if (PAD_justRepeated(pad, BTN_MOD_PLUS) || PAD_justRepeated(pad, BTN_MOD_MINUS)) {
            if (PAD_isPressed(pad, BTN_MOD_BRIGHTNESS)) {
                // <<< 关键修正：不再调用ShowOverlay，而是直接更新上下文的值
                g_sysui_ctx.active_overlay = SYSUI_OVERLAY_BRIGHTNESS;
                g_sysui_ctx.overlay_value = GetBrightness(); // 每次都重新获取最新值
                g_sysui_ctx.overlay_min = BRIGHTNESS_MIN;
                g_sysui_ctx.overlay_max = BRIGHTNESS_MAX;
            } else { // BTN_MOD_COLORTEMP
                g_sysui_ctx.active_overlay = SYSUI_OVERLAY_COLORTEMP;
                g_sysui_ctx.overlay_value = GetColortemp(); // 每次都重新获取最新值
                g_sysui_ctx.overlay_min = COLORTEMP_MIN;
                g_sysui_ctx.overlay_max = COLORTEMP_MAX;
            }
            setting_adjusted = true;
        }
    }
    // 2. 处理不需要修饰键的快捷键（音量）
    else if (!PAD_isPressed(pad, BTN_MOD_BRIGHTNESS) && !PAD_isPressed(pad, BTN_MOD_COLORTEMP)) {
        if (PAD_justRepeated(pad, BTN_MOD_PLUS) || PAD_justRepeated(pad, BTN_MOD_MINUS)) {
            // <<< 关键修正：直接更新上下文的值
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_VOLUME;
            g_sysui_ctx.overlay_value = GetVolume(); // 每次都重新获取最新值
            g_sysui_ctx.overlay_min = VOLUME_MIN;
            g_sysui_ctx.overlay_max = VOLUME_MAX;
            setting_adjusted = true;
        }
    }

    // 3. 处理浮层的自动隐藏逻辑
    if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
        if (setting_adjusted) {
             g_sysui_ctx.overlay_display_start_time = now; // 正在调整，重置计时器
        }
        else if (now - g_sysui_ctx.overlay_display_start_time > OVERLAY_TIMEOUT_MS) {
            g_sysui_ctx.active_overlay = SYSUI_OVERLAY_NONE; // 超时，隐藏
        }
    }
    
    // <<< 关键修正：检查浮层状态或值是否发生变化
    if ( (last_overlay_state != g_sysui_ctx.active_overlay) ||
         (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE && last_overlay_value != g_sysui_ctx.overlay_value) )
    {
        state_changed = true;
    }

    return state_changed;
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
    g_sysui_ctx.overlay_value = value;
    g_sysui_ctx.overlay_min = min;
    g_sysui_ctx.overlay_max = max;
    g_sysui_ctx.overlay_display_start_time = SDL_GetTicks();
}

// --- 内部渲染函数实现 ---

static void SysUI_RenderTopBar(void) {
    SDL_Surface* screen = g_sysui_ctx.screen_surface;
    int ow = 0; // 右侧硬件组宽度

    // 核心逻辑：判断是渲染浮层还是常规状态图标
    if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
        // --- 这部分逻辑从 GFX_blitHardwareGroup 迁移而来 ---
		ow = SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + 4);
		int ox = screen->w - SCALE1(PADDING) - ow;
		int oy = SCALE1(PADDING);
		GFX_blitPillColor(ASSET_WHITE_PILL, screen, &(SDL_Rect){ox, oy, ow, SCALE1(PILL_SIZE)}, THEME_COLOR2, RGB_WHITE);

        int asset;
        switch(g_sysui_ctx.active_overlay) {
            case SYSUI_OVERLAY_BRIGHTNESS: asset = ASSET_BRIGHTNESS; break;
            case SYSUI_OVERLAY_COLORTEMP: asset = ASSET_COLORTEMP; break;
            case SYSUI_OVERLAY_VOLUME:
            default:
                asset = (g_sysui_ctx.overlay_value > 0) ? ASSET_VOLUME : ASSET_VOLUME_MUTE;
                break;
        }

		SDL_Rect asset_rect;
		GFX_assetRect(asset, &asset_rect);
		int ax = ox + (SCALE1(PILL_SIZE) - asset_rect.w) / 2;
		int ay = oy + (SCALE1(PILL_SIZE) - asset_rect.h) / 2;
		GFX_blitAssetColor(asset, NULL, screen, &(SDL_Rect){ax, ay}, THEME_COLOR6_255);

		ox += SCALE1(PILL_SIZE);
		oy += SCALE1((PILL_SIZE - SETTINGS_SIZE) / 2);
		GFX_blitPill(ASSET_BAR_BG_MENU, screen, &(SDL_Rect){ox, oy, SCALE1(SETTINGS_WIDTH), SCALE1(SETTINGS_SIZE)});
		
		float percent = ((float)(g_sysui_ctx.overlay_value - g_sysui_ctx.overlay_min) / (g_sysui_ctx.overlay_max - g_sysui_ctx.overlay_min));
		if (percent > 0) {
			GFX_blitPillDark(ASSET_BAR, screen, &(SDL_Rect){ox, oy, SCALE1(SETTINGS_WIDTH) * percent, SCALE1(SETTINGS_SIZE)});
		}
    } else {
        // --- 这部分是 GFX_blitHardwareGroup 的 else 分支 ---
        // 调用原函数，但传入 show_setting=0，强制只显示状态图标
        ow = GFX_blitHardwareGroup(screen, 0); 
    }

    // 绘制标题
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
    SDL_Surface* screen = g_sysui_ctx.screen_surface;
    
    // 处理左侧按钮
    if (strlen(g_sysui_ctx.bottom_hints.left_button) > 0) {
        char* hints_left[] = { g_sysui_ctx.bottom_hints.left_button, g_sysui_ctx.bottom_hints.left_hint, NULL };
        GFX_blitButtonGroup(hints_left, 0, screen, 0); // Align left
    }
    
    // 处理右侧按钮
    char* hints_right[5] = {NULL, NULL, NULL, NULL, NULL}; // 最多支持2对按钮 (4个字符串 + NULL)
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
        // GFX_blitButtonGroup的第一个按钮对被认为是 "primary"
        GFX_blitButtonGroup(hints_right, 1, screen, 1); // Align right
    }
}