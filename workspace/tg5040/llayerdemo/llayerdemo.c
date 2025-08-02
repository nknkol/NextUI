#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h> // For O_RDWR

// 包含用户态API头文件
#include "videoOutPort.h"
#include "ion_mem_alloc.h"

/*
 * ==========================================================================================
 * 手动定义来自 sunxi_display2.h 的关键结构体和宏定义
 * 我们只定义本次IOCTL调用所必需的部分，以修正颜色空间
 * ==========================================================================================
 */
// #define an IOCTL command, you can find it in sunxi_display2.h
// DISP_LAYER_SET_CONFIG2 -> 0x49
#define DISP_IOCTL_ARG_WITH_CONFIG      (0x49)

// enum disp_layer_mode, from sunxi_display2.h
enum cdx_disp_layer_mode {
	LAYER_MODE_BUFFER = 0,
	LAYER_MODE_COLOR = 1,
};

// struct disp_rect, from sunxi_display2.h
struct cdx_disp_rect {
	int x;
	int y;
	unsigned int width;
	unsigned int height;
};

// struct disp_rectsz, from sunxi_display2.h
struct cdx_disp_rectsz {
	unsigned int width;
	unsigned int height;
};

// struct disp_rect64, from sunxi_display2.h
struct cdx_disp_rect64 {
	long long x;
	long long y;
	long long width;
	long long height;
};

// enum disp_pixel_format, partial definition from sunxi_display2.h
typedef enum {
    CDX_DISP_FORMAT_ARGB_8888                   = 0x00,
    // ... other formats
} cdx_disp_pixel_format;

// enum disp_color_space, partial definition from sunxi_display2.h
typedef enum {
    CDX_DISP_BT601                              = 0x104,
    CDX_DISP_GBR                                = 0x100, // The key for RGB color space
    // ... other color spaces
} cdx_disp_color_space;

// enum disp_buffer_flags, from sunxi_display2.h
enum cdx_disp_buffer_flags {
	DISP_BF_NORMAL = 0,
    // ... other flags
};

// struct disp_fb_info2, from sunxi_display2.h
struct cdx_disp_fb_info2 {
	int                      fd;
	struct cdx_disp_rectsz   size[3];
	unsigned int             align[3];
	cdx_disp_pixel_format    format;
	cdx_disp_color_space     color_space; // The field we need to fix!
	int                      trd_right_fd;
	int                      pre_multiply; // bool is not a C standard type
	struct cdx_disp_rect64   crop;
	enum cdx_disp_buffer_flags flags;
    // ... other fields are omitted for simplicity
};

// struct disp_layer_info2, from sunxi_display2.h
struct cdx_disp_layer_info2 {
	enum cdx_disp_layer_mode      mode;
	unsigned char                 zorder;
	unsigned char                 alpha_mode;
	unsigned char                 alpha_value;
	struct cdx_disp_rect          screen_win;
	int                           b_trd_out;
	int                           out_trd_mode;
	union {
		unsigned int              color;
		struct cdx_disp_fb_info2  fb;
	};
    // ... other fields are omitted for simplicity
};

// struct disp_layer_config2, from sunxi_display2.h
struct cdx_disp_layer_config2 {
	struct cdx_disp_layer_info2 info;
	int enable; // bool is not a C standard type
	unsigned int channel;
	unsigned int layer_id;
};


// 屏幕分辨率 (根据您的日志调整为 1024x768)
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define OVERLAY_LAYER_ID  DISP_LAYER_ID_0
#define OVERLAY_COLOR     0x80FF0000

