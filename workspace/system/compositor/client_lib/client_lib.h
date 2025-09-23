// client_lib.h
#ifndef CLIENT_LIB_H
#define CLIENT_LIB_H

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

// Functions for client applications
int client_connect(int slot_hint);
void client_disconnect(int slot_id);

// MODIFIED: 返回的是可直接渲染的ION缓冲区指针
uint8_t* client_get_render_buffer(int slot_id); 
void client_present(int slot_id, uint8_t* buffer_ptr); // 提交指定的buffer

// Signal handling
void client_install_signal_handlers();
bool client_is_paused();

// Management commands
void client_request_exclusive(int slot_id);
void client_release_exclusive(int slot_id);
void client_enable_render_pause(int slot_id);

#endif // CLIENT_LIB_H