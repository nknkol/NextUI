#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <msettings.h>
#include <errno.h>

#include "api.h"
#include "utils.h"
#include "sysui.h"
#include "lang.h"
#include "defines.h"
#include "keyboard.h" 

// --- 数据结构 ---

typedef struct {
    char name[MAX_PATH];
    bool is_dir;
    size_t size;
} FileInfo;

// --- 全局变量 ---

static SDL_Surface* screen;
static bool quit_app = false;

static FileInfo* file_list = NULL;
static int file_count = 0;
static char current_path[MAX_PATH];
static int selected_index = 0;
static int scroll_offset = 0;

static bool show_context_menu = false;
static int menu_selected_index = 0;

typedef enum {
    OP_NONE,
    OP_COPY,
    OP_CUT
} ClipboardOperation;

static ClipboardOperation clipboard_op = OP_NONE;
static char clipboard_path[MAX_PATH] = {0};

// --- 函数声明 ---
static void populate_file_list(const char* path);
static void free_file_list(void);
static void render_file_list(void);
static void render_context_menu(void);
static void handle_context_menu_input(void);
static void handle_file_operations(void);
static int compare_file_info(const void* a, const void* b);
static const char* format_file_size(size_t size, bool is_dir);
static int show_confirm_dialog(const char* message);
static int copy_recursive(const char *src, const char *dst);
static int delete_recursive(const char *path);

// --- 核心实现 ---

static int compare_file_info(const void* a, const void* b) {
    FileInfo* file_a = (FileInfo*)a;
    FileInfo* file_b = (FileInfo*)b;
    if (file_a->is_dir != file_b->is_dir) return file_b->is_dir - file_a->is_dir;
    return strcasecmp(file_a->name, file_b->name);
}

static const char* format_file_size(size_t size, bool is_dir) {
    static char size_str[32];
    if (is_dir) return "<DIR>";
    if (size < 1024) snprintf(size_str, sizeof(size_str), "%zu B", size);
    else if (size < 1024 * 1024) snprintf(size_str, sizeof(size_str), "%.1f KB", (double)size / 1024);
    else if (size < 1024 * 1024 * 1024) snprintf(size_str, sizeof(size_str), "%.1f MB", (double)size / (1024 * 1024));
    else snprintf(size_str, sizeof(size_str), "%.1f GB", (double)size / (1024 * 1024 * 1024));
    return size_str;
}

static void free_file_list(void) {
    if (file_list) free(file_list);
    file_list = NULL;
    file_count = 0;
}

static void populate_file_list(const char* path) {
    free_file_list();
    strncpy(current_path, path, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';

    DIR* dir = opendir(path);
    if (!dir) {
        strncpy(current_path, SDCARD_PATH, sizeof(current_path) - 1);
        dir = opendir(current_path);
        if (!dir) return;
    }

    struct dirent* entry;
    int capacity = 20;
    file_list = malloc(sizeof(FileInfo) * capacity);
    
    if (strcmp(path, SDCARD_PATH) != 0 && strcmp(path, "/") != 0) {
        strcpy(file_list[file_count].name, "..");
        file_list[file_count].is_dir = true;
        file_list[file_count].size = 0;
        file_count++;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (file_count >= capacity) {
            capacity *= 2;
            file_list = realloc(file_list, sizeof(FileInfo) * capacity);
        }

        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            strncpy(file_list[file_count].name, entry->d_name, sizeof(file_list[file_count].name) - 1);
            file_list[file_count].is_dir = S_ISDIR(st.st_mode);
            file_list[file_count].size = st.st_size;
            file_count++;
        }
    }
    closedir(dir);
    
    if (file_count > 0) qsort(file_list, file_count, sizeof(FileInfo), compare_file_info);
    
    selected_index = 0;
    scroll_offset = 0;
}

