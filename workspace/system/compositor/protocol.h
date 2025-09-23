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

#define SOCKET_PATH "/tmp/compositor_socket"
#define MGMT_SOCKET_PATH "/tmp/compositor_mgmt_socket"

#define SIGNAL_RESUME SIGUSR1
#define SIGNAL_PAUSE  SIGUSR2

#define SHM_CONTROL_PATH_PREFIX "/nextui_control_slot"
typedef struct {
    pid_t client_pid;
    bool supports_render_pause;
} ClientControlBlock;

typedef enum {
    MSG_TYPE_REGISTER,
    MSG_TYPE_UNREGISTER,
    MSG_TYPE_PRESENT_FRAME,
    MSG_TYPE_MGMT_COMMAND,
    MSG_TYPE_BUFFER_RELEASED    // <--- 【新增】合成器通知客户端缓冲区已释放
} MessageType;

typedef struct {
    MessageType type;
    int slot_id;
} PresentFrameMessage;

typedef struct {
    MessageType type;
    int slot_id;
    pid_t pid;
} RegisterMessage;

typedef struct {
    MessageType type;
    char cmd_str[128];
} MgmtCommandMessage;

// <--- 【新增】合成器释放缓冲区时发送的消息 ---
typedef struct {
    MessageType type;
    int buffer_fd; // 用fd来唯一标识被释放的缓冲区
} BufferReleasedMessage;

#endif // PROTOCOL_H