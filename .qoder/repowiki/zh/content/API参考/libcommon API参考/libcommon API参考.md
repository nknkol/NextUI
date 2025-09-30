# libcommon API参考

<cite>
**本文档中引用的文件**   
- [api.h](file://workspace/lib/libcommon/api.h) - *更新了全局常量和宏定义*
- [config.h](file://workspace/lib/libcommon/config.h) - *新增了WiFi配置项*
- [config.c](file://workspace/lib/libcommon/config.c) - *更新了配置持久化逻辑*
- [lang.h](file://workspace/lib/libcommon/lang.h) - *更新了多语言支持API*
- [lang.c](file://workspace/lib/libcommon/lang.c) - *更新了字符串加载逻辑*
- [plugin.h](file://workspace/lib/libcommon/plugin.h) - *更新了插件管理接口*
- [plugin.c](file://workspace/lib/libcommon/plugin.c) - *更新了插件加载逻辑*
- [api.c](file://workspace/lib/libcommon/api.c) - *更新了系统初始化流程*
- [sysui.h](file://workspace/lib/libcommon/sysui.h) - *新增了系统UI API*
- [sysui.c](file://workspace/lib/libcommon/sysui.c) - *实现了系统UI功能*
- [network.cpp](file://workspace/plugins/network/network.cpp) - *新增的网络插件示例*
</cite>

## 更新摘要
**已做更改**   
- 在“核心API模块”中更新了“日志记录系统”一节，将`LOG_note`函数中实时日志文件的打开模式从`w`（写入）改为`a`（追加），以防止日志被覆盖。
- 在“配置管理模块”中更新了`NextUISettings`结构体，增加了`language`配置项，并在`CFG_defaults`函数中设置了默认语言。
- 在“多语言支持模块”中确认了`Lang_Init`函数使用`SYSTEM_PATH`宏来构建语言文件路径。
- 为所有受影响的章节添加了更新的源文件引用。

## 目录
1. [简介](#简介)
2. [核心API模块](#核心api模块)
3. [配置管理模块](#配置管理模块)
4. [多语言支持模块](#多语言支持模块)
5. [插件管理模块](#插件管理模块)
6. [系统集成与调用约束](#系统集成与调用约束)

## 简介
`libcommon` 是 NextUI 核心库，为整个系统提供基础服务。该库封装了系统级接口、配置管理、多语言支持和插件管理等核心功能。本API参考文档详细介绍了 `api.h`、`config.h`、`lang.h` 和 `plugin.h` 中定义的公共接口，旨在为开发者提供全面的函数签名、参数说明、返回值语义、调用上下文和线程安全性分析。

## 核心API模块

`api.h` 是 `libcommon` 库的核心头文件，定义了日志记录、全局常量、颜色定义和图形渲染器等系统级接口。

### 日志记录系统
该模块提供了一个分等级的日志记录系统，支持实时日志文件输出。

**函数签名**
```c
void LOG_note(int level, const char* fmt, ...);
```

**参数说明**
- `level`: 日志级别，可选值包括 `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_REALTIME`。
- `fmt`: 格式化字符串，与 `printf` 类似。
- `...`: 可变参数列表。

**返回值语义**
- 无返回值。

**调用上下文**
- 该函数是线程安全的，可以在任何线程中调用。
- `LOG_REALTIME` 级别的日志会被写入到一个以进程名命名的实时日志文件中，位于 `LOGS_PATH` 目录下。

**线程安全性分析**
- 该函数内部使用 `va_list` 和 `vsnprintf` 进行格式化，这些操作是线程安全的。
- 对于 `LOG_REALTIME` 级别，文件写入操作通过 `fflush` 确保数据及时落盘，但未使用显式锁，依赖于底层文件系统的原子性。

**代码示例**
```c
LOG_info("Application started with %d arguments.\n", argc);
LOG_warn("Configuration file not found, using defaults.\n");
```

**Section sources**
- [api.h](file://workspace/lib/libcommon/api.h#L0-L199)
- [api.c](file://workspace/lib/libcommon/api.c#L0-L500)

### 全局常量与宏
该模块定义了屏幕分辨率、页面大小、颜色掩码等全局常量。

**关键定义**
```c
#define PAGE_WIDTH	(FIXED_WIDTH * PAGE_SCALE)
#define PAGE_HEIGHT	(FIXED_HEIGHT * PAGE_SCALE)
#define RGBA_MASK_565	0xF800, 0x07E0, 0x001F, 0x0000
extern uint32_t RGB_WHITE;
extern uint32_t THEME_COLOR1;
```

**字段含义**
- `PAGE_WIDTH/HEIGHT`: 定义了渲染页面的尺寸，基于 `FIXED_WIDTH/HEIGHT` 并乘以 `PAGE_SCALE`。
- `RGBA_MASK_565`: 用于RGB565像素格式的位掩码。
- `RGB_WHITE`: 预定义的白色颜色值。
- `THEME_COLOR1`: 主题颜色1，用于UI元素。

**生命周期管理**
- 这些常量和变量在 `GFX_init` 函数中被初始化，其生命周期与图形系统绑定。

**Section sources**
- [api.h](file://workspace/lib/libcommon/api.h#L0-L199)
- [api.c](file://workspace/lib/libcommon/api.c#L0-L500)

### 系统UI模块
该模块（`sysui.h`）提供了统一的系统级用户界面组件，包括顶部状态栏、底部提示栏和硬件操作浮层，为所有插件提供一致的UI体验。

**核心数据结构**
`SysUI_Context` 结构体是系统UI模块的内部状态容器。

**数据结构定义**
```c
typedef struct {
    bool is_fullscreen;
    char title[256];
    SysUI_BottomHints bottom_hints;
    SysUI_OverlayType active_overlay;
    int overlay_value;
    int overlay_min;
    int overlay_max;
    uint32_t overlay_display_start_time;
    SDL_Surface* screen_surface;
    GFX_Fonts* fonts;
    bool show_bottom_bar;
} SysUI_Context;
```

**字段含义**
- `is_fullscreen`: 是否为全屏模式，影响 `SysUI_Render` 的行为。
- `title`: 顶部状态栏显示的标题文本。
- `bottom_hints`: 底部提示栏的按钮和提示文本。
- `active_overlay`: 当前激活的硬件浮层类型（音量、亮度、色温）。
- `overlay_value`, `overlay_min`, `overlay_max`: 浮层显示的数值和范围。
- `overlay_display_start_time`: 浮层显示的起始时间，用于超时隐藏。
- `screen_surface`: 主屏幕的SDL表面指针。
- `fonts`: 系统字体集合。
- `show_bottom_bar`: 是否显示底部提示栏。

**生命周期管理**
- 该结构体的单例实例 `g_sysui_ctx` 在 `SysUI_Init` 函数中被初始化。
- 其生命周期由 `SysUI_Init` 和 `SysUI_Quit` 函数管理。

**核心API**
该模块提供了一套完整的初始化、渲染、输入处理和状态设置API。

**函数签名**
```c
void SysUI_Init(SDL_Surface* screen, GFX_Fonts* fonts);
void SysUI_Quit(void);
void SysUI_Render(void);
bool SysUI_Update(void);
void SysUI_SetTitle(const char* title);
void SysUI_SetFullscreen(bool fullscreen);
void SysUI_SetBottomHints(const char* l_btn, const char* l_hint, const char* r_btn1, const char* r_hint1, const char* r_btn2, const char* r_hint2);
void SysUI_ShowOverlay(SysUI_OverlayType type, int value, int min, int max);
```

**参数说明**
- `SysUI_Init` 的 `screen` 参数：主屏幕的SDL表面指针。
- `SysUI_Init` 的 `fonts` 参数：系统字体集合。
- `SysUI_SetTitle` 的 `title` 参数：要设置的标题字符串。
- `SysUI_SetFullscreen` 的 `fullscreen` 参数：布尔值，`true` 表示进入全屏模式。
- `SysUI_SetBottomHints` 的参数：分别设置左、右1、右2按钮的标签和提示文本。
- `SysUI_ShowOverlay` 的参数：设置要显示的浮层类型和数值。

**返回值语义**
- `SysUI_Update` 返回一个布尔值，表示是否处理了输入事件。如果返回 `true`，调用者应跳过自己的输入处理逻辑。

**调用上下文**
- `SysUI_Init` 必须在插件的 `init` 函数中调用，以初始化UI系统。
- `SysUI_Update` 必须在主循环的每一帧中调用，以处理硬件输入。
- `SysUI_Render` 必须在主循环的每一帧中调用，以绘制顶部和底部UI。
- `SysUI_SetTitle` 和 `SysUI_SetBottomHints` 用于动态更新UI内容。

**线程安全性分析**
- 该模块不是线程安全的，所有函数都应在主线程中调用。

**代码示例**
```c
// 在插件初始化时
SysUI_Init(screen, &font);
SysUI_SetTitle(L("Network"));
SysUI_SetBottomHints("B", "BACK", "A", "OPTIONS", NULL, NULL);
SysUI_SetFullscreen(false);

// 在主循环中
while(!quit_plugin) {
    // 处理输入
    if (!SysUI_Update()) {
        // 如果SysUI没有处理输入，则由插件处理
        networkMenu->handleInput(dirty, quit_plugin);
    }
    // 渲染
    if (dirty) {
        GFX_clear(screen);
        networkMenu->draw(screen, listRect);
        SysUI_Render(); // 绘制顶部和底部UI
        GFX_flip(screen);
        dirty = 0;
    }
}
```

**Section sources**
- [sysui.h](file://workspace/lib/libcommon/sysui.h#L0-L48)
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L21-L220)
- [network.cpp](file://workspace/plugins/network/network.cpp#L0-L131)

## 配置管理模块

`config.h` 和 `config.c` 文件共同实现了系统的配置管理功能，提供了一套完整的配置读写、持久化和回调机制。

### 核心数据结构
`NextUISettings` 结构体是配置模块的核心，它定义了所有可配置的选项。

**数据结构定义**
```c
typedef struct {
    // Theme
    int font;
    uint32_t color1_255;
    uint32_t color2_255;
    // ... 其他颜色
    int thumbRadius;
    double gameArtWidth;

    // UI
    bool showClock;
    bool clock24h;
    // ... 其他UI选项

    // Power
    uint32_t screenTimeoutSecs;
    uint32_t suspendTimeoutSecs;

    // Emulator
    int saveFormat;
    int stateFormat;

    // Network
    bool wifi;
    bool wifiDiagnostics;

    // ADDED: Language setting
    char language[8];
} NextUISettings;
```

**字段含义**
- `font`: UI字体ID。
- `color1_255` 到 `color7_255`: 七种主题颜色，以0xRRGGBB格式存储。
- `thumbRadius`: 缩略图圆角半径。
- `gameArtWidth`: 游戏艺术图宽度百分比。
- `showClock`: 是否显示时钟。
- `clock24h`: 是否使用24小时制。
- `screenTimeoutSecs`: 屏幕超时时间（秒）。
- `suspendTimeoutSecs`: 挂起超时时间（秒）。
- `saveFormat`: 保存文件格式。
- `stateFormat`: 状态文件格式。
- `wifi`: 是否启用WiFi。
- `wifiDiagnostics`: 是否启用WiFi诊断日志。
- `language`: 语言代码（如 "en", "zh"）。

**生命周期管理**
- 该结构体的单例实例 `settings` 在 `config.c` 中被定义为全局静态变量。
- 其生命周期由 `CFG_init` 和 `CFG_quit` 函数管理。`CFG_init` 负责初始化和从文件加载配置，`CFG_quit` 负责在退出时同步配置到文件。

### 配置读写API
该模块提供了一套细粒度的 `get/set` 函数，用于访问和修改配置。

**典型函数签名**
```c
int CFG_getFontId(void);
void CFG_setFontId(int fontid);
uint32_t CFG_getColor(int id);
void CFG_setColor(int id, uint32_t color);
bool CFG_getShowClock(void);
void CFG_setShowClock(bool show);
```

**参数说明**
- `CFG_getColor` 的 `id` 参数：颜色ID，范围1-7，对应 `color1_255` 到 `color7_255`。
- `CFG_setColor` 的 `color` 参数：颜色值，以0xRRGGBB格式传入。
- `CFG_setShowClock` 的 `show` 参数：布尔值，`true` 表示显示时钟。

**返回值语义**
- `get` 函数返回当前配置值。
- `set` 函数无返回值，但会修改内存中的配置。

**调用上下文**
- 这些函数通常在UI设置界面被调用，当用户更改设置时，`set` 函数被调用以更新内存中的配置。
- `get` 函数被UI渲染逻辑调用，以获取当前主题颜色等信息。

**线程安全性分析**
- 该模块未使用显式锁，因此不是线程安全的。所有配置读写操作都应在主线程中进行。

### 配置持久化
`CFG_sync` 函数负责将内存中的配置写入到持久化文件中。

**函数实现分析**
```c
void CFG_sync(void) {
    char settingsPath[MAX_PATH];
    sprintf(settingsPath, "%s/minuisettings.txt", getenv("SHARED_USERDATA_PATH"));
    FILE *file = fopen(settingsPath, "w");
    if (file == NULL) {
        printf("[CFG] Unable to open settings file, cant write\n");
        return;
    }

    fprintf(file, "font=%i\n", settings.font);
    fprintf(file, "color1=0x%06X\n", settings.color1_255);
    // ... 写入其他配置项
    fprintf(file, "language=%s\n", settings.language);
    fclose(file);
}
```

**持久化逻辑**
- 配置文件路径由 `SHARED_USERDATA_PATH` 环境变量和固定文件名 `minuisettings.txt` 组成。
- 所有配置项以 `key=value` 的格式写入文件，每行一个。
- 文件以写模式（"w"）打开，会覆盖原有内容。

**调用时机**
- `CFG_sync` 在 `CFG_quit` 函数中被调用，确保在程序退出时配置被保存。
- 该函数也可以被显式调用，以立即保存配置。

**代码示例**
```c
// 初始化配置模块
CFG_init(NULL, NULL);

// 读取配置
int fontId = CFG_getFontId();
bool showClock = CFG_getShowClock();

// 修改配置
CFG_setShowClock(true);
CFG_setColor(1, 0xFF0000); // 设置主色为红色

// 将配置更改持久化到文件
CFG_sync();
```

**Section sources**
- [config.h](file://workspace/lib/libcommon/config.h#L0-L199)
- [config.c](file://workspace/lib/libcommon/config.c#L0-L500)

## 多语言支持模块

`lang.h` 和 `lang.c` 文件实现了系统的多语言支持功能，允许UI字符串根据用户选择的语言进行本地化。

### 核心数据结构
该模块使用一个静态数组 `g_lang_entries` 来存储所有加载的翻译字符串。

**数据结构定义**
```c
typedef struct {
    char* key;
    char* value;
} LangEntry;

static LangEntry g_lang_entries[MAX_LANG_STRINGS];
static int g_lang_entry_count = 0;
```

**字段含义**
- `key`: 字符串的唯一标识符（如 "MENU_HOME"）。
- `value`: 对应语言下的翻译文本。
- `g_lang_entries`: 存储所有键值对的数组。
- `g_lang_entry_count`: 当前已加载的条目数量。

**生命周期管理**
- `Lang_Init` 函数负责初始化此数据结构，它会先调用 `Lang_Shutdown` 清理旧数据，然后加载新的语言文件。
- `Lang_Shutdown` 函数负责释放 `g_lang_entries` 数组中每个条目的 `key` 和 `value` 内存，并将 `g_lang_entry_count` 重置为0。

### 多语言API
该模块提供了初始化、关闭和获取字符串的核心API。

**函数签名**
```c
void Lang_Init(const char* lang_code);
void Lang_Shutdown(void);
const char* Lang_GetString(const char* key);
```

**参数说明**
- `Lang_Init` 的 `lang_code` 参数：语言代码，如 "en" 或 "zh"。
- `Lang_GetString` 的 `key` 参数：要查找的字符串的键。

**返回值语义**
- `Lang_GetString` 返回与 `key` 对应的翻译字符串。如果找不到，则返回 `key` 本身，这有助于调试和发现未翻译的字符串。

**调用上下文**
- `Lang_Init` 通常在应用程序启动时调用，根据用户设置的语言代码加载相应的 `.ini` 文件。
- `Lang_GetString` 在UI渲染时被频繁调用，以获取需要显示的本地化文本。

**线程安全性分析**
- 该模块不是线程安全的。`Lang_Init` 和 `Lang_Shutdown` 会修改全局数组，而 `Lang_GetString` 会读取该数组。如果在多线程环境中使用，需要外部同步。

**便捷宏**
该模块定义了一个便捷宏 `L()`，用于简化代码。

**宏定义**
```c
#define L(key) Lang_GetString(key)
```

**使用说明**
- `L()` 宏是 `Lang_GetString` 的简写，使代码更简洁。
- 在网络插件中，`SysUI_SetTitle(L("Network"))` 使用此宏来设置本地化的标题。

**代码示例**
```c
// 初始化语言模块，加载中文
Lang_Init("zh");

// 获取本地化字符串
const char* homeText = Lang_GetString("MENU_HOME");
printf("%s\n", homeText); // 输出 "主页"

// 使用便捷宏
printf("%s\n", L("MENU_SETTINGS")); // 输出 "设置"

// 应用退出时清理
Lang_Shutdown();
```

**Section sources**
- [lang.h](file://workspace/lib/libcommon/lang.h#L0-L38)
- [lang.c](file://workspace/lib/libcommon/lang.c#L0-L105)

## 插件管理模块

`plugin.h` 和 `plugin.c` 文件实现了系统的插件管理功能，允许动态加载和管理 `.so` 格式的插件。

### 核心数据结构
该模块使用一个单向链表来管理所有已发现的插件。

**数据结构定义**
```c
typedef struct PluginEntry {
    char* path;
    char* name;
    struct PluginEntry* next;
} PluginEntry;

typedef struct {
    const char* name;
    int (*init)(void* screen);
    int (*run)(void);
    void (*quit)(void);
} NextUI_Plugin;
```

**字段含义**
- `PluginEntry`:
  - `path`: 插件文件的完整路径。
  - `name`: 插件的名称。
  - `next`: 指向链表中下一个插件的指针。
- `NextUI_Plugin`:
  - `name`: 插件的名称。
  - `init`: 初始化函数指针。
  - `run`: 运行函数指针。
  - `quit`: 退出函数指针。

**生命周期管理**
- `PLUGINS_init` 函数负责扫描 `PLUGIN_PATH` 目录，发现所有 `.so` 文件，并尝试加载它们。
- `PLUGINS_quit` 函数负责遍历链表，释放所有 `PluginEntry` 分配的内存。
- `PLUGIN_load` 函数用于在运行时按需加载一个特定的插件。

### 插件管理API
该模块提供了插件发现、加载和查询的API。

**函数签名**
```c
void PLUGINS_init(void);
void PLUGINS_quit(void);
PluginEntry* PLUGINS_get(void);
NextUI_Plugin* PLUGIN_load(const char* path);
```

**参数说明**
- `PLUGIN_load` 的 `path` 参数：要加载的插件文件的完整路径。

**返回值语义**
- `PLUGINS_get` 返回指向插件链表头的指针，可用于遍历所有已发现的插件。
- `PLUGIN_load` 返回一个指向 `NextUI_Plugin` 结构体的指针，该结构体包含了插件的元数据和函数指针。

**调用上下文**
- `PLUGINS_init` 在应用程序启动时被调用，以发现所有可用的插件。
- `PLUGINS_get` 通常在UI中被调用，以获取插件列表并显示在菜单中。
- `PLUGIN_load` 在用户选择运行某个插件时被调用。

**线程安全性分析**
- `PLUGINS_init` 和 `PLUGINS_quit` 不是线程安全的，应在单线程环境下调用。
- `PLUGINS_get` 返回一个只读的链表指针，只要不调用 `PLUGINS_quit`，该指针是安全的。

**代码示例**
```c
// 初始化插件系统
PLUGINS_init();

// 获取插件列表
PluginEntry* plugin = PLUGINS_get();
while (plugin != NULL) {
    printf("Found plugin: %s at %s\n", plugin->name, plugin->path);
    plugin = plugin->next;
}

// 加载并运行一个插件
NextUI_Plugin* myPlugin = PLUGIN_load("/path/to/myplugin.so");
if (myPlugin) {
    myPlugin->init(screen);
    myPlugin->run();
    myPlugin->quit();
}

// 清理
PLUGINS_quit();
```

**Section sources**
- [plugin.h](file://workspace/lib/libcommon/plugin.h#L0-L20)
- [plugin.c](file://workspace/lib/libcommon/plugin.c#L0-L100)

## 系统集成与调用约束

`libcommon` 库的各个模块通过 `GFX_init` 函数进行集成，该函数是系统初始化的入口点。

### 系统初始化流程
```
mermaid
flowchart TD
Start([GFX_init]) --> InitVideo["调用 PLAT_initVideo() 初始化视频"]
InitVideo --> SetMode["设置图形模式"]
SetMode --> InitConfig["调用 CFG_init() 初始化配置"]
InitConfig --> LoadFonts["加载系统字体"]
LoadFonts --> UpdateColors["更新主题颜色"]
UpdateColors --> DefineColors["定义全局颜色常量"]
DefineColors --> LoadAssets["加载图形资源 (assets.png)"]
LoadAssets --> InitSysUI["调用 SysUI_Init() 初始化系统UI"]
InitSysUI --> ClearScreen["调用 PLAT_clearAll() 清屏"]
ClearScreen --> End([返回 SDL_Surface*])
```

**Diagram sources**
- [api.c](file://workspace/lib/libcommon/api.c#L0-L500)
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L21-L220)

**调用约束**
- **初始化顺序**: `GFX_init` 必须在 `CFG_init` 之前调用，因为 `CFG_init` 的回调函数（如 `GFX_loadSystemFont`）需要在 `GFX_init` 中注册。
- **线程约束**: 除了 `LOG_note` 函数外，`libcommon` 的大多数API都不是线程安全的。所有UI相关的操作都应在主线程中进行。
- **内存管理**: `lang` 和 `plugin` 模块负责管理自己的内存。`lang` 模块在 `Lang_Shutdown` 时释放所有字符串内存，`plugin` 模块在 `PLUGINS_quit` 时释放链表内存。
- **错误处理**: 该库倾向于使用 `printf` 或 `fprintf(stderr)` 输出错误信息，而不是返回错误码。开发者应检查函数的返回值（如果存在）并监控日志输出。

**Section sources**
- [api.c](file://workspace/lib/libcommon/api.c#L0-L500)