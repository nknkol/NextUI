// compositor.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include "sdl.h"

#include "protocol.h" 
#include "api.h"

// 保存每个客户端槽位的信息
typedef struct {
    void* shm_ptr;              // 共享内存映射地址
    ClientControlBlock* control; // 指向控制块
    uint8_t* framebuffer;       // 指向帧缓冲数据
    SDL_Surface* surface;       // 用于渲染的SDL表面
} ClientSlot;

static ClientSlot g_client_slots[MAX_CLIENTS];
static int g_active_slot = -1;
static int g_overlay_slot = -1;
static int g_cmd_fifo_fd = -1;
static volatile bool g_running = true;

// 清理函数
void cleanup() {
    printf("Compositor shutting down...\n");
    g_running = false;
    
    if (g_cmd_fifo_fd != -1) {
        close(g_cmd_fifo_fd);
        unlink(FIFO_PATH);
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_client_slots[i].shm_ptr) {
            munmap(g_client_slots[i].shm_ptr, sizeof(ClientControlBlock) + DEMO_BUFFER_SIZE);
        }
        // shm_fd is managed by client_lib, no need to close here
        char shm_path[64];
        snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_PATH_PREFIX, i);
        shm_unlink(shm_path);
        
        if (g_client_slots[i].surface) {
            SDL_FreeSurface(g_client_slots[i].surface);
        }
    }
    GFX_quit();
}

void handle_signal(int sig) {
    cleanup();
    exit(0);
}

// 初始化IPC资源
int setup_ipc_and_surfaces() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        char shm_path[64];
        snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_PATH_PREFIX, i);
        
        int shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            perror("shm_open"); return -1;
        }
        
        size_t shm_size = sizeof(ClientControlBlock) + DEMO_BUFFER_SIZE;
        if (ftruncate(shm_fd, shm_size) == -1) {
            perror("ftruncate"); close(shm_fd); return -1;
        }
        
        g_client_slots[i].shm_ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd); // fd can be closed after mmap
        if (g_client_slots[i].shm_ptr == MAP_FAILED) {
            perror("mmap"); return -1;
        }
        
        g_client_slots[i].control = (ClientControlBlock*)g_client_slots[i].shm_ptr;
        g_client_slots[i].framebuffer = (uint8_t*)g_client_slots[i].shm_ptr + sizeof(ClientControlBlock);
        
        memset(g_client_slots[i].control, 0, sizeof(ClientControlBlock));
        
        // 创建一个对应的 SDL_Surface 来接收像素数据
        g_client_slots[i].surface = SDL_CreateRGBSurfaceWithFormatFrom(
            g_client_slots[i].framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 
            DEMO_BPP * 8, DEMO_PITCH, SDL_PIXELFORMAT_RGBA8888);
        
        if (!g_client_slots[i].surface) {
            fprintf(stderr, "Failed to create surface for slot %d\n", i); return -1;
        }
        SDL_SetSurfaceBlendMode(g_client_slots[i].surface, SDL_BLENDMODE_BLEND);

        printf("Created SHM and Surface for slot %d\n", i);
    }
    
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo"); return -1;
    }
    g_cmd_fifo_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (g_cmd_fifo_fd == -1) {
        perror("open fifo"); return -1;
    }
    
    printf("Command FIFO created at %s\n", FIFO_PATH);
    return 0;
}

void process_command(const char* cmd_str) {
    pid_t pid;
    int slot_id;

    if (sscanf(cmd_str, "REGISTER %d %d", &pid, &slot_id) == 2) {
        if (slot_id >= 0 && slot_id < MAX_CLIENTS && g_client_slots[slot_id].control->client_pid == 0) {
            printf("Registering client PID %d to slot %d\n", pid, slot_id);
            g_client_slots[slot_id].control->client_pid = pid;
            g_client_slots[slot_id].control->is_active = true;
            if (g_active_slot == -1) {
                g_active_slot = slot_id;
                 kill(pid, SIGNAL_RESUME); // 激活第一个连接的客户端
            } else {
                 kill(pid, SIGNAL_PAUSE);
            }
        }
    } else if (sscanf(cmd_str, "UNREGISTER %d", &pid) == 1) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_client_slots[i].control->client_pid == pid) {
                printf("Unregistering client PID %d from slot %d\n", pid, i);
                memset(g_client_slots[i].control, 0, sizeof(ClientControlBlock));
                if (g_active_slot == i) g_active_slot = -1;
                if (g_overlay_slot == i) g_overlay_slot = -1;
                break;
            }
        }
    } else if (sscanf(cmd_str, "PRESENT %d", &slot_id) == 1) {
        if (slot_id >= 0 && slot_id < MAX_CLIENTS) {
            g_client_slots[slot_id].control->is_dirty = true;
        }
    } else if (sscanf(cmd_str, "SET_ACTIVE %d", &slot_id) == 1) {
        if (slot_id >= 0 && slot_id < MAX_CLIENTS && g_client_slots[slot_id].control->is_active) {
            printf("Switching active view to slot %d\n", slot_id);
            g_active_slot = slot_id;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(g_client_slots[i].control->is_active && g_client_slots[i].control->client_pid != 0) {
                    if (i == g_active_slot || i == g_overlay_slot) {
                         kill(g_client_slots[i].control->client_pid, SIGNAL_RESUME);
                    } else {
                         kill(g_client_slots[i].control->client_pid, SIGNAL_PAUSE);
                    }
                }
            }
        }
    } else if (sscanf(cmd_str, "SET_OVERLAY %d", &slot_id) == 1) {
         if (slot_id >= 0 && slot_id < MAX_CLIENTS && g_client_slots[slot_id].control->is_active) {
            printf("Setting overlay to slot %d\n", slot_id);
            g_overlay_slot = slot_id;
            if (g_client_slots[slot_id].control->client_pid != 0) {
                kill(g_client_slots[slot_id].control->client_pid, SIGNAL_RESUME);
            }
         }
    } else if (strncmp(cmd_str, "CLEAR_OVERLAY", 13) == 0) {
        printf("Clearing overlay\n");
        if (g_overlay_slot != -1 && g_overlay_slot != g_active_slot) {
            if (g_client_slots[g_overlay_slot].control->client_pid != 0) {
                kill(g_client_slots[g_overlay_slot].control->client_pid, SIGNAL_PAUSE);
            }
        }
        g_overlay_slot = -1;
    }
}


int main(int argc, char* argv[]) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    GFX_init(MODE_MAIN);
    
    if (setup_ipc_and_surfaces() != 0) {
        fprintf(stderr, "Failed to setup IPC\n");
        GFX_quit();
        return 1;
    }

    printf("Compositor started successfully.\n");

    char cmd_buffer[256];
    while (g_running) {
        // a. 处理命令
        int bytes_read = read(g_cmd_fifo_fd, cmd_buffer, sizeof(cmd_buffer) - 1);
        if (bytes_read > 0) {
            cmd_buffer[bytes_read] = '\0';
            process_command(cmd_buffer);
        }

        // b. 使用平台的分层API进行渲染
        PLAT_clearLayers(0); // 清空所有层

        // 渲染激活的客户端到背景层 (layer 1)
        if (g_active_slot != -1) {
            ClientSlot* slot = &g_client_slots[g_active_slot];
            if (slot->control->is_active) {
                // (不需要更新surface，因为它直接指向共享内存)
                PLAT_drawOnLayer(slot->surface, 0, 0, DEMO_WIDTH, DEMO_HEIGHT, 1.0f, false, 1);
                slot->control->is_dirty = false; // 标记为已渲染
            }
        }

        // 渲染叠加层到前景层 (layer 2)
        if (g_overlay_slot != -1) {
             ClientSlot* slot = &g_client_slots[g_overlay_slot];
             if (slot->control->is_active) {
                PLAT_drawOnLayer(slot->surface, 0, 0, DEMO_WIDTH, DEMO_HEIGHT, 1.0f, false, 2);
                slot->control->is_dirty = false;
             }
        }

        // 提交所有层到屏幕
        PLAT_GPU_Flip();
        
        usleep(16000); // 维持~60FPS
    }
    
    cleanup();
    return 0;
}