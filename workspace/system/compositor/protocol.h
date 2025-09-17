// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

// --- 基于您提供的 defines.h 和 platform.h ---
// 使用 tg5040 (非 brick) 的分辨率作为 Demo 标准
#define DEMO_WIDTH 1280
#define DEMO_HEIGHT 720
#define DEMO_BPP 4 // 使用 RGBA8888 以支持透明度
#define DEMO_PITCH (DEMO_WIDTH * DEMO_BPP)
#define DEMO_BUFFER_SIZE (DEMO_PITCH * DEMO_HEIGHT)

#define MAX_CLIENTS 2 // Demo 中我们只需要两个客户端

// --- IPC 路径定义 ---
#define SHM_PATH_PREFIX "/nextui_slot"
#define FIFO_PATH "/tmp/compositor_cmd_fifo"

// --- 信号定义 ---
#define SIGNAL_RESUME SIGUSR1
#define SIGNAL_PAUSE  SIGUSR2

// 共享内存中的控制块
// 每个客户端的共享内存区域开头都会有这个结构
typedef struct {
    pid_t client_pid;      // 客户端进程ID
    bool is_active;        // 客户端是否正在运行
    bool is_dirty;         // 客户端是否有新的一帧需要合成
    // 可以添加更多元数据，如画面尺寸等
} ClientControlBlock;

// 客户端 -> 合成器的命令
// 格式为： "COMMAND ARG1 ARG2\n"
// 例如: "REGISTER 1234 0\n"
//       "PRESENT 0\n"
typedef enum {
    CMD_UNKNOWN,
    CMD_REGISTER,     // 客户端注册: REGISTER <pid> <slot_hint>
    CMD_UNREGISTER,   // 客户端注销: UNREGISTER <pid>
    CMD_PRESENT,      // 提交新的一帧: PRESENT <slot_id>
} CommandType;

// 管理器 -> 合成器的命令 (用于快速切换和叠加)
// 例如: "SET_ACTIVE 1\n"
//       "SET_OVERLAY 1\n"
typedef enum {
    MGMT_CMD_SET_ACTIVE,   // 设置激活的客户端: SET_ACTIVE <slot_id>
    MGMT_CMD_SET_OVERLAY,  // 设置叠加层客户端: SET_OVERLAY <slot_id>
    MGMT_CMD_CLEAR_OVERLAY,// 清除叠加层: CLEAR_OVERLAY
} ManagementCommandType;


#endif // PROTOCOL_H