// client_lib.h
#ifndef CLIENT_LIB_H
#define CLIENT_LIB_H

#include <stdint.h>
#include <stdbool.h>

// 连接到合成器，获取一个渲染槽位
// 返回槽位ID，如果失败返回-1
int client_connect(int slot_hint);

// 断开与合成器的连接
void client_disconnect(int slot_id);

// 获取当前帧的帧缓冲地址
uint8_t* client_get_framebuffer(int slot_id);

// 通知合成器一帧已经渲染完毕
void client_present(int slot_id);

// 安装信号处理器以响应暂停/恢复
void client_install_signal_handlers(void);

// 检查是否应该暂停渲染
bool client_is_paused(void);

#endif // CLIENT_LIB_H