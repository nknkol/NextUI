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
// --- 新增：参数辅助函数的实现 ---
PluginArg* PLUGIN_createArg(const char* key, const char* value) {
    PluginArg* arg = (PluginArg*)malloc(sizeof(PluginArg));
    if (!arg) return NULL;
    arg->key = strdup(key);
    arg->value = strdup(value);
    arg->next = NULL;
    return arg;
}

void PLUGIN_freeArgs(PluginArg* args) {
    PluginArg* current = args;
    while (current != NULL) {
        PluginArg* next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
}

// --- 新增：核心调用逻辑的实现 ---

// 内部帮助函数：根据名称查找插件条目
static PluginEntry* find_plugin_entry(const char* plugin_name) {
    PluginEntry* current = PLUGINS_get();
    while (current != NULL) {
        if (strcmp(current->name, plugin_name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}


int PLUGIN_invokeAction(const char* plugin_name, const char* action_name, PluginArg* args) {
    LOG_note(LOG_REALTIME, "Attempting to invoke action '%s' from plugin '%s'\n", action_name, plugin_name);
    
    PluginEntry* entry = find_plugin_entry(plugin_name);
    if (!entry) {
        LOG_error("Plugin '%s' not found.\n", plugin_name);
        return -1; // 插件未找到
    }

    NextUI_Plugin* plugin = PLUGIN_load(entry->path);
    if (!plugin) {
        LOG_error("Failed to load plugin from '%s'.\n", entry->path);
        return -2; // 插件加载失败
    }

    if (!plugin->actions || plugin->action_count == 0) {
        LOG_warn("Plugin '%s' does not register any actions.\n", plugin_name);
        return -3; // 插件无动作
    }

    for (int i = 0; i < plugin->action_count; ++i) {
        if (strcmp(plugin->actions[i].name, action_name) == 0) {
            if (plugin->actions[i].execute) {
                LOG_note(LOG_REALTIME, "Executing action '%s'...\n", action_name);
                return plugin->actions[i].execute(args);
            }
        }
    }

    LOG_warn("Action '%s' not found in plugin '%s'.\n", action_name, plugin_name);
    return -4; // 动作未找到
}

// 注意：openPage 会启动一个插件的完整UI，它是一个阻塞操作。
// 你需要在一个新的上下文中运行它，或者修改你的主循环来处理插件的运行。
// 这里我们假设它会像启动普通游戏一样启动插件。
int PLUGIN_openPage(const char* plugin_name, const char* page_name, PluginArg* args) {
    LOG_note(LOG_REALTIME, "Attempting to open page '%s' from plugin '%s'\n", page_name, plugin_name);

    PluginEntry* entry = find_plugin_entry(plugin_name);
    if (!entry) {
        LOG_error("Plugin '%s' not found.\n", plugin_name);
        return -1;
    }
    
    NextUI_Plugin* plugin = PLUGIN_load(entry->path);
    if (!plugin) {
        LOG_error("Failed to load plugin from '%s'.\n", entry->path);
        return -2;
    }

    if (!plugin->open_page) {
        LOG_warn("Plugin '%s' does not support opening specific pages.\n", plugin_name);
        return -3;
    }

    // 调用页面打开函数，它应该处理初始化并进入自己的主循环
    return plugin->open_page(page_name, args);
}