int main(int argc, char *argv[])
{
    dispOutPort *hdl = NULL;
    struct SunxiMemOpsS *memops = NULL;
    void *buffer_vaddr = NULL;
    int buffer_fd = -1;
    int ret = -1;

    printf("--- 系统顶层覆盖Demo (终极IOCTL补丁版) ---\n");
    printf("按下 Ctrl+C 退出程序。\n\n");

    // 步骤 1: 创建视频输出端口
    printf("[步骤 1] 创建视频输出端口 (图层ID: %d)...\n", OVERLAY_LAYER_ID);
    hdl = CreateVideoOutport(OVERLAY_LAYER_ID); //
    if (hdl == NULL) {
        printf("CreateVideoOutport 失败。\n");
        return -1;
    }
    printf("成功获取图层句柄: %p\n\n", hdl);

    // 步骤 2: 初始化ION内存分配器 (我们需要自己分配内存，以获取buffer_fd)
    printf("[步骤 2] 初始化ION内存分配器...\n");
    memops = GetMemAdapterOpsS(); //
    if (memops == NULL || SunxiMemOpen(memops) != 0) { //
        printf("初始化内存分配器失败。\n");
        DestroyVideoOutport(hdl);
        return -1;
    }
    size_t buffer_size = SCREEN_WIDTH * SCREEN_HEIGHT * 4;
    buffer_vaddr = SunxiMemPalloc(memops, buffer_size); //
    if (buffer_vaddr == NULL) {
        printf("SunxiMemPalloc() 失败。\n");
        SunxiMemClose(memops);
        DestroyVideoOutport(hdl);
        return -1;
    }
    buffer_fd = SunxiMemGetBufferFd(memops, buffer_vaddr); //
    if (buffer_fd < 0) {
        printf("SunxiMemGetBufferFd() 失败。\n");
        // ... cleanup
        return -1;
    }
    printf("内存分配成功, vaddr: %p, fd: %d\n\n", buffer_vaddr, buffer_fd);

    // 步骤 3: 填充缓冲区
    printf("[步骤 3] 填充缓冲区为半透明红色...\n");
    uint32_t *pixels = (uint32_t *)buffer_vaddr;
    for (size_t i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++) {
        pixels[i] = OVERLAY_COLOR;
    }
    SunxiMemFlushCache(memops, buffer_vaddr, buffer_size); //
    printf("填充并刷新缓存完成。\n\n");


    // 步骤 4: 初始化图层
    printf("[步骤 4] 调用 DispInit 初始化图层...\n");
    VoutRect display_rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    ret = DispInit(hdl, 1, ROTATION_ANGLE_0, &display_rect); //
    if(ret != 0) {
        printf("DispInit 失败。\n");
        // ... cleanup
        return -1;
    }
    printf("图层初始化成功。\n\n");


    // =================================================================
    // 步骤 5: (最终修正) 使用 DispSetIoctl 发送完美的配置
    // =================================================================
    printf("[步骤 5] 使用 DispSetIoctl 发送底层配置以修正颜色空间...\n");
    struct cdx_disp_layer_config2 config;
    memset(&config, 0, sizeof(struct cdx_disp_layer_config2));

    config.channel  = 0;
    config.layer_id = OVERLAY_LAYER_ID;
    config.enable   = 1;

    config.info.zorder        = 255;
    config.info.mode          = LAYER_MODE_BUFFER;
    config.info.alpha_mode    = 0; // Per-pixel alpha
    config.info.alpha_value   = 255;

    config.info.screen_win.x      = 0;
    config.info.screen_win.y      = 0;
    config.info.screen_win.width  = SCREEN_WIDTH;
    config.info.screen_win.height = SCREEN_HEIGHT;

    // --- fb: 帧缓冲信息，这是修正的核心 ---
    config.info.fb.fd           = buffer_fd;
    config.info.fb.format       = CDX_DISP_FORMAT_ARGB_8888;
    config.info.fb.size[0].width = SCREEN_WIDTH;
    config.info.fb.size[0].height= SCREEN_HEIGHT;
    // ** THE FIX **: 设置正确的RGB颜色空间，而不是YUV的BT.601
    config.info.fb.color_space  = CDX_DISP_GBR;

    printf(" - 配置颜色空间为: CDX_DISP_GBR (0x%X)\n", CDX_DISP_GBR);

    // 调用 IOCTL 补丁
    ret = DispSetIoctl(hdl, DISP_IOCTL_ARG_WITH_CONFIG, (unsigned long)&config); //
    if (ret != 0) {
        printf("DispSetIoctl 失败, ret: %d\n", ret);
        // ... cleanup
        return -1;
    }

    printf("\n叠加层已显示！程序将保持运行，请查看屏幕效果。\n");
    printf("按下 Ctrl+C 即可关闭叠加层并退出。\n");

    // 步骤 6: 保持运行
    while(1) {
        sleep(1);
    }

    // 在实际产品中，你需要捕获Ctrl+C信号(SIGINT)来执行下面的清理代码
    printf("\n程序退出，清理资源...\n");
    // 禁用图层...
    close(buffer_fd);
    SunxiMemPfree(memops, buffer_vaddr);
    SunxiMemClose(memops);
    DestroyVideoOutport(hdl);
    return 0;
}