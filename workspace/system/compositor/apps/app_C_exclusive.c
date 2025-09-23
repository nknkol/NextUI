// apps/app_C_exclusive.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>

#include "sdl.h"
#include "client_lib.h"
#include "protocol.h"
#include "api.h"
#include "defines.h"

// 这是一个统一的渲染函数，将场景直接绘制到给定的内存缓冲区
void render_scene_direct(uint8_t* framebuffer, TTF_Font* font, bool is_exclusive_mode) {
    if(!framebuffer) return;

    // 创建一个临时的SDL_Surface来包装ION内存，以便使用SDL的绘图函数
    // 这是一个零拷贝操作，SDL_Surface只是一个指向现有内存的“视图”
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 // ARGB format for DRM FOURCC
    );
    if (!target_surface) {
        fprintf(stderr, "SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    // --- 渲染逻辑 ---
    static int box_x = 0;
    static int x_direction = 4;
    static Uint32 last_time = 0;
    static int frame_count = 0;
    static char fps_text[128] = "Starting...";

    if(last_time == 0) last_time = SDL_GetTicks();

    Uint32 white = SDL_MapRGB(target_surface->format, 255, 255, 255);
    Uint32 red = SDL_MapRGB(target_surface->format, 255, 0, 0);

    SDL_FillRect(target_surface, NULL, white);
    box_x += x_direction;
    if (box_x <= 0 || box_x >= (target_surface->w - 100)) {
        x_direction = -x_direction;
    }
    SDL_Rect rect = {box_x, (target_surface->h / 2) - 50, 100, 100};
    SDL_FillRect(target_surface, &rect, red); 
    
    frame_count++;
    Uint32 current_time = SDL_GetTicks();
    if (current_time > last_time + 1000) {
        float fps = frame_count / ((current_time - last_time) / 1000.0f);
        snprintf(fps_text, sizeof(fps_text), "FPS: %.2f | Zero-Copy Mode", fps);
        last_time = current_time;
        frame_count = 0;
    }

    if (font) {
        SDL_Surface* text_surface = TTF_RenderUTF8_Blended(font, fps_text, (SDL_Color){0, 0, 0, 255});
        if(text_surface) {
            SDL_Rect dest_rect = {20, 20, text_surface->w, text_surface->h};
            SDL_BlitSurface(text_surface, NULL, target_surface, &dest_rect);
            SDL_FreeSurface(text_surface);
        }
    }
    
    SDL_FreeSurface(target_surface);
}

int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU);
    PAD_init();
    client_install_signal_handlers();

    int client_slot_id = client_connect((argc > 1) ? atoi(argv[1]) : 0);
    if (client_slot_id == -1) {
        fprintf(stderr, "Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(client_slot_id);
    client_request_exclusive(client_slot_id);

    bool running = true; 
    bool is_exclusive_mode = true;

    while(running) {
        GFX_startFrame();
        PAD_poll();
        if (PAD_justPressed(BTN_SELECT)) running = false;
        
        if (PAD_justPressed(BTN_START)) {
            is_exclusive_mode = !is_exclusive_mode;
            if (is_exclusive_mode) {
                client_request_exclusive(client_slot_id);
            } else {
                client_release_exclusive(client_slot_id);
            }
        }

        if (!client_is_paused()) {
            // 1. 获取下一帧可用的ION缓冲区
            uint8_t* framebuffer = client_get_render_buffer(client_slot_id);
            if (framebuffer) {
                // 2. 直接在该缓冲区上渲染
                render_scene_direct(framebuffer, font.large, is_exclusive_mode);
                
                // 3. 提交该缓冲区 (发送fd给合成器)
                client_present(client_slot_id, framebuffer);
            }
        } else {
             // Keep UI responsive even when paused
             GFX_sync_fixed_rate(60.0);
        }
        GFX_sync_fixed_rate(60.0);
    }

    client_disconnect(client_slot_id);
    PAD_quit();
    GFX_quit();
    
    return 0;
}