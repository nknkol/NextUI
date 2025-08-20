#ifndef SYSUI_H
#define SYSUI_H

#include "sdl.h"
#include "api.h"
#include <stdbool.h>

typedef enum {
    SYSUI_OVERLAY_NONE,
    SYSUI_OVERLAY_VOLUME,
    SYSUI_OVERLAY_BRIGHTNESS,
    SYSUI_OVERLAY_COLORTEMP,
} SysUI_OverlayType;

typedef struct {
    char left_button[16];
    char left_hint[64];
    char right_button1[16];
    char right_hint1[64];
    char right_button2[16];
    char right_hint2[64];
} SysUI_BottomHints;

typedef struct {
    bool is_fullscreen;
    char title[256];
    SysUI_BottomHints bottom_hints;
    SysUI_OverlayType active_overlay;
    int overlay_value;
    int overlay_min;
    int overlay_max;
    uint32_t overlay_display_start_time;
    SDL_Surface* screen_surface;
    GFX_Fonts* fonts;
} SysUI_Context;


// --- 公共 API ---
void SysUI_Init(SDL_Surface* screen, GFX_Fonts* fonts);
void SysUI_Quit(void);
void SysUI_Render(void);
bool SysUI_Update(PAD_Context* pad);
void SysUI_SetTitle(const char* title);
void SysUI_SetFullscreen(bool fullscreen);
void SysUI_SetBottomHints(const char* l_btn, const char* l_hint, const char* r_btn1, const char* r_hint1, const char* r_btn2, const char* r_hint2);
void SysUI_ShowOverlay(SysUI_OverlayType type, int value, int min, int max);

#endif // SYSUI_H