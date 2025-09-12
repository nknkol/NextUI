#ifndef BT_VOLUME_SYNC_H
#define BT_VOLUME_SYNC_H

#include <stdint.h>

// ++ 添加所有共享的宏定义 ++
#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10
#define COLORTEMP_MIN 	0
#define COLORTEMP_MAX 	40
// -- 结束添加 --

// 启动蓝牙音量同步线程
void start_bt_volume_sync_thread(void);

// 停止蓝牙音量同步线程 (可选，用于优雅退出)
void stop_bt_volume_sync_thread(void);

// 将系统音量同步到蓝牙设备 (由 keymon 在音量键按下时调用)
void sync_volume_to_bt(int msettings_volume);

#endif // BT_VOLUME_SYNC_H