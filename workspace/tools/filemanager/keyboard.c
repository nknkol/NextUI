/**
 * A self-contained virtual keyboard implementation in C for NextUI plugins.
 * Faithfully ported from the original C++ KeyboardPrompt to match system UI/UX.
 * Revision 3: Corrected keycap size, fonts, and rendering method to match
 * native system keyboard visuals and fixed compile errors.
 */
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "keyboard.h"
#include "defines.h"
#include "utils.h"
#include "sysui.h"

// --- 内部定义 ---

#define KEYBOARD_ROWS 5
#define KEYBOARD_COLS 14
#define MAX_INPUT_TEXT 255

typedef const char* KeyboardLayout[KEYBOARD_ROWS][KEYBOARD_COLS];

// --- 键盘布局 ---

static KeyboardLayout keyboard_layout_lowercase = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "\b"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\", ""},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "\r", "", ""},
    {"z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "", "", "", ""},
    {"SHIFT", " ", "CANCEL", "", "", "", "", "", "", "", "", "", "", ""}
};

static KeyboardLayout keyboard_layout_uppercase = {
    {"~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "\b"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}", "|", ""},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "\r", "", ""},
    {"Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "", "", "", ""},
    {"shift", " ", "CANCEL", "", "", "", "", "", "", "", "", "", "", ""}
};

// --- 状态结构体 ---
typedef struct {
    int row;
    int col;
    int layout_mode; // 0 for lowercase, 1 for uppercase
    char current_text[MAX_INPUT_TEXT];
} KeyboardState;

// --- 内部辅助函数 ---

static int get_row_len(const KeyboardLayout layout, int row) {
    int len = 0;
    for (int i = 0; i < KEYBOARD_COLS && strlen(layout[row][i]) > 0; i++) {
        len++;
    }
    return len;
}

static void render_keyboard(SDL_Surface* screen, GFX_Fonts* fonts, KeyboardState* state) {
    const KeyboardLayout* current_layout = (state->layout_mode == 0) ? &keyboard_layout_lowercase : &keyboard_layout_uppercase;

    int input_box_y = SCALE1(PADDING + PILL_SIZE + 10);
    SDL_Rect input_box_rect = {SCALE1(PADDING), input_box_y, screen->w - SCALE1(PADDING * 2), SCALE1(PILL_SIZE)};
    GFX_blitPill(ASSET_BLACK_PILL, screen, &input_box_rect);
    
    SDL_Color white = {255, 255, 255, 255};
    if (strlen(state->current_text) > 0) {
        SDL_Surface *text_surf = TTF_RenderUTF8_Blended(fonts->medium, state->current_text, white);
        if (text_surf) {
            SDL_Rect text_dst = {input_box_rect.x + SCALE1(8), input_box_rect.y + (input_box_rect.h - text_surf->h) / 2};
            SDL_BlitSurface(text_surf, NULL, screen, &text_dst);
            SDL_FreeSurface(text_surf);
        }
    }

    int key_w = SCALE1(BUTTON_SIZE);
    int key_h = SCALE1(BUTTON_SIZE);
    int key_spacing = SCALE1(4);
    int total_height = (KEYBOARD_ROWS * key_h) + ((KEYBOARD_ROWS - 1) * key_spacing);
    int start_y = input_box_y + input_box_rect.h + (screen->h - (input_box_y + input_box_rect.h) - total_height - SCALE1(PILL_SIZE + PADDING)) / 2;

    for (int r = 0; r < KEYBOARD_ROWS; r++) {
        int row_len = get_row_len(*current_layout, r);
        int total_width = 0;
        
        for(int c=0; c<row_len; c++) {
            const char* key_label = (*current_layout)[r][c];
            if (strlen(key_label) > 1) {
                int text_w;
                TTF_SizeUTF8(fonts->tiny, key_label, &text_w, NULL);
                total_width += text_w + SCALE1(16);
            } else {
                total_width += key_w;
            }
        }
        total_width += (row_len - 1) * key_spacing;
        
        int current_x = (screen->w - total_width) / 2;

        for (int c = 0; c < row_len; c++) {
            const char* key_label = (*current_layout)[r][c];
            bool is_special_key = strlen(key_label) > 1;
            int current_key_width = is_special_key ? 0 : key_w;
            if(is_special_key) {
                int text_w; TTF_SizeUTF8(fonts->tiny, key_label, &text_w, NULL);
                current_key_width = text_w + SCALE1(16);
            }

            SDL_Rect key_rect = {current_x, start_y + r * (key_h + key_spacing), current_key_width, key_h};
            bool is_selected = (r == state->row && c == state->col);
            
            if(is_special_key) {
                GFX_blitPill(is_selected ? ASSET_WHITE_PILL : ASSET_DARK_GRAY_PILL, screen, &key_rect);
            } else {
                GFX_blitAssetColor(ASSET_BUTTON, NULL, screen, &key_rect, is_selected ? THEME_COLOR1 : THEME_COLOR3);
            }

            SDL_Color key_color = is_selected ? COLOR_BLACK : COLOR_WHITE;
            SDL_Surface* key_surf = TTF_RenderUTF8_Blended(fonts->tiny, key_label, key_color);
            if (key_surf) {
                SDL_Rect key_text_dst = {key_rect.x + (key_rect.w - key_surf->w) / 2, key_rect.y + (key_rect.h - key_surf->h) / 2};
                SDL_BlitSurface(key_surf, NULL, screen, &key_text_dst);
                SDL_FreeSurface(key_surf);
            }
            current_x += current_key_width + key_spacing;
        }
    }
}

