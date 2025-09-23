// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

#define DEMO_WIDTH 1024
#define DEMO_HEIGHT 768
#define DEMO_BPP 4 
#define DEMO_PITCH (DEMO_WIDTH * DEMO_BPP)
#define DEMO_BUFFER_SIZE (DEMO_PITCH * DEMO_HEIGHT)

#define MAX_CLIENTS 2

// MODIFIED: IPC从FIFO改为Unix Domain Socket
#define SOCKET_PATH "/tmp/compositor_socket"
// FIX: 为管理命令新增一个独立的DGRAM Socket路径，避免消息丢失
#define MGMT_SOCKET_PATH "/tmp/compositor_mgmt_socket"


// 信号定义保持不变
#define SIGNAL_RESUME SIGUSR1
#define SIGNAL_PAUSE  SIGUSR2

// 客户端控制块现在非常小，只用于状态同步，仍然使用shm_open
#define SHM_CONTROL_PATH_PREFIX "/nextui_control_slot"
typedef struct {
    pid_t client_pid;
    bool supports_render_pause;
} ClientControlBlock;


// NEW: 定义通过Socket传递的消息类型
typedef enum {
    MSG_TYPE_REGISTER,
    MSG_TYPE_UNREGISTER,
    MSG_TYPE_PRESENT_FRAME, // 客户端提交一帧
    MSG_TYPE_MGMT_COMMAND   // 其他管理命令
} MessageType;

// NEW: 客户端提交一帧时发送的消息结构
// 这个结构体将和文件描述符一起通过 sendmsg 发送
typedef struct {
    MessageType type;
    int slot_id;
    // ... 未来可以添加其他元数据，如时间戳
} PresentFrameMessage;

// NEW: 客户端注册时发送的消息
typedef struct {
    MessageType type;
    int slot_id;
    pid_t pid;
} RegisterMessage;

// NEW: 管理命令消息
typedef struct {
    MessageType type;
    char cmd_str[128];
} MgmtCommandMessage;


#endif // PROTOCOL_H