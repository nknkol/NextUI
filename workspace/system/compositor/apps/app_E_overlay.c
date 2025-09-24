// app_E_overlay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// 替换为您项目中的头文件
#include "sdl.h"
#include "api.h"
// 结束替换

#include "client_lib.h"
#include "protocol.h"


void render_scene_E(uint8_t* framebuffer, int region_x, int region_y, int region_w, int region_h) {
    if (!framebuffer) return;
    
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 
    );
    if (!target_surface) {
        fprintf(stderr, "App E: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    // 静态变量只初始化一次
    static int box_x = -1, box_y = -1;
    static int x_direction = 4, y_direction = 4; 
    
    // 首次运行时，将方块位置初始化在区域左上角
    if (box_x == -1) {
        box_x = region_x;
        box_y = region_y;
    }

    SDL_FillRect(target_surface, NULL, 0); 

    // 更新位置
    box_x += x_direction;
    box_y += y_direction;

    // 在区域边界内反弹
    if (box_x <= region_x || box_x >= (region_x + region_w - 100)) {
        x_direction = -x_direction; 
    }
    if (box_y <= region_y || box_y >= (region_y + region_h - 100)) {
        y_direction = -y_direction; 
    }

    Uint32 blue = SDL_MapRGBA(target_surface->format, 0, 0, 255, 160); 
    SDL_Rect rect = { box_x, box_y, 100, 100 };
    SDL_FillRect(target_surface, &rect, blue);
    
    SDL_FreeSurface(target_surface);
}


int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU); 
    PAD_init();
    
    client_install_signal_handlers();

    // 默认请求 slot 3
    int requested_slot = 3;
    if (argc > 1) {
       requested_slot = atoi(argv[1]);
    }

    ClientConnection* conn = client_connect(requested_slot, "App E (Overlay Diag)", CLIENT_TYPE_OVERLAY); 
    if (conn == NULL) {
        fprintf(stderr, "App E: Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(conn);

    // 定义此应用的目标叠加层索引和区域 (左下角)
    int my_overlay_index = 2; // 使用2号叠加层
    int my_width = 300;
    int my_height = 250;
    int my_x = 20; 
    int my_y = DEMO_HEIGHT - my_height - 20;

    client_set_overlay_region(conn, my_overlay_index, my_x, my_y, my_width, my_height);

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
        
        uint8_t* framebuffer = client_get_render_buffer(conn);
        if (framebuffer) {
            render_scene_E(framebuffer, my_x, my_y, my_width, my_height);
            client_present(conn, framebuffer);
        }
        
        GFX_sync_fixed_rate(60.0);
    }
    
    client_clear_overlay_index(conn, my_overlay_index);
    client_disconnect(conn);
    
    PAD_quit();
    GFX_quit();
    printf("App E disconnected.\n");
    return 0;
}