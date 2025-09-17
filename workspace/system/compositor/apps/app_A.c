// app_A.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "client_lib.h"
#include "protocol.h"

// 简单的绘制函数
void draw_rect(uint8_t* buffer, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < DEMO_WIDTH && j >= 0 && j < DEMO_HEIGHT) {
                uint32_t* pixel = (uint32_t*)(buffer + j * DEMO_PITCH + i * DEMO_BPP);
                *pixel = color;
            }
        }
    }
}

int main() {
    client_install_signal_handlers();
    int slot_id = client_connect(0); // 请求0号槽位
    if (slot_id < 0) {
        fprintf(stderr, "App A: Failed to connect to compositor.\n");
        return 1;
    }

    printf("App A (PID %d) connected to slot %d.\n", getpid(), slot_id);

    int x = 100, y = 100, dx = 5, dy = 5;
    int frame_count = 0;
    
    while(1) {
        if (!client_is_paused()) {
            uint8_t* framebuffer = client_get_framebuffer(slot_id);
            if (!framebuffer) break;
            
            // 1. 绘制背景
            uint32_t bg_color = 0xFF1e2329; // 深灰色背景, A R G B
            for (int i = 0; i < DEMO_WIDTH * DEMO_HEIGHT; i++) {
                ((uint32_t*)framebuffer)[i] = bg_color;
            }
            
            // 2. 绘制移动的方块
            x += dx;
            y += dy;
            if (x < 0 || x > DEMO_WIDTH - 100) dx = -dx;
            if (y < 0 || y > DEMO_HEIGHT - 100) dy = -dy;
            draw_rect(framebuffer, x, y, 100, 100, 0xFF9b2257); // 粉色方块
            
            // 3. 提交帧
            client_present(slot_id);
            frame_count++;
        }
        
        usleep(16000); // 约60FPS
    }

    client_disconnect(slot_id);
    printf("App A disconnected.\n");
    return 0;
}