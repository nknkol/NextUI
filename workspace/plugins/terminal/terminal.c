// terminal.c (真实终端版 + 详细的实时日志)

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
#define TERMINAL_FONT_WIDTH 8
#define TERMINAL_FONT_HEIGHT 16

typedef enum {
    VIEW_LIST,
    VIEW_TERMINAL,
} ViewState;

typedef struct TerminalSession {
    int id;
    char name[64];
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
    "`1234567890-=", " qwertyuiop[]\\", " asdfghjkl;'", " zxcvbnm,./", " "
};
#define OSK_ROWS (sizeof(osk_layout) / sizeof(osk_layout[0]))

// --- 回调函数 ---
static void vte_write_cb(struct tsm_vte *vte, const char *u8, size_t len, void *data) {
    TerminalSession *s = (TerminalSession*)data;
    if (s && s->ptm_fd >= 0) {
        if (write(s->ptm_fd, u8, len) < 0) {
            LOG_note(LOG_REALTIME, "[Terminal] vte_write_cb: write() to ptm_fd failed: %s\n", strerror(errno));
        }
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
static void kill_session(TerminalSession* session) {
    LOG_note(LOG_REALTIME, "[Terminal] kill_session: Killing session ID %d, PID %d\n", session->id, session->pid);
    if (session && session->initialized && session->pid > 0) {
        kill(session->pid, SIGKILL);
        waitpid(session->pid, NULL, 0);
        close(session->ptm_fd);
        tsm_vte_unref(session->vte);
        tsm_screen_unref(session->screen);
    }
    session->initialized = false;
    session->active = false;
    LOG_note(LOG_REALTIME, "[Terminal] kill_session: Session cleanup complete.\n");
}

static bool initialize_real_terminal(TerminalSession* session) {
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: START for session ID %d\n", session->id);

    if (session->initialized) {
        LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Already initialized.\n");
        return true;
    }

    int ptm_fd = -1, pts_fd = -1;
    pid_t pid;

    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Calling openpty()...\n");
    if (openpty(&ptm_fd, &pts_fd, NULL, NULL, NULL) < 0) {
        LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: openpty() FAILED: %s\n", strerror(errno));
        return false;
    }
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: openpty() OK. ptm_fd=%d\n", ptm_fd);

    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Calling fork()...\n");
    pid = fork();
    if (pid < 0) {
        LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: fork() FAILED: %s\n", strerror(errno));
        close(ptm_fd); close(pts_fd);
        return false;
    }
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: fork() OK. pid=%d\n", pid);

    if (pid == 0) { // 子进程
        close(ptm_fd);
        setsid();
        ioctl(pts_fd, TIOCSCTTY, NULL);
        dup2(pts_fd, STDIN_FILENO); dup2(pts_fd, STDOUT_FILENO); dup2(pts_fd, STDERR_FILENO);
        close(pts_fd);
        setenv("TERM", "xterm-256color", 1);
        setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
        setenv("HOME", SDCARD_PATH, 1);
        char *args[] = {"/bin/sh", NULL};
        execv(args[0], args);
        // 如果 execv 执行，下面的代码不会运行
        LOG_note(LOG_REALTIME, "[Terminal] CHILD PROCESS: execv() FAILED: %s\n", strerror(errno));
        exit(1);
    }

    // 父进程
    close(pts_fd);
    fcntl(ptm_fd, F_SETFL, fcntl(ptm_fd, F_GETFL, 0) | O_NONBLOCK);
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Parent process configured.\n");
    
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Calling tsm_screen_new()...\n");
    if (tsm_screen_new(&session->screen, NULL, NULL) != 0) {
        LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: tsm_screen_new() FAILED.\n");
        close(ptm_fd); kill(pid, SIGKILL); return false;
    }
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: tsm_screen_new() OK.\n");

    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Calling tsm_vte_new()...\n");
    if (tsm_vte_new(&session->vte, session->screen, vte_write_cb, session, NULL, NULL) != 0) {
        LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: tsm_vte_new() FAILED.\n");
        tsm_screen_unref(session->screen); close(ptm_fd); kill(pid, SIGKILL); return false;
    }
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: tsm_vte_new() OK.\n");
    
    unsigned int term_w = (screen->w - SCALE1(PADDING * 2)) / SCALE1(TERMINAL_FONT_WIDTH);
    unsigned int term_h = (screen->h - SCALE1(PADDING * 2) - ((OSK_ROWS + 2) * SCALE1(TERMINAL_FONT_HEIGHT))) / SCALE1(TERMINAL_FONT_HEIGHT);
    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: Resizing screen to %ux%u\n", term_w, term_h);
    tsm_screen_resize(session->screen, term_w, term_h);

    session->pid = pid;
    session->ptm_fd = ptm_fd;
    session->initialized = true;

    LOG_note(LOG_REALTIME, "[Terminal] initialize_real_terminal: END (SUCCESS)\n");
    return true;
}

static void create_session(void) {
    if (session_count >= MAX_SESSIONS) return;
    TerminalSession* new_session = &sessions[session_count];
    memset(new_session, 0, sizeof(TerminalSession));
    new_session->id = next_session_id++;
    snprintf(new_session->name, sizeof(new_session->name), "Session %d", new_session->id);
    new_session->initialized = false;
    new_session->pid = -1;
    new_session->ptm_fd = -1;
    new_session->active = true;
    
    session_count++;
    selected_index = session_count - 1;
    LOG_note(LOG_REALTIME, "[Terminal] create_session: Created placeholder for session '%s'.\n", new_session->name);
}

static void delete_session(void) {
    if (session_count == 0) return;
    
    kill_session(&sessions[selected_index]);

    // Shift remaining sessions in the array
    for (int i = selected_index; i < session_count; i++) {
        sessions[i] = sessions[i + 1];
    }
    
    // Adjust selected_index
    if (selected_index >= session_count && session_count > 0) {
        selected_index = session_count - 1;
    } else if (session_count == 0) {
        selected_index = 0;
    }
}

// ... (render and input functions are unchanged, copy them here) ...
static void render_session_list(void) { /* ... */ }
static void render_osk(void) { /* ... */ }
static void render_terminal_view(void) { /* ... */ }
static void handle_list_input(void) { /* ... */ }
static void handle_terminal_input(void) { /* ... */ }

#pragma region Render and Input Functions
// (Pasting the unchanged render and input functions here for completeness)
static void render_session_list(void) {
    if (session_count > 0) {
        if (selected_index < list_start_index) list_start_index = selected_index;
        if (selected_index >= list_start_index + items_per_page) list_start_index = selected_index - items_per_page + 1;
        
        int end_index = MIN(list_start_index + items_per_page, session_count);
        
        int safe_area_top = SCALE1(PADDING + PILL_SIZE);
        int available_height = screen->h - safe_area_top - SCALE1(PADDING + PILL_SIZE);
        int list_block_height = MAIN_ROW_COUNT * PILL_HEIGHT;
        int list_oy = safe_area_top + ((available_height - list_block_height) / 2);

        for (int i = list_start_index; i < end_index; i++) {
            int row = i - list_start_index;
            int pill_y = list_oy + (row * PILL_HEIGHT);
            
            if (i == selected_index) {
                SDL_Rect pill_rect = {SCALE1(BUTTON_MARGIN), pill_y, screen->w - SCALE1(BUTTON_MARGIN * 2), PILL_HEIGHT};
                GFX_blitPillDark(ASSET_WHITE_PILL, screen, &pill_rect);
            }

            SDL_Color text_color = (i == selected_index) ? uintToColour(THEME_COLOR5_255) : uintToColour(THEME_COLOR4_255);
            SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font.large, sessions[i].name, text_color);
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
    int osk_h = (OSK_ROWS + 1) * SCALE1(TERMINAL_FONT_HEIGHT);
    int osk_y_start = screen->h - osk_h;
    
    SDL_Rect bg = {0, osk_y_start, screen->w, osk_h};
    SDL_FillRect(screen, &bg, SDL_MapRGB(screen->format, 20, 20, 20));

    for (int y=0; y < OSK_ROWS; ++y) {
        const char *row_str = osk_layout[y];
        for (int x=0; x < strlen(row_str); ++x) {
            char c[2] = {row_str[x], 0};
            SDL_Color color = (y == osk_y && x == osk_x) ? COLOR_BLACK : COLOR_WHITE;
            SDL_Color bgcolor = (y == osk_y && x == osk_x) ? COLOR_WHITE : (SDL_Color){20,20,20,255};

            SDL_Surface *s = TTF_RenderUTF8_Shaded(mono_font, c, color, bgcolor);
            if (s) {
                SDL_Rect dst = {x * SCALE1(TERMINAL_FONT_WIDTH) + SCALE1(PADDING), osk_y_start + y * SCALE1(TERMINAL_FONT_HEIGHT), s->w, s->h};
                SDL_BlitSurface(s, NULL, screen, &dst);
                SDL_FreeSurface(s);
            }
        }
    }
}

static void render_terminal_view(void) {
    GFX_clear(screen);
    TerminalSession* session = &sessions[selected_index];
    
    if (session->initialized) {
        tsm_screen_draw(session->screen, draw_cb, screen);
        render_osk();
    } else {
        GFX_blitMessage(font.large, "Initializing session...", screen, NULL);
    }
}

static void handle_list_input(void) {
    if (SysUI_Update()) return;

    if (PAD_justPressed(BTN_B)) { quit_plugin = true; }
    else if (PAD_justRepeated(BTN_UP)) { if (session_count > 0) { selected_index = (selected_index - 1 + session_count) % session_count; } }
    else if (PAD_justRepeated(BTN_DOWN)) { if (session_count > 0) { selected_index = (selected_index + 1) % session_count; } }
    else if (PAD_justPressed(BTN_Y)) { create_session(); }
    else if (PAD_justPressed(BTN_X)) { delete_session(); }
    else if (PAD_justPressed(BTN_A)) {
        if (session_count > 0) {
            current_view = VIEW_TERMINAL;
            SysUI_SetFullscreen(true);
            if (!initialize_real_terminal(&sessions[selected_index])) {
                // Handle initialization failure
                LOG_note(LOG_REALTIME, "[Terminal] Failed to initialize session %d from list view.\n", sessions[selected_index].id);
                GFX_blitMessage(font.large, "Failed to start session!", screen, NULL);
                GFX_flip(screen);
                SDL_Delay(1000); // Show message for a second
                current_view = VIEW_LIST;
                SysUI_SetFullscreen(false);
            }
        }
    }
}

static void handle_terminal_input(void) {
    TerminalSession* s = &sessions[selected_index];
    
    if (PAD_justRepeated(BTN_UP) && osk_y > 0) osk_y--;
    if (PAD_justRepeated(BTN_DOWN) && osk_y < OSK_ROWS - 1) osk_y++;
    if (PAD_justRepeated(BTN_LEFT) && osk_x > 0) osk_x--;
    if (PAD_justRepeated(BTN_RIGHT) && osk_x < strlen(osk_layout[osk_y]) - 1) osk_x++;

    if (PAD_justPressed(BTN_A)) { char ch = osk_layout[osk_y][osk_x]; write(s->ptm_fd, &ch, 1); }
    if (PAD_justPressed(BTN_Y)) { char ch = ' '; write(s->ptm_fd, &ch, 1); }
    if (PAD_justPressed(BTN_X)) { char bs[] = "\x7f"; write(s->ptm_fd, bs, 1); }
    if (PAD_justPressed(BTN_START)) { char cr[] = "\r"; write(s->ptm_fd, cr, 1); }
    if (PAD_justPressed(BTN_SELECT)) { char tab[] = "\t"; write(s->ptm_fd, tab, 1); }
    if (PAD_justPressed(BTN_L1)) { osk_active = !osk_active; }
    if (PAD_justPressed(BTN_B)) { current_view = VIEW_LIST; SysUI_SetFullscreen(false); }
}
#pragma endregion

// --- 插件生命周期函数 ---
static int plugin_init(void* main_screen) {
    LOG_note(LOG_REALTIME, "====================================================\n");
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: START\n");

    screen = (SDL_Surface*)main_screen;
    quit_plugin = false;
    current_view = VIEW_LIST;
    session_count = 0;
    selected_index = 0;
    list_start_index = 0;
    next_session_id = 1;
    items_per_page = MAIN_ROW_COUNT;
    memset(sessions, 0, sizeof(sessions));
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: Static variables initialized.\n");

    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: Calling SysUI_Init()...\n");
    SysUI_Init(screen, &font);
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: SysUI_Init() OK.\n");

    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: Calling PWR_init()...\n");
    PWR_init();
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: PWR_init() OK.\n");
    
    char mono_font_path[MAX_PATH];
    snprintf(mono_font_path, sizeof(mono_font_path), "%s/mono.ttf", RES_PATH);
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: Trying font at %s\n", mono_font_path);
    if (!exists(mono_font_path)) {
       snprintf(mono_font_path, sizeof(mono_font_path), "%s/font.ttf", RES_PATH);
       LOG_note(LOG_REALTIME, "[Terminal] plugin_init: mono.ttf not found, fallback to %s\n", mono_font_path);
    }
    mono_font = TTF_OpenFont(mono_font_path, SCALE1(TERMINAL_FONT_HEIGHT - 2));
    if (!mono_font) {
        LOG_note(LOG_REALTIME, "[Terminal] plugin_init: FAILED to load any font!\n");
        return -1;
    }
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: Font loaded successfully.\n");

    create_session();
    
    LOG_note(LOG_REALTIME, "[Terminal] plugin_init: END (SUCCESS)\n");
    LOG_note(LOG_REALTIME, "====================================================\n");
    return 0;
}

static int plugin_run() {
    LOG_note(LOG_REALTIME, "[Terminal] plugin_run: START\n");
    int dirty = 1;
    int show_setting = 0;
    
    while (!quit_plugin) {
        PAD_poll();
        PWR_update(&dirty, &show_setting, NULL, NULL);

        int old_dirty = dirty;
        dirty = 0;

        if (current_view == VIEW_LIST) {
            handle_list_input();
        } else if (current_view == VIEW_TERMINAL && active_session_idx != -1) {
            handle_terminal_input();
        }

        if (active_session_idx != -1 && sessions[active_session_idx].initialized) {
            TerminalSession* s = &sessions[active_session_idx];
            char buf[4096];
            ssize_t len = read(s->ptm_fd, buf, sizeof(buf));
            if (len > 0) {
                tsm_vte_input(s->vte, buf, len);
                dirty = 1;
            } else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                delete_session();
                current_view = VIEW_LIST;
                SysUI_SetFullscreen(false);
                dirty = 1;
            }
        }
        
        if (old_dirty || pad.just_pressed || pad.just_released || pad.just_repeated) {
            dirty = 1;
        }

        if (dirty) {
            if (current_view == VIEW_LIST) {
                GFX_clear(screen);
                SysUI_SetTitle("Terminal");
                SysUI_SetBottomHints("Y", "New", "X", "Delete", "A", "Enter");
                render_session_list();
                SysUI_Render();
            } else {
                render_terminal_view();
            }
            GFX_flip(screen);
            dirty = 0;
        }
        GFX_sync();
    }
    LOG_note(LOG_REALTIME, "[Terminal] plugin_run: END\n");
    return 0;
}

static void plugin_quit(void) {
    LOG_note(LOG_REALTIME, "[Terminal] plugin_quit: START\n");
    for (int i = 0; i < MAX_SESSIONS; ++i) {
        if (sessions[i].active) {
            kill_session(&sessions[i]);
        }
    }

    if (mono_font) {
        TTF_CloseFont(mono_font);
        mono_font = NULL;
    }
    SysUI_SetFullscreen(false);
    SysUI_Quit();
    PWR_quit();
    LOG_note(LOG_REALTIME, "[Terminal] plugin_quit: END\n");
}

// --- 插件导出 ---
static NextUI_Plugin terminal_plugin_export = {
    .name = "Terminal",
    // .display_path = SDCARD_PATH "/Tools/System",
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

NextUI_Plugin* GetPlugin(void) {
    return &terminal_plugin_export;
}