static void render_file_list(void) {
    const int max_visible_items = 5;
    int visible_item_count = MIN(file_count, max_visible_items);
    int list_block_height = visible_item_count * SCALE1(PILL_SIZE);
    int list_oy = SCALE1(PADDING + PILL_SIZE) + ((screen->h - SCALE1((PADDING+PILL_SIZE)*2) - list_block_height) / 2);
    int end_index = scroll_offset + visible_item_count;
    if (end_index > file_count) end_index = file_count;

    if (file_count > 0 && selected_index >= scroll_offset && selected_index < end_index) {
        int selected_row = selected_index - scroll_offset;
        GFX_blitPillLight(ASSET_WHITE_PILL, screen, &(SDL_Rect){
            SCALE1(PADDING), list_oy + selected_row * SCALE1(PILL_SIZE),
            screen->w - SCALE1(PADDING * 2), SCALE1(PILL_SIZE)
        });
    }

    for (int i = scroll_offset; i < end_index; ++i) {
        bool is_selected = (i == selected_index);
        int item_y = list_oy + (i - scroll_offset) * SCALE1(PILL_SIZE);

        char display_text[MAX_PATH + 5];
        snprintf(display_text, sizeof(display_text), "%s %s",
                 file_list[i].is_dir ? "[D]" : "[F]", file_list[i].name);
        const char* size_str = format_file_size(file_list[i].size, file_list[i].is_dir);

        if (is_selected) {
            SDL_Color black_text = uintToColour(THEME_COLOR5_255);
            SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.large, display_text, black_text);
            if(text_surf) {
                int pill_w = text_surf->w + SCALE1(BUTTON_PADDING * 2);
                int max_pill_w = (screen->w - SCALE1(PADDING * 2)) * 0.7;
                if (pill_w > max_pill_w) pill_w = max_pill_w;

                GFX_blitPill(ASSET_WHITE_PILL, screen, &(SDL_Rect){SCALE1(PADDING), item_y, pill_w, SCALE1(PILL_SIZE)});
                
                char truncated[MAX_PATH+5];
                GFX_truncateText(font.large, display_text, truncated, pill_w - SCALE1(BUTTON_PADDING*2), 0);
                SDL_FreeSurface(text_surf);
                text_surf = TTF_RenderUTF8_Blended(font.large, truncated, black_text);

                if (text_surf) {
                    SDL_BlitSurface(text_surf, NULL, screen, &(SDL_Rect){
                        SCALE1(PADDING + BUTTON_PADDING), item_y + (SCALE1(PILL_SIZE) - text_surf->h) / 2});
                    SDL_FreeSurface(text_surf);
                }
            }
            SDL_Surface* size_surf = TTF_RenderUTF8_Blended(font.large, size_str, COLOR_WHITE);
            if (size_surf) {
                int size_x = screen->w - SCALE1(PADDING + BUTTON_PADDING) - size_surf->w;
                SDL_BlitSurface(size_surf, NULL, screen, &(SDL_Rect){size_x, item_y + (SCALE1(PILL_SIZE) - size_surf->h) / 2});
                SDL_FreeSurface(size_surf);
            }
        } else {
            char truncated[MAX_PATH+5];
            int max_text_width = (screen->w - SCALE1(PADDING * 2 + BUTTON_PADDING * 2)) * 0.7;
            GFX_truncateText(font.large, display_text, truncated, max_text_width, 0);
            SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.large, truncated, COLOR_WHITE);
            if(text_surf) {
                SDL_BlitSurface(text_surf, NULL, screen, &(SDL_Rect){
                    SCALE1(PADDING + BUTTON_PADDING), item_y + (SCALE1(PILL_SIZE) - text_surf->h) / 2});
                SDL_FreeSurface(text_surf);
            }
            SDL_Surface* size_surf = TTF_RenderUTF8_Blended(font.large, size_str, COLOR_WHITE);
            if (size_surf) {
                int size_x = screen->w - SCALE1(PADDING + BUTTON_PADDING) - size_surf->w;
                SDL_BlitSurface(size_surf, NULL, screen, &(SDL_Rect){size_x, item_y + (SCALE1(PILL_SIZE) - size_surf->h) / 2});
                SDL_FreeSurface(size_surf);
            }
        }
    }
}

/**
 * @brief 渲染Y键上下文菜单 (使用小气泡风格和合适尺寸)
 */
