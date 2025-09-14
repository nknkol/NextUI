// terminal.c (完整修复版本 + 缩放功能)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "plugin.h"
#include "sysui.h"
#include <libtsm.h>

// --- 宏和枚举定义 ---
#define MAX_SESSIONS 5
#define PILL_HEIGHT SCALE1(PILL_SIZE)
#define TERMINAL_FONT_WIDTH 6
#define TERMINAL_FONT_HEIGHT 12
#define TLOG(fmt, ...) LOG_note(LOG_REALTIME, "[Terminal] " fmt, ##__VA_ARGS__)

// 缩放相关常量
#define MIN_SCALE 50  // 最小缩放50%
#define MAX_SCALE 200 // 最大缩放200%
#define SCALE_STEP 10 // 每次缩放步长10%

typedef enum {
    VIEW_LIST,
    VIEW_TERMINAL,
} ViewState;

typedef struct TerminalSession {
    int id;
    char name[64];
    bool active;
    bool initialized;
    pid_t pid;
    int ptm_fd;
    struct tsm_screen *screen;
    struct tsm_vte *vte;
} TerminalSession;

// 面板相关枚举和定义
typedef enum {
    PANEL_SNIPPETS,
    PANEL_NUMBERS, 
    PANEL_SYMBOLS,
    PANEL_HELP
} PanelMode;

// --- 前向声明 ---
static void render_right_panel(int panel_x, int panel_y, int panel_w, int panel_h);

// --- 静态变量 ---
static SDL_Surface* screen;
static bool quit_plugin = false;
static ViewState current_view = VIEW_LIST;
static TTF_Font* mono_font = NULL;

static TerminalSession sessions[MAX_SESSIONS];
static int session_count = 0;
static int next_session_id = 1;
static int active_session_idx = -1;
static int selected_index = 0;
static int list_start_index = 0;
static int items_per_page = 0;

static bool osk_active = true;
static int osk_x = 0, osk_y = 0;

// 缩放相关变量
static int terminal_scale = 100; // 默认100%缩放
static bool scale_changed = false;

// 键盘布局
static const char* osk_layout[] = {
    "1234567890-=",     // 数字行
    "qwertyuiop[]",     // 第一字母行  
    "asdfghjkl;'\\",    // 第二字母行
    "zxcvbnm,./",       // 第三字母行
    ""                  // 功能键行（单独处理）
};
#define OSK_ROWS 5

// 功能键定义
static const char* function_keys[] = {
    "ESC", "TAB", "RET", "BS", "SPC", "↑", "↓", "HIDE"
};
#define FUNC_KEY_COUNT 8

// 面板相关变量 - 默认改为HELP
static int panel_selected = 0;
static int panel_scroll = 0;
static PanelMode panel_mode = PANEL_HELP;

// 终端滚动相关变量
static int terminal_scroll_offset = 0;
static bool is_scrolling = false;

// 快捷代码段
static const char* quick_snippets[] = {
    "ls -la", "cd ..", "pwd", "cat ", "nano ",
    "mkdir ", "rm -rf ", "chmod +x ", "grep -r ",
    "find . -name ", "tar -xzf ", "df -h", 
    "ps aux", "top", "history"
};
#define SNIPPET_COUNT (sizeof(quick_snippets) / sizeof(quick_snippets[0]))

// --- 回调函数 ---
static void vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data) {
    TerminalSession *s = (TerminalSession*)data;
    if (s && s->ptm_fd >= 0) {
        if (write(s->ptm_fd, u8, len) < 0) { /* 忽略错误 */ }
    }
}

static int draw_cb(struct tsm_screen *screen, uint64_t id, const uint32_t *ch,
                   size_t len, unsigned int width, unsigned int posx,
                   unsigned int posy, const struct tsm_screen_attr *attr,
                   tsm_age_t age, void *data)
{
    SDL_Surface *target_screen = (SDL_Surface*)data;
    char buf[8] = {0};
    if (len == 0 || !mono_font) return 0;
    tsm_ucs4_to_utf8(ch[0], buf);
    
    // 应用缩放
    int scaled_font_width = (SCALE1(TERMINAL_FONT_WIDTH) * terminal_scale) / 100;
    int scaled_font_height = (SCALE1(TERMINAL_FONT_HEIGHT) * terminal_scale) / 100;
    
    SDL_Color fg = { attr->fr, attr->fg, attr->fb, 255 };
    SDL_Color bg = { attr->br, attr->bg, attr->bb, 255 };
    SDL_Rect bg_rect = {
        SCALE1(PADDING) + posx * scaled_font_width,
        SCALE1(PADDING) + posy * scaled_font_height,
        width * scaled_font_width,
        scaled_font_height
    };
    SDL_FillRect(target_screen, &bg_rect, SDL_MapRGB(target_screen->format, bg.r, bg.g, bg.b));
    
    if (buf[0] != 0 && buf[0] != ' ') {
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, buf, fg);
        if (text_surface) {
            // 如果需要缩放文字，创建缩放后的surface
            if (terminal_scale != 100) {
                int new_w = (text_surface->w * terminal_scale) / 100;
                int new_h = (text_surface->h * terminal_scale) / 100;
                if (new_w > 0 && new_h > 0) {
                    SDL_Surface* scaled_surface = SDL_CreateRGBSurface(0, new_w, new_h, 
                        text_surface->format->BitsPerPixel,
                        text_surface->format->Rmask, text_surface->format->Gmask,
                        text_surface->format->Bmask, text_surface->format->Amask);
                    if (scaled_surface) {
                        SDL_BlitScaled(text_surface, NULL, scaled_surface, NULL);
                        SDL_Rect text_rect = { bg_rect.x, bg_rect.y, scaled_surface->w, scaled_surface->h };
                        SDL_BlitSurface(scaled_surface, NULL, target_screen, &text_rect);
                        SDL_FreeSurface(scaled_surface);
                    }
                }
            } else {
                SDL_Rect text_rect = { bg_rect.x, bg_rect.y, text_surface->w, text_surface->h };
                SDL_BlitSurface(text_surface, NULL, target_screen, &text_rect);
            }
            SDL_FreeSurface(text_surface);
        }
    }
    return 0;
}

// --- 辅助函数 ---
static void cleanup_session_resources(TerminalSession* session) {
    if (session && session->active) {
        TLOG("cleanup_session_resources: Cleaning up session ID %d\n", session->id);
        if (session->initialized) {
            TLOG("cleanup_session_resources: Killing PID %d\n", session->pid);
            kill(session->pid, SIGKILL);
            waitpid(session->pid, NULL, 0);
            close(session->ptm_fd);
            if(session->vte) tsm_vte_unref(session->vte);
            if(session->screen) tsm_screen_unref(session->screen);
        }
        memset(session, 0, sizeof(TerminalSession));
    }
}

// terminal.c (替换整个函数)
static void update_terminal_size(TerminalSession* session) {
    if (!session || !session->screen) return;

    // 根据缩放计算字体大小
    int scaled_font_width = (SCALE1(TERMINAL_FONT_WIDTH) * terminal_scale) / 100;
    int scaled_font_height = (SCALE1(TERMINAL_FONT_HEIGHT) * terminal_scale) / 100;

    // 安全检查：防止缩放导致字体尺寸为0，避免除零错误
    if (scaled_font_width <= 0) scaled_font_width = 1;
    if (scaled_font_height <= 0) scaled_font_height = 1;
    
    // 动态计算为OSK预留的高度
    int osk_reserved_height = 0;
    if (osk_active) {
        // 这个高度应该与 render_osk 中实际占用的空间相匹配
        int key_height = SCALE1(12);
        int key_spacing = SCALE1(1);
        int osk_area_h = OSK_ROWS * (key_height + key_spacing) + SCALE1(5) + SCALE1(15); // 键盘区 + 底部边距 + 顶部提示区
        osk_reserved_height = osk_area_h;
    }

    // 计算终端的宽度和高度
    unsigned int term_w = (screen->w - SCALE1(PADDING * 2)) / scaled_font_width;
    unsigned int term_h = 0;
    
    // 先用有符号整数计算，防止负数回绕
    int available_height = screen->h - SCALE1(PADDING * 2) - osk_reserved_height;
    if (available_height > 0) {
        term_h = available_height / scaled_font_height;
    }

    // --- 关键修复 ---
    // 安全检查：确保终端尺寸至少为1x1，防止无效尺寸导致libtsm状态错误
    if (term_w < 1) term_w = 1;
    if (term_h < 1) term_h = 1;

    tsm_screen_resize(session->screen, term_w, term_h);
    
    // 同时更新pty的窗口大小，这样像 `ls` 这样的命令才能正确感知宽度
    struct winsize ws = { .ws_row = term_h, .ws_col = term_w };
    ioctl(session->ptm_fd, TIOCSWINSZ, &ws);
    
    TLOG("Terminal resized to %ux%u (scale: %d%%, osk: %s)\n", term_w, term_h, terminal_scale, osk_active ? "on" : "off");
}

static bool initialize_real_terminal(TerminalSession* session) {
    TLOG("initialize_real_terminal: START for session ID %d\n", session->id);
    if (session->initialized) { TLOG("initialize_real_terminal: Already initialized.\n"); return true; }

    int ptm_fd = -1, pts_fd = -1;
    pid_t pid;

    TLOG("initialize_real_terminal: Calling openpty()...\n");
    if (openpty(&ptm_fd, &pts_fd, NULL, NULL, NULL) < 0) { TLOG("initialize_real_terminal: openpty() FAILED: %s\n", strerror(errno)); return false; }
    TLOG("initialize_real_terminal: openpty() OK. ptm_fd=%d\n", ptm_fd);

    TLOG("initialize_real_terminal: Calling fork()...\n");
    pid = fork();
    if (pid < 0) { TLOG("initialize_real_terminal: fork() FAILED: %s\n", strerror(errno)); close(ptm_fd); close(pts_fd); return false; }
    
    if (pid == 0) { // 子进程
        close(ptm_fd); 
        setsid(); 
        ioctl(pts_fd, TIOCSCTTY, NULL);
        dup2(pts_fd, STDIN_FILENO); 
        dup2(pts_fd, STDOUT_FILENO); 
        dup2(pts_fd, STDERR_FILENO);
        close(pts_fd);
        
        // 设置环境变量
        setenv("TERM", "xterm-256color", 1); 
        setenv("PATH", SYSTEM_PATH "/bin:/usr/bin:/bin:/usr/sbin:/sbin", 1); // 建议把你自己的bin目录也加到PATH里
        setenv("HOME", SDCARD_PATH, 1);
        setenv("USER", "root", 1);
        setenv("LOGNAME", "root", 1);
        
        // --- 语言和数据文件路径设置 ---
        setenv("LANG", "zh_CN.UTF-8", 1);
        setenv("TEXTDOMAINDIR", SYSTEM_PATH "/lang", 1); // 用于语言包 (.mo)
        setenv("XDG_DATA_DIRS", SYSTEM_PATH, 1);         // 用于 share 目录
        setenv("XDG_CONFIG_DIRS", SYSTEM_PATH, 1);       // 用于 etc 目录
        // 切换到SDCARD目录
        if (chdir(SDCARD_PATH) != 0) {
            // 如果SDCARD_PATH不可访问，切换到根目录
            chdir("/");
        }
        
        char *args[] = {BIN_PATH "/fish", NULL};
        execv(args[0], args);
        exit(1);
    }

    close(pts_fd);
    fcntl(ptm_fd, F_SETFL, fcntl(ptm_fd, F_GETFL, 0) | O_NONBLOCK);
    TLOG("initialize_real_terminal: Parent process configured for PID %d.\n", pid);
    
    TLOG("initialize_real_terminal: Calling tsm_screen_new()...\n");
    if (tsm_screen_new(&session->screen, NULL, NULL) != 0) { TLOG("initialize_real_terminal: tsm_screen_new() FAILED.\n"); close(ptm_fd); kill(pid, SIGKILL); return false; }
    TLOG("initialize_real_terminal: tsm_screen_new() OK.\n");

    // 设置滚动缓冲区大小
    tsm_screen_set_max_sb(session->screen, 1000); // 1000行滚动历史
    TLOG("initialize_real_terminal: Set scrollback buffer to 1000 lines.\n");

    TLOG("initialize_real_terminal: Calling tsm_vte_new()...\n");
    if (tsm_vte_new(&session->vte, session->screen, vte_write_cb, session, NULL, NULL) != 0) { TLOG("initialize_real_terminal: tsm_vte_new() FAILED.\n"); tsm_screen_unref(session->screen); close(ptm_fd); kill(pid, SIGKILL); return false; }
    TLOG("initialize_real_terminal: tsm_vte_new() OK.\n");
    
    // 初始化终端大小
    update_terminal_size(session);

    session->pid = pid;
    session->ptm_fd = ptm_fd;
    session->initialized = true;

    TLOG("initialize_real_terminal: END (SUCCESS)\n");
    return true;
}

static void create_session(void) {
    if (session_count >= MAX_SESSIONS) return;
    int slot = -1;
    for (int i = 0; i < MAX_SESSIONS; ++i) { if (!sessions[i].active) { slot = i; break; } }
    if (slot == -1) return;

    TerminalSession* new_session = &sessions[slot];
    memset(new_session, 0, sizeof(TerminalSession));
    new_session->id = next_session_id++;
    snprintf(new_session->name, sizeof(new_session->name), "Session %d", new_session->id);
    new_session->active = true;
    session_count++;
    selected_index = slot;
    TLOG("create_session: Created placeholder for session '%s'.\n", new_session->name);
}

static void delete_session(void) {
    if (session_count == 0) return;
    TLOG("delete_session: Deleting session ID %d\n", sessions[selected_index].id);
    
    cleanup_session_resources(&sessions[selected_index]);
    session_count--;
    
    if (session_count > 0) {
        int new_select = -1;
        for (int i = 0; i < MAX_SESSIONS; ++i) { if (sessions[i].active) { new_select = i; break; } }
        selected_index = (new_select != -1) ? new_select : 0;
    } else { selected_index = 0; }
    TLOG("delete_session: Done. Active sessions: %d\n", session_count);
}

// --- 渲染函数 ---
static void render_session_list(void) {
    if (session_count > 0) {
        int visible_count = 0;
        int visible_indices[MAX_SESSIONS];
        for (int i = 0; i < MAX_SESSIONS; ++i) { if (sessions[i].active) { visible_indices[visible_count++] = i; } }
        int current_visible_idx = -1;
        for (int i = 0; i < visible_count; ++i) { if (visible_indices[i] == selected_index) { current_visible_idx = i; break; } }
        if (current_visible_idx < list_start_index) list_start_index = current_visible_idx;
        if (current_visible_idx >= list_start_index + items_per_page) list_start_index = current_visible_idx - items_per_page + 1;
        int list_oy = SCALE1(PADDING + PILL_SIZE) + ((screen->h - (SCALE1(PADDING + PILL_SIZE) * 2) - (MAIN_ROW_COUNT * PILL_HEIGHT)) / 2);
        for (int i = 0; i < items_per_page; ++i) {
            int visible_idx = list_start_index + i;
            if (visible_idx >= visible_count) break;
            int session_idx = visible_indices[visible_idx];
            int pill_y = list_oy + (i * PILL_HEIGHT);
            if (session_idx == selected_index) {
                SDL_Rect pill_rect = {SCALE1(BUTTON_MARGIN), pill_y, screen->w - SCALE1(BUTTON_MARGIN * 2), PILL_HEIGHT};
                GFX_blitPillDark(ASSET_WHITE_PILL, screen, &pill_rect);
            }
            SDL_Color text_color = (session_idx == selected_index) ? uintToColour(THEME_COLOR5_255) : uintToColour(THEME_COLOR4_255);
            SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font.large, sessions[session_idx].name, text_color);
            if (text_surface) {
                SDL_Rect text_rect = {SCALE1(BUTTON_MARGIN + BUTTON_PADDING), pill_y + (PILL_HEIGHT - text_surface->h) / 2, text_surface->w, text_surface->h};
                SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                SDL_FreeSurface(text_surface);
            }
        }
    } else {
        GFX_blitMessage(font.large, "No sessions. Press (Y) to create one.", screen, NULL);
    }
}

// 渲染右侧快捷面板
static void render_right_panel(int panel_x, int panel_y, int panel_w, int panel_h) {
    // 面板背景
    SDL_Rect panel_bg = {panel_x, panel_y, panel_w, panel_h};
    SDL_FillRect(screen, &panel_bg, SDL_MapRGB(screen->format, 22, 22, 22));
    
    // 面板标题
    const char* titles[] = {"SNIPPETS", "NUMBERS", "SYMBOLS", "HELP"};
    SDL_Color title_color = {120, 150, 255, 255};
    SDL_Surface* title_surface = TTF_RenderUTF8_Blended(mono_font, titles[panel_mode], title_color);
    if (title_surface) {
        SDL_Rect title_rect = {
            panel_x + (panel_w - title_surface->w) / 2,
            panel_y + SCALE1(2),
            title_surface->w, title_surface->h
        };
        SDL_BlitSurface(title_surface, NULL, screen, &title_rect);
        SDL_FreeSurface(title_surface);
    }
    
    int content_y = panel_y + SCALE1(16);
    int line_height = SCALE1(12);
    int max_lines = (panel_h - SCALE1(20)) / line_height;
    
    switch (panel_mode) {
        case PANEL_SNIPPETS: {
            // 显示代码片段
            for (int i = 0; i < SNIPPET_COUNT && i < max_lines; i++) {
                int visible_idx = panel_scroll + i;
                if (visible_idx >= SNIPPET_COUNT) break;
                
                bool selected = (visible_idx == panel_selected);
                SDL_Color bg_color = selected ? 
                    (SDL_Color){40, 70, 140, 255} : (SDL_Color){22, 22, 22, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){180, 180, 180, 255};
                
                SDL_Rect line_bg = {panel_x + SCALE1(2), content_y + i * line_height, panel_w - SCALE1(4), line_height};
                SDL_FillRect(screen, &line_bg, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, quick_snippets[visible_idx], text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        panel_x + SCALE1(4),
                        content_y + i * line_height + (line_height - text_surface->h) / 2,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
            break;
        }
        
        case PANEL_NUMBERS: {
            // 数字快捷键面板
            const char* numbers[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
            int grid_cols = 5;
            int btn_w = (panel_w - SCALE1(12)) / grid_cols;
            int btn_h = SCALE1(18);
            
            for (int i = 0; i < 10; i++) {
                int col = i % grid_cols;
                int row = i / grid_cols;
                int btn_x = panel_x + SCALE1(4) + col * btn_w;
                int btn_y = content_y + row * (btn_h + SCALE1(2));
                
                bool selected = (i == panel_selected);
                SDL_Color bg_color = selected ? 
                    (SDL_Color){60, 100, 200, 255} : (SDL_Color){35, 35, 35, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){200, 200, 200, 255};
                
                SDL_Rect btn_rect = {btn_x, btn_y, btn_w - SCALE1(1), btn_h};
                SDL_FillRect(screen, &btn_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, numbers[i], text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        btn_x + (btn_w - text_surface->w) / 2,
                        btn_y + (btn_h - text_surface->h) / 2,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
            break;
        }
        
        case PANEL_SYMBOLS: {
            // 符号快捷键面板
            const char* symbols[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", 
                                    "[", "]", "{", "}", "|", "\\", "<", ">", "?", "~"};
            int grid_cols = 5;
            int btn_w = (panel_w - SCALE1(12)) / grid_cols;
            int btn_h = SCALE1(16);
            
            for (int i = 0; i < 20; i++) {
                int col = i % grid_cols;
                int row = i / grid_cols;
                int btn_x = panel_x + SCALE1(4) + col * btn_w;
                int btn_y = content_y + row * (btn_h + SCALE1(2));
                
                bool selected = (i == panel_selected);
                SDL_Color bg_color = selected ? 
                    (SDL_Color){60, 100, 200, 255} : (SDL_Color){35, 35, 35, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){200, 200, 200, 255};
                
                SDL_Rect btn_rect = {btn_x, btn_y, btn_w - SCALE1(1), btn_h};
                SDL_FillRect(screen, &btn_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, symbols[i], text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        btn_x + (btn_w - text_surface->w) / 2,
                        btn_y + (btn_h - text_surface->h) / 2,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
            break;
        }
        
        case PANEL_HELP: {
            // 帮助信息面板
            const char* help_lines[] = {
                "SHORTCUTS:",
                "Y = Space",
                "X = Backspace", 
                "START = Enter",
                "",
                "NAVIGATION:",
                "D-PAD = Move",
                "A = Select",
                "B = Back",
                "",
                "SCALING:",
                "SEL+R1 = Zoom In",
                "SEL+L1 = Zoom Out",
                "",
                "PANEL:",
                "R1 = Switch",
                "SELECT = Focus"
            };
            
            for (int i = 0; i < 17 && i < max_lines; i++) {
                if (strlen(help_lines[i]) == 0) continue; // 跳过空行
                
                SDL_Color text_color;
                if (strstr(help_lines[i], ":")) {
                    text_color = (SDL_Color){100, 140, 255, 255}; // 标题颜色
                } else {
                    text_color = (SDL_Color){160, 160, 160, 255}; // 普通文本
                }
                
                SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, help_lines[i], text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        panel_x + SCALE1(4),
                        content_y + i * line_height,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
            break;
        }
    }
}

// 分屏虚拟键盘渲染
static void render_osk(void) {
    if (!osk_active || !mono_font) return;
    
    // 计算分屏尺寸
    int keyboard_width = screen->w * 60 / 100;  // 左侧60%
    int panel_width = screen->w - keyboard_width - SCALE1(4); // 右侧剩余部分
    
    // 键盘参数 - 紧凑尺寸
    int key_width = SCALE1(14);
    int key_height = SCALE1(12);
    int key_spacing = SCALE1(1);
    
    int osk_h = OSK_ROWS * (key_height + key_spacing);
    int osk_y_start = screen->h - osk_h - SCALE1(5);
    
    // 整体背景
    SDL_Rect full_bg = {0, osk_y_start - SCALE1(3), screen->w, osk_h + SCALE1(8)};
    SDL_FillRect(screen, &full_bg, SDL_MapRGB(screen->format, 12, 12, 12));
    
    // 左侧键盘背景
    SDL_Rect kb_bg = {0, osk_y_start - SCALE1(2), keyboard_width, osk_h + SCALE1(4)};
    SDL_FillRect(screen, &kb_bg, SDL_MapRGB(screen->format, 18, 18, 18));
    
    // 渲染键盘（左侧）
    for (int row = 0; row < OSK_ROWS; ++row) {
        int row_y = osk_y_start + row * (key_height + key_spacing);
        
        if (row == 4) { // 功能键行
            int total_width = FUNC_KEY_COUNT * key_width + (FUNC_KEY_COUNT - 1) * key_spacing;
            int start_x = (keyboard_width - total_width) / 2;
            
            for (int col = 0; col < FUNC_KEY_COUNT; col++) {
                int key_x = start_x + col * (key_width + key_spacing);
                bool selected = (row == osk_y && col == osk_x);
                
                SDL_Color bg_color = selected ? 
                    (SDL_Color){50, 90, 180, 255} : (SDL_Color){30, 30, 30, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){150, 150, 150, 255};
                
                SDL_Rect key_rect = {key_x, row_y, key_width, key_height};
                SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                if (selected) {
                    SDL_Rect border = {key_x-1, row_y-1, key_width+2, key_height+2};
                    SDL_FillRect(screen, &border, SDL_MapRGB(screen->format, 180, 180, 180));
                    SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                }
                
                SDL_Surface *text_surface = TTF_RenderUTF8_Blended(mono_font, function_keys[col], text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        key_x + (key_width - text_surface->w) / 2,
                        row_y + (key_height - text_surface->h) / 2,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
        } else { // 普通字符行
            const char *row_str = osk_layout[row];
            int key_count = strlen(row_str);
            int total_width = key_count * key_width + (key_count - 1) * key_spacing;
            int start_x = (keyboard_width - total_width) / 2;
            
            for (int col = 0; col < key_count; col++) {
                char key_char = row_str[col];
                int key_x = start_x + col * (key_width + key_spacing);
                bool selected = (row == osk_y && col == osk_x);
                
                SDL_Color bg_color = selected ? 
                    (SDL_Color){50, 90, 180, 255} : (SDL_Color){30, 30, 30, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){150, 150, 150, 255};
                
                SDL_Rect key_rect = {key_x, row_y, key_width, key_height};
                SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                if (selected) {
                    SDL_Rect border = {key_x-1, row_y-1, key_width+2, key_height+2};
                    SDL_FillRect(screen, &border, SDL_MapRGB(screen->format, 180, 180, 180));
                    SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                }
                
                char key_text[2] = {key_char, 0};
                SDL_Surface *text_surface = TTF_RenderUTF8_Blended(mono_font, key_text, text_color);
                if (text_surface) {
                    SDL_Rect text_rect = {
                        key_x + (key_width - text_surface->w) / 2,
                        row_y + (key_height - text_surface->h) / 2,
                        text_surface->w, text_surface->h
                    };
                    SDL_BlitSurface(text_surface, NULL, screen, &text_rect);
                    SDL_FreeSurface(text_surface);
                }
            }
        }
    }
    
    // 渲染右侧面板
    render_right_panel(keyboard_width + SCALE1(2), osk_y_start - SCALE1(2), panel_width, osk_h + SCALE1(4));
    
    // 分割线
    SDL_Rect divider = {keyboard_width, osk_y_start - SCALE1(2), SCALE1(2), osk_h + SCALE1(4)};
    SDL_FillRect(screen, &divider, SDL_MapRGB(screen->format, 60, 60, 60));
    
    // 底部提示
    SDL_Color hint_color = {100, 100, 100, 255};
    SDL_Surface* hint_surface = TTF_RenderUTF8_Blended(mono_font, 
        "L1: Toggle | R1: Panel | L2/R2: Scroll | SELECT: Focus | SEL+R1/L1: Scale", hint_color);
    if (hint_surface) {
        SDL_Rect hint_rect = {
            SCALE1(3), 
            osk_y_start - SCALE1(15), 
            hint_surface->w, 
            hint_surface->h
        };
        SDL_BlitSurface(hint_surface, NULL, screen, &hint_rect);
        SDL_FreeSurface(hint_surface);
    }
}

static void render_terminal_view(void) {
    GFX_clear(screen);
    TerminalSession* session = &sessions[selected_index];
    if (session->initialized) { 
        tsm_screen_draw(session->screen, draw_cb, screen); 
        
        // 渲染滚动指示器
        if (is_scrolling) {
            SDL_Color indicator_color = {100, 150, 255, 255};
            char scroll_text[32];
            snprintf(scroll_text, sizeof(scroll_text), "SCROLL: %d", terminal_scroll_offset);
            SDL_Surface* scroll_surface = TTF_RenderUTF8_Blended(mono_font, scroll_text, indicator_color);
            if (scroll_surface) {
                SDL_Rect scroll_bg = {
                    screen->w - scroll_surface->w - SCALE1(10), 
                    SCALE1(5), 
                    scroll_surface->w + SCALE1(6), 
                    scroll_surface->h + SCALE1(4)
                };
                SDL_FillRect(screen, &scroll_bg, SDL_MapRGB(screen->format, 40, 40, 40));
                
                SDL_Rect scroll_rect = {
                    screen->w - scroll_surface->w - SCALE1(7), 
                    SCALE1(7), 
                    scroll_surface->w, 
                    scroll_surface->h
                };
                SDL_BlitSurface(scroll_surface, NULL, screen, &scroll_rect);
                SDL_FreeSurface(scroll_surface);
            }
            
            // 滚动提示
            SDL_Color hint_color = {80, 80, 80, 255};
            SDL_Surface* hint_surface = TTF_RenderUTF8_Blended(mono_font, "L2/R2: Scroll | Any key: Exit scroll", hint_color);
            if (hint_surface) {
                SDL_Rect hint_rect = {
                    SCALE1(5), 
                    SCALE1(5), 
                    hint_surface->w, 
                    hint_surface->h
                };
                SDL_BlitSurface(hint_surface, NULL, screen, &hint_rect);
                SDL_FreeSurface(hint_surface);
            }
        }
        
        // 显示缩放指示器
        if (terminal_scale != 100) {
            SDL_Color scale_color = {255, 200, 100, 255};
            char scale_text[32];
            snprintf(scale_text, sizeof(scale_text), "SCALE: %d%%", terminal_scale);
            SDL_Surface* scale_surface = TTF_RenderUTF8_Blended(mono_font, scale_text, scale_color);
            if (scale_surface) {
                SDL_Rect scale_bg = {
                    SCALE1(5), 
                    screen->h - SCALE1(25), 
                    scale_surface->w + SCALE1(6), 
                    scale_surface->h + SCALE1(4)
                };
                SDL_FillRect(screen, &scale_bg, SDL_MapRGB(screen->format, 40, 40, 40));
                
                SDL_Rect scale_rect = {
                    SCALE1(8), 
                    screen->h - SCALE1(23), 
                    scale_surface->w, 
                    scale_surface->h
                };
                SDL_BlitSurface(scale_surface, NULL, screen, &scale_rect);
                SDL_FreeSurface(scale_surface);
            }
        }
        
        render_osk(); 
    } else { 
        GFX_blitMessage(font.large, "Initializing session...", screen, NULL); 
    }
}

// --- 输入处理函数 ---
static void handle_list_input(void) {
    if (SysUI_Update()) return;
    if (PAD_justPressed(BTN_B)) { quit_plugin = true; }
    else if (PAD_justRepeated(BTN_UP) && session_count > 0) { int p=selected_index; do { selected_index=(selected_index-1+MAX_SESSIONS)%MAX_SESSIONS; } while(!sessions[selected_index].active&&selected_index!=p); }
    else if (PAD_justRepeated(BTN_DOWN) && session_count > 0) { int p=selected_index; do { selected_index=(selected_index+1)%MAX_SESSIONS; } while(!sessions[selected_index].active&&selected_index!=p); }
    else if (PAD_justPressed(BTN_Y)) { create_session(); }
    else if (PAD_justPressed(BTN_X)) { delete_session(); }
    else if (PAD_justPressed(BTN_A) && session_count > 0) {
        current_view = VIEW_TERMINAL;
        SysUI_SetFullscreen(true);
        if (!initialize_real_terminal(&sessions[selected_index])) {
            TLOG("handle_list_input: Failed to initialize session! Returning to list.\n");
            current_view = VIEW_LIST;
            SysUI_SetFullscreen(false);
        }
    }
}

static void handle_terminal_input(void) {
    TerminalSession* s = &sessions[selected_index];
    static bool focus_on_panel = false;

    // --- 1. 优先处理所有`SELECT`组合键 ---
    if (PAD_isPressed(BTN_SELECT)) {
        // 组合键: SELECT + R1 (放大)
        if (PAD_justPressed(BTN_R1)) {
            if (terminal_scale < MAX_SCALE) {
                terminal_scale += SCALE_STEP;
                scale_changed = true;
                if (s->screen) {
                    tsm_screen_sb_reset(s->screen);
                    terminal_scroll_offset = 0;
                    is_scrolling = false;
                }
                update_terminal_size(s);
                TLOG("Terminal scaled up to %d%%\n", terminal_scale);
            }
            return; // 消费事件并返回
        }
        
        // 组合键: SELECT + L1 (缩小)
        if (PAD_justPressed(BTN_L1)) {
            if (terminal_scale > MIN_SCALE) {
                terminal_scale -= SCALE_STEP;
                scale_changed = true;
                if (s->screen) {
                    tsm_screen_sb_reset(s->screen);
                    terminal_scroll_offset = 0;
                    is_scrolling = false;
                }
                update_terminal_size(s);
                TLOG("Terminal scaled down to %d%%\n", terminal_scale);
            }
            return; // 消费事件并返回
        }

        // 组合键: SELECT + START (切换焦点)
        if (PAD_justPressed(BTN_START)) {
            focus_on_panel = !focus_on_panel;
            TLOG("Focus switched to %s\n", focus_on_panel ? "panel" : "keyboard");
            return; // 消费事件并返回
        }

        // 如果按下了SELECT但不是有效的组合键, 阻止任何其他按键的单点操作
        // (例如防止按住SELECT再按X时触发退格)
        if (PAD_anyJustPressed()) {
            return;
        }
    }

    // --- 2. 处理滚动 (会中断后续操作) ---
    if (PAD_justPressed(BTN_L2) || PAD_justRepeated(BTN_L2)) {
        if (s->screen) { tsm_screen_sb_up(s->screen, 1); is_scrolling = true; }
        return;
    }
    if (PAD_justPressed(BTN_R2) || PAD_justRepeated(BTN_R2)) {
        if (s->screen) { tsm_screen_sb_down(s->screen, 1); }
        return;
    }
    if (is_scrolling && PAD_anyJustPressed()) {
        if (s->screen) tsm_screen_sb_reset(s->screen);
        is_scrolling = false;
    }

    // --- 3. 处理独立的单点按键功能 ---
    
    // R1键切换面板模式
    if (PAD_justPressed(BTN_R1)) {
        panel_mode = (panel_mode + 1) % 4;
        panel_selected = 0; panel_scroll = 0;
        TLOG("Panel mode switched to %d\n", panel_mode);
    }
    
    // L1键切换键盘显示
    if (PAD_justPressed(BTN_L1)) { 
        osk_active = !osk_active;
        update_terminal_size(s);
        TLOG("OSK toggled via L1, now %s\n", osk_active ? "visible" : "hidden");
    }

    // A键作为主要的回车/确认键
    if (PAD_justPressed(BTN_A)) {
        if (osk_active) {
            if (focus_on_panel) { // 面板确认
                if (panel_mode == PANEL_SNIPPETS) { if (panel_selected < SNIPPET_COUNT) write(s->ptm_fd, quick_snippets[panel_selected], strlen(quick_snippets[panel_selected])); } 
                else if (panel_mode == PANEL_NUMBERS) { char n = '0' + panel_selected; write(s->ptm_fd, &n, 1); } 
                else if (panel_mode == PANEL_SYMBOLS) { const char* sym[] = {"!","@","#","$","%","^","&","*","(",")","[","]","{","}","|","\\","<",">","?","~"}; if (panel_selected < 20) write(s->ptm_fd, sym[panel_selected], strlen(sym[panel_selected])); }
            } else { // 虚拟键盘确认
                if (osk_y == 4) {
                    const char* f_keys[] = {"\x1b", "\t", "\r", "\x7f", " ", "\x1b[A", "\x1b[B"};
                    if (osk_x < 7) write(s->ptm_fd, f_keys[osk_x], strlen(f_keys[osk_x]));
                    else { osk_active = false; update_terminal_size(s); } // HIDE
                } else { char ch = osk_layout[osk_y][osk_x]; write(s->ptm_fd, &ch, 1); }
            }
        } else { // 键盘隐藏时, A键直接作为回车
            char enter[] = "\r"; write(s->ptm_fd, enter, 1);
        }
    } else {
        // 方向键导航
        if (osk_active) {
            if (focus_on_panel) {
                if (PAD_justRepeated(BTN_UP)) { if (panel_mode == PANEL_SNIPPETS) { if (panel_selected > 0) panel_selected--; if (panel_selected < panel_scroll) panel_scroll=panel_selected; } else if (panel_mode == PANEL_NUMBERS || panel_mode == PANEL_SYMBOLS) { if (panel_selected >= 5) panel_selected -= 5; } } 
                else if (PAD_justRepeated(BTN_DOWN)) { if (panel_mode == PANEL_SNIPPETS) { if (panel_selected < SNIPPET_COUNT - 1) panel_selected++; int max_v = (screen->h / 4) / SCALE1(12)-1; if (panel_selected >= panel_scroll + max_v) panel_scroll=panel_selected-max_v+1; } else if (panel_mode == PANEL_NUMBERS) { if (panel_selected < 5) panel_selected += 5; } else if (panel_mode == PANEL_SYMBOLS) { if (panel_selected < 15) panel_selected += 5; } } 
                else if (PAD_justRepeated(BTN_LEFT)) { if ((panel_mode == PANEL_NUMBERS || panel_mode == PANEL_SYMBOLS) && (panel_selected % 5 > 0)) panel_selected--; } 
                else if (PAD_justRepeated(BTN_RIGHT)) { if ((panel_mode == PANEL_NUMBERS || panel_mode == PANEL_SYMBOLS) && (panel_selected % 5 < 4)) panel_selected++; }
            } else {
                if (PAD_justRepeated(BTN_UP) && osk_y > 0) osk_y--;
                if (PAD_justRepeated(BTN_DOWN) && osk_y < OSK_ROWS - 1) osk_y++;
                int max_col = (osk_y == 4) ? FUNC_KEY_COUNT - 1 : strlen(osk_layout[osk_y]) - 1;
                if (PAD_justRepeated(BTN_LEFT) && osk_x > 0) osk_x--;
                if (PAD_justRepeated(BTN_RIGHT) && osk_x < max_col) osk_x++;
                if (osk_x > max_col) osk_x = max_col;
            }
        }
    }
    
    // 全局快捷输入键 (Y, X)
    if (PAD_justPressed(BTN_Y)) { char space[] = " "; write(s->ptm_fd, space, 1); }
    if (PAD_justPressed(BTN_X)) { char bs[] = "\x7f"; write(s->ptm_fd, bs, 1); }
    
    // START 和 SELECT 单独按下无功能, 所以这里没有它们的处理代码

    // B 键返回主界面
    if (PAD_justPressed(BTN_B)) { 
        current_view = VIEW_LIST; 
        SysUI_SetFullscreen(false); 
    }
}

// --- 插件生命周期 ---
static int plugin_init(void* main_screen) {
    TLOG("====================================================\n");
    TLOG("plugin_init: START\n");
    
    // 重置所有静态变量 - 修复只能打开一次的问题
    quit_plugin = false;
    current_view = VIEW_LIST;
    active_session_idx = -1;
    selected_index = 0;
    list_start_index = 0;
    session_count = 0;
    next_session_id = 1;
    
    // 重置键盘状态
    osk_active = true;
    osk_x = 0;
    osk_y = 0;
    
    // 重置面板状态 - 默认显示Help
    panel_selected = 0;
    panel_scroll = 0;
    panel_mode = PANEL_HELP;
    
    // 重置滚动状态
    terminal_scroll_offset = 0;
    is_scrolling = false;
    
    // 重置缩放状态
    terminal_scale = 100;
    scale_changed = false;
    
    screen = (SDL_Surface*)main_screen;
    items_per_page = MAIN_ROW_COUNT;
    memset(sessions, 0, sizeof(sessions));
    TLOG("plugin_init: All static variables reset.\n");

    TLOG("plugin_init: Calling SysUI_Init()...\n");
    SysUI_Init(screen, &font);
    TLOG("plugin_init: SysUI_Init() OK.\n");
    
    char mono_font_path[MAX_PATH];
    snprintf(mono_font_path, sizeof(mono_font_path), "%s/mono.ttf", RES_PATH);
    TLOG("plugin_init: Trying font at %s\n", mono_font_path);
    if (!exists(mono_font_path)) {
       snprintf(mono_font_path, sizeof(mono_font_path), "%s/font.ttf", RES_PATH);
       TLOG("plugin_init: mono.ttf not found, fallback to %s\n", mono_font_path);
    }
    
    // 使用更小的字体尺寸
    mono_font = TTF_OpenFont(mono_font_path, SCALE1(9));
    if (!mono_font) { 
        TLOG("plugin_init: FAILED to load any font!\n"); 
        return -1; 
    }
    TLOG("plugin_init: Font loaded successfully with size %d.\n", SCALE1(9));

    create_session();
    
    TLOG("plugin_init: END (SUCCESS)\n");
    return 0;
}

static int plugin_run() {
    TLOG("plugin_run: START\n");
    int dirty = 1;
    int show_setting = 0;
    while (!quit_plugin) {
        PAD_poll();
        PWR_update(&dirty, &show_setting, NULL, NULL);

        int old_dirty = dirty; dirty = 0;
        if (current_view == VIEW_LIST) { handle_list_input(); }
        else if (current_view == VIEW_TERMINAL && sessions[selected_index].active) {
            TerminalSession* s = &sessions[selected_index];
            if (s->initialized) {
                handle_terminal_input();
                char buf[4096];
                ssize_t len = read(s->ptm_fd, buf, sizeof(buf));
                if (len > 0) { tsm_vte_input(s->vte, buf, len); dirty = 1; }
                else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    TLOG("plugin_run: PTY read error on session %d, deleting.\n", s->id);
                    delete_session(); current_view = VIEW_LIST; SysUI_SetFullscreen(false); dirty = 1;
                }
            } else { dirty = 1; }
        }
        
        // 如果缩放改变了，标记需要重绘
        if (scale_changed) {
            dirty = 1;
            scale_changed = false;
        }
        
        if (old_dirty || pad.just_pressed || pad.just_released || pad.just_repeated) dirty = 1;
        if (dirty) {
            GFX_clear(screen);
            if (current_view == VIEW_LIST) {
                SysUI_SetTitle("Terminal");
                SysUI_SetBottomHints("Y", "New", "X", "Delete", "A", "Enter");
                render_session_list();
                SysUI_Render();
            } else { render_terminal_view(); }
            GFX_flip(screen);
        }
        GFX_sync();
    }
    TLOG("plugin_run: END\n");
    return 0;
}

static void plugin_quit(void) {
    TLOG("plugin_quit: START\n");
    for (int i = 0; i < MAX_SESSIONS; ++i) { if (sessions[i].active) { cleanup_session_resources(&sessions[i]); } }
    if (mono_font) { TTF_CloseFont(mono_font); mono_font = NULL; }
    SysUI_SetFullscreen(false);
    SysUI_Quit();
    PWR_quit();
    TLOG("plugin_quit: END\n");
}

static NextUI_Plugin terminal_plugin_export = {
    .name = "Terminal", //.display_path = SDCARD_PATH "/Tools/System",
    .init = plugin_init, .run = plugin_run, .quit = plugin_quit,
};
NextUI_Plugin* GetPlugin(void) { return &terminal_plugin_export; }