char* ShowKeyboard(SDL_Surface* screen, GFX_Fonts* fonts, const char* title, const char* initial_text) {
    KeyboardState state;
    memset(&state, 0, sizeof(KeyboardState));
    if (initial_text) {
        strncpy(state.current_text, initial_text, MAX_INPUT_TEXT - 1);
    }

    char* result = NULL;
    int running = 1;
    
    SysUI_SetTitle(title);

    while(running) {
        GFX_startFrame();
        PAD_poll();
        
        const KeyboardLayout* current_layout = (state.layout_mode == 0) ? &keyboard_layout_lowercase : &keyboard_layout_uppercase;

        if (PAD_justPressed(BTN_UP)) { if (state.row > 0) state.row--; }
        else if (PAD_justPressed(BTN_DOWN)) { if (state.row < KEYBOARD_ROWS - 1) state.row++; }
        else if (PAD_justPressed(BTN_LEFT)) { if (state.col > 0) state.col--; }
        else if (PAD_justPressed(BTN_RIGHT)) { if (state.col < get_row_len(*current_layout, state.row) - 1) state.col++; }
        else if (PAD_justPressed(BTN_Y)) {
             int len = strlen(state.current_text);
             if (len > 0) state.current_text[len - 1] = '\0';
        }
        else if (PAD_justPressed(BTN_B)) { running = 0; }
        else if (PAD_justPressed(BTN_A)) {
            const char* key = (*current_layout)[state.row][state.col];
            if (strcmp(key, "SHIFT") == 0 || strcmp(key, "shift") == 0) {
                state.layout_mode = 1 - state.layout_mode;
            } else if (strcmp(key, "\b") == 0) {
                int len = strlen(state.current_text);
                if (len > 0) state.current_text[len - 1] = '\0';
            } else if (strcmp(key, "\r") == 0) {
                result = strdup(state.current_text);
                running = 0;
            } else if (strcmp(key, "CANCEL") == 0) {
                running = 0;
            } else if (strcmp(key, " ") == 0) {
                 if (strlen(state.current_text) < MAX_INPUT_TEXT - 1) strcat(state.current_text, " ");
            } else {
                if (strlen(state.current_text) < MAX_INPUT_TEXT - 1) strcat(state.current_text, key);
            }
        }
        
        int max_col = get_row_len(*current_layout, state.row) - 1;
        if (state.col > max_col) state.col = max_col;

        GFX_clear(screen);
        render_keyboard(screen, fonts, &state);
        SysUI_SetBottomHints("Y", "Backspace", "B", "Cancel", "A", "OK");
        SysUI_Render();
        GFX_flip(screen);
    }
    
    return result;
}

