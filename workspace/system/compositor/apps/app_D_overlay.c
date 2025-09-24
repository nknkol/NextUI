// app_D_overlay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h> // 需要包含数学库

// 替换为您项目中的头文件
#include "sdl.h"
#include "api.h"
// 结束替换

#include "client_lib.h"
#include "protocol.h"


void render_scene_D(uint8_t* framebuffer, int region_x, int region_y, int region_w, int region_h) {
    if (!framebuffer) return;
    
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 
    );
    if (!target_surface) {
        fprintf(stderr, "App D: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    static float angle = 0.0f;
    angle += 0.05f; // 控制旋转速度
    if (angle > 2.0f * M_PI) {
        angle -= 2.0f * M_PI;
    }

    SDL_FillRect(target_surface, NULL, 0); 

    // 计算圆周运动的坐标
    int center_x = region_x + region_w / 2;
    int center_y = region_y + region_h / 2;
    int radius = (region_w < region_h ? region_w : region_h) / 2 - 50; // 半径为区域的一半再减去方块的一半

    int box_x = center_x + (int)(radius * cos(angle));
    int box_y = center_y + (int)(radius * sin(angle));

    Uint32 green = SDL_MapRGBA(target_surface->format, 0, 255, 0, 200); 
    SDL_Rect rect = { box_x - 50, box_y - 50, 100, 100 }; // 减去方块宽高的一半以使其中心在圆周上
    SDL_FillRect(target_surface, &rect, green);
    
    SDL_FreeSurface(target_surface);
}


int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU); 
    PAD_init();
    
    client_install_signal_handlers();

    // 默认请求 slot 2
    int requested_slot = 2;
    if (argc > 1) {
       requested_slot = atoi(argv[1]);
    }

    ClientConnection* conn = client_connect(requested_slot, "App D (Overlay Circle)", CLIENT_TYPE_OVERLAY); 
    if (conn == NULL) {
        fprintf(stderr, "App D: Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(conn);

    // 定义此应用的目标叠加层索引和区域 (左上角)
    int my_overlay_index = 1; // 使用1号叠加层
    int my_width = 250;
    int my_height = 250;
    int my_x = 20; 
    int my_y = 20;                        

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
            render_scene_D(framebuffer, my_x, my_y, my_width, my_height);
            client_present(conn, framebuffer);
        }
        
        GFX_sync_fixed_rate(60.0);
    }
    
    client_clear_overlay_index(conn, my_overlay_index);
    client_disconnect(conn);
    
    PAD_quit();
    GFX_quit();
    printf("App D disconnected.\n");
    return 0;
}