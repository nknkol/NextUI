// app_B_overlay.c
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

// 修改 render_scene_B 函数，让它接收区域坐标作为参数
void render_scene_B(uint8_t* framebuffer, int region_x, int region_y, int region_w, int region_h) {
    if (!framebuffer) return;
    
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 
    );
    if (!target_surface) {
        fprintf(stderr, "App B: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    static int box_offset_y = 0;
    static int y_direction = 3; 

    // 用透明色清空整个1024x768的画布
    SDL_FillRect(target_surface, NULL, 0); 

    box_offset_y += y_direction;
    // 确保方块在区域高度内反弹
    if (box_offset_y <= 0 || box_offset_y >= (region_h - 100)) {
        y_direction = -y_direction; 
    }

    // 在指定的区域内绘制一个垂直移动的方块
    Uint32 red = SDL_MapRGBA(target_surface->format, 255, 0, 0, 180); 
    // 方块的位置现在基于传入的区域坐标
    SDL_Rect rect = {
        region_x + (region_w / 2) - 50, // 在区域的水平中心
        region_y + box_offset_y,        // 在区域内垂直移动
        100, 
        100
    };
    SDL_FillRect(target_surface, &rect, red);
    
    SDL_FreeSurface(target_surface);
}


int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU); 
    PAD_init();
    
    client_install_signal_handlers();

    // 默认请求 slot 1, 可由启动参数覆盖
    int requested_slot = 1;
    if (argc > 1) {
       requested_slot = atoi(argv[1]);
    }

    ClientConnection* conn = client_connect(requested_slot, "App B (Overlay Vert)", CLIENT_TYPE_OVERLAY); 
    if (conn == NULL) {
        fprintf(stderr, "App B: Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(conn);

    // 定义此应用的目标叠加层索引和区域
    int my_overlay_index = 0; // 使用0号叠加层
    int my_width = 200;
    int my_height = 200;
    int my_x = DEMO_WIDTH - my_width - 20; // 屏幕右上角
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
             // 将区域坐标传递给渲染函数
            render_scene_B(framebuffer, my_x, my_y, my_width, my_height);
            client_present(conn, framebuffer);
        }
        
        GFX_sync_fixed_rate(60.0);
    }
    
    client_clear_overlay_index(conn, my_overlay_index);
    client_disconnect(conn);
    
    PAD_quit();
    GFX_quit();
    printf("App B disconnected.\n");
    return 0;
}