# 配置系统API

<cite>
**本文档中引用的文件**   
- [config.h](file://workspace/lib/libcommon/config.h)
- [config.c](file://workspace/lib/libcommon/config.c)
- [minarch.c](file://workspace/apps/minarch/minarch.c)
- [nextui.c](file://workspace/apps/nextui/nextui.c)
- [system.cfg](file://skeleton/SYSTEM/system.cfg)
</cite>

## 目录
1. [引言](#引言)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构概述](#架构概述)
5. [详细组件分析](#详细组件分析)
6. [依赖分析](#依赖分析)
7. [性能考虑](#性能考虑)
8. [故障排除指南](#故障排除指南)
9. [结论](#结论)

## 引言
本文档深入文档化 `libcommon` 库中的配置管理系统。该系统为 NextUI 用户界面和 Minarch 核心应用提供统一的配置管理功能，涵盖主题、UI 行为、电源管理、网络设置等多个方面。系统通过内存中的结构体 `NextUISettings` 管理配置，并将其持久化到文本文件 `minuisettings.txt` 中。本文档将详细描述 `config.h` 中声明的所有 API 函数，解释其存储格式、默认值机制、内存管理策略、错误处理方式，并通过实际代码片段展示其在应用模块中的使用。

## 项目结构
配置管理系统的核心文件位于 `workspace/lib/libcommon/` 目录下。主要文件包括 `config.h`（头文件，声明 API 和数据结构）和 `config.c`（实现文件，包含函数的具体逻辑）。该系统被 `workspace/apps/nextui/` 和 `workspace/apps/minarch/` 等上层应用所依赖。系统级配置文件 `system.cfg` 位于 `skeleton/SYSTEM/` 目录，用于定义系统级别的硬件和渲染设置，与 `libcommon` 的用户配置系统并行存在。

``mermaid
graph TD
subgraph "配置系统核心"
A[config.h] --> B[config.c]
B --> C[NextUISettings 结构体]
end
subgraph "应用层"
D[nextui.c] --> B
E[minarch.c] --> B
end
subgraph "配置文件"
F[minuisettings.txt] < --> B
G[system.cfg] -.-> D
end
A -- "声明" --> B
B -- "实现" --> C
D -- "读取/写入" --> F
E -- "读取" --> F
```

**图源**
- [config.h](file://workspace/lib/libcommon/config.h#L1-L227)
- [config.c](file://workspace/lib/libcommon/config.c#L1-L796)
- [nextui.c](file://workspace/apps/nextui/nextui.c#L1-L1000)
- [minarch.c](file://workspace/apps/minarch/minarch.c#L1-L1000)

**本节来源**
- [config.h](file://workspace/lib/libcommon/config.h#L1-L227)
- [config.c](file://workspace/lib/libcommon/config.c#L1-L796)

## 核心组件
配置系统的核心是一个名为 `NextUISettings` 的 C 结构体，它在 `config.h` 中定义，包含了所有可配置的选项。该结构体在 `config.c` 中作为一个全局静态变量 `settings` 被实例化。系统通过一组 `CFG_get*` 和 `CFG_set*` 前缀的函数来安全地访问和修改这个结构体的成员，避免了直接的全局变量访问，提高了代码的封装性和可维护性。

**本节来源**
- [config.h](file://workspace/lib/libcommon/config.h#L60-L150)
- [config.c](file://workspace/lib/libcommon/config.c#L10-L15)

## 架构概述
该配置系统采用经典的“内存-文件”双层架构。启动时，系统从文件加载配置到内存中的 `settings` 结构体；运行时，所有模块通过 API 函数读写内存中的配置；退出时，调用 `CFG_sync` 函数将内存中的配置回写到文件，实现持久化。这种设计保证了运行时的高性能访问，同时确保了配置的持久性。

``mermaid
sequenceDiagram
participant App as "应用模块"
participant ConfigAPI as "CFG_* API"
participant Memory as "内存 (settings)"
participant File as "文件 (minuisettings.txt)"
Note over App,ConfigAPI : 应用启动
App->>ConfigAPI : CFG_init()
ConfigAPI->>File : fopen("minuisettings.txt", "r")
alt 文件存在且可读
File-->>ConfigAPI : 返回文件句柄
loop 逐行读取
ConfigAPI->>File : fgets()
ConfigAPI->>ConfigAPI : sscanf() 解析
ConfigAPI->>Memory : CFG_set*() 更新内存
end
ConfigAPI->>File : fclose()
else 文件不存在或读取失败
File-->>ConfigAPI : 返回 NULL
ConfigAPI->>Memory : CFG_defaults() 加载默认值
end
ConfigAPI-->>App : 初始化完成
Note over App,ConfigAPI : 运行时
App->>ConfigAPI : CFG_getShowClock()
ConfigAPI->>Memory : 读取 settings.showClock
ConfigAPI-->>App : 返回布尔值
App->>ConfigAPI : CFG_setShowClock(true)
ConfigAPI->>Memory : 设置 settings.showClock = true
Note over App,ConfigAPI : 应用退出
App->>ConfigAPI : CFG_quit()
ConfigAPI->>ConfigAPI : CFG_sync()
ConfigAPI->>File : fopen("minuisettings.txt", "w")
alt 文件可写
File-->>ConfigAPI : 返回文件柄
loop 逐项写入
ConfigAPI->>Memory : 读取配置项
ConfigAPI->>File : fprintf() 写入文件
end
ConfigAPI->>File : fclose()
else 文件不可写
File-->>ConfigAPI : 返回 NULL
ConfigAPI->>App : printf() 输出错误日志
end
```

**图源**
- [config.c](file://workspace/lib/libcommon/config.c#L100-L250)
- [config.c](file://workspace/lib/libcommon/config.c#L600-L700)

## 详细组件分析
### 配置数据结构分析
`NextUISettings` 结构体是配置系统的核心数据模型，它将所有配置项组织在一个单一的、易于管理的结构中。该结构体按功能分组，如主题、UI、电源、模拟器等，提高了代码的可读性。

``mermaid
classDiagram
class NextUISettings {
+int font
+uint32_t color1_255
+uint32_t color2_255
+uint32_t color3_255
+uint32_t color4_255
+uint32_t color5_255
+uint32_t color6_255
+uint32_t color7_255
+int thumbRadius
+int gameSwitcherScaling
+double gameArtWidth
+FontLoad_callback_t onFontChange
+ColorSet_callback_t onColorSet
+bool showClock
+bool clock24h
+bool showBatteryPercent
+bool showMenuAnimations
+bool showMenuTransitions
+bool showRecents
+bool showGameArt
+bool romsUseFolderBackground
+bool showQuickSwitcherUi
+int defaultView
+bool muteLeds
+uint32_t screenTimeoutSecs
+uint32_t suspendTimeoutSecs
+int saveFormat
+int stateFormat
+bool haptics
+bool wifi
+bool wifiDiagnostics
+char language[8]
}
```

**图源**
- [config.h](file://workspace/lib/libcommon/config.h#L60-L150)

**本节来源**
- [config.h](file://workspace/lib/libcommon/config.h#L60-L150)

### API 函数详解
配置系统提供了一套完整的 `CFG_get*` 和 `CFG_set*` 函数，用于安全地访问 `NextUISettings` 结构体的成员。

#### 初始化与同步
- **`CFG_init(FontLoad_callback_t, ColorSet_callback_t)`**: 初始化配置系统。首先调用 `CFG_defaults` 将 `settings` 结构体填充为默认值，然后尝试从 `SHARED_USERDATA_PATH/minuisettings.txt` 文件中读取配置并覆盖内存中的值。如果文件不存在或读取失败，则使用默认值。该函数还接受两个回调函数指针，用于在字体或颜色更改时通知其他模块。
- **`CFG_sync(void)`**: 将内存中的 `settings` 结构体内容同步（写入）到 `minuisettings.txt` 文件。如果文件无法打开（例如磁盘满或权限不足），函数会通过 `printf` 输出错误日志 `[CFG] Unable to open settings file, cant write` 并返回，不会中断程序执行。
- **`CFG_quit(void)`**: 在应用退出时调用，内部会调用 `CFG_sync` 来确保配置被保存。

#### 配置项获取与设置
每个配置项都有对应的 `get` 和 `set` 函数。`set` 函数通常包含输入验证和范围限制（clamp）。
- **`CFG_getFontId()` / `CFG_setFontId(int)`**: 获取/设置 UI 字体 ID。`set` 函数会将输入值限制在 0 到 2 之间，并根据 ID 决定加载 `font1.ttf`、`font2.ttf` 或 `font3.ttf`，然后调用 `onFontChange` 回调。
- **`CFG_getColor(int)` / `CFG_setColor(int, uint32_t)`**: 获取/设置指定 ID 的颜色（0xRRGGBB 格式）。`set` 函数会更新 `settings` 结构体和一个旧的全局变量（用于兼容性），并触发 `onColorSet` 回调。
- **`CFG_getScreenTimeoutSecs()` / `CFG_setScreenTimeoutSecs(uint32_t)`**: 获取/设置屏幕超时时间（秒）。
- **`CFG_getShowClock()` / `CFG_setShowClock(bool)`**: 获取/设置是否在状态栏显示时钟。
- **`CFG_getGameArtWidth()` / `CFG_setGameArtWidth(double)`**: 获取/设置游戏艺术图的宽度百分比（0.0 到 1.0 之间）。`set` 函数会使用 `clampd` 确保值在有效范围内。
- **`CFG_getLanguage()` / `CFG_setLanguage(const char*)`**: 新增的函数，用于获取/设置语言。`set` 函数会立即调用 `Lang_Init` 来应用新的语言设置。

#### 其他辅助函数
- **`CFG_get(const char*, char*)`**: 一个通用的获取函数，通过字符串键名（如 "font", "color1"）来获取配置值，并将结果格式化为字符串存入输出缓冲区。用于需要动态键名的场景。
- **`CFG_print(void)`**: 将当前所有配置项以 JSON 格式打印到控制台，主要用于调试。

**本节来源**
- [config.h](file://workspace/lib/libcommon/config.h#L155-L225)
- [config.c](file://workspace/lib/libcommon/config.c#L100-L700)

### 实际应用示例
以下代码片段展示了配置系统在不同模块中的使用方式。

#### 在 Minarch 核心中使用
在 `minarch.c` 中，`CFG_getSaveFormat()` 函数被用来决定保存文件的格式（`.sav` 或 `.srm`），从而影响文件的读写方式（是否使用压缩库）。

```c
// 示例：根据配置决定保存格式
static void SRAM_getPath(char* filename) {
    if (CFG_getSaveFormat() == SAVE_FORMAT_SRM) {
        formatSavePath(work_name, filename, ".srm");
    } else {
        formatSavePath(work_name, filename, ".sav");
    }
}
```

**本节来源**
- [minarch.c](file://workspace/apps/minarch/minarch.c#L688-L693)

#### 在 NextUI 中使用
在 `nextui.c` 中，`CFG_getDefaultView()` 函数在应用启动时被调用，以确定用户上次退出时所在的界面，从而恢复会话。

```c
// 示例：启动时恢复默认视图
int currentScreen = CFG_getDefaultView();
```

此外，`CFG_getMenuTransitions()` 被用作条件判断，决定动画的持续时间，实现了配置驱动的 UI 行为。

```c
// 示例：根据配置决定动画时长
GFX_animateSurfaceOpacity(converted, 0, 0, cw, ch, 255, 0, CFG_getMenuTransitions() ? 200 : 20, 1);
```

**本节来源**
- [nextui.c](file://workspace/apps/nextui/nextui.c#L110)
- [nextui.c](file://workspace/apps/nextui/nextui.c#L74)

## 依赖分析
配置系统 `libcommon` 是一个基础库，被 `nextui` 和 `minarch` 等应用模块所依赖。它自身依赖于 `utils.h` 和 `defines.h` 提供的通用工具和宏定义。此外，`config.c` 中还包含了 `lang.h`，表明配置系统与语言模块有直接的交互（通过 `CFG_setLanguage` 触发 `Lang_Init`）。

``mermaid
graph TD
A[libcommon] --> B[utils.h]
A --> C[defines.h]
A --> D[lang.h]
E[nextui] --> A
F[minarch] --> A
```

**图源**
- [config.c](file://workspace/lib/libcommon/config.c#L2-L5)

**本节来源**
- [config.c](file://workspace/lib/libcommon/config.c#L2-L5)

## 性能考虑
该配置系统的设计在性能上表现良好。运行时，所有配置访问都是对内存中结构体的直接读写，速度极快。初始化和退出时的文件 I/O 操作是主要的性能开销，但由于这些操作只在启动和关闭时发生一次，因此对整体性能影响很小。使用 `clamp` 和 `clampd` 函数进行输入验证，虽然有轻微的计算开销，但能有效防止无效数据导致的程序错误，是值得的。

## 故障排除指南
- **问题：配置更改后未生效**
  - **检查**：确认是否调用了相应的 `CFG_set*` 函数，而不是直接修改了某个变量。
  - **检查**：对于字体和颜色，确认 `onFontChange` 或 `onColorSet` 回调是否被正确注册和执行。
- **问题：配置无法保存**
  - **检查**：查看控制台日志，如果出现 `[CFG] Unable to open settings file, cant write`，说明 `CFG_sync` 函数在写入文件时失败。
  - **原因**：可能是 `SHARED_USERDATA_PATH` 环境变量未设置、目标目录不存在或没有写入权限、磁盘已满。
  - **解决**：确保环境变量正确，检查目录权限和磁盘空间。
- **问题：启动时总是使用默认设置**
  - **检查**：查看控制台日志，如果出现 `[CFG] Unable to open settings file, loading defaults`，说明 `CFG_init` 函数在读取文件时失败。
  - **原因**：配置文件 `minuisettings.txt` 不存在或路径错误。
  - **解决**：检查文件路径和权限。

**本节来源**
- [config.c](file://workspace/lib/libcommon/config.c#L110-L120)
- [config.c](file://workspace/lib/libcommon/config.c#L600-L610)

## 结论
`libcommon` 中的配置管理系统是一个设计良好、易于使用的模块。它通过清晰的 API、内存-文件双层架构和完善的错误处理，为整个应用提供了稳定可靠的配置管理服务。其基于文本的键值对存储格式简单易懂，便于手动编辑和调试。开发者在使用时应始终通过 `CFG_get*` 和 `CFG_set*` 函数来访问配置，避免直接操作全局变量，并注意在应用退出前调用 `CFG_quit` 以确保配置被正确保存。