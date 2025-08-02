#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "ion_mem_alloc.h"

// IOCTL command and structures manually defined
#define DISP_LAYER_SET_CONFIG2 0x49

enum cdx_disp_layer_mode { LAYER_MODE_BUFFER = 0 };
struct cdx_disp_rect { int x; int y; unsigned int width; unsigned int height; };
struct cdx_disp_rectsz { unsigned int width; unsigned int height; };
struct cdx_disp_rect64 { long long x; long long y; long long width; long long height; };
typedef enum { CDX_DISP_FORMAT_ARGB_8888 = 0x00 } cdx_disp_pixel_format;
typedef enum { CDX_DISP_GBR = 0x100 } cdx_disp_color_space;
enum cdx_disp_buffer_flags { DISP_BF_NORMAL = 0 };

struct cdx_disp_fb_info2 {
	int32_t fd; struct cdx_disp_rectsz size[3]; uint32_t align[3];
	cdx_disp_pixel_format format; cdx_disp_color_space color_space;
	int32_t trd_right_fd; int32_t pre_multiply; struct cdx_disp_rect64 crop;
	enum cdx_disp_buffer_flags flags;
};

struct cdx_disp_layer_info2 {
	enum cdx_disp_layer_mode mode; uint8_t zorder; uint8_t alpha_mode;
	uint8_t alpha_value; struct cdx_disp_rect screen_win;
	int32_t b_trd_out; int32_t out_trd_mode;
	union { uint32_t color; struct cdx_disp_fb_info2 fb; };
};

struct cdx_disp_layer_config2 {
	struct cdx_disp_layer_info2 info; int32_t enable;
	uint32_t channel; uint32_t layer_id;
};

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define OVERLAY_COLOR 0xFFFF0000 // 使用完全不透明的红色

// ** 最终测试目标: 抢占 nextui 的图层 **
#define TARGET_CHANNEL   1 // nextui 使用的 channel 1
#define TARGET_LAYER_ID  0 // nextui 使用的 layer 0
#define TARGET_ZORDER   16 // nextui 使用的 z-order 16

int main(int argc, char *argv[])
{
    printf("--- 系统覆盖Demo (独立运行最终测试版) ---\n");
    printf("目标: channel %d, layer %d, z-order %d\n\n", TARGET_CHANNEL, TARGET_LAYER_ID, TARGET_ZORDER);

    int disp_fd = open("/dev/disp", O_RDWR);
    if (disp_fd < 0) {
        perror("[错误] 打开 /dev/disp 失败");
        return -1;
    }
    printf("[成功] 打开 /dev/disp, fd = %d\n", disp_fd);
    
    struct SunxiMemOpsS *memops = GetMemAdapterOpsS();
    if (memops == NULL || SunxiMemOpen(memops) != 0) {
        printf("[错误] 初始化内存分配器失败。\n");
        close(disp_fd);
        return -1;
    }
    size_t buffer_size = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
    void* buffer_vaddr = SunxiMemPalloc(memops, buffer_size);
    int buffer_fd = SunxiMemGetBufferFd(memops, buffer_vaddr);
    if (buffer_vaddr == NULL || buffer_fd < 0) {
        printf("[错误] ION内存分配失败。\n");
        SunxiMemClose(memops);
        close(disp_fd);
        return -1;
    }
    printf("[成功] ION内存分配, fd = %d\n", buffer_fd);

    uint32_t *pixels = (uint32_t *)buffer_vaddr;
    for (size_t i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        pixels[i] = OVERLAY_COLOR;
    }
    SunxiMemFlushCache(memops, buffer_vaddr, buffer_size);

    struct cdx_disp_layer_config2 config;
    memset(&config, 0, sizeof(struct cdx_disp_layer_config2));

    config.channel  = TARGET_CHANNEL;
    config.layer_id = TARGET_LAYER_ID;
    config.enable   = 1;
    config.info.zorder = TARGET_ZORDER;
    config.info.mode = LAYER_MODE_BUFFER;
    config.info.alpha_mode = 1; 
    config.info.alpha_value = 255; 
    config.info.screen_win = (struct cdx_disp_rect){0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    config.info.fb.fd = buffer_fd;
    config.info.fb.format = CDX_DISP_FORMAT_ARGB_8888;
    config.info.fb.size[0].width = SCREEN_WIDTH;
    config.info.fb.size[0].height= SCREEN_HEIGHT;
    config.info.fb.align[0] = 4;
    config.info.fb.color_space  = CDX_DISP_GBR;

    unsigned long arg[3] = {0, (unsigned long)&config, 1};
    printf("执行 ioctl(DISP_LAYER_SET_CONFIG2) ...\n");
    int ret = ioctl(disp_fd, DISP_LAYER_SET_CONFIG2, &arg);
    if (ret != 0) {
        perror("[错误] ioctl(DISP_LAYER_SET_CONFIG2) 失败");
        // ... (省略清理代码)
        return -1;
    }
    
    printf("\n[成功] ioctl 调用成功返回。\n");
    printf("图层应该已显示，程序将保持运行。\n");
    printf("请观察屏幕，然后按 Ctrl+C 退出。\n");

    while(1) {
        sleep(1);
    }
    
    return 0;
}