static void render_context_menu(void) {
    const char* menu_items_base[] = {"Copy", "Cut", "Delete", "Rename", "New Folder", NULL};
    const char* menu_items_paste[] = {"Copy", "Cut", "Paste", "Delete", "Rename", "New Folder", NULL};
    const char** current_menu = (clipboard_op != OP_NONE) ? menu_items_paste : menu_items_base;
    
    int menu_count = 0;
    while(current_menu[menu_count]) menu_count++;
    
    // --- 使用小尺寸常量 ---
    int bubble_size = SCALE1(BUTTON_SIZE); // 20x20
    int text_margin = SCALE1(BUTTON_MARGIN); // 5
    int item_h = bubble_size;
    int item_spacing = SCALE1(4);

    // --- 计算菜单整体尺寸和位置 ---
    // 先计算最宽的菜单项，以确定整个菜单面板的宽度
    int max_item_w = 0;
    for (int i = 0; i < menu_count; i++) {
        SDL_Surface* temp_surf = TTF_RenderUTF8_Blended(font.small, current_menu[i], COLOR_WHITE);
        if (temp_surf) {
            int current_w = bubble_size + text_margin + temp_surf->w;
            if (current_w > max_item_w) {
                max_item_w = current_w;
            }
            SDL_FreeSurface(temp_surf);
        }
    }
    
    int menu_w = max_item_w + SCALE1(PADDING); // 加上一些内边距
    int menu_h = (menu_count * item_h) + ((menu_count + 1) * item_spacing);
    int menu_x = (screen->w - menu_w) / 2;
    int menu_y = (screen->h - menu_h) / 2;

    // 绘制一个大的深色背景面板
    GFX_blitRect(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){menu_x, menu_y, menu_w, menu_h});

    // 循环绘制每个菜单项
    for (int i = 0; i < menu_count; i++) {
        bool is_selected = (i == menu_selected_index);
        int item_y = menu_y + item_spacing + i * (item_h + item_spacing);
        int item_x = menu_x + SCALE1(PADDING) / 2;

        SDL_Color text_color = is_selected ? uintToColour(THEME_COLOR5_255) : COLOR_WHITE;

        // 绘制气泡 (选中:白色, 未选中:深灰色)
        int bubble_x = item_x;
        GFX_blitAsset(is_selected ? ASSET_BUTTON : ASSET_OPTION, NULL, screen, &(SDL_Rect){bubble_x, item_y, bubble_size, bubble_size});

        // 绘制文本 (使用 font.small)
        int text_x = bubble_x + bubble_size + text_margin;
        SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.small, current_menu[i], text_color);
        if (text_surf) {
            SDL_BlitSurface(text_surf, NULL, screen, &(SDL_Rect){text_x, item_y + (item_h - text_surf->h) / 2});
            SDL_FreeSurface(text_surf);
        }
    }
}

static void handle_context_menu_input(void) {
    int menu_count = (clipboard_op != OP_NONE) ? 6 : 5;
    
    if (PAD_justPressed(BTN_UP)) {
        menu_selected_index = (menu_selected_index - 1 + menu_count) % menu_count;
    } else if (PAD_justPressed(BTN_DOWN)) {
        menu_selected_index = (menu_selected_index + 1) % menu_count;
    } else if (PAD_justPressed(BTN_Y) || PAD_justPressed(BTN_B)) {
        show_context_menu = false;
    } else if (PAD_justPressed(BTN_A)) {
        handle_file_operations();
    }
}

int main(int argc, char* argv[]) {
    PWR_setCPUSpeed(CPU_SPEED_MENU);
    screen = GFX_init(MODE_MAIN);
    PAD_init();
    PWR_init();
    InitSettings();
    Lang_Init(CFG_getLanguage());

    SysUI_Init(screen, &font);
    populate_file_list(SDCARD_PATH);

    int dirty = 1;
    bool sys_input_handled = false;
    const int max_visible = 5;

    while (!quit_app) {
        GFX_startFrame();
        PAD_poll();
        sys_input_handled = SysUI_Update();
        if (sys_input_handled) dirty = 1;

        if (show_context_menu) {
            handle_context_menu_input();
            dirty = 1;
        } else if (!sys_input_handled) {
            if (PAD_justRepeated(BTN_UP) || PAD_justRepeated(BTN_DOWN)) {
                if (PAD_justRepeated(BTN_UP)) {
                    if (selected_index > 0) {
                        selected_index--;
                        if (selected_index < scroll_offset) scroll_offset = selected_index;
                    }
                } else {
                    if (selected_index < file_count - 1) {
                        selected_index++;
                        if (selected_index >= scroll_offset + max_visible) scroll_offset = selected_index - max_visible + 1;
                    }
                }
                dirty = 1;
            } else if (PAD_justPressed(BTN_A)) {
                if (file_count > 0 && file_list[selected_index].is_dir) {
                    char next_path[MAX_PATH];
                    if (strcmp(file_list[selected_index].name, "..") == 0) {
                        strcpy(next_path, current_path);
                        char* last_slash = strrchr(next_path, '/');
                        if (last_slash > next_path) *last_slash = '\0';
                        else strcpy(next_path, SDCARD_PATH);
                    } else {
                        snprintf(next_path, sizeof(next_path), "%s/%s", current_path, file_list[selected_index].name);
                    }
                    populate_file_list(next_path);
                    dirty = 1;
                }
            } else if (PAD_justPressed(BTN_B)) {
                if (strcmp(current_path, SDCARD_PATH) != 0) {
                    char parent_path[MAX_PATH];
                    strcpy(parent_path, current_path);
                    char* last_slash = strrchr(parent_path, '/');
                    if (last_slash > parent_path) *last_slash = '\0';
                    else strcpy(parent_path, SDCARD_PATH);
                    populate_file_list(parent_path);
                    dirty = 1;
                }
            } else if (PAD_justPressed(BTN_Y)) {
                if(file_count > 0) { 
                    show_context_menu = true;
                    menu_selected_index = 0;
                    dirty = 1;
                }
            } else if (PAD_justPressed(BTN_START)) {
                quit_app = true;
            }
        }
        
        if (dirty) {
            GFX_clear(screen);
            SysUI_SetTitle(current_path);
            if (clipboard_op == OP_NONE)
                SysUI_SetBottomHints("Y", "Menu", "A", "Open", "START", "Exit");
            else
                SysUI_SetBottomHints("Y", "Menu(Paste)", "A", "Open", "START", "Exit");

            render_file_list();
            if (show_context_menu) render_context_menu();
            SysUI_Render();
            GFX_flip(screen);
            dirty = 0;
        } else {
            GFX_sync();
        }
    }

    free_file_list();
    SysUI_Quit();
    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();
    return EXIT_SUCCESS;
}

