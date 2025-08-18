#ifndef __PLUGIN_H__
#define __PLUGIN_H__

// 插件条目，使用链表结构
typedef struct PluginEntry {
    char* path;
    char* name;
    struct PluginEntry* next;
} PluginEntry;

// 插件的生命周期函数接口
typedef struct {
    const char* name;
    int (*init)(void* screen);
    int (*run)(void);
    void (*quit)(void);
} NextUI_Plugin;

#define GET_PLUGIN_SYMBOL "GetPlugin"

#endif