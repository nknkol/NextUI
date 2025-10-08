// client_lib.h
#ifndef CLIENT_LIB_H
#define CLIENT_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"
#ifdef __cplusplus
extern "C" {
#endif
// --- NEW: Handle-based API ---

// Forward declaration of the opaque handle structure.
// The user of the library only deals with pointers to it.
struct ClientConnection;
typedef struct ClientConnection ClientConnection;


/**
 * @brief Connects to the compositor and establishes a new client session.
 * @param slot_hint A preferred slot ID, or -1 for any.
 * @param app_name The name of this client part (e.g., "My App UI").
 * @param type The type of client, used by the compositor for layout rules.
 * @return A pointer to a ClientConnection handle on success, or NULL on failure.
 */
ClientConnection* client_connect(int slot_hint, const char* app_name, ClientType type);

/**
 * @brief Disconnects from the compositor and frees all associated resources.
 * @param handle The connection handle returned by client_connect.
 */
void client_disconnect(ClientConnection* handle);

/**
 * @brief Retrieves a writable buffer for the client to render into.
 * @param handle The connection handle.
 * @return A pointer to the framebuffer memory, or NULL if not available.
 */
uint8_t* client_get_render_buffer(ClientConnection* handle);

/**
 * @brief Presents the rendered buffer to the compositor for display.
 * @param handle The connection handle.
 * @param buffer_ptr The pointer to the buffer that was just rendered, returned by client_get_render_buffer.
 */
void client_present(ClientConnection* handle, uint8_t* buffer_ptr);

/**
 * @brief Gets the slot ID assigned by the compositor for this connection.
 * @param handle The connection handle.
 * @return The assigned slot ID.
 */
int client_get_slot_id(ClientConnection* handle);

typedef struct {
    uint8_t* ptr;
    int fd;
} ClientRenderTarget;

ClientRenderTarget client_get_render_target(ClientConnection* handle);

// --- Process-Wide Functions ---

/**
 * @brief Installs signal handlers to respond to compositor's pause/resume signals.
 * This only needs to be called once per process.
 */
void client_install_signal_handlers();

/**
 * @brief Checks if the process has been paused by the compositor.
 * This is a process-wide state.
 * @return True if the process is paused, false otherwise.
 */
bool client_is_paused();


// --- Per-Connection Configuration and Commands ---

void client_enable_render_pause(ClientConnection* handle);
void client_set_exclusive_support(ClientConnection* handle, bool supported);

void client_set_foreground(ClientConnection* handle, const char* mode);
void client_hide(ClientConnection* handle);
void client_terminate(ClientConnection* handle);

/**
 * @brief 将当前客户端设置为一个指定区域的叠加层
 * @param handle 连接句柄
 * @param overlay_index 目标叠加层索引 (0-3)
 * @param x, y, width, height 屏幕上的目标渲染区域
 */
void client_set_overlay_region(ClientConnection* handle, int overlay_index, int x, int y, int width, int height);

/**
 * @brief 清除一个指定的叠加层
 * @param handle 连接句柄
 * @param overlay_index 要清除的叠加层索引 (0-3)
 */
void client_clear_overlay_index(ClientConnection* handle, int overlay_index);

int client_list_clients(ClientConnection* handle, ClientListResponse* response);


#ifdef __cplusplus
}
#endif

#endif // CLIENT_LIB_H