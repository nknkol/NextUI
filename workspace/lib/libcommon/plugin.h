#ifndef __PLUGIN_H__
#define __PLUGIN_H__

typedef struct PluginArg {
    char* key;
    char* value;
    struct PluginArg* next;
} PluginArg;

typedef struct {
    const char* name;
    int (*execute)(PluginArg* args); 
} PluginAction;

typedef struct PluginEntry {
    char* path;
    char* name;
    char* display_path; 
    struct PluginEntry* next;
} PluginEntry;

typedef struct {
    const char* name;
    const char* display_path; 
    int (*init)(void* screen);
    int (*run)(void);
    void (*quit)(void);
    // 功能1: 注册动作列表
    PluginAction* actions;
    int action_count;

    // 功能2: 注册页面打开函数
    // page_name: 要打开的页面标识符, e.g., "terminal_view"
    // args: 传递给页面的参数
    // 返回值: 0 表示成功, 非0表示失败
    int (*open_page)(const char* page_name, PluginArg* args);

} NextUI_Plugin;

#define GET_PLUGIN_SYMBOL "GetPlugin"

#endif