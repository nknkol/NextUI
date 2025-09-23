// client_lib.c
#include "client_lib.h"
#include <ion_mem_alloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>

#define NUM_BUFFERS 2 // 双缓冲

typedef struct {
    void* ptr;
    int fd;
} IonBuffer;

static IonBuffer g_ion_buffers[NUM_BUFFERS];
static int g_current_buffer_idx = 0;

static int g_socket_fd = -1;
static int g_mgmt_socket_fd = -1; // FIX: 管理命令专用socket
static int g_control_shm_fd = -1;
static ClientControlBlock* g_control_block_ptr = NULL;
static struct SunxiMemOpsS* g_memops = NULL;

static volatile bool g_is_paused = true;

// --- 信号处理 (不变) ---
static void pause_handler(int sig) { g_is_paused = true; }
static void resume_handler(int sig) { g_is_paused = false; }

void client_install_signal_handlers() {
    signal(SIGNAL_PAUSE, pause_handler);
    signal(SIGNAL_RESUME, resume_handler);
}

bool client_is_paused() { return g_is_paused; }

// --- 核心实现 ---

int client_connect(int slot_hint) {
    // 1. 初始化ION内存分配器
    g_memops = GetMemAdapterOpsS();
    if (SunxiMemOpen(g_memops) != 0) {
        perror("SunxiMemOpen");
        return -1;
    }

    // 2. 为双缓冲分配ION内存
    for (int i = 0; i < NUM_BUFFERS; i++) {
        g_ion_buffers[i].ptr = SunxiMemPalloc(g_memops, DEMO_BUFFER_SIZE);
        if (!g_ion_buffers[i].ptr) {
            perror("SunxiMemPalloc");
            return -1;
        }
        g_ion_buffers[i].fd = SunxiMemGetBufferFd(g_memops, g_ion_buffers[i].ptr);
        if (g_ion_buffers[i].fd < 0) {
            perror("SunxiMemGetBufferFd");
            return -1;
        }
    }
    g_current_buffer_idx = 0;
    
    // 3. 连接到Unix Domain Socket (用于帧数据)
    g_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socket_fd == -1) {
        perror("socket");
        return -1;
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect to compositor socket");
        close(g_socket_fd);
        return -1;
    }

    // FIX: 创建并连接到管理命令的DGRAM Socket
    g_mgmt_socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_mgmt_socket_fd == -1) {
        perror("mgmt socket");
        close(g_socket_fd);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MGMT_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(g_mgmt_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect to compositor mgmt socket");
        close(g_socket_fd);
        close(g_mgmt_socket_fd);
        return -1;
    }
    printf("Client: Frame socket and Management socket connected.\n"); fflush(stdout);


    // 4. 映射小型的控制块 (用于状态同步)
    char shm_path[64];
    snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_CONTROL_PATH_PREFIX, slot_hint);
    g_control_shm_fd = shm_open(shm_path, O_RDWR, 0);
    if (g_control_shm_fd == -1) { return -1; }
    g_control_block_ptr = mmap(NULL, sizeof(ClientControlBlock), PROT_READ | PROT_WRITE, MAP_SHARED, g_control_shm_fd, 0);
    if (g_control_block_ptr == MAP_FAILED) { return -1; }

    // 5. 发送注册消息
    RegisterMessage reg_msg;
    reg_msg.type = MSG_TYPE_REGISTER;
    reg_msg.slot_id = slot_hint;
    reg_msg.pid = getpid();
    write(g_socket_fd, &reg_msg, sizeof(reg_msg));
    
    return slot_hint;
}

void client_disconnect(int slot_id) {
    if (g_socket_fd != -1) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }
    // FIX: 关闭管理命令socket
    if (g_mgmt_socket_fd != -1) {
        close(g_mgmt_socket_fd);
        g_mgmt_socket_fd = -1;
    }
    if (g_control_block_ptr) {
        munmap(g_control_block_ptr, sizeof(ClientControlBlock));
    }
    if (g_control_shm_fd != -1) {
        close(g_control_shm_fd);
    }
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if(g_ion_buffers[i].ptr) {
            SunxiMemPfree(g_memops, g_ion_buffers[i].ptr);
        }
    }
    SunxiMemClose(g_memops);
}

uint8_t* client_get_render_buffer(int slot_id) {
    return (uint8_t*)g_ion_buffers[g_current_buffer_idx].ptr;
}

void client_present(int slot_id, uint8_t* buffer_ptr) {
    if (g_socket_fd == -1) return;

    int presented_idx = -1;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (g_ion_buffers[i].ptr == buffer_ptr) {
            presented_idx = i;
            break;
        }
    }
    if (presented_idx == -1) return; // 无效的buffer指针

    // FIX: 刷新CPU缓存以解决花屏问题
    if (g_memops && g_ion_buffers[presented_idx].ptr) {
        SunxiMemFlushCache(g_memops, g_ion_buffers[presented_idx].ptr, DEMO_BUFFER_SIZE);
    }

    PresentFrameMessage msg;
    msg.type = MSG_TYPE_PRESENT_FRAME;
    msg.slot_id = slot_id;

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    struct msghdr msgh = {0};
    struct iovec iov[1];
    
    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof(msg);

    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = cmsg_buf;
    msgh.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msgh);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int*)CMSG_DATA(cmsg) = g_ion_buffers[presented_idx].fd;
    
    sendmsg(g_socket_fd, &msgh, 0);

    // 切换到下一个缓冲区
    g_current_buffer_idx = (g_current_buffer_idx + 1) % NUM_BUFFERS;
}


static void send_management_command(const char* cmd) {
    // FIX: 使用专用的管理命令socket
    if (g_mgmt_socket_fd == -1) return;
    MgmtCommandMessage msg;
    msg.type = MSG_TYPE_MGMT_COMMAND;
    strncpy(msg.cmd_str, cmd, sizeof(msg.cmd_str) - 1);
    msg.cmd_str[sizeof(msg.cmd_str) - 1] = '\0';

    // NEW LOG: 打印将要发送的命令
    printf("Client: Sending MGMT command: [%s]\n", msg.cmd_str);
    fflush(stdout);

    write(g_mgmt_socket_fd, &msg, sizeof(msg));
}

void client_request_exclusive(int slot_id) {
    char cmd[64];
    // FIX: 移除命令末尾的 '\n'
    snprintf(cmd, sizeof(cmd), "REQUEST_EXCLUSIVE %d", slot_id);
    send_management_command(cmd);
}

void client_release_exclusive(int slot_id) {
    char cmd[64];
    // FIX: 移除命令末尾的 '\n'
    snprintf(cmd, sizeof(cmd), "RELEASE_EXCLUSIVE %d", slot_id);
    send_management_command(cmd);
}

void client_enable_render_pause(int slot_id) {
    if (g_control_block_ptr) {
        g_control_block_ptr->supports_render_pause = true;
    }
}