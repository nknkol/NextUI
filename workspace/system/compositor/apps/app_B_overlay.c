// app_B_overlay.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "client_lib.h"
#include "protocol.h"

void draw_rect_alpha(uint8_t* buffer, int x, int y, int w, int h, uint32_t color) {
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
    int slot_id = client_connect(1); // 请求1号槽位
    if (slot_id < 0) {
        fprintf(stderr, "App B: Failed to connect to compositor.\n");
        return 1;
    }

    printf("App B (PID %d) connected to slot %d.\n", getpid(), slot_id);

    int frame_count = 0;
    while(1) {
        if (!client_is_paused()) {
            uint8_t* framebuffer = client_get_framebuffer(slot_id);
            if (!framebuffer) break;
            
            // 1. 清空为全透明
            memset(framebuffer, 0, DEMO_BUFFER_SIZE);
            
            // 2. 绘制半透明顶部栏
            draw_rect_alpha(framebuffer, 0, 0, DEMO_WIDTH, 80, 0x80000000); // 50%透明黑色
            
            // 3. 提交帧
            client_present(slot_id);
            frame_count++;
        }
        
        usleep(16000); // 约60FPS
    }
    
    client_disconnect(slot_id);
    printf("App B disconnected.\n");
    return 0;
}