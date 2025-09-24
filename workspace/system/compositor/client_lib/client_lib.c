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

#define NUM_BUFFERS 2 

typedef struct {
    void* ptr;
    int fd;
} IonBuffer;

// NEW: All per-connection state is now in this struct
struct ClientConnection {
    int slot_id;
    int socket_fd;
    int mgmt_socket_fd;
    int control_shm_fd;
    ClientControlBlock* control_block_ptr;
    struct SunxiMemOpsS* memops;
    IonBuffer ion_buffers[NUM_BUFFERS];
    int current_buffer_idx;
    bool is_connected;
};

// Process-wide state for signal handling remains global
static volatile bool g_is_paused = true;

static void pause_handler(int sig) { g_is_paused = true; }
static void resume_handler(int sig) { g_is_paused = false; }

void client_install_signal_handlers() {
    signal(SIGNAL_PAUSE, pause_handler);
    signal(SIGNAL_RESUME, resume_handler);
}

bool client_is_paused() { return g_is_paused; }

int client_get_slot_id(ClientConnection* handle) {
    if (!handle) return -1;
    return handle->slot_id;
}

ClientConnection* client_connect(int slot_hint, const char* app_name, ClientType type) {
    ClientConnection* handle = (ClientConnection*)calloc(1, sizeof(ClientConnection));
    if (!handle) {
        perror("malloc ClientConnection");
        return NULL;
    }
    
    // Initialize handle state
    handle->slot_id = -1;
    handle->socket_fd = -1;
    handle->mgmt_socket_fd = -1;
    handle->control_shm_fd = -1;

    handle->memops = GetMemAdapterOpsS();
    if (SunxiMemOpen(handle->memops) != 0) {
        perror("SunxiMemOpen");
        free(handle);
        return NULL;
    }

    for (int i = 0; i < NUM_BUFFERS; i++) {
        handle->ion_buffers[i].ptr = SunxiMemPalloc(handle->memops, DEMO_BUFFER_SIZE);
        if (!handle->ion_buffers[i].ptr) {
            perror("SunxiMemPalloc");
            // Cleanup already allocated buffers
            for (int j = 0; j < i; j++) SunxiMemPfree(handle->memops, handle->ion_buffers[j].ptr);
            SunxiMemClose(handle->memops);
            free(handle);
            return NULL;
        }
        handle->ion_buffers[i].fd = SunxiMemGetBufferFd(handle->memops, handle->ion_buffers[i].ptr);
    }

    handle->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (handle->socket_fd == -1) {
        perror("socket");
        client_disconnect(handle); // Use disconnect for proper cleanup
        return NULL; 
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(handle->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect to compositor socket");
        client_disconnect(handle);
        return NULL;
    }

    RegisterMessage reg_msg;
    reg_msg.type = MSG_TYPE_REGISTER;
    reg_msg.pid = getpid();
    reg_msg.slot_id = slot_hint; 
    reg_msg.client_type = type;
    strncpy(reg_msg.app_name, app_name, sizeof(reg_msg.app_name) - 1);
    reg_msg.app_name[sizeof(reg_msg.app_name) - 1] = '\0';

    if (write(handle->socket_fd, &reg_msg, sizeof(reg_msg)) != sizeof(reg_msg)) {
        perror("Failed to send registration message");
        client_disconnect(handle);
        return NULL;
    }

    ssize_t n = read(handle->socket_fd, &handle->slot_id, sizeof(handle->slot_id));
    if (n != sizeof(handle->slot_id) || handle->slot_id < 0) {
        fprintf(stderr, "Failed to get a valid slot ID from compositor.\n");
        client_disconnect(handle);
        return NULL;
    }
    printf("Client '%s': Successfully registered, assigned Slot ID: %d\n", app_name, handle->slot_id);

    handle->mgmt_socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (handle->mgmt_socket_fd == -1) {
        perror("mgmt socket");
        client_disconnect(handle);
        return NULL;
    }

    char client_mgmt_path[128];
    snprintf(client_mgmt_path, sizeof(client_mgmt_path), "/tmp/client_mgmt_%d_%d", getpid(), handle->slot_id);
    unlink(client_mgmt_path);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, client_mgmt_path, sizeof(addr.sun_path) - 1);
    if (bind(handle->mgmt_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind client mgmt socket");
        client_disconnect(handle);
        return NULL;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MGMT_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(handle->mgmt_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect to compositor mgmt socket");
        client_disconnect(handle);
        return NULL;
    }
    
    char shm_path[64];
    snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_CONTROL_PATH_PREFIX, handle->slot_id);
    handle->control_shm_fd = shm_open(shm_path, O_RDWR, 0);
    if (handle->control_shm_fd == -1) { 
        perror("control shm_open");
        client_disconnect(handle); 
        return NULL;
    }
    handle->control_block_ptr = mmap(NULL, sizeof(ClientControlBlock), PROT_READ | PROT_WRITE, MAP_SHARED, handle->control_shm_fd, 0);
    if (handle->control_block_ptr == MAP_FAILED) {
        perror("control mmap");
        client_disconnect(handle);
        return NULL;
    }
    
    handle->is_connected = true; 
    return handle; 
}

void client_disconnect(ClientConnection* handle) {
    if (!handle) return;

    handle->is_connected = false; 

    char client_mgmt_path[128];
    snprintf(client_mgmt_path, sizeof(client_mgmt_path), "/tmp/client_mgmt_%d_%d", getpid(), handle->slot_id);
    unlink(client_mgmt_path);

    if (handle->socket_fd != -1) close(handle->socket_fd);
    if (handle->mgmt_socket_fd != -1) close(handle->mgmt_socket_fd);
    if (handle->control_block_ptr) munmap(handle->control_block_ptr, sizeof(ClientControlBlock));
    if (handle->control_shm_fd != -1) close(handle->control_shm_fd);

    if (handle->memops) {
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if(handle->ion_buffers[i].ptr) {
                SunxiMemPfree(handle->memops, handle->ion_buffers[i].ptr);
            }
        }
        SunxiMemClose(handle->memops);
    }
    free(handle);
}

uint8_t* client_get_render_buffer(ClientConnection* handle) {
    if (!handle || !handle->is_connected) return NULL;
    return (uint8_t*)handle->ion_buffers[handle->current_buffer_idx].ptr;
}

void client_present(ClientConnection* handle, uint8_t* buffer_ptr) {
    if (!handle || handle->socket_fd == -1 || !handle->is_connected) return;

    int presented_idx = -1;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (handle->ion_buffers[i].ptr == buffer_ptr) {
            presented_idx = i;
            break;
        }
    }
    if (presented_idx == -1) return;

    if (handle->memops) {
        SunxiMemFlushCache(handle->memops, handle->ion_buffers[presented_idx].ptr, DEMO_BUFFER_SIZE);
    }

    PresentFrameMessage msg;
    msg.type = MSG_TYPE_PRESENT_FRAME;
    msg.slot_id = handle->slot_id;

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
    *(int*)CMSG_DATA(cmsg) = handle->ion_buffers[presented_idx].fd;
    
    if (sendmsg(handle->socket_fd, &msgh, 0) < 0) {
        perror("Client: sendmsg failed, disconnecting");
        handle->is_connected = false; // Mark as disconnected to prevent further calls
        return;
    }

    char ack_buffer;
    ssize_t n = read(handle->socket_fd, &ack_buffer, 1);
    if (n <= 0) {
        handle->is_connected = false;
        return;
    }

    handle->current_buffer_idx = (handle->current_buffer_idx + 1) % NUM_BUFFERS;
}


static void send_management_command(ClientConnection* handle, const char* cmd) {
    if (!handle || handle->mgmt_socket_fd == -1 || !handle->is_connected) return;
    MgmtCommandMessage msg;
    msg.type = MSG_TYPE_MGMT_COMMAND;
    strncpy(msg.cmd_str, cmd, sizeof(msg.cmd_str) - 1);
    msg.cmd_str[sizeof(msg.cmd_str) - 1] = '\0';
    write(handle->mgmt_socket_fd, &msg, sizeof(msg));
}

void client_enable_render_pause(ClientConnection* handle) {
    if (handle && handle->control_block_ptr) {
        handle->control_block_ptr->supports_render_pause = true;
    }
}

void client_set_exclusive_support(ClientConnection* handle, bool supported) {
    if (handle && handle->control_block_ptr) {
        handle->control_block_ptr->supports_exclusive_mode = supported;
    }
}

void client_set_foreground(ClientConnection* handle, const char* mode) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "SET_FOREGROUND %d %s", handle->slot_id, mode);
    send_management_command(handle, cmd);
}

void client_hide(ClientConnection* handle) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "HIDE_CLIENT %d", handle->slot_id);
    send_management_command(handle, cmd);
}

void client_terminate(ClientConnection* handle) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "TERMINATE_CLIENT %d", handle->slot_id);
    send_management_command(handle, cmd);
}

void client_set_overlay(ClientConnection* handle) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "SET_OVERLAY %d", handle->slot_id);
    send_management_command(handle, cmd);
}

void client_clear_overlay(ClientConnection* handle) {
    send_management_command(handle, "CLEAR_OVERLAY");
}

int client_list_clients(ClientConnection* handle, ClientListResponse* response) {
    if (!handle || handle->mgmt_socket_fd == -1 || !handle->is_connected || !response) return -1;
    
    send_management_command(handle, "LIST_CLIENTS");
    
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(handle->mgmt_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t n = recv(handle->mgmt_socket_fd, response, sizeof(ClientListResponse), 0);

    if (n < 0 || n != sizeof(ClientListResponse)) {
        if (n < 0) perror("recv for client list");
        else fprintf(stderr, "Received incomplete client list\n");
        return -1;
    }
    return 0;
}