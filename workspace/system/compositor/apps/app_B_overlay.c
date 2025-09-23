// app_B_overlay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "sdl.h"
#include "api.h"
#include "client_lib.h"
#include "protocol.h"

// MODIFIED: 渲染函数现在接收一个原始的帧缓冲区指针
void render_scene_B(uint8_t* framebuffer) {
    if (!framebuffer) return;

    // NEW: 创建一个临时的SDL_Surface来包装ION内存，以便使用SDL的绘图函数
    // 这是一个零拷贝操作，SDL_Surface只是一个指向现有内存的“视图”
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 // ARGB format for DRM FOURCC
    );
    if (!target_surface) {
        fprintf(stderr, "App B: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    static int box_y = 0;
    static int y_direction = 3; 

    // 1. 清空为全透明背景 (0x00000000)
    SDL_FillRect(target_surface, NULL, 0);

    // 2. 更新方块位置
    box_y += y_direction;
    if (box_y <= 0 || box_y >= (target_surface->h - 100)) {
        y_direction = -y_direction; // 碰到边缘则反向
    }

    // 3. 绘制垂直移动的红色方块
    Uint32 red = SDL_MapRGBA(target_surface->format, 255, 0, 0, 255);
    SDL_Rect rect = {(target_surface->w / 2) - 50, box_y, 100, 100};
    SDL_FillRect(target_surface, &rect, red);

    // NEW: 释放包装器，这不会释放底层的ION内存
    SDL_FreeSurface(target_surface);
}


int main(int argc, char* argv[]) {
    printf("App B: Starting up...\n");
    fflush(stdout);

    GFX_init(MODE_MENU); 
    PAD_init();
    
    // MODIFIED: 不再需要本地缓冲区 (local_buffer)

    client_install_signal_handlers();
    printf("App B: Signal handlers installed.\n");
    fflush(stdout);

    int slot_id = client_connect(1); // 请求1号槽位
    if (slot_id < 0) {
        fprintf(stderr, "App B: Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    // 启用协作式暂停
    client_enable_render_pause(slot_id);

    printf("App B (PID %d) connected to slot %d.\n", getpid(), slot_id);
    fflush(stdout);

    bool running = true;
    while(running) {
        if (client_is_paused()) {
             usleep(16000); 
             continue;
        }

        PAD_poll();
        if (PAD_justPressed(BTN_SELECT)) {
             running = false;
        }

        // --- MODIFIED: 新的零拷贝渲染流程 ---
        
        // 1. 获取一个可供渲染的ION缓冲区
        uint8_t* framebuffer = client_get_render_buffer(slot_id);
        if (!framebuffer) {
             GFX_sync_fixed_rate(60.0);
             continue;
        }
        
        // 2. 直接在获取到的ION缓冲区上绘制最新一帧
        render_scene_B(framebuffer);
        
        // 3. 提交刚刚渲染好的缓冲区给合成器
        client_present(slot_id, framebuffer);
        
        GFX_sync_fixed_rate(60.0);
    }
    
    // 清理
    client_disconnect(slot_id);
    PAD_quit();
    GFX_quit();
    printf("App B disconnected.\n");
    return 0;
}