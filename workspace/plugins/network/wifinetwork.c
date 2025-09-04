/**
 * WifiManager Plugin for NextUI
 *
 * A C-language plugin to scan, connect to, and manage Wi-Fi networks.
 * This implementation is based on the C++ WifiMenu from the settings app
 * and follows the plugin structure demonstrated in ledcontrol.c.
 *
 * Final Revision: Corrected bottom hint display logic to be persistent
 * and ensured UI titles are correctly restored after leaving the keyboard.
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <msettings.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"
#include "plugin.h"
#include "sysui.h"
#include "keyboard.h"

// --- 插件内部状态定义 ---

#define WIFI_SCAN_INTERVAL_S 5
#define VISIBLE_ITEM_COUNT 5
#define OPTION_PADDING 8

typedef enum {
    VIEW_NETWORK_LIST,
    VIEW_PASSWORD,
} PluginView;

typedef struct {
    struct WIFI_network net;
    bool is_known;
    bool is_connected;
} WifiNetworkInfo;

typedef enum {
    WIFI_STATE_OFF,
    WIFI_STATE_ON,
    WIFI_STATE_TURNING_ON,
    WIFI_STATE_TURNING_OFF
} WifiUIState;

typedef enum {
    ACTION_NONE,
    ACTION_ENABLE,
    ACTION_DISABLE
} WifiAction;

// --- 插件静态全局变量 ---

static SDL_Surface* screen;
static int quit_plugin;
static int dirty;
static bool is_scrolling = false;

// --- 多线程与数据同步 ---
static pthread_t wifi_scan_thread;
static pthread_mutex_t list_mutex;
static volatile bool g_scan_results_updated = false;
static volatile bool g_thread_running = false;

static pthread_t wifi_action_thread;
static pthread_mutex_t action_mutex;
static volatile WifiAction g_wifi_action = ACTION_NONE;

static pthread_cond_t scan_cond;
static pthread_mutex_t scan_cond_mutex;

// --- 数据与状态 ---
static WifiNetworkInfo network_list[SCAN_MAX_RESULTS];
static int network_count;
static struct WIFI_connection current_connection;
static volatile WifiUIState g_wifi_ui_state = WIFI_STATE_OFF;

static PluginView current_view;
static int selected_index;
static int list_start_index;
static int show_setting = 0;

// --- 键盘调用封装 ---
static char* SysUI_ShowKeyboard(const char* title, const char* initial_text) {
    return ShowKeyboard(screen, &font, title, initial_text);
}

// --- UTF-8 解码辅助函数 ---
static void unescape_wifi_ssid(const char* src, char* dst, size_t dst_size) {
    size_t dst_len = 0;
    while (*src && dst_len < dst_size - 1) {
        if (*src == '\\' && *(src + 1) == 'x' && isxdigit((unsigned char)*(src + 2)) && isxdigit((unsigned char)*(src + 3))) {
            char hex[3] = {*(src + 2), *(src + 3), 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 4;
        } else {
            *dst++ = *src++;
        }
        dst_len++;
    }
    *dst = '\0';
}

// --- 后台线程函数 ---
static void* wifi_action_thread_func(void* arg) {
    pthread_mutex_lock(&action_mutex);
    WifiAction action = g_wifi_action;
    g_wifi_action = ACTION_NONE;
    pthread_mutex_unlock(&action_mutex);

    if (action == ACTION_ENABLE) WIFI_enable(true);
    else if (action == ACTION_DISABLE) WIFI_enable(false);

    pthread_mutex_lock(&scan_cond_mutex);
    pthread_cond_signal(&scan_cond);
    pthread_mutex_unlock(&scan_cond_mutex);
    return NULL;
}

static void* wifi_scanner_thread_func(void* arg) {
    while (g_thread_running) {
        bool is_enabled = WIFI_enabled();
        if (is_enabled) {
            g_wifi_ui_state = WIFI_STATE_ON;
            struct WIFI_network local_scanned_networks[SCAN_MAX_RESULTS];
            int local_count = WIFI_scan(local_scanned_networks, SCAN_MAX_RESULTS);

            if (local_count >= 0) {
                pthread_mutex_lock(&list_mutex);
                WIFI_connectionInfo(&current_connection);
                network_count = local_count;
                for (int i = 0; i < network_count; i++) {
                    network_list[i].net = local_scanned_networks[i];
                    network_list[i].is_connected = (strcmp(current_connection.ssid, local_scanned_networks[i].ssid) == 0);
                    network_list[i].is_known = WIFI_isKnown(local_scanned_networks[i].ssid, local_scanned_networks[i].security);
                }
                pthread_mutex_unlock(&list_mutex);
                g_scan_results_updated = true;
            }
        } else {
            g_wifi_ui_state = WIFI_STATE_OFF;
            pthread_mutex_lock(&list_mutex);
            if (network_count > 0) {
                network_count = 0;
                g_scan_results_updated = true;
            }
            pthread_mutex_unlock(&list_mutex);
        }
        
        pthread_mutex_lock(&scan_cond_mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += WIFI_SCAN_INTERVAL_S;
        pthread_cond_timedwait(&scan_cond, &scan_cond_mutex, &ts);
        pthread_mutex_unlock(&scan_cond_mutex);
    }
    return NULL;
}

// --- 渲染与辅助函数 ---

static int get_icons_width_for_info(WifiNetworkInfo* info) {
    if (!info) return 0;
    int width = 0;
    SDL_Rect asset_rect;
    
    GFX_assetRect(ASSET_WIFI, &asset_rect);
    width += asset_rect.w;
    width += SCALE1(OPTION_PADDING);

    if (info->is_connected) {
        GFX_assetRect(ASSET_CHECKCIRCLE, &asset_rect);
        width += asset_rect.w;
    } else if (info->net.security != SECURITY_NONE) {
        GFX_assetRect(ASSET_LOCK, &asset_rect);
        width += asset_rect.w + SCALE1(2);
    }
    return width;
}

static void update_scrolling_status() {
    is_scrolling = false;
    if (current_view != VIEW_NETWORK_LIST || selected_index == 0) return;

    pthread_mutex_lock(&list_mutex);
    if (selected_index - 1 >= network_count) {
        pthread_mutex_unlock(&list_mutex);
        return;
    }

    WifiNetworkInfo* info = &network_list[selected_index - 1];
    char decoded_ssid[SSID_MAX * 2];
    unescape_wifi_ssid(info->net.ssid, decoded_ssid, sizeof(decoded_ssid));

    int text_width;
    TTF_SizeUTF8(font.large, decoded_ssid, &text_width, NULL);
    
    int max_pill_width = screen->w - SCALE1(PADDING * 2);
    int icons_width = get_icons_width_for_info(info);
    int available_text_width = max_pill_width - (SCALE1(OPTION_PADDING) * 2) - icons_width - SCALE1(OPTION_PADDING);
    
    if (text_width > available_text_width) {
        is_scrolling = GFX_resetScrollText(font.large, decoded_ssid, available_text_width);
    }
    pthread_mutex_unlock(&list_mutex);
}

static void render_bottom_info(void) {
    if (current_view != VIEW_NETWORK_LIST || selected_index == 0) return;
    
    pthread_mutex_lock(&list_mutex);
    if (network_count == 0 || (selected_index - 1 >= network_count)) {
        pthread_mutex_unlock(&list_mutex);
        return;
    }

    const char* mac_address = network_list[selected_index - 1].net.bssid;
    pthread_mutex_unlock(&list_mutex);
    
    SDL_Color text_color = uintToColour(THEME_COLOR4_255);
    SDL_Surface* text_surf = TTF_RenderUTF8_Blended(font.tiny, mac_address, text_color);
    
    int y = screen->h - SCALE1(PADDING + PILL_SIZE + FONT_TINY + 4);
    int x = (screen->w - text_surf->w) / 2;

    SDL_Rect dst_rect = {x, y, text_surf->w, text_surf->h};
    SDL_BlitSurface(text_surf, NULL, screen, &dst_rect);
    SDL_FreeSurface(text_surf);
}

static void render_list_view() {
    pthread_mutex_lock(&list_mutex);

    int total_items = network_count + 1;
    int safe_area_top = SCALE1(PADDING + PILL_SIZE);
    int safe_area_bottom = screen->h - SCALE1(PADDING + PILL_SIZE);
    int available_height = safe_area_bottom - safe_area_top;
    int list_block_height = VISIBLE_ITEM_COUNT * SCALE1(PILL_SIZE);
    int list_oy = safe_area_top + ((available_height - list_block_height) / 2);

    for (int i = 0; i < VISIBLE_ITEM_COUNT; ++i) {
        int current_item_index = list_start_index + i;
        if (current_item_index >= total_items) break;

        bool is_selected = (current_item_index == selected_index);
        int y = list_oy + (i * SCALE1(PILL_SIZE));
        SDL_Rect item_rect = {SCALE1(PADDING), y, screen->w - SCALE1(PADDING * 2), SCALE1(PILL_SIZE)};
        
        char label_text_decoded[SSID_MAX * 2];
        const char* label_text;
        const char* value_text = "";
        WifiNetworkInfo* net_info = (current_item_index > 0) ? &network_list[current_item_index - 1] : NULL;
        
        if (current_item_index == 0) {
            label_text = "Wi-Fi";
            switch(g_wifi_ui_state) {
                case WIFI_STATE_ON: value_text = "On"; break;
                case WIFI_STATE_OFF: value_text = "Off"; break;
                case WIFI_STATE_TURNING_ON: value_text = "Turning on..."; break;
                case WIFI_STATE_TURNING_OFF: value_text = "Turning off..."; break;
            }
        } else {
            unescape_wifi_ssid(net_info->net.ssid, label_text_decoded, sizeof(label_text_decoded));
            label_text = label_text_decoded;
        }

        if (is_selected) {
            GFX_blitPillLight(ASSET_WHITE_PILL, screen, &item_rect);
        }

        if (strlen(value_text) > 0) {
            SDL_Color value_text_color = is_selected ? uintToColour(THEME_COLOR4_255) : uintToColour(THEME_COLOR6_255);
            SDL_Surface* value_surf = TTF_RenderUTF8_Blended(font.large, value_text, value_text_color);
            SDL_Rect value_dst = { item_rect.x + item_rect.w - value_surf->w - SCALE1(OPTION_PADDING), item_rect.y + SCALE1(3) };
            SDL_BlitSurface(value_surf, NULL, screen, &value_dst);
            SDL_FreeSurface(value_surf);
        } else if (net_info) {
            int icon_x = item_rect.x + item_rect.w - SCALE1(OPTION_PADDING);
            uint32_t icon_color = is_selected ? THEME_COLOR4 : THEME_COLOR6;

            int signal_asset = (net_info->net.rssi >= -60) ? ASSET_WIFI : (net_info->net.rssi >= -70) ? ASSET_WIFI_MED : ASSET_WIFI_LOW;
            SDL_Rect asset_rect_signal;
            GFX_assetRect(signal_asset, &asset_rect_signal);
            icon_x -= asset_rect_signal.w;
            SDL_Rect signal_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_signal.h) / 2};
            GFX_blitAssetColor(signal_asset, NULL, screen, &signal_dst, icon_color);

            icon_x -= SCALE1(OPTION_PADDING);
            if (net_info->is_connected) {
                SDL_Rect asset_rect_conn;
                GFX_assetRect(ASSET_CHECKCIRCLE, &asset_rect_conn);
                icon_x -= asset_rect_conn.w;
                SDL_Rect connected_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_conn.h) / 2};
                GFX_blitAssetColor(ASSET_CHECKCIRCLE, NULL, screen, &connected_dst, icon_color);
            } else if (net_info->net.security != SECURITY_NONE) {
                SDL_Rect asset_rect_lock;
                GFX_assetRect(ASSET_LOCK, &asset_rect_lock);
                icon_x -= (asset_rect_lock.w + SCALE1(2));
                SDL_Rect lock_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_lock.h) / 2};
                GFX_blitAssetColor(ASSET_LOCK, NULL, screen, &lock_dst, icon_color);
            }
        }

        if (is_selected) {
            int text_width = 0;
            TTF_SizeUTF8(font.large, label_text, &text_width, NULL);
            int white_pill_width = text_width + SCALE1(OPTION_PADDING * 2);
            
            if (is_scrolling) {
                int max_pill_width = screen->w - SCALE1(PADDING * 2);
                int icons_width = get_icons_width_for_info(net_info);
                int available_text_width = max_pill_width - (SCALE1(OPTION_PADDING) * 2) - icons_width - SCALE1(OPTION_PADDING);
                white_pill_width = available_text_width + SCALE1(OPTION_PADDING * 2);
            }

            SDL_Rect white_pill_rect = {item_rect.x, item_rect.y, white_pill_width, item_rect.h};
            GFX_blitPillDark(ASSET_WHITE_PILL, screen, &white_pill_rect);
            
            SDL_Color color = uintToColour(THEME_COLOR5_255);
            if (is_scrolling) {
                int max_pill_width = screen->w - SCALE1(PADDING * 2);
                int icons_width = get_icons_width_for_info(net_info);
                int available_text_width = max_pill_width - (SCALE1(OPTION_PADDING) * 2) - icons_width - SCALE1(OPTION_PADDING);
                GFX_scrollTextTexture(font.large, label_text, item_rect.x + SCALE1(OPTION_PADDING),
                                      item_rect.y + SCALE1(3), available_text_width, 0, color, 1.0f, 0);
            } else {
                SDL_Surface* label_surf = TTF_RenderUTF8_Blended(font.large, label_text, color);
                SDL_Rect label_dst = {item_rect.x + SCALE1(OPTION_PADDING), item_rect.y + SCALE1(3)};
                SDL_BlitSurface(label_surf, NULL, screen, &label_dst);
                SDL_FreeSurface(label_surf);
            }
        } else {
            SDL_Color label_text_color = uintToColour(THEME_COLOR4_255);
            SDL_Surface* label_surf = TTF_RenderUTF8_Blended(font.large, label_text, label_text_color);
            SDL_Rect label_dst = {item_rect.x + SCALE1(OPTION_PADDING), item_rect.y + SCALE1(3)};
            SDL_BlitSurface(label_surf, NULL, screen, &label_dst);
            SDL_FreeSurface(label_surf);
        }
    }
    pthread_mutex_unlock(&list_mutex);
}

static void render_plugin_page() {
    if (current_view == VIEW_NETWORK_LIST) {
        render_list_view();
    }
}

// --- 输入处理函数 ---
static void handle_list_input() {
    pthread_mutex_lock(&list_mutex);
    int total_items = network_count + 1;
    pthread_mutex_unlock(&list_mutex);
    
    int old_selected_index = selected_index;

    if (PAD_justPressed(BTN_UP)) {
        selected_index = (selected_index - 1 + total_items) % total_items;
        if (selected_index < list_start_index) list_start_index = selected_index;
        if (total_items > VISIBLE_ITEM_COUNT && selected_index == total_items - 1) list_start_index = total_items - VISIBLE_ITEM_COUNT;
    } else if (PAD_justPressed(BTN_DOWN)) {
        selected_index = (selected_index + 1) % total_items;
        if (selected_index >= list_start_index + VISIBLE_ITEM_COUNT) list_start_index = selected_index - VISIBLE_ITEM_COUNT + 1;
        if (selected_index == 0) list_start_index = 0;
    } else if (PAD_justPressed(BTN_L1)) {
        selected_index -= VISIBLE_ITEM_COUNT;
        if (selected_index < 0) selected_index = 0;
        list_start_index = selected_index;
    } else if (PAD_justPressed(BTN_R1)) {
        selected_index += VISIBLE_ITEM_COUNT;
        if (selected_index >= total_items) selected_index = total_items - 1;
        list_start_index = selected_index - VISIBLE_ITEM_COUNT + 1;
        if (list_start_index < 0) list_start_index = 0;
    }

    pthread_mutex_lock(&list_mutex);
    bool is_selected_item_connected = false;
    if (selected_index > 0 && selected_index -1 < network_count) {
        is_selected_item_connected = network_list[selected_index - 1].is_connected;
    }
    pthread_mutex_unlock(&list_mutex);

    if (is_selected_item_connected && PAD_justPressed(BTN_X)) {
        WIFI_disconnect();
    } else if (PAD_justPressed(BTN_A)) {
        if (selected_index == 0) {
            pthread_mutex_lock(&action_mutex);
            if (g_wifi_action == ACTION_NONE && (g_wifi_ui_state == WIFI_STATE_ON || g_wifi_ui_state == WIFI_STATE_OFF)) {
                g_wifi_action = (g_wifi_ui_state == WIFI_STATE_ON) ? ACTION_DISABLE : ACTION_ENABLE;
                g_wifi_ui_state = (g_wifi_action == ACTION_DISABLE) ? WIFI_STATE_TURNING_OFF : WIFI_STATE_TURNING_ON;
                pthread_create(&wifi_action_thread, NULL, wifi_action_thread_func, NULL);
                pthread_detach(wifi_action_thread);
            }
            pthread_mutex_unlock(&action_mutex);
        } else {
            pthread_mutex_lock(&list_mutex);
            if (selected_index -1 < network_count) {
                WifiNetworkInfo* selected_net = &network_list[selected_index - 1];
                if (!selected_net->is_connected) {
                     if (selected_net->net.security != SECURITY_NONE && !selected_net->is_known) {
                        current_view = VIEW_PASSWORD;
                    } else {
                        WIFI_connect(selected_net->net.ssid, selected_net->net.security);
                    }
                }
            }
            pthread_mutex_unlock(&list_mutex);
        }
    } else if (PAD_justPressed(BTN_B)) {
        quit_plugin = 1;
    }
    
    if (selected_index != old_selected_index) dirty = true;
    if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B) || PAD_justPressed(BTN_X)) dirty = true;

    if (total_items <= VISIBLE_ITEM_COUNT) list_start_index = 0;
    else if (list_start_index > total_items - VISIBLE_ITEM_COUNT) list_start_index = total_items - VISIBLE_ITEM_COUNT;
    if (list_start_index < 0) list_start_index = 0;

    char* l_btn = (BTN_SLEEP == BTN_POWER) ? "POWER" : "MENU";
    char* l_hint = "Sleep";
    char* r_btn1 = "B"; char* r_hint1 = "Back";
    char* r_btn2 = "A"; char* r_hint2 = "Select";

    if (is_selected_item_connected) { r_btn2 = "X"; r_hint2 = "Disconnect"; }
    
    SysUI_SetBottomHints(l_btn, l_hint, r_btn1, r_hint1, r_btn2, r_hint2);
}

// --- 插件生命周期函数 ---

static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0; dirty = 1; network_count = 0;
    selected_index = 0; list_start_index = 0;
    current_view = VIEW_NETWORK_LIST;
    g_wifi_ui_state = WIFI_enabled() ? WIFI_STATE_ON : WIFI_STATE_OFF;
    
    SysUI_Init(screen, &font);
    SysUI_SetTitle("Wi-Fi Manager");
    SysUI_SetFullscreen(false);
    SysUI_ShowBottomBar(true);
    PWR_init();

    pthread_mutex_init(&list_mutex, NULL);
    pthread_mutex_init(&action_mutex, NULL);
    pthread_mutex_init(&scan_cond_mutex, NULL);
    pthread_cond_init(&scan_cond, NULL);

    g_thread_running = true;
    if (pthread_create(&wifi_scan_thread, NULL, wifi_scanner_thread_func, NULL) != 0) {
        LOG_error("Failed to create Wi-Fi scanner thread.\n");
        g_thread_running = false;
    }
    return 0;
}

static int plugin_run() {
    bool input_handled_by_sysui = false;
    while (!quit_plugin) {
        GFX_startFrame();
        PAD_poll();
        PWR_update(&dirty, &show_setting, NULL, NULL);
        input_handled_by_sysui = SysUI_Update();

        if (g_scan_results_updated) {
            pthread_mutex_lock(&list_mutex);
            int total_items = network_count + 1;
            if (selected_index >= total_items) selected_index = total_items > 0 ? total_items - 1 : 0;
            pthread_mutex_unlock(&list_mutex);
            g_scan_results_updated = false;
            dirty = true;
            update_scrolling_status();
        }

        if (!input_handled_by_sysui) {
            int old_selected_index = selected_index;
            if (current_view == VIEW_NETWORK_LIST) {
                handle_list_input();
            } else if (current_view == VIEW_PASSWORD) {
                pthread_mutex_lock(&list_mutex);
                WifiNetworkInfo* selected_net = &network_list[selected_index - 1];
                char title[128];
                char decoded_ssid[SSID_MAX*2];
                unescape_wifi_ssid(selected_net->net.ssid, decoded_ssid, sizeof(decoded_ssid));
                snprintf(title, sizeof(title), "Password for %s", decoded_ssid);
                pthread_mutex_unlock(&list_mutex);
                
                char* password = SysUI_ShowKeyboard(title, "");
                
                if (password) {
                    pthread_mutex_lock(&list_mutex);
                    WIFI_connectPass(selected_net->net.ssid, selected_net->net.security, password);
                    pthread_mutex_unlock(&list_mutex);
                    free(password);
                }
                current_view = VIEW_NETWORK_LIST;
                dirty = true;
                SysUI_SetTitle("Wi-Fi Manager");
            }
            if (old_selected_index != selected_index) update_scrolling_status();
        }
        
        if (is_scrolling) dirty = true;

        if (dirty) {
            GFX_clear(screen);
            if (is_scrolling) GFX_clearLayers(4);
            
            render_plugin_page();
            SysUI_Render();
            render_bottom_info();
            GFX_flip(screen);
            dirty = false;
        } else {
            GFX_delay();
        }
    }
    return 0;
}

static void plugin_quit(void) {
    if (g_thread_running) {
        g_thread_running = false;
        pthread_mutex_lock(&scan_cond_mutex);
        pthread_cond_signal(&scan_cond);
        pthread_mutex_unlock(&scan_cond_mutex);
        pthread_join(wifi_scan_thread, NULL);
    }
    pthread_mutex_destroy(&list_mutex);
    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&scan_cond_mutex);
    pthread_cond_destroy(&scan_cond);

    PWR_quit();
    SysUI_Quit();
}

// --- 插件导出 ---
static NextUI_Plugin wifi_plugin_Export = {
    .name = "Wi-Fi",
    .display_path = SDCARD_PATH "/Tools/Settings/Network",
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

#ifdef __cplusplus
extern "C" {
#endif
NextUI_Plugin* GetPlugin(void) {
    return &wifi_plugin_Export;
}
#ifdef __cplusplus
}
#endif