static int show_confirm_dialog(const char* message) { return 0; }
static int copy_recursive(const char *src, const char *dst) { return 0; }
static int delete_recursive(const char *path) { return 0; }


static void handle_file_operations(void) {
    char selected_path[MAX_PATH];
    if (file_count > 0 && selected_index < file_count) {
        snprintf(selected_path, sizeof(selected_path), "%s/%s", current_path, file_list[selected_index].name);
    } else {
        selected_path[0] = '\0';
    }

    // 映射菜单索引到具体操作
    const char* op_str = NULL;
    const char* menu_items_base[] = {"Copy", "Cut", "Delete", "Rename", "New Folder", NULL};
    const char* menu_items_paste[] = {"Copy", "Cut", "Paste", "Delete", "Rename", "New Folder", NULL};
    op_str = (clipboard_op != OP_NONE) ? menu_items_paste[menu_selected_index] : menu_items_base[menu_selected_index];


    if (strcmp(op_str, "Copy") == 0) {
        if (strlen(selected_path) > 0 && strcmp(file_list[selected_index].name, "..") != 0) {
            clipboard_op = OP_COPY;
            strcpy(clipboard_path, selected_path);
        }
    } else if (strcmp(op_str, "Cut") == 0) {
        if (strlen(selected_path) > 0 && strcmp(file_list[selected_index].name, "..") != 0) {
            clipboard_op = OP_CUT;
            strcpy(clipboard_path, selected_path);
        }
    } else if (strcmp(op_str, "Paste") == 0) {
        if (clipboard_op != OP_NONE && strlen(clipboard_path) > 0) {
            char dst_name[MAX_PATH];
            snprintf(dst_name, sizeof(dst_name), "%s/%s", current_path, baseName(clipboard_path));
            if (clipboard_op == OP_COPY) {
                // copy_recursive(clipboard_path, dst_name);
            } else if (clipboard_op == OP_CUT) {
                rename(clipboard_path, dst_name);
            }
            clipboard_op = OP_NONE;
            clipboard_path[0] = '\0';
            populate_file_list(current_path);
        }
    } else if (strcmp(op_str, "Delete") == 0) {
        if (strlen(selected_path) > 0 && strcmp(file_list[selected_index].name, "..") != 0) {
            char msg[MAX_PATH + 64];
            snprintf(msg, sizeof(msg), "Delete '%s'?", file_list[selected_index].name);
            if (show_confirm_dialog(msg)) {
                // delete_recursive(selected_path);
                populate_file_list(current_path);
            }
        }
    } else if (strcmp(op_str, "Rename") == 0) {
        if (strlen(selected_path) > 0 && strcmp(file_list[selected_index].name, "..") != 0) {
            char* new_name = ShowKeyboard(screen, &font, "Rename", file_list[selected_index].name);
            if (new_name && strlen(new_name) > 0) {
                char new_path[MAX_PATH];
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, new_name);
                rename(selected_path, new_path);
                populate_file_list(current_path);
            }
            if (new_name) free(new_name);
        }
    } else if (strcmp(op_str, "New Folder") == 0) {
        char* folder_name = ShowKeyboard(screen, &font, "New Folder", NULL);
        if (folder_name && strlen(folder_name) > 0) {
            char new_folder_path[MAX_PATH];
            snprintf(new_folder_path, sizeof(new_folder_path), "%s/%s", current_path, folder_name);
            mkdir(new_folder_path, 0755);
            populate_file_list(current_path);
        }
        if (folder_name) free(folder_name);
    }
    show_context_menu = false;
}