// compositor.c
#define EGL_EGLEXT_PROTOTYPES 1
#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "protocol.h"


static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_ptr = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_ptr = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES_ptr = NULL;

typedef struct {
    ClientControlBlock* control;
    void* control_shm_ptr;

    EGLImageKHR current_egl_image;
    GLuint texture_id;
    int client_sock_fd;
} ClientSlot;

static ClientSlot g_client_slots[MAX_CLIENTS];
static volatile bool g_running = true;
static int g_listen_sock_fd = -1;
static int g_mgmt_sock_fd = -1;


static int g_active_slot = -1;
static int g_overlay_slot = -1;
static bool g_exclusive_mode = false;
static int g_exclusive_slot = -1;
static int g_home_slot = -1; // 新增：用于存储 Home 应用的插槽 ID


static SDL_Window* g_window = NULL;
static SDL_GLContext g_gl_context = NULL;
static EGLDisplay g_egl_display = EGL_NO_DISPLAY;

static GLuint g_shader_program;
static GLuint g_vbo;

const char* vertex_shader_source =
    "attribute vec2 a_position;"
    "attribute vec2 a_texCoord;"
    "varying vec2 v_texCoord;"
    "void main()"
    "{"
    "   gl_Position = vec4(a_position, 0.0, 1.0);"
    "   v_texCoord = a_texCoord;"
    "}";

const char* fragment_shader_source =
    "precision mediump float;"
    "varying vec2 v_texCoord;"
    "uniform sampler2D s_texture;"
    "void main()"
    "{"
    "   gl_FragColor = texture2D(s_texture, v_texCoord);"
    "}";


static GLuint load_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int init_sdl_and_gl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);


    g_window = SDL_CreateWindow("Compositor", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                DEMO_WIDTH, DEMO_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    g_gl_context = SDL_GL_CreateContext(g_window);
    if (!g_gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_GL_MakeCurrent(g_window, g_gl_context) < 0) {
        fprintf(stderr, "SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        return -1;
    }
    
    if (SDL_GL_SetSwapInterval(1) < 0) {
        fprintf(stderr, "Warning: Unable to set VSync via SDL_GL_SetSwapInterval. Will rely on manual pacing.\n");
    }

    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay failed after SDL setup.\n");
        return -1;
    }

    eglCreateImageKHR_ptr = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR_ptr = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    glEGLImageTargetTexture2DOES_ptr = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");

    if (!eglCreateImageKHR_ptr || !eglDestroyImageKHR_ptr || !glEGLImageTargetTexture2DOES_ptr) {
        fprintf(stderr, "Failed to get required EGL/GL extension function pointers.\n");
        return -1;
    }

    GLuint vs = load_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fs = load_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    if (vs == 0 || fs == 0) return -1;
    
    g_shader_program = glCreateProgram();
    glAttachShader(g_shader_program, vs);
    glAttachShader(g_shader_program, fs);
    glLinkProgram(g_shader_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(g_shader_program);
    glViewport(0, 0, DEMO_WIDTH, DEMO_HEIGHT);

    GLfloat vertices[] = {
        -1.0f,  1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f,  1.0f,   1.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 1.0f
    };
    
    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLint pos_loc = glGetAttribLocation(g_shader_program, "a_position");
    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);

    GLint tex_loc = glGetAttribLocation(g_shader_program, "a_texCoord");
    glEnableVertexAttribArray(tex_loc);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    
    return 0;
}


