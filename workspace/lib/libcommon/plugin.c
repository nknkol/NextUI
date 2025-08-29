// workspace/lib/libcommon/plugin.c

#include <dlfcn.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "api.h"
#include "utils.h" // 需要 getDisplayName
#include "defines.h"

// 静态全局变量，指向插件链表的头
static PluginEntry* plugin_list_head = NULL;

void PLUGINS_init(void) {
    LOG_note(LOG_REALTIME, "PLUGINS_init: Starting plugin scan...\n");
    
    DIR *dh = opendir(PLUGIN_PATH);
    if (dh == NULL) {
        LOG_note(LOG_REALTIME, "PLUGINS_init: ERROR: Could not open plugin directory at %s\n", PLUGIN_PATH);
        return;
    }

    LOG_note(LOG_REALTIME, "PLUGINS_init: Successfully opened plugin directory: %s\n", PLUGIN_PATH);
    
    int plugins_found = 0;
    struct dirent *dp;
    char full_path[MAX_PATH];
    while ((dp = readdir(dh)) != NULL) {
        if (suffixMatch(".so", dp->d_name)) {
            snprintf(full_path, sizeof(full_path), "%s/%s", PLUGIN_PATH, dp->d_name);
            LOG_note(LOG_REALTIME, "PLUGINS_init: Found potential plugin: %s\n", full_path);

            void* handle = dlopen(full_path, RTLD_LAZY);
            if (!handle) {
                LOG_note(LOG_REALTIME, "PLUGINS_init: dlopen FAILED for %s. Error: %s\n", full_path, dlerror());
                continue;
            }

            dlerror(); // 清除旧错误
            NextUI_Plugin* (*get_plugin)(void) = dlsym(handle, GET_PLUGIN_SYMBOL);
            if (dlerror() != NULL) {
                LOG_note(LOG_REALTIME, "PLUGINS_init: dlsym FAILED for %s. Not a valid NextUI plugin.\n", full_path);
                dlclose(handle);
                continue;
            }

            NextUI_Plugin* plugin_info = get_plugin();
            if (!plugin_info || !plugin_info->name) {
                LOG_note(LOG_REALTIME, "PLUGINS_init: Plugin %s did not return valid info or name.\n", full_path);
                dlclose(handle);
                continue;
            }

            LOG_note(LOG_REALTIME, "PLUGINS_init: SUCCESS: Validated plugin '%s' from %s\n", plugin_info->name, full_path);

            PluginEntry* new_entry = (PluginEntry*)malloc(sizeof(PluginEntry));
            new_entry->path = strdup(full_path);
            new_entry->name = strdup(plugin_info->name);
            
            // --- 新增代码块开始 ---
            if (plugin_info->display_path) {
                new_entry->display_path = strdup(plugin_info->display_path);
                 LOG_note(LOG_REALTIME, "PLUGINS_init: Plugin '%s' registered for path: %s\n", new_entry->name, new_entry->display_path);
            } else {
                new_entry->display_path = NULL; // 如果插件未指定路径，则为 NULL
            }
            // --- 新增代码块结束 ---

            new_entry->next = plugin_list_head;
            plugin_list_head = new_entry;
            plugins_found++;
            
            dlclose(handle);
        }
    }
    closedir(dh);
    LOG_note(LOG_REALTIME, "PLUGINS_init: Scan complete. Found %d valid plugins.\n", plugins_found);
}

void PLUGINS_quit(void) {
    PluginEntry* current = plugin_list_head;
    while (current != NULL) {
        PluginEntry* next = current->next;
        free(current->path);
        free(current->name);
        free(current->display_path); // 新增：释放 display_path 内存
        free(current);
        current = next;
    }
    plugin_list_head = NULL;
}

PluginEntry* PLUGINS_get(void) {
    return plugin_list_head;
}

NextUI_Plugin* PLUGIN_load(const char* path) {
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        LOG_error("Cannot open library: %s\n", dlerror());
        return NULL;
    }
    dlerror();
    NextUI_Plugin* (*get_plugin)(void) = dlsym(handle, GET_PLUGIN_SYMBOL);
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        LOG_error("Cannot load symbol '%s': %s\n", GET_PLUGIN_SYMBOL, dlsym_error);
        dlclose(handle);
        return NULL;
    }
    return get_plugin();
}