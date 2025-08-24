#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main() {
    int fd;
    drmModeRes *resources;
    drmModeConnector *connector = NULL;
    drmModeEncoder *encoder = NULL;
    drmModeCrtc *crtc = NULL;
    int i, j;

    // 1. 打开 DRM 设备
    // 通常是 /dev/dri/card0
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("无法打开 DRM 设备");
        return -1;
    }

    // 2. 获取 DRM 资源 (CRTC, Encoder, Connector)
    resources = drmModeGetResources(fd);
    if (!resources) {
        perror("无法获取 DRM 资源");
        close(fd);
        return -1;
    }

    // 3. 查找一个已连接的 Connector
    // Connector 代表一个物理输出端口 (如 HDMI, VGA, DSI)
    for (i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            // 找到了一个连接的屏幕，就用它
            printf("找到了一个连接的屏幕: connector id %d\n", connector->connector_id);
            break;
        }
        drmModeFreeConnector(connector);
        connector = NULL;
    }

    if (!connector) {
        fprintf(stderr, "没有找到任何连接的屏幕\n");
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    // 4. 获取屏幕的首选显示模式 (分辨率和刷新率)
    drmModeModeInfo *mode = &connector->modes[0];
    printf("使用模式: %s (%dx%d@%dHz)\n", mode->name, mode->hdisplay, mode->vdisplay, mode->vrefresh);

    // 5. 查找与 Connector 匹配的 Encoder 和 CRTC
    // Encoder 负责将像素数据转换为显示信号 (如 HDMI 信号)
    for (i = 0; i < resources->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, resources->encoders[i]);
        if (encoder->encoder_id == connector->encoder_id) {
            printf("找到了匹配的 encoder id %d\n", encoder->encoder_id);
            break;
        }
        drmModeFreeEncoder(encoder);
        encoder = NULL;
    }

    if (!encoder) {
        fprintf(stderr, "没有找到匹配的 Encoder\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    // CRTC (CRT Controller) 是一个扫描引擎，负责从内存中读取像素并发送给 Encoder
    crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    if (!crtc) {
        fprintf(stderr, "没有找到匹配的 CRTC\n");
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }
    printf("找到了匹配的 CRTC id %d\n", crtc->crtc_id);

    // 6. 创建一个 Dumb Buffer (简单的内存帧缓冲区)
    struct drm_mode_create_dumb create_req = {0};
    struct drm_mode_map_dumb map_req = {0};
    uint32_t fb_id;

    create_req.width = mode->hdisplay;
    create_req.height = mode->vdisplay;
    create_req.bpp = 32; // 32 位色深 (ARGB8888 或 XRGB8888)
    ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);

    // 7. 将 Dumb Buffer 转换为一个 Framebuffer ID
    uint32_t handles[4] = {create_req.handle};
    uint32_t pitches[4] = {create_req.pitch};
    uint32_t offsets[4] = {0};
    drmModeAddFB2(fd, create_req.width, create_req.height,
                  DRM_FORMAT_XRGB8888, // 假设是 32-bit XRGB 格式，这是最常见的
                  handles, pitches, offsets, &fb_id, 0);

    // 8. 内存映射 (mmap) Framebuffer，以便我们能用 CPU 写入数据
    map_req.handle = create_req.handle;
    ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
    uint32_t *fb_ptr = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);

    // 9. 绘制红色方块 (实际上是填充整个屏幕为红色)
    // 颜色格式是 0x00RRGGBB (因为我们用了 XRGB8888)
    for (j = 0; j < create_req.height; j++) {
        for (i = 0; i < create_req.width; i++) {
            fb_ptr[j * (create_req.pitch / 4) + i] = 0x00FF0000; // 红色
        }
    }

    // 10. 设置显示模式 (Mode Setting)
    // 这是最关键的一步，它告诉显示控制器使用我们的 Framebuffer 进行显示
    drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, mode);

    printf("屏幕应该已经变红了。程序将在 5 秒后退出...\n");
    sleep(5);

    // 11. 恢复原始的显示模式
    drmModeSetCrtc(fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector->connector_id, 1, &crtc->mode);

    // 12. 清理资源
    munmap(fb_ptr, create_req.size);
    drmModeRmFB(fd, fb_id);
    struct drm_mode_destroy_dumb destroy_req = { .handle = create_req.handle };
    ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_req);

    drmModeFreeCrtc(crtc);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(fd);

    printf("程序退出。\n");
    return 0;
}