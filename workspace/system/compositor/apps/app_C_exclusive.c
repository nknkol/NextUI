// apps/app_C_exclusive.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>

// These would be part of your project's include structure
#include "sdl.h"
#include "api.h"
#include "defines.h"
// End of project includes

#include "client_lib.h"
#include "protocol.h"


void render_scene_direct(uint8_t* framebuffer, TTF_Font* font, bool is_exclusive_mode) {
    if(!framebuffer) return;
    
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 
    );
    if (!target_surface) {
        fprintf(stderr, "SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    static int box_x = 0;
    static int x_direction = 4;
    
    // --- 修改开始 ---

    // FPS 计算相关的变量保持 static
    static Uint32 last_time = 0;
    static int frame_count = 0;
    static char fps_part_text[32] = "FPS: ..."; // 只存储FPS部分的文本

    if(last_time == 0) last_time = SDL_GetTicks();

    // 绘制背景和移动的方块 (这部分不变)
    Uint32 white = SDL_MapRGB(target_surface->format, 255, 255, 255);
    Uint32 red = SDL_MapRGB(target_surface->format, 255, 0, 0);

    SDL_FillRect(target_surface, NULL, white);
    box_x += x_direction;
    if (box_x <= 0 || box_x >= (target_surface->w - 100)) {
        x_direction = -x_direction;
    }
    SDL_Rect rect = {box_x, (target_surface->h / 2) - 50, 100, 100};
    SDL_FillRect(target_surface, &rect, red); 
    
    // FPS 计算逻辑保持每秒更新一次
    frame_count++;
    Uint32 current_time = SDL_GetTicks();
    if (current_time > last_time + 1000) {
        float fps = frame_count / ((current_time - last_time) / 1000.0f);
        snprintf(fps_part_text, sizeof(fps_part_text), "FPS: %.2f", fps);
        last_time = current_time;
        frame_count = 0;
    }

    // 最终要渲染的文本，每一帧都重新合成
    char final_text[128];
    const char* mode_str = is_exclusive_mode ? "EXCLUSIVE" : "NORMAL";
    snprintf(final_text, sizeof(final_text), "%s | Mode: %s", fps_part_text, mode_str);

    // 使用 final_text 进行渲染
    if (font) {
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font, (const char*)final_text, (SDL_Color){0, 0, 0, 255});
        if(text_surface) {
            SDL_Rect dest_rect = {20, 20, text_surface->w, text_surface->h};
            SDL_BlitSurface(text_surface, NULL, target_surface, &dest_rect);
            SDL_FreeSurface(text_surface);
        }
    }
    
    // --- 修改结束 ---
    
    SDL_FreeSurface(target_surface);
}

int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU);
    PAD_init();
    
    // This is process-wide, so it's called once.
    client_install_signal_handlers();

    // --- NEW API USAGE ---
    ClientConnection* conn = client_connect((argc > 1) ? atoi(argv[1]) : 0, "App C (Main)", CLIENT_TYPE_NORMAL);
    if (conn == NULL) {
        fprintf(stderr, "Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(conn);
    client_set_exclusive_support(conn, true);
    
    // Set initial state to NORMAL foreground
    client_set_foreground(conn, "NORMAL");

    bool running = true; 
    bool is_exclusive_mode = false;

    while(running) {
        GFX_startFrame();
        PAD_poll();
        if (PAD_justPressed(BTN_SELECT)) running = false;
        
        // Use the new SET_FOREGROUND command to toggle modes
        if (PAD_justPressed(BTN_START)) {
            is_exclusive_mode = !is_exclusive_mode;
            if (is_exclusive_mode) {
                client_set_foreground(conn, "EXCLUSIVE");
            } else {
                client_set_foreground(conn, "NORMAL");
            }
        }

        // This is a process-wide check
        if (!client_is_paused()) {
            uint8_t* framebuffer = client_get_render_buffer(conn);
            if (framebuffer) {
                render_scene_direct(framebuffer, font.large, is_exclusive_mode);
                client_present(conn, framebuffer);
            }
        } else {
             usleep(16000); // Sleep when paused to reduce CPU usage
        }
        GFX_sync_fixed_rate(60.0);
    }

    client_disconnect(conn);
    // --- END NEW API USAGE ---

    PAD_quit();
    GFX_quit();
    
    return 0;
}