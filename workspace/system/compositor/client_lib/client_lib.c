// client_lib.c
#include "client_lib.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

static int g_shm_fds[MAX_CLIENTS];
static void* g_shm_ptrs[MAX_CLIENTS];
static volatile bool g_is_paused = true; // 默认启动时是暂停状态

static void pause_handler(int sig) {
    g_is_paused = true;
}

static void resume_handler(int sig) {
    g_is_paused = false;
}

void client_install_signal_handlers() {
    signal(SIGNAL_PAUSE, pause_handler);
    signal(SIGNAL_RESUME, resume_handler);
}

bool client_is_paused() {
    return g_is_paused;
}

int client_connect(int slot_hint) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_shm_fds[i] = -1;
        g_shm_ptrs[i] = NULL;
    }
    
    char shm_path[64];
    snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_PATH_PREFIX, slot_hint);

    int fd = shm_open(shm_path, O_RDWR, 0);
    if (fd == -1) {
        perror("client shm_open");
        return -1;
    }
    
    size_t shm_size = sizeof(ClientControlBlock) + DEMO_BUFFER_SIZE;
    void* ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("client mmap");
        close(fd);
        return -1;
    }
    
    g_shm_fds[slot_hint] = fd;
    g_shm_ptrs[slot_hint] = ptr;

    // 发送注册命令
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd == -1) {
        perror("client open fifo");
        munmap(ptr, shm_size);
        close(fd);
        return -1;
    }
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "REGISTER %d %d\n", getpid(), slot_hint);
    write(fifo_fd, cmd, strlen(cmd));
    close(fifo_fd);
    
    return slot_hint;
}

void client_disconnect(int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS) return;
    
    // 发送注销命令
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd != -1) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "UNREGISTER %d\n", getpid());
        write(fifo_fd, cmd, strlen(cmd));
        close(fifo_fd);
    }
    
    if (g_shm_ptrs[slot_id]) {
        munmap(g_shm_ptrs[slot_id], sizeof(ClientControlBlock) + DEMO_BUFFER_SIZE);
        g_shm_ptrs[slot_id] = NULL;
    }
    if (g_shm_fds[slot_id] != -1) {
        close(g_shm_fds[slot_id]);
        g_shm_fds[slot_id] = -1;
    }
}

uint8_t* client_get_framebuffer(int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS || !g_shm_ptrs[slot_id]) return NULL;
    return (uint8_t*)g_shm_ptrs[slot_id] + sizeof(ClientControlBlock);
}

void client_present(int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS) return;
    
    int fifo_fd = open(FIFO_PATH, O_WRONLY);
    if (fifo_fd != -1) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "PRESENT %d\n", slot_id);
        write(fifo_fd, cmd, strlen(cmd));
        close(fifo_fd);
    }
}