static void render_slot(int slot_id, bool blend) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].texture_id == 0) {
        return;
    }

    if (blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_client_slots[slot_id].texture_id);
    GLint sampler_loc = glGetUniformLocation(g_shader_program, "s_texture");
    glUniform1i(sampler_loc, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int setup_ipc() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        char shm_path[64];
        snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_CONTROL_PATH_PREFIX, i);
        shm_unlink(shm_path);
        int shm_fd = shm_open(shm_path, O_CREAT | O_RDWR, 0666);
        if(shm_fd < 0) { perror("control shm_open"); return -1; }

        if(ftruncate(shm_fd, sizeof(ClientControlBlock)) == -1) { perror("control ftruncate"); return -1; }

        g_client_slots[i].control_shm_ptr = mmap(NULL, sizeof(ClientControlBlock), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        if(g_client_slots[i].control_shm_ptr == MAP_FAILED) { perror("control mmap"); return -1; }

        g_client_slots[i].control = (ClientControlBlock*)g_client_slots[i].control_shm_ptr;
        memset(g_client_slots[i].control, 0, sizeof(ClientControlBlock));
        g_client_slots[i].client_sock_fd = -1;
        g_client_slots[i].current_egl_image = EGL_NO_IMAGE_KHR;
        g_client_slots[i].texture_id = 0;
    }

    struct sockaddr_un addr;

    g_listen_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(g_listen_sock_fd < 0) { perror("socket"); return -1; }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);
    if (bind(g_listen_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("socket bind");
        return -1;
    }
    if (listen(g_listen_sock_fd, 5) == -1) {
        perror("socket listen");
        return -1;
    }
    
    g_mgmt_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_mgmt_sock_fd < 0) {
        perror("mgmt socket");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MGMT_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(MGMT_SOCKET_PATH);
    if (bind(g_mgmt_sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("mgmt socket bind");
        return -1;
    }

    return 0;
}

static void pause_client(int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].control->client_pid == 0) return;
    pid_t pid = g_client_slots[slot_id].control->client_pid;
    printf("Compositor: Pausing client '%s' (PID %d) in slot %d.\n", g_client_slots[slot_id].control->app_name, (int)pid, slot_id);
    fflush(stdout);
    if (g_client_slots[slot_id].control->supports_render_pause) {
        kill(pid, SIGNAL_PAUSE);
    } else {
        kill(pid, SIGSTOP);
    }
}

static void resume_client(int slot_id) {
    if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].control->client_pid == 0) return;
    pid_t pid = g_client_slots[slot_id].control->client_pid;
    printf("Compositor: Resuming client '%s' (PID %d) in slot %d.\n", g_client_slots[slot_id].control->app_name, (int)pid, slot_id);
    fflush(stdout);
    if (g_client_slots[slot_id].control->supports_render_pause) {
        kill(pid, SIGNAL_RESUME);
    } else {
        kill(pid, SIGCONT);
    }
}

void process_management_command(const char* cmd_str, const struct sockaddr_un* client_addr, socklen_t client_len) {
    printf("Compositor: Processing management command: [%s]\n", cmd_str);
    fflush(stdout);

    int slot_id;
    char mode_str[32];

    if (strcmp(cmd_str, "LIST_CLIENTS") == 0) {
        ClientListResponse response;
        response.count = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_client_slots[i].client_sock_fd != -1 && g_client_slots[i].control->client_type != CLIENT_TYPE_SWITCHER_UI) {
                ClientInfo* info = &response.clients[response.count];
                info->slot_id = i;
                info->pid = g_client_slots[i].control->client_pid;
                strncpy(info->app_name, g_client_slots[i].control->app_name, sizeof(info->app_name));
                info->client_type = g_client_slots[i].control->client_type;
                info->supports_render_pause = g_client_slots[i].control->supports_render_pause;
                info->supports_exclusive_mode = g_client_slots[i].control->supports_exclusive_mode;
                response.count++;
            }
        }
        if (client_addr) {
            sendto(g_mgmt_sock_fd, &response, sizeof(response), 0, (struct sockaddr*)client_addr, client_len);
        }

    } else if (sscanf(cmd_str, "SET_FOREGROUND %d %31s", &slot_id, mode_str) == 2) {
        if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].client_sock_fd == -1) return;

        if (g_exclusive_mode) {
            int old_exclusive_slot = g_exclusive_slot;
            g_exclusive_mode = false;
            g_exclusive_slot = -1;
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if(g_client_slots[i].client_sock_fd != -1 && i != old_exclusive_slot) resume_client(i);
            }
            
            g_overlay_slot = -1; 
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (g_client_slots[i].client_sock_fd != -1 && g_client_slots[i].control->client_type == CLIENT_TYPE_OVERLAY) {
                    printf("Compositor: Re-instating slot %d ('%s') as overlay.\n", i, g_client_slots[i].control->app_name);
                    g_overlay_slot = i;
                    resume_client(i);
                    break; 
                }
            }
        }

        if (strcmp(mode_str, "EXCLUSIVE") == 0) {
            if (!g_client_slots[slot_id].control->supports_exclusive_mode) {
                printf("Compositor: Client in slot %d does not support exclusive mode.\n", slot_id);
                return;
            }
            printf("Compositor: Activating exclusive mode for slot %d.\n", slot_id);
            g_exclusive_mode = true;
            g_exclusive_slot = slot_id;
            g_active_slot = -1; 
            g_overlay_slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (g_client_slots[i].client_sock_fd != -1) {
                    if (i == slot_id) resume_client(i);
                    else pause_client(i);
                }
            }
        } else if (strcmp(mode_str, "NORMAL") == 0) {
             printf("Compositor: Setting slot %d as normal foreground.\n", slot_id);
            if (g_active_slot != -1 && g_active_slot != slot_id) {
                pause_client(g_active_slot);
            }
            g_active_slot = slot_id;
            resume_client(g_active_slot);
        }

    } else if (sscanf(cmd_str, "HIDE_CLIENT %d", &slot_id) == 1) {
        if (slot_id < 0 || slot_id >= MAX_CLIENTS) return;
        printf("Compositor: Hiding client in slot %d.\n", slot_id);
        pause_client(slot_id);
        if (g_active_slot == slot_id) g_active_slot = -1;
        if (g_overlay_slot == slot_id) g_overlay_slot = -1;
        if (g_exclusive_slot == slot_id) {
             g_exclusive_mode = false;
             g_exclusive_slot = -1;
             for (int i = 0; i < MAX_CLIENTS; i++) {
                if(g_client_slots[i].client_sock_fd != -1) resume_client(i);
            }
        }

    } else if (sscanf(cmd_str, "TERMINATE_CLIENT %d", &slot_id) == 1) {
        if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].client_sock_fd == -1) return;
        pid_t pid = g_client_slots[slot_id].control->client_pid;
        printf("Compositor: Terminating client '%s' (PID %d) in slot %d.\n", g_client_slots[slot_id].control->app_name, (int)pid, slot_id);
        kill(pid, SIGTERM);

    } else if (sscanf(cmd_str, "SET_OVERLAY %d", &slot_id) == 1) {
        if (slot_id < 0 || slot_id >= MAX_CLIENTS || g_client_slots[slot_id].client_sock_fd == -1) return;
        printf("Compositor: Setting slot %d as overlay.\n", slot_id);
        g_overlay_slot = slot_id;
        resume_client(slot_id);

    } else if (strcmp(cmd_str, "CLEAR_OVERLAY") == 0) {
        printf("Compositor: Clearing overlay.\n");
        if (g_overlay_slot != -1 && g_overlay_slot != g_active_slot) {
            pause_client(g_overlay_slot);
        }
        g_overlay_slot = -1;
        
    } else {
        fprintf(stderr, "Compositor: Failed to parse management command: [%s]\n", cmd_str);
        fflush(stderr);
    }
    
    glFinish();
}

void handle_client_message(int slot_id) {
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    char msg_buf[1024];

    struct iovec iov[1];
    iov[0].iov_base = msg_buf;
    iov[0].iov_len = sizeof(msg_buf);

    struct msghdr msgh = {0};
    msgh.msg_iov = iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = cmsg_buf;
    msgh.msg_controllen = sizeof(cmsg_buf);

    ssize_t n = recvmsg(g_client_slots[slot_id].client_sock_fd, &msgh, 0);
    if (n <= 0) {
        printf("Client '%s' in slot %d disconnected.\n", g_client_slots[slot_id].control->app_name, slot_id);
        close(g_client_slots[slot_id].client_sock_fd);
        g_client_slots[slot_id].client_sock_fd = -1;
        memset(g_client_slots[slot_id].control, 0, sizeof(ClientControlBlock));
        if (g_client_slots[slot_id].texture_id != 0) {
             glDeleteTextures(1, &g_client_slots[slot_id].texture_id);
             g_client_slots[slot_id].texture_id = 0;
        }
        if (g_client_slots[slot_id].current_egl_image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR_ptr(g_egl_display, g_client_slots[slot_id].current_egl_image);
            g_client_slots[slot_id].current_egl_image = EGL_NO_IMAGE_KHR;
        }
        if (g_active_slot == slot_id) g_active_slot = -1;
        if (g_overlay_slot == slot_id) g_overlay_slot = -1;
        if (g_exclusive_slot == slot_id) {
            g_exclusive_slot = -1;
            g_exclusive_mode = false;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (g_client_slots[i].client_sock_fd != -1) resume_client(i);
            }
        }
        return;
    }
    
    MessageType type = ((MessageType*)msg_buf)[0];

    if (type == MSG_TYPE_PRESENT_FRAME) {
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msgh);
        if (cmsg && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int fd = *(int*)CMSG_DATA(cmsg);
            
            EGLint attribs[] = {
                EGL_WIDTH, DEMO_WIDTH,
                EGL_HEIGHT, DEMO_HEIGHT,
                EGL_LINUX_DRM_FOURCC_EXT, 0x34325241, 
                EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
                EGL_DMA_BUF_PLANE0_PITCH_EXT, DEMO_PITCH,
                EGL_NONE
            };

            EGLImageKHR new_image = eglCreateImageKHR_ptr(g_egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
            close(fd);

            if (new_image != EGL_NO_IMAGE_KHR) {
                if (g_client_slots[slot_id].texture_id == 0) {
                    glGenTextures(1, &g_client_slots[slot_id].texture_id);
                    glBindTexture(GL_TEXTURE_2D, g_client_slots[slot_id].texture_id);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }
                
                glBindTexture(GL_TEXTURE_2D, g_client_slots[slot_id].texture_id);
                glEGLImageTargetTexture2DOES_ptr(GL_TEXTURE_2D, (GLeglImageOES)new_image);
                
                if (g_client_slots[slot_id].current_egl_image != EGL_NO_IMAGE_KHR) {
                    eglDestroyImageKHR_ptr(g_egl_display, g_client_slots[slot_id].current_egl_image);
                }
                g_client_slots[slot_id].current_egl_image = new_image;

            } else {
                fprintf(stderr, "eglCreateImageKHR failed for slot %d. EGL error: 0x%x\n", slot_id, eglGetError());
            }

            char ack = 1;
            if (write(g_client_slots[slot_id].client_sock_fd, &ack, 1) < 0) {
                perror("Compositor: failed to send ack to client");
            }
        }
    } else {
        fprintf(stderr, "Compositor: Received message of unknown type %d on frame socket from slot %d.\n", (int)type, slot_id);
    }
}

void handle_signal(int sig) {
    g_running = false;
}

void cleanup() {
    printf("Compositor shutting down...\n");
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if (g_client_slots[i].client_sock_fd != -1) {
            kill(g_client_slots[i].control->client_pid, SIGTERM);
        }
    }
    sleep(1); 

    if (g_listen_sock_fd != -1) {
        close(g_listen_sock_fd);
        unlink(SOCKET_PATH);
    }
    if (g_mgmt_sock_fd != -1) {
        close(g_mgmt_sock_fd);
        unlink(MGMT_SOCKET_PATH);
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if(g_client_slots[i].client_sock_fd != -1) close(g_client_slots[i].client_sock_fd);
        if (g_client_slots[i].texture_id != 0) glDeleteTextures(1, &g_client_slots[i].texture_id);
        if (g_client_slots[i].current_egl_image != EGL_NO_IMAGE_KHR && eglDestroyImageKHR_ptr) eglDestroyImageKHR_ptr(g_egl_display, g_client_slots[i].current_egl_image);
        if (g_client_slots[i].control_shm_ptr) munmap(g_client_slots[i].control_shm_ptr, sizeof(ClientControlBlock));
        
        char shm_path[64];
        snprintf(shm_path, sizeof(shm_path), "%s_%d", SHM_CONTROL_PATH_PREFIX, i);
        shm_unlink(shm_path);
    }
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_shader_program) glDeleteProgram(g_shader_program);
    if (g_gl_context) SDL_GL_DeleteContext(g_gl_context);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--home-slot") == 0 && i + 1 < argc) {
            g_home_slot = atoi(argv[i + 1]);
            i++; 
        }
    }
    if (g_home_slot != -1) {
        printf("Compositor: Home slot designated to %d\n", g_home_slot);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    if (init_sdl_and_gl() != 0) { 
        cleanup(); 
        return 1; 
    }
    if (setup_ipc() != 0) { 
        cleanup(); 
        return 1; 
    }
    
    printf("Compositor started successfully.\n");

    const double target_fps = 60.0;
    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    uint64_t frame_duration_ticks = (uint64_t)(perf_freq / target_fps);
    uint64_t next_frame_time = SDL_GetPerformanceCounter();


    while (g_running) {
        if (!g_exclusive_mode && g_active_slot == -1 && g_home_slot != -1) {
            if (g_home_slot >= 0 && g_home_slot < MAX_CLIENTS && g_client_slots[g_home_slot].client_sock_fd != -1) {
                 printf("Compositor: No active client. Returning to home slot %d.\n", g_home_slot);
                 char cmd_str[128];
                 snprintf(cmd_str, sizeof(cmd_str), "SET_FOREGROUND %d NORMAL", g_home_slot);
                 process_management_command(cmd_str, NULL, 0);
            }
        }

        struct pollfd fds[MAX_CLIENTS + 2];
        int nfds = 0;

        fds[nfds].fd = g_listen_sock_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        fds[nfds].fd = g_mgmt_sock_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        int slot_map[MAX_CLIENTS + 2];
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (g_client_slots[i].client_sock_fd != -1) {
                fds[nfds].fd = g_client_slots[i].client_sock_fd;
                fds[nfds].events = POLLIN;
                slot_map[nfds] = i;
                nfds++;
            }
        }

        int ret = poll(fds, nfds, 0);
        if (ret > 0) {
            
            if (fds[0].revents & POLLIN) {
                int new_fd = accept(g_listen_sock_fd, NULL, NULL);
                if (new_fd != -1) {
                    RegisterMessage reg_msg;
                    ssize_t n = read(new_fd, &reg_msg, sizeof(reg_msg));
                    if (n == sizeof(reg_msg) && reg_msg.type == MSG_TYPE_REGISTER) {
                         int assigned_slot = -1;
                         int requested_slot = reg_msg.slot_id; 

                         if (requested_slot >= 0 && requested_slot < MAX_CLIENTS) {
                             if (g_client_slots[requested_slot].client_sock_fd == -1) {
                                 assigned_slot = requested_slot;
                                 printf("Client '%s' requested specific slot %d, and it was available. Granted.\n", reg_msg.app_name, assigned_slot);
                             } else {
                                 fprintf(stderr, "Client '%s' requested busy slot %d. Rejecting.\n", reg_msg.app_name, requested_slot);
                             }
                         } 
                         else {
                             for (int i = 0; i < MAX_CLIENTS; i++) {
                                 if (g_client_slots[i].client_sock_fd == -1) {
                                     assigned_slot = i;
                                     printf("Client '%s' requested any slot. Assigning first available: %d.\n", reg_msg.app_name, assigned_slot);
                                     break;
                                 }
                             }
                         }

                         if (write(new_fd, &assigned_slot, sizeof(assigned_slot)) < 0) {
                             perror("Failed to send assigned slot ID");
                             close(new_fd);
                         } else {
                            if (assigned_slot != -1) {
                                g_client_slots[assigned_slot].client_sock_fd = new_fd;
                                g_client_slots[assigned_slot].control->client_pid = reg_msg.pid;
                                g_client_slots[assigned_slot].control->client_type = reg_msg.client_type;
                                strncpy(g_client_slots[assigned_slot].control->app_name, reg_msg.app_name, sizeof(g_client_slots[assigned_slot].control->app_name));
                                printf("Client '%s' (PID %d) connected and assigned to slot %d\n", reg_msg.app_name, reg_msg.pid, assigned_slot);

                                if (reg_msg.client_type == CLIENT_TYPE_OVERLAY) {
                                    printf("Compositor: Auto-activating new client in slot %d ('%s') as overlay.\n", assigned_slot, reg_msg.app_name);
                                    if (g_overlay_slot != -1) {
                                        pause_client(g_overlay_slot);
                                    }
                                    g_overlay_slot = assigned_slot;
                                    resume_client(assigned_slot);
                                }

                            } else {
                                fprintf(stderr, "No available slots for client '%s'. Connection rejected.\n", reg_msg.app_name);
                                close(new_fd); 
                            }
                         }
                    } else {
                        fprintf(stderr, "Invalid registration message received. Closing connection.\n");
                        int err_slot = -1;
                        write(new_fd, &err_slot, sizeof(err_slot)); 
                        close(new_fd);
                    }
                }
            }
            
            if (fds[1].revents & POLLIN) {
                char mgmt_buf[sizeof(MgmtCommandMessage)];
                struct sockaddr_un client_addr;
                socklen_t client_len = sizeof(client_addr);
                ssize_t n = recvfrom(g_mgmt_sock_fd, mgmt_buf, sizeof(mgmt_buf), 0, (struct sockaddr*)&client_addr, &client_len);

                if (n > 0 && ((MessageType*)mgmt_buf)[0] == MSG_TYPE_MGMT_COMMAND) {
                    MgmtCommandMessage* msg = (MgmtCommandMessage*)mgmt_buf;
                    process_management_command(msg->cmd_str, &client_addr, client_len);
                }
            }
            
            for (int i = 2; i < nfds; i++) {
                if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
                    handle_client_message(slot_map[i]);
                }
            }
        }

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(g_shader_program);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        GLint pos_loc = glGetAttribLocation(g_shader_program, "a_position");
        glEnableVertexAttribArray(pos_loc);
        glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
        GLint tex_loc = glGetAttribLocation(g_shader_program, "a_texCoord");
        glEnableVertexAttribArray(tex_loc);
        glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

        if (g_exclusive_mode) {
            render_slot(g_exclusive_slot, false);
        } else {
            render_slot(g_active_slot, false);
            render_slot(g_overlay_slot, true);
        }
        
        SDL_GL_SwapWindow(g_window);

        next_frame_time += frame_duration_ticks;
        uint64_t current_time = SDL_GetPerformanceCounter();

        if (current_time < next_frame_time) {
            uint32_t delay_ms = ((next_frame_time - current_time) * 1000) / perf_freq;
            if (delay_ms > 2) { 
                SDL_Delay(delay_ms - 2);
            }
            while (SDL_GetPerformanceCounter() < next_frame_time) {}
        } else {
            next_frame_time = SDL_GetPerformanceCounter() + frame_duration_ticks;
        }
    }
    
    cleanup();
    return 0;
}