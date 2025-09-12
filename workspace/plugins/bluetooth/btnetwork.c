/**
 * BluetoothManager Plugin for NextUI
 *
 * A C-language plugin to scan, pair, and manage Bluetooth devices.
 * This implementation is based on the C++ BtMenu from the settings app
 * and mirrors the structure of the wifinetwork.c plugin.
 *
 * MODIFIED: Refactored to be fully asynchronous to prevent UI blocking.
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

// --- 插件内部状态定义 ---

#define BT_SCAN_INTERVAL_S 5
#define VISIBLE_ITEM_COUNT 5
#define OPTION_PADDING 8


typedef enum {
    VIEW_DEVICE_LIST,
    VIEW_OPTIONS,
} PluginView;

typedef struct {
    struct BT_device dev;
    struct BT_devicePaired paired_dev;
    bool is_paired;
} BluetoothDeviceInfo;


// --- MODIFIED: Enhanced state machine ---
typedef enum {
    BT_STATE_UNINITIALIZED,
    BT_STATE_INITIALIZING,
    BT_STATE_OFF,
    BT_STATE_ON,
    BT_STATE_TURNING_ON,
    BT_STATE_TURNING_OFF
} BluetoothUIState;

typedef enum {
    ACTION_NONE,
    ACTION_ENABLE,
    ACTION_DISABLE,
    ACTION_START_DISCOVERY,
    ACTION_STOP_DISCOVERY,
} BluetoothAction;


// --- 插件静态全局变量 ---

static SDL_Surface* screen;
static int quit_plugin;
static int dirty;

// --- 多线程与数据同步 ---
static pthread_t bt_scan_thread;
static pthread_mutex_t list_mutex;
static volatile bool g_scan_results_updated = false;
static volatile bool g_thread_running = false; // Controls the scanner thread

static pthread_t bt_action_thread;
static pthread_mutex_t action_mutex;
static volatile BluetoothAction g_bt_action = ACTION_NONE;

static pthread_cond_t scan_cond;
static pthread_mutex_t scan_cond_mutex;


// --- 数据与状态 ---
static BluetoothDeviceInfo device_list[SCAN_MAX_RESULTS];
static int device_count;
// --- MODIFIED: Set initial state ---
static volatile BluetoothUIState g_bt_ui_state = BT_STATE_UNINITIALIZED;

static PluginView current_view;
static int selected_index;
static int list_start_index;
static int show_setting = 0;


// --- 后台线程函数 ---

// --- MODIFIED: Action thread now updates the final state ---
static void* bt_action_thread_func(void* arg) {
    pthread_mutex_lock(&action_mutex);
    BluetoothAction action = g_bt_action;
    g_bt_action = ACTION_NONE; // Consume the action
    pthread_mutex_unlock(&action_mutex);

    if (action == ACTION_ENABLE) {
        BT_enable(true);
    } else if (action == ACTION_DISABLE) {
        BT_enable(false);
    } else if (action == ACTION_START_DISCOVERY) {
        BT_discovery(1);
    } else if (action == ACTION_STOP_DISCOVERY) {
        BT_discovery(0);
    }

    // After the action is complete, update the state based on the actual result
    if (action == ACTION_ENABLE || action == ACTION_DISABLE) {
        g_bt_ui_state = BT_enabled() ? BT_STATE_ON : BT_STATE_OFF;
        dirty = true; // Trigger a UI refresh to show the new state
    }

    // Wake up the scanner thread to let it re-evaluate the new state
    pthread_mutex_lock(&scan_cond_mutex);
    pthread_cond_signal(&scan_cond);
    pthread_mutex_unlock(&scan_cond_mutex);
    return NULL;
}

// --- MODIFIED: Scanner thread is now state-aware ---
static void* bt_scanner_thread_func(void* arg) {
    while (g_thread_running) {
        // Only perform scanning and device logic if the state is ON
        if (g_bt_ui_state == BT_STATE_ON) {
            // This logic is now safe because we know BT is enabled and stable.
            if (!BT_isConnected() && !BT_discovering()) {
                LOG_note(LOG_REALTIME, "Not connected and not discovering, starting Bluetooth discovery...\n");
                BT_discovery(true);
            } else if (BT_isConnected() && BT_discovering()) {
                LOG_note(LOG_REALTIME, "Device is connected, stopping Bluetooth discovery to maintain connection.\n");
                BT_discovery(false);
            }

            struct BT_device local_available_devices[SCAN_MAX_RESULTS];
            struct BT_devicePaired local_paired_devices[SCAN_MAX_RESULTS];
            
            int available_count = BT_availableDevices(local_available_devices, SCAN_MAX_RESULTS);
            int paired_count = BT_pairedDevices(local_paired_devices, SCAN_MAX_RESULTS);

            LOG_note(LOG_REALTIME, "BT_availableDevices returned: %d\n", available_count);
            LOG_note(LOG_REALTIME, "BT_pairedDevices returned: %d\n", paired_count);

            if (available_count >= 0 && paired_count >= 0) {
                pthread_mutex_lock(&list_mutex);
                device_count = 0;
                
                // Paired devices first
                for (int i = 0; i < paired_count; i++) {
                    if (device_count < SCAN_MAX_RESULTS) {
                        device_list[device_count].paired_dev = local_paired_devices[i];
                        strncpy(device_list[device_count].dev.name, local_paired_devices[i].remote_name, sizeof(device_list[device_count].dev.name) - 1);
                        strncpy(device_list[device_count].dev.addr, local_paired_devices[i].remote_addr, sizeof(device_list[device_count].dev.addr) - 1);
                        device_list[device_count].is_paired = true;
                        device_count++;
                    }
                }
                
                // Then available devices, avoiding duplicates
                for (int i = 0; i < available_count; i++) {
                     bool found = false;
                     for (int j = 0; j < paired_count; j++) {
                         if (strcmp(local_available_devices[i].addr, local_paired_devices[j].remote_addr) == 0) {
                             found = true;
                             break;
                         }
                     }
                     if (!found && device_count < SCAN_MAX_RESULTS) {
                         device_list[device_count].dev = local_available_devices[i];
                         device_list[device_count].is_paired = false;
                         device_count++;
                     }
                }

                pthread_mutex_unlock(&list_mutex);
                g_scan_results_updated = true;
            }
        } else {
            // If Bluetooth is not ON (it's off, turning off, initializing, etc.),
            // ensure the device list is cleared.
            pthread_mutex_lock(&list_mutex);
            if (device_count > 0) {
                device_count = 0;
                g_scan_results_updated = true;
            }
            pthread_mutex_unlock(&list_mutex);
        }
        
        // Wait for the defined interval or until an action signals a state change
        pthread_mutex_lock(&scan_cond_mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += BT_SCAN_INTERVAL_S;
        pthread_cond_timedwait(&scan_cond, &scan_cond_mutex, &ts);
        pthread_mutex_unlock(&scan_cond_mutex);
    }
    
    if (BT_discovering()) {
        LOG_note(LOG_REALTIME, "Stopping Bluetooth discovery on thread exit.\n");
        BT_discovery(false);
    }

    return NULL;
}


// --- NEW: Asynchronous initialization thread ---
static void* bt_init_thread_func(void* arg) {
    LOG_note(LOG_REALTIME, "Bluetooth init thread started.\n");
    
    // Perform the blocking initialization
    BT_init();

    LOG_note(LOG_REALTIME, "BT_init() completed.\n");
    
    // After init, determine the correct state
    g_bt_ui_state = BT_enabled() ? BT_STATE_ON : BT_STATE_OFF;
    dirty = true; // Trigger UI refresh

    // Now that BT stack is ready, start the scanner thread
    g_thread_running = true;
    if (pthread_create(&bt_scan_thread, NULL, bt_scanner_thread_func, NULL) != 0) {
        LOG_error("Failed to create Bluetooth scanner thread.\n");
        g_thread_running = false;
        g_bt_ui_state = BT_STATE_OFF; // Fallback to a safe state
    }

    return NULL;
}


// --- 渲染与辅助函数 ---

static int get_icons_width_for_info(BluetoothDeviceInfo* info) {
    if (!info) return 0;
    int width = 0;
    SDL_Rect asset_rect;
    
    GFX_assetRect(ASSET_WIFI, &asset_rect); // Using WIFI asset for size estimation
    width += asset_rect.w;
    width += SCALE1(OPTION_PADDING);

    if (info->is_paired && info->paired_dev.is_connected) {
        GFX_assetRect(ASSET_CHECKCIRCLE, &asset_rect);
        width += asset_rect.w;
    } else if (info->is_paired) {
        GFX_assetRect(ASSET_LOCK, &asset_rect);
        width += asset_rect.w + SCALE1(2);
    }
    return width;
}

static void render_list_view() {
    pthread_mutex_lock(&list_mutex);

    int total_items = device_count + 1; // +1 for the enable/disable toggle
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
        
        const char* label_text;
        const char* value_text = "";
        BluetoothDeviceInfo* dev_info = (current_item_index > 0) ? &device_list[current_item_index - 1] : NULL;
        
        if (current_item_index == 0) {
            label_text = "Bluetooth";
            // --- MODIFIED: Handle new states in renderer ---
            switch(g_bt_ui_state) {
                case BT_STATE_UNINITIALIZED: value_text = "Status Unknown"; break;
                case BT_STATE_INITIALIZING:  value_text = "Initializing..."; break;
                case BT_STATE_ON:            value_text = "On"; break;
                case BT_STATE_OFF:           value_text = "Off"; break;
                case BT_STATE_TURNING_ON:    value_text = "Turning on..."; break;
                case BT_STATE_TURNING_OFF:   value_text = "Turning off..."; break;
            }
        } else {
            label_text = dev_info->dev.name;
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
        } else if (dev_info) {
             int icon_x = item_rect.x + item_rect.w - SCALE1(OPTION_PADDING);
            uint32_t icon_color = is_selected ? THEME_COLOR4 : THEME_COLOR6;

            int signal_asset = ASSET_WIFI;
            if(dev_info->is_paired) {
                 signal_asset = (dev_info->paired_dev.rssi >= -60) ? ASSET_WIFI : (dev_info->paired_dev.rssi >= -70) ? ASSET_WIFI_MED : ASSET_WIFI_LOW;
            }

            SDL_Rect asset_rect_signal;
            GFX_assetRect(signal_asset, &asset_rect_signal);
            icon_x -= asset_rect_signal.w;
            SDL_Rect signal_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_signal.h) / 2};
            GFX_blitAssetColor(signal_asset, NULL, screen, &signal_dst, icon_color);

            icon_x -= SCALE1(OPTION_PADDING);
            if (dev_info->is_paired && dev_info->paired_dev.is_connected) {
                SDL_Rect asset_rect_conn;
                GFX_assetRect(ASSET_CHECKCIRCLE, &asset_rect_conn);
                icon_x -= asset_rect_conn.w;
                SDL_Rect connected_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_conn.h) / 2};
                GFX_blitAssetColor(ASSET_CHECKCIRCLE, NULL, screen, &connected_dst, icon_color);
            } else if (dev_info->is_paired) {
                 SDL_Rect asset_rect_lock;
                GFX_assetRect(ASSET_LOCK, &asset_rect_lock);
                icon_x -= (asset_rect_lock.w + SCALE1(2));
                SDL_Rect lock_dst = {icon_x, item_rect.y + (item_rect.h - asset_rect_lock.h) / 2};
                GFX_blitAssetColor(ASSET_LOCK, NULL, screen, &lock_dst, icon_color);
            }
        }

        SDL_Color label_text_color = is_selected ? uintToColour(THEME_COLOR5_255) : uintToColour(THEME_COLOR4_255);
        SDL_Surface* label_surf = TTF_RenderUTF8_Blended(font.large, label_text, label_text_color);
        if(is_selected){
            int text_width;
            TTF_SizeUTF8(font.large, label_text, &text_width, NULL);
            int white_pill_width = text_width + SCALE1(OPTION_PADDING*2);
            GFX_blitPillDark(ASSET_WHITE_PILL, screen, &(SDL_Rect){item_rect.x, item_rect.y, white_pill_width,item_rect.h});
        }

        SDL_Rect label_dst = {item_rect.x + SCALE1(OPTION_PADDING), item_rect.y + SCALE1(3)};
        SDL_BlitSurface(label_surf, NULL, screen, &label_dst);
        SDL_FreeSurface(label_surf);
    }
    pthread_mutex_unlock(&list_mutex);
}

static void render_plugin_page() {
    if (current_view == VIEW_DEVICE_LIST) {
        render_list_view();
    }
}


// --- 输入处理函数 ---
static void handle_list_input() {
    pthread_mutex_lock(&list_mutex);
    int total_items = device_count + 1;
    pthread_mutex_unlock(&list_mutex);
    
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
    } else if(PAD_justPressed(BTN_SELECT)){
         pthread_mutex_lock(&action_mutex);
         if(g_bt_action == ACTION_NONE){
             if(BT_discovering()){
                 g_bt_action = ACTION_STOP_DISCOVERY;
             } else {
                 g_bt_action = ACTION_START_DISCOVERY;
             }
             pthread_create(&bt_action_thread, NULL, bt_action_thread_func, NULL);
             pthread_detach(bt_action_thread);
         }
         pthread_mutex_unlock(&action_mutex);
    } else if (PAD_justPressed(BTN_A)) {
        if (selected_index == 0) {
            pthread_mutex_lock(&action_mutex);
            // Only allow enable/disable if the BT stack is in a stable state
            if (g_bt_action == ACTION_NONE && (g_bt_ui_state == BT_STATE_ON || g_bt_ui_state == BT_STATE_OFF)) {
                g_bt_action = (g_bt_ui_state == BT_STATE_ON) ? ACTION_DISABLE : ACTION_ENABLE;
                // Immediately update UI to show "turning on/off" for responsiveness
                g_bt_ui_state = (g_bt_action == ACTION_DISABLE) ? BT_STATE_TURNING_OFF : BT_STATE_TURNING_ON;
                pthread_create(&bt_action_thread, NULL, bt_action_thread_func, NULL);
                pthread_detach(bt_action_thread);
            }
            pthread_mutex_unlock(&action_mutex);
        } else {
            pthread_mutex_lock(&list_mutex);
            if (selected_index -1 < device_count) {
                BluetoothDeviceInfo* selected_dev = &device_list[selected_index - 1];
                if(selected_dev->is_paired){
                    if(selected_dev->paired_dev.is_connected) BT_disconnect(selected_dev->dev.addr);
                    else BT_connect(selected_dev->dev.addr);
                } else {
                    BT_pair(selected_dev->dev.addr);
                }
            }
            pthread_mutex_unlock(&list_mutex);
        }
    } else if(PAD_justPressed(BTN_X)){
         pthread_mutex_lock(&list_mutex);
         if (selected_index -1 < device_count) {
             BluetoothDeviceInfo* selected_dev = &device_list[selected_index - 1];
             if(selected_dev->is_paired){
                 BT_unpair(selected_dev->dev.addr);
             }
         }
         pthread_mutex_unlock(&list_mutex);
    }else if (PAD_justPressed(BTN_B)) {
        quit_plugin = 1;
    }
    
    dirty = true;

    if (total_items <= VISIBLE_ITEM_COUNT) list_start_index = 0;
    else if (list_start_index > total_items - VISIBLE_ITEM_COUNT) list_start_index = total_items - VISIBLE_ITEM_COUNT;
    if (list_start_index < 0) list_start_index = 0;

    char* l_btn = (BTN_SLEEP == BTN_POWER) ? "POWER" : "MENU";
    char* l_hint = "Sleep";
    char* r_btn1 = "B"; char* r_hint1 = "Back";
    char* r_btn2 = "A"; char* r_hint2 = "Select";

    SysUI_SetBottomHints(l_btn, l_hint, r_btn1, r_hint1, r_btn2, r_hint2);
}

// --- 插件生命周期函数 ---

// --- MODIFIED: Init is now non-blocking ---
static int plugin_init(void* main_screen) {
    screen = (SDL_Surface*)main_screen;
    quit_plugin = 0; dirty = 1; device_count = 0;
    selected_index = 0; list_start_index = 0;
    current_view = VIEW_DEVICE_LIST;
    g_bt_ui_state = BT_STATE_INITIALIZING; // Set initial state for the UI
    
    SysUI_Init(screen, &font);
    SysUI_SetTitle("Bluetooth Manager");
    SysUI_SetFullscreen(false);
    SysUI_ShowBottomBar(true);
    PWR_init();
    // BT_init(); // Moved to a background thread

    pthread_mutex_init(&list_mutex, NULL);
    pthread_mutex_init(&action_mutex, NULL);
    pthread_mutex_init(&scan_cond_mutex, NULL);
    pthread_cond_init(&scan_cond, NULL);

    // Create and start the initialization thread
    pthread_t init_thread;
    if (pthread_create(&init_thread, NULL, bt_init_thread_func, NULL) != 0) {
         LOG_error("Failed to create Bluetooth init thread.\n");
         g_bt_ui_state = BT_STATE_OFF; // Fallback to a safe state on error
    }
    pthread_detach(init_thread); // We don't need to join it, let it manage itself

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
            int total_items = device_count + 1;
            if (selected_index >= total_items) selected_index = total_items > 0 ? total_items - 1 : 0;
            pthread_mutex_unlock(&list_mutex);
            g_scan_results_updated = false;
            dirty = true;
        }

        if (!input_handled_by_sysui) {
            if (current_view == VIEW_DEVICE_LIST) {
                handle_list_input();
            }
        }
        
        if (dirty) {
            GFX_clear(screen);
            render_plugin_page();
            SysUI_Render();
            GFX_flip(screen);
            dirty = false;
        } else {
            GFX_delay();
        }
    }
    return 0;
}

// --- MODIFIED: Quit logic is cleaner ---
static void plugin_quit(void) {
    // Check if the scanner thread was ever started
    if (g_thread_running) {
        g_thread_running = false;
        // Signal and join the scanner thread to ensure a clean shutdown
        pthread_mutex_lock(&scan_cond_mutex);
        pthread_cond_signal(&scan_cond);
        pthread_mutex_unlock(&scan_cond_mutex);
        pthread_join(bt_scan_thread, NULL);
    }
    pthread_mutex_destroy(&list_mutex);
    pthread_mutex_destroy(&action_mutex);
    pthread_mutex_destroy(&scan_cond_mutex);
    pthread_cond_destroy(&scan_cond);

    PWR_quit();
    BT_quit(); // De-initialize the BT stack on plugin exit
    SysUI_Quit();
}

// --- 插件导出 ---
static NextUI_Plugin bt_plugin_Export = {
    .name = "Bluetooth",
    .display_path = SDCARD_PATH "/Tools/Settings/Network",
    .init = plugin_init,
    .run = plugin_run,
    .quit = plugin_quit,
};

#ifdef __cplusplus
extern "C" {
#endif
NextUI_Plugin* GetPlugin(void) {
    return &bt_plugin_Export;
}
#ifdef __cplusplus
}
#endif