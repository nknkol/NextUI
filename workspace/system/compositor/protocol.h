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

#define MAX_CLIENTS 4 
#define MAX_OVERLAYS 4 // <--- 新增: 定义最大叠加层数量

#define SOCKET_PATH "/tmp/compositor_socket"
#define MGMT_SOCKET_PATH "/tmp/compositor_mgmt_socket"

#define SIGNAL_RESUME SIGUSR1
#define SIGNAL_PAUSE  SIGUSR2

#define SHM_CONTROL_PATH_PREFIX "/nextui_control_slot"


typedef enum {
    CLIENT_TYPE_NORMAL,         
    CLIENT_TYPE_OVERLAY,        
    CLIENT_TYPE_SWITCHER_UI,    
} ClientType;


typedef struct {
    pid_t client_pid;
    bool supports_render_pause;
    bool supports_exclusive_mode; 
    ClientType client_type;       
    char app_name[64];            
} ClientControlBlock;

typedef enum {
    MSG_TYPE_REGISTER,
    MSG_TYPE_UNREGISTER,
    MSG_TYPE_PRESENT_FRAME,
    MSG_TYPE_MGMT_COMMAND,
    MSG_TYPE_BUFFER_RELEASED    
} MessageType;

typedef struct {
    MessageType type;
    int slot_id;
} PresentFrameMessage;


typedef struct {
    MessageType type;
    int slot_id;
    pid_t pid;
    ClientType client_type; 
    char app_name[64];      
} RegisterMessage;

/*
 * MgmtCommandMessage's cmd_str will now support more complex commands:
 * - "SET_FOREGROUND <client_slot_id> <MODE>"
 * - "HIDE_CLIENT <client_slot_id>"
 * - "TERMINATE_CLIENT <client_slot_id>"
 * - "SET_OVERLAY <client_slot_id> <overlay_index> <x> <y> <width> <height>"
 * - "CLEAR_OVERLAY <overlay_index>"
 * - "LIST_CLIENTS"
 */
typedef struct {
    MessageType type;
    char cmd_str[128];
} MgmtCommandMessage;


typedef struct {
    MessageType type;
    int buffer_fd; 
} BufferReleasedMessage;


#define MAX_CLIENT_INFO 4

typedef struct {
    int slot_id;
    pid_t pid;
    char app_name[64];
    ClientType client_type;
    bool supports_render_pause;
    bool supports_exclusive_mode;
} ClientInfo;

typedef struct {
    int count;
    ClientInfo clients[MAX_CLIENT_INFO];
} ClientListResponse;


#endif // PROTOCOL_H