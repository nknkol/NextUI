// terminal.c (已修正函数重复定义错误)

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
static const char* osk_layout[] = {
    "1234567890-=",     // 数字行
    "qwertyuiop[]",     // 第一字母行  
    "asdfghjkl;'\\",    // 第二字母行
    "zxcvbnm,./",       // 第三字母行
    ""                  // 功能键行（单独处理）
};

static const char* function_keys[] = {
    "ESC", "TAB", "RET", "BS", "SPC", "↑", "↓", "HIDE"
};
#define FUNC_KEY_COUNT 8

#define OSK_ROWS (sizeof(osk_layout) / sizeof(osk_layout[0]))

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
    SDL_Color fg = { attr->fr, attr->fg, attr->fb, 255 };
    SDL_Color bg = { attr->br, attr->bg, attr->bb, 255 };
    SDL_Rect bg_rect = {
        SCALE1(PADDING) + posx * SCALE1(TERMINAL_FONT_WIDTH),
        SCALE1(PADDING) + posy * SCALE1(TERMINAL_FONT_HEIGHT),
        width * SCALE1(TERMINAL_FONT_WIDTH),
        SCALE1(TERMINAL_FONT_HEIGHT)
    };
    SDL_FillRect(target_screen, &bg_rect, SDL_MapRGB(target_screen->format, bg.r, bg.g, bg.b));
    if (buf[0] != 0 && buf[0] != ' ') {
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(mono_font, buf, fg);
        if (text_surface) {
            SDL_Rect text_rect = { bg_rect.x, bg_rect.y, text_surface->w, text_surface->h };
            SDL_BlitSurface(text_surface, NULL, target_screen, &text_rect);
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
            
            // 设置环境变量 - 使用SDCARD_PATH作为HOME
            setenv("TERM", "xterm-256color", 1); 
            setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin", 1); 
            setenv("HOME", SDCARD_PATH, 1);  // 使用定义的SDCARD_PATH
            setenv("USER", "root", 1);
            setenv("LOGNAME", "root", 1);
            
            // 切换到SDCARD目录
            if (chdir(SDCARD_PATH) != 0) {
                // 如果SDCARD_PATH不可访问，切换到根目录
                chdir("/");
            }
            
            char *args[] = {"/bin/sh", "-l", NULL};  // 添加-l参数加载配置
            execv(args[0], args);
            exit(1);
        }

    close(pts_fd);
    fcntl(ptm_fd, F_SETFL, fcntl(ptm_fd, F_GETFL, 0) | O_NONBLOCK);
    TLOG("initialize_real_terminal: Parent process configured for PID %d.\n", pid);
    
    TLOG("initialize_real_terminal: Calling tsm_screen_new()...\n");
    if (tsm_screen_new(&session->screen, NULL, NULL) != 0) { TLOG("initialize_real_terminal: tsm_screen_new() FAILED.\n"); close(ptm_fd); kill(pid, SIGKILL); return false; }
    TLOG("initialize_real_terminal: tsm_screen_new() OK.\n");

    TLOG("initialize_real_terminal: Calling tsm_vte_new()...\n");
    if (tsm_vte_new(&session->vte, session->screen, vte_write_cb, session, NULL, NULL) != 0) { TLOG("initialize_real_terminal: tsm_vte_new() FAILED.\n"); tsm_screen_unref(session->screen); close(ptm_fd); kill(pid, SIGKILL); return false; }
    TLOG("initialize_real_terminal: tsm_vte_new() OK.\n");
    
    unsigned int term_w = (screen->w - SCALE1(PADDING * 2)) / SCALE1(TERMINAL_FONT_WIDTH);
    unsigned int term_h = (screen->h - SCALE1(PADDING * 2) - ((OSK_ROWS + 2) * SCALE1(TERMINAL_FONT_HEIGHT))) / SCALE1(TERMINAL_FONT_HEIGHT);
    tsm_screen_resize(session->screen, term_w, term_h);
    TLOG("initialize_real_terminal: Resized screen to %ux%u\n", term_w, term_h);

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
static void render_osk(void) {
    if (!osk_active || !mono_font) return;
    
    // 大幅缩小尺寸
    int key_width = SCALE1(18);   // 从30减少到18
    int key_height = SCALE1(16);  // 从22减少到16  
    int key_spacing = SCALE1(1);  // 从2减少到1
    
    int osk_h = OSK_ROWS * (key_height + key_spacing);
    int osk_y_start = screen->h - osk_h - SCALE1(3); // 减少底部边距
    
    // 键盘背景 - 更紧凑
    SDL_Rect bg = {0, osk_y_start - SCALE1(2), screen->w, osk_h + SCALE1(4)};
    SDL_FillRect(screen, &bg, SDL_MapRGB(screen->format, 15, 15, 15));
    
    for (int row = 0; row < OSK_ROWS; ++row) {
        int row_y = osk_y_start + row * (key_height + key_spacing);
        
        if (row == 4) { // 功能键行
            int total_width = FUNC_KEY_COUNT * key_width + (FUNC_KEY_COUNT - 1) * key_spacing;
            int start_x = (screen->w - total_width) / 2;
            
            for (int col = 0; col < FUNC_KEY_COUNT; col++) {
                int key_x = start_x + col * (key_width + key_spacing);
                bool selected = (row == osk_y && col == osk_x);
                
                // 更深色的背景，减少视觉干扰
                SDL_Color bg_color = selected ? 
                    (SDL_Color){60, 100, 200, 255} : (SDL_Color){35, 35, 35, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){160, 160, 160, 255};
                
                SDL_Rect key_rect = {key_x, row_y, key_width, key_height};
                SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                // 简化边框
                if (selected) {
                    SDL_Rect border = {key_x-1, row_y-1, key_width+2, key_height+2};
                    SDL_FillRect(screen, &border, SDL_MapRGB(screen->format, 200, 200, 200));
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
            int start_x = (screen->w - total_width) / 2;
            
            for (int col = 0; col < key_count; col++) {
                char key_char = row_str[col];
                int key_x = start_x + col * (key_width + key_spacing);
                bool selected = (row == osk_y && col == osk_x);
                
                SDL_Color bg_color = selected ? 
                    (SDL_Color){60, 100, 200, 255} : (SDL_Color){35, 35, 35, 255};
                SDL_Color text_color = selected ? 
                    (SDL_Color){255, 255, 255, 255} : (SDL_Color){160, 160, 160, 255};
                
                SDL_Rect key_rect = {key_x, row_y, key_width, key_height};
                SDL_FillRect(screen, &key_rect, SDL_MapRGB(screen->format, bg_color.r, bg_color.g, bg_color.b));
                
                if (selected) {
                    SDL_Rect border = {key_x-1, row_y-1, key_width+2, key_height+2};
                    SDL_FillRect(screen, &border, SDL_MapRGB(screen->format, 200, 200, 200));
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
    
    // 简化提示信息，放在键盘内部
    SDL_Color hint_color = {100, 100, 100, 255};
    SDL_Surface* hint_surface = TTF_RenderUTF8_Blended(mono_font, 
        "L1: Toggle", hint_color);
    if (hint_surface) {
        SDL_Rect hint_rect = {
            SCALE1(3), 
            osk_y_start - SCALE1(12), 
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
    if (session->initialized) { tsm_screen_draw(session->screen, draw_cb, screen); render_osk(); } 
    else { GFX_blitMessage(font.large, "Initializing session...", screen, NULL); }
}
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
    
    // 键盘导航
    if (PAD_justRepeated(BTN_UP) && osk_y > 0) osk_y--;
    if (PAD_justRepeated(BTN_DOWN) && osk_y < OSK_ROWS - 1) osk_y++;
    
    // 处理左右导航
    int max_col = 0;
    if (osk_y == 4) { // 功能键行
        max_col = FUNC_KEY_COUNT - 1;
    } else {
        max_col = strlen(osk_layout[osk_y]) - 1;
    }
    
    if (PAD_justRepeated(BTN_LEFT) && osk_x > 0) osk_x--;
    if (PAD_justRepeated(BTN_RIGHT) && osk_x < max_col) osk_x++;
    
    // 边界检查
    if (osk_x > max_col) osk_x = max_col;
    
    // A键：选择当前按键
    if (PAD_justPressed(BTN_A)) {
        if (osk_y == 4) { // 功能键行
            switch (osk_x) {
                case 0: { // ESC
                    char esc[] = "\x1b";
                    write(s->ptm_fd, esc, 1);
                    break;
                }
                case 1: { // TAB
                    char tab[] = "\t";
                    write(s->ptm_fd, tab, 1);
                    break;
                }
                case 2: { // RET (Enter)
                    char enter[] = "\r";
                    write(s->ptm_fd, enter, 1);
                    break;
                }
                case 3: { // BS (Backspace)
                    char bs[] = "\x7f";
                    write(s->ptm_fd, bs, 1);
                    break;
                }
                case 4: { // SPC (Space)
                    char space[] = " ";
                    write(s->ptm_fd, space, 1);
                    break;
                }
                case 5: { // UP ARROW
                    char up[] = "\x1b[A";
                    write(s->ptm_fd, up, 3);
                    break;
                }
                case 6: { // DOWN ARROW  
                    char down[] = "\x1b[B";
                    write(s->ptm_fd, down, 3);
                    break;
                }
                case 7: { // HIDE键盘 - 修复这个功能
                    osk_active = false;
                    TLOG("handle_terminal_input: OSK hidden via HIDE key\n");
                    break;
                }
            }
        } else { // 普通字符
            char ch = osk_layout[osk_y][osk_x];
            write(s->ptm_fd, &ch, 1);
        }
    }
    
    // 快捷键保持不变
    if (PAD_justPressed(BTN_Y)) { 
        char space[] = " ";
        write(s->ptm_fd, space, 1);
    }
    
    if (PAD_justPressed(BTN_X)) { 
        char bs[] = "\x7f";
        write(s->ptm_fd, bs, 1);
    }
    
    if (PAD_justPressed(BTN_START)) { 
        char enter[] = "\r";
        write(s->ptm_fd, enter, 1);
    }
    
    if (PAD_justPressed(BTN_SELECT)) { 
        char tab[] = "\t";
        write(s->ptm_fd, tab, 1);
    }
    
    // L1键切换键盘显示/隐藏 - 修复这个功能
    if (PAD_justPressed(BTN_L1)) { 
        osk_active = !osk_active;
        TLOG("handle_terminal_input: OSK toggled via L1, now %s\n", osk_active ? "visible" : "hidden");
    }
    
    // R1键也可以切换键盘
    if (PAD_justPressed(BTN_R1)) {
        osk_active = !osk_active;
        TLOG("handle_terminal_input: OSK toggled via R1, now %s\n", osk_active ? "visible" : "hidden");
    }
    
    // 返回主界面
    if (PAD_justPressed(BTN_B)) { 
        current_view = VIEW_LIST; 
        SysUI_SetFullscreen(false); 
    }
}

// --- 插件生命周期 ---
static int plugin_init(void* main_screen) {
    TLOG("====================================================\n");
    TLOG("plugin_init: START\n");
    
    // 重置静态变量！
    quit_plugin = false;
    current_view = VIEW_LIST;
    active_session_idx = -1;
    selected_index = 0;
    list_start_index = 0;
    session_count = 0;
    next_session_id = 1; 
    osk_active = true;
    osk_x = 0;
    osk_y = 0;
    
    screen = (SDL_Surface*)main_screen;
    items_per_page = MAIN_ROW_COUNT;
    memset(sessions, 0, sizeof(sessions));

    TLOG("plugin_init: Static variables initialized.\n");

    TLOG("plugin_init: Calling SysUI_Init()...\n");
    SysUI_Init(screen, &font);
    TLOG("plugin_init: SysUI_Init() OK.\n");

    // TLOG("plugin_init: Calling PWR_init()...\n");
    // PWR_init();
    // TLOG("plugin_init: PWR_init() OK.\n");
    
    // char mono_font_path[MAX_PATH];
    // snprintf(mono_font_path, sizeof(mono_font_path), "%s/mono.ttf", RES_PATH);
    // TLOG("plugin_init: Trying font at %s\n", mono_font_path);
    // if (!exists(mono_font_path)) {
    //    snprintf(mono_font_path, sizeof(mono_font_path), "%s/font.ttf", RES_PATH);
    //    TLOG("plugin_init: mono.ttf not found, fallback to %s\n", mono_font_path);
    // }
    // mono_font = TTF_OpenFont(mono_font_path, SCALE1(TERMINAL_FONT_HEIGHT - 2));
    // if (!mono_font) { TLOG("plugin_init: FAILED to load any font!\n"); return -1; }
    // TLOG("plugin_init: Font loaded successfully.\n");
    char mono_font_path[MAX_PATH];
    snprintf(mono_font_path, sizeof(mono_font_path), "%s/mono.ttf", RES_PATH);
    TLOG("plugin_init: Trying font at %s\n", mono_font_path);
    if (!exists(mono_font_path)) {
       snprintf(mono_font_path, sizeof(mono_font_path), "%s/font.ttf", RES_PATH);
       TLOG("plugin_init: mono.ttf not found, fallback to %s\n", mono_font_path);
    }
    
    // 调整字体大小 - 使用更小的尺寸
    mono_font = TTF_OpenFont(mono_font_path, SCALE1(9)); // 固定使用10而不是TERMINAL_FONT_HEIGHT-2
    if (!mono_font) { 
        TLOG("plugin_init: FAILED to load any font!\n"); 
        return -1; 
    }
    TLOG("plugin_init: Font loaded successfully.\n");

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
    .name = "Terminal", .display_path = SDCARD_PATH "/Tools/System",
    .init = plugin_init, .run = plugin_run, .quit = plugin_quit,
};
NextUI_Plugin* GetPlugin(void) { return &terminal_plugin_export; }