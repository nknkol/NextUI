#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "plugin.h" // 插件接口
#include "sysui.h"  // SysUI 接口

// --- 宏和枚举定义 ---

#define MAX_SESSIONS 10
// PILL_HEIGHT 现在直接使用 nextui 的标准尺寸
#define PILL_HEIGHT SCALE1(PILL_SIZE) 

// 插件内部视图状态
typedef enum {
    VIEW_LIST,
    VIEW_TERMINAL,
} ViewState;

// 终端会话结构
typedef struct {
    int id;
    char name[64];
} TerminalSession;

// --- 静态变量 ---

// 插件作用域内的静态变量
static SDL_Surface* screen;
static bool quit_plugin = false;
static ViewState current_view = VIEW_LIST;

// 会话管理
static TerminalSession sessions[MAX_SESSIONS];
static int session_count = 0;
static int next_session_id = 1;

// 列表状态
static int selected_index = 0;
static int list_start_index = 0;
// items_per_page 将在 plugin_init 中进行初始化
static int items_per_page = 0;

// --- 辅助函数 ---

// 创建一个新的终端会话
static void create_session(void) {
    if (session_count >= MAX_SESSIONS) {
        return; // 达到最大数量
    }
    TerminalSession* new_session = &sessions[session_count];
    new_session->id = next_session_id++;
    snprintf(new_session->name, sizeof(new_session->name), "Terminal %d", new_session->id);
    session_count++;
    selected_index = session_count - 1; // 自动选中新创建的会话
}

// 删除一个终端会话
static void delete_session(void) {
    if (session_count == 0) {
        return;
    }

    // 从数组中移除选中的项
    for (int i = selected_index; i < session_count - 1; i++) {
        sessions[i] = sessions[i + 1];
    }
    session_count--;

    // 调整选中索引
    if (selected_index >= session_count && session_count > 0) {
        selected_index = session_count - 1;
    } else if (session_count == 0) {
        selected_index = 0;
    }
}

// --- 渲染函数 ---

// 渲染会话列表
static void render_session_list(void) {
    // 1. 确保列表滚动位置正确
    if (selected_index < list_start_index) {
        list_start_index = selected_index;
    }
    if (selected_index >= list_start_index + items_per_page) {
        list_start_index = selected_index - items_per_page + 1;
    }
    if (session_count > 0 && list_start_index > session_count - items_per_page) {
         list_start_index = MAX(0, session_count - items_per_page);
    }
    
    int end_index = MIN(list_start_index + items_per_page, session_count);
    
    // 2. 计算垂直居中布局 (参考 nextui.c)
    int safe_area_top = SCALE1(PADDING + PILL_SIZE);
    int safe_area_bottom = screen->h - SCALE1(PADDING + PILL_SIZE);
    int available_height = safe_area_bottom - safe_area_top;
    int list_block_height = MAIN_ROW_COUNT * PILL_HEIGHT;
    int list_oy = safe_area_top + ((available_height - list_block_height) / 2);

    // 3. 渲染可见的列表项
    for (int i = list_start_index; i < end_index; i++) {
        int row = i - list_start_index;
        int pill_y = list_oy + (row * PILL_HEIGHT);
        
        // 只有选中的项才绘制背景气泡
        if (i == selected_index) {
            SDL_Rect pill_rect = {
                SCALE1(BUTTON_MARGIN),
                pill_y,
                screen->w - SCALE1(BUTTON_MARGIN * 2),
                PILL_HEIGHT
            };
            GFX_blitPillDark(ASSET_WHITE_PILL, screen, &pill_rect);
        }

        SDL_Color text_color = (i == selected_index) ? uintToColour(THEME_COLOR5_255) : uintToColour(THEME_COLOR4_255);
        
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font.large, sessions[i].name, text_color);
        if (text_surface) {
            SDL_Rect text_rect = {
                SCALE1(BUTTON_MARGIN + BUTTON_PADDING),
                pill_y + (PILL_HEIGHT - text_surface->h) / 2,
                text_surface->w,
                text_surface->h
            };
            SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
            SDL_FreeSurface(text_surface);
        }
    }
    
    // 4. 渲染滚动指示器 (如果需要)
    if (session_count > items_per_page) {
        #define SCROLL_WIDTH 24
        #define SCROLL_HEIGHT 4
        int ox = (screen->w - SCALE1(SCROLL_WIDTH)) / 2;
        int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
        if (list_start_index > 0)
            GFX_blitAsset(ASSET_SCROLL_UP, NULL, screen, &(SDL_Rect){ox, SCALE1(PADDING + PILL_SIZE)});
        if (end_index < session_count)
            GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen, &(SDL_Rect){ox, screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + oy});
    }

    // 5. 如果没有会话，显示提示信息
    if (session_count == 0) {
        GFX_blitMessage(font.large, "No sessions. Press (Y) to create one.", screen, NULL);
    }
}


// 渲染模拟的终端视图
static void render_terminal_view(void) {
    GFX_clear(screen); // 清理为黑屏

    // 绘制一个简单的提示和一个闪烁的光标
    char prompt[128];
    snprintf(prompt, sizeof(prompt), "%s $", sessions[selected_index].name);
    
    SDL_Color text_color = COLOR_WHITE;
    SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font.large, prompt, text_color);
    if (text_surface) {
        SDL_Rect text_rect = {SCALE1(10), SCALE1(10), text_surface->w, text_surface->h};
        SDL_BlitSurface(text_surface, NULL, screen, &text_rect);

        // 绘制闪烁的光标
        static uint32_t last_blink_time = 0;
        static bool show_cursor = true;
        if (SDL_GetTicks() - last_blink_time > 500) {
            show_cursor = !show_cursor;
            last_blink_time = SDL_GetTicks();
        }

        if (show_cursor) {
            SDL_Rect cursor_rect = {text_rect.x + text_surface->w + SCALE1(5), text_rect.y, SCALE1(10), text_surface->h};
            SDL_FillRect(screen, &cursor_rect, RGB_WHITE);
        }

        SDL_FreeSurface(text_surface);
    }
    
    GFX_blitMessage(font.medium, "This is a simulated terminal.\nPress (B) to exit.", screen, &(SDL_Rect){0, screen->h / 2, screen->w, SCALE1(60)});
}


// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = false;
    current_view = VIEW_LIST;
    session_count = 0;
    selected_index = 0;
    list_start_index = 0;
    next_session_id = 1;

    // 在运行时初始化 items_per_page
    items_per_page = MAIN_ROW_COUNT;

    SysUI_Init(screen, &font);
    PWR_init(); // 初始化电源管理
    
    // 初始创建一个会话
    create_session();

    return 0;
}

static int plugin_run() {
    int dirty = 1;
    int show_setting = 0; // 0=无, 1=亮度, 2=音量, 3=色温
    bool input_blocked_by_sysui = false;

    while (!quit_plugin) {
        PAD_poll();
        PWR_update(&dirty, &show_setting, NULL, NULL); // 总是运行以处理电源键

        // 根据当前视图处理输入
        if (current_view == VIEW_LIST) {
            input_blocked_by_sysui = SysUI_Update(); // 在列表视图中处理系统UI事件

            if (!input_blocked_by_sysui) {
                 if (PAD_justPressed(BTN_B)) {
                    quit_plugin = true;
                } else if (PAD_justRepeated(BTN_UP)) {
                    if (session_count > 0) {
                        selected_index = (selected_index - 1 + session_count) % session_count;
                        dirty = 1;
                    }
                } else if (PAD_justRepeated(BTN_DOWN)) {
                    if (session_count > 0) {
                        selected_index = (selected_index + 1) % session_count;
                        dirty = 1;
                    }
                } else if (PAD_justPressed(BTN_Y)) {
                    create_session();
                    dirty = 1;
                } else if (PAD_justPressed(BTN_X)) {
                    delete_session();
                    dirty = 1;
                } else if (PAD_justPressed(BTN_A)) {
                    if (session_count > 0) {
                        current_view = VIEW_TERMINAL;
                        SysUI_SetFullscreen(true);
                        dirty = 1;
                    }
                }
            }
        } else if (current_view == VIEW_TERMINAL) {
            // 在终端视图中，不调用 SysUI_Update()
            if (PAD_justPressed(BTN_B)) {
                current_view = VIEW_LIST;
                SysUI_SetFullscreen(false);
                dirty = 1;
            }
            dirty = 1; // 终端视图总是需要重绘以实现光标闪烁
        }

        if (dirty) {
            if (current_view == VIEW_LIST) {
                GFX_clear(screen);
                SysUI_SetTitle("Terminal");
                SysUI_SetBottomHints("Y", "Create", "X", "Delete", "A", "Enter");
                render_session_list();
                SysUI_Render(); // 绘制顶部和底部栏 (包括亮度/音量等浮层)
            } else {
                render_terminal_view(); // 渲染终端，不含SysUI
            }
            GFX_flip(screen);
            dirty = 0;
        }

        GFX_sync();
    }
    return 0;
}

static void plugin_quit(void) {
    // 确保退出时恢复非全屏状态
    SysUI_SetFullscreen(false);
    SysUI_Quit();
    PWR_quit(); // 清理电源管理
}

// --- 插件导出 ---

static NextUI_Plugin terminal_plugin_export = {
    .name = "Terminal",
    .display_path = SDCARD_PATH "/Tools/System", // 显示在工具/系统分类下
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

NextUI_Plugin* GetPlugin(void) {
    return &terminal_plugin_export;
}

