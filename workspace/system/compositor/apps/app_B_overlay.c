// app_B_overlay.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// These would be part of your project's include structure
#include "sdl.h"
#include "api.h"
// End of project includes

#include "client_lib.h"
#include "protocol.h"


void render_scene_B(uint8_t* framebuffer) {
    if (!framebuffer) return;
    
    SDL_Surface* target_surface = SDL_CreateRGBSurfaceFrom(
        (void*)framebuffer, DEMO_WIDTH, DEMO_HEIGHT, 32, DEMO_PITCH,
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000 
    );
    if (!target_surface) {
        fprintf(stderr, "App B: SDL_CreateRGBSurfaceFrom failed: %s\n", SDL_GetError());
        return;
    }

    static int box_y = 0;
    static int y_direction = 3; 

    // Use a transparent color (alpha=0) to clear the background for an overlay
    SDL_FillRect(target_surface, NULL, 0); 

    box_y += y_direction;
    if (box_y <= 0 || box_y >= (target_surface->h - 100)) {
        y_direction = -y_direction; 
    }

    // Render a semi-transparent red rectangle
    Uint32 red = SDL_MapRGBA(target_surface->format, 255, 0, 0, 180); 
    SDL_Rect rect = {(target_surface->w / 2) - 50, box_y, 100, 100};
    SDL_FillRect(target_surface, &rect, red);
    
    SDL_FreeSurface(target_surface);
}


int main(int argc, char* argv[]) {
    GFX_init(MODE_MENU); 
    PAD_init();
    
    // This is process-wide, so it's called once.
    client_install_signal_handlers();

    // --- NEW API USAGE ---
    ClientConnection* conn = client_connect(1, "App B (Overlay)", CLIENT_TYPE_OVERLAY); 
    if (conn == NULL) {
        fprintf(stderr, "App B: Failed to connect to compositor.\n");
        GFX_quit();
        return 1;
    }
    
    client_enable_render_pause(conn);

    bool running = true;
    while(running) {
        // This is a process-wide check
        if (client_is_paused()) {
             usleep(16000); 
             continue;
        }

        PAD_poll();
        if (PAD_justPressed(BTN_SELECT)) {
             running = false;
        }
        
        uint8_t* framebuffer = client_get_render_buffer(conn);
        if (!framebuffer) {
             GFX_sync_fixed_rate(60.0);
             continue;
        }
        
        render_scene_B(framebuffer);
        client_present(conn, framebuffer);
        GFX_sync_fixed_rate(60.0);
    }
    
    client_disconnect(conn);
    // --- END NEW API USAGE ---
    
    PAD_quit();
    GFX_quit();
    printf("App B disconnected.\n");
    return 0;
}