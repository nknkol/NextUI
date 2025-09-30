# 系统UI交互API

<cite>
**本文档引用的文件**   
- [sysui.h](file://workspace/lib/libcommon/sysui.h) - *在最近的提交中更新*
- [sysui.c](file://workspace/lib/libcommon/sysui.c) - *在最近的提交中更新*
- [api.c](file://workspace/lib/libcommon/api.c) - *包含图形原语实现*
- [network.cpp](file://workspace/plugins/network/network.cpp) - *新增插件，展示了SysUI_Update的使用模式*
- [defines.h](file://workspace/lib/libcommon/defines.h) - *包含UI常量定义*
- [platform.h](file://workspace/lib/libcommon/platform.h) - *包含输入宏定义*
- [lang.h](file://workspace/lib/libcommon/lang.h) - *多语言支持头文件*
- [lang.c](file://workspace/lib/libcommon/lang.c) - *多语言支持实现*
</cite>

## 更新摘要
**变更内容**   
- 更新了**核心UI接口分析**部分，新增了`SysUI_Update`函数的详细分析，包括其作为输入处理前置过滤器的新角色。
- 新增了**SysUI_Update函数与输入处理**章节，深入解析了`SysUI_Update`在主循环中的关键作用及其与`network`插件的集成。
- 更新了**非阻塞式UI调用实践**部分，增加了`network`插件如何利用`SysUI_Update`处理硬件输入的实践案例。
- 修正了关于`SysUI_ShowOverlay`函数的描述，明确指出其不再直接处理输入，而是由`SysUI_Update`统一管理。
- 更新了所有相关的**章节来源**和**图表来源**，以反映对`network.cpp`等新文件的分析。

## 目录
1. [简介](#简介)
2. [核心UI接口分析](#核心ui接口分析)
3. [UI元素样式与控制参数](#ui元素样式与控制参数)
4. [非阻塞式UI调用实践](#非阻塞式ui调用实践)
5. [多线程同步机制与风险](#多线程同步机制与风险)

## 简介
本文档详细说明`sysui.h`中提供的系统级UI交互接口，包括弹窗消息显示、进度条控制、确认对话框等核心功能。文档将深入解析这些函数如何与主UI线程通信，确保界面更新的原子性与一致性。同时，将文档化UI元素的样式控制参数、超时机制、用户响应捕获方式，并结合`settings`工具或`bootlogo`显示流程，展示非阻塞式UI调用的最佳实践。最后，分析在多线程环境下调用这些接口的同步机制与潜在死锁风险。

**章节来源**
- [sysui.h](file://workspace/lib/libcommon/sysui.h#L1-L48)

## 核心UI接口分析

### 系统UI上下文与初始化
`sysui`模块通过一个全局的静态上下文`g_sysui_ctx`来管理所有UI状态，确保了整个系统中UI状态的唯一性和一致性。该上下文在`SysUI_Init`函数中被初始化。

**SysUI_Context 结构体**
该结构体定义了系统UI的核心状态，包含以下关键成员：

- **is_fullscreen**: 布尔值，指示当前是否为全屏模式。
- **title**: 字符数组，存储顶部标题栏的文本内容。
- **bottom_hints**: `SysUI_BottomHints`结构体，用于存储底部按钮提示信息。
- **active_overlay**: `SysUI_OverlayType`枚举，表示当前激活的覆盖层类型（如音量、亮度）。
- **overlay_value, overlay_min, overlay_max**: 整数，用于表示覆盖层的当前值、最小值和最大值。
- **overlay_display_start_time**: 32位无符号整数，记录覆盖层开始显示的时间戳（毫秒）。
- **screen_surface**: SDL_Surface指针，指向主屏幕表面。
- **fonts**: GFX_Fonts指针，指向系统字体集合。
- **show_bottom_bar**: 布尔值，控制底部提示栏的显示与隐藏。

``mermaid
classDiagram
class SysUI_Context {
+bool is_fullscreen
+char title[256]
+SysUI_BottomHints bottom_hints
+SysUI_OverlayType active_overlay
+int overlay_value
+int overlay_min
+int overlay_max
+uint32_t overlay_display_start_time
+SDL_Surface* screen_surface
+GFX_Fonts* fonts
+bool show_bottom_bar
}
class SysUI_BottomHints {
+char left_button[16]
+char left_hint[64]
+char right_button1[16]
+char right_hint1[64]
+char right_button2[16]
+char right_hint2[64]
}
class SysUI_OverlayType {
<<enumeration>>
SYSUI_OVERLAY_NONE
SYSUI_OVERLAY_VOLUME
SYSUI_OVERLAY_BRIGHTNESS
SYSUI_OVERLAY_COLORTEMP
}
SysUI_Context --> SysUI_BottomHints : "包含"
```

**图表来源**
- [sysui.h](file://workspace/lib/libcommon/sysui.h#L23-L35)

**章节来源**
- [sysui.h](file://workspace/lib/libcommon/sysui.h#L1-L48)
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)

### 主要UI交互函数
`sysui`模块提供了一套简洁的公共API来控制UI的显示。

**初始化与退出**
- **SysUI_Init(SDL_Surface* screen, GFX_Fonts* fonts)**: 初始化系统UI，设置屏幕表面和字体。该函数会清零全局上下文`g_sysui_ctx`，并设置默认标题。
- **SysUI_Quit(void)**: 清理资源，目前未分配动态内存，因此为空实现。

**UI状态控制**
- **SysUI_SetTitle(const char* title)**: 设置顶部标题栏的文本。函数内部使用`strncpy`进行安全复制，并支持通过`L()`宏进行多语言翻译。
- **SysUI_SetFullscreen(bool fullscreen)**: 设置全屏模式。当为全屏时，`SysUI_Render`函数将直接返回，不进行任何渲染。
- **SysUI_ShowBottomBar(bool show)**: 控制底部提示栏的显示与隐藏。

**底部提示栏设置**
- **SysUI_SetBottomHints(...)**: 设置底部提示栏的按钮和提示文本。该函数接受左右两侧最多两个按钮的标签和提示信息，并进行多语言处理。

**覆盖层（Overlay）显示**
- **SysUI_ShowOverlay(SysUI_OverlayType type, int value, int min, int max)**: 显示一个临时的覆盖层（如音量、亮度调节）。该函数会更新`active_overlay`类型，并重置`overlay_display_start_time`为当前时间。

### SysUI_Update函数与输入处理
根据最新的代码变更，`SysUI_Update`函数的角色已从一个简单的状态更新器演变为一个**关键的输入处理前置过滤器**。它现在负责在主事件循环中优先处理所有与系统UI相关的硬件输入。

**核心逻辑分析**
`SysUI_Update`函数的执行逻辑如下：
1.  **静音事件处理**: 首先检查全局静音状态的变化。如果检测到变化，立即激活音量覆盖层并返回`true`，以确保UI能立即响应。
2.  **功能键检测**: 检查`BTN_MOD_BRIGHTNESS`和`BTN_MOD_COLORTEMP`等功能键是否被按下。
3.  **调节键检测**: 检查`BTN_MOD_PLUS`和`BTN_MOD_MINUS`等调节键是否被重复触发。
4.  **覆盖层状态决策**:
    - 如果检测到功能键被按下或调节键被触发，则重置覆盖层的显示计时器，并根据按下的功能键类型（亮度、色温）或默认（音量）来激活相应的覆盖层。
    - 如果没有相关按键操作，则根据当前覆盖层类型决定隐藏策略：对于亮度和色温，立即隐藏；对于音量，则使用超时机制。
5.  **返回值**: 函数返回一个布尔值，指示UI是否需要重绘。当有交互发生、状态改变或需要重绘时，返回`true`。

**与插件的集成**
`network`插件的代码清晰地展示了这一新模式：
```c++
// 在主循环中，首先调用SysUI_Update
input_handled_by_sysui = SysUI_Update();
if (input_handled_by_sysui) {
    dirty = 1; // 标记需要重绘
}
// 仅当SysUI没有处理输入时，才将输入传递给插件的菜单
if (!input_handled_by_sysui) {
    networkMenu->handleInput(dirty, quit_plugin);
}
```
这种模式确保了系统级的UI交互（如调节音量）具有最高优先级，不会被任何插件或应用的输入处理所阻塞。

``mermaid
sequenceDiagram
participant Application as "应用程序"
participant SysUI as "SysUI模块"
participant GFX as "GFX图形库"
Application->>SysUI : SysUI_Update()
SysUI->>SysUI : 检查静音状态变化
alt 静音状态改变
SysUI->>SysUI : 激活音量覆盖层
SysUI->>Application : 返回 true (已处理)
else 无静音变化
SysUI->>SysUI : 检查功能键和调节键
alt 有相关按键操作
SysUI->>SysUI : 重置计时器，激活对应覆盖层
SysUI->>Application : 返回 true (已处理)
else 无按键操作
SysUI->>SysUI : 检查超时，决定是否隐藏覆盖层
SysUI->>Application : 返回 true/false (根据状态)
end
end
Application->>SysUI : SysUI_Render()
SysUI->>GFX : SysUI_RenderTopBar()
SysUI->>GFX : SysUI_RenderBottomBar()
GFX->>GFX : 调用GFX_blitHardwareGroup等函数
```

**图表来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)
- [network.cpp](file://workspace/plugins/network/network.cpp#L70-L75)

**章节来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)
- [network.cpp](file://workspace/plugins/network/network.cpp#L70-L75)

## UI元素样式与控制参数

### 顶部标题栏渲染
`SysUI_RenderTopBar`函数负责渲染顶部标题栏。其主要逻辑如下：
1.  **硬件状态显示**: 首先调用`GFX_blitHardwareGroup`来显示音量、电池等硬件状态图标。`show_setting_flag`参数根据`active_overlay`的类型被设置为1、2或3，以指示当前正在调整的设置。
2.  **标题文本渲染**: 如果标题不为空，则使用`GFX_truncateText`函数截断过长的文本，然后使用`TTF_RenderUTF8_Blended`生成文本表面，并通过`GFX_blitPillLight`绘制一个白色的胶囊背景，最后将文本Blit到屏幕上。

### 底部提示栏渲染
`SysUI_RenderBottomBar`函数负责渲染底部提示栏。
1.  **右侧按钮组**: 首先检查并渲染右侧的两个按钮组（`right_button1`和`right_button2`），使用`GFX_blitButtonGroup`函数。
2.  **左侧按钮组或硬件提示**: 如果存在`active_overlay`，则调用`GFX_blitHardwareHints`显示与该覆盖层相关的硬件提示（如“音量+/-”）。否则，如果存在左侧按钮，则调用`GFX_blitButtonGroup`渲染左侧按钮组。

### 样式控制与图形原语
`sysui`模块本身不直接处理复杂的图形绘制，而是依赖于`api.c`中定义的`GFX`系列函数作为图形原语。

- **GFX_blitPillLight**: 用于绘制带有白色背景的胶囊状按钮。在`sysui`中用于顶部标题栏的背景。
- **GFX_blitButtonGroup**: 用于批量渲染一组按钮和其提示文本。它接受一个字符串数组（按钮标签和提示成对出现）和对齐方式。
- **GFX_blitHardwareGroup**: 用于渲染一组固定的硬件状态图标（如音量、电池、WiFi）。
- **GFX_blitHardwareHints**: 用于渲染与当前硬件设置（音量、亮度、色温）相关的动态提示文本。

这些函数的实现位于`api.c`中，它们封装了SDL的底层绘图操作，如`SDL_BlitSurface`和`TTF_RenderUTF8_Blended`，并应用了统一的主题颜色和布局。

``mermaid
flowchart TD
A[SysUI_Render] --> B{is_fullscreen?}
B --> |是| C[返回]
B --> |否| D[SysUI_RenderTopBar]
D --> E[GFX_blitHardwareGroup]
D --> F[GFX_truncateText]
D --> G[TTF_RenderUTF8_Blended]
D --> H[GFX_blitPillLight]
D --> I[SDL_BlitSurface]
A --> J[SysUI_RenderBottomBar]
J --> K{show_bottom_bar?}
K --> |否| L[返回]
K --> |是| M[GFX_blitButtonGroup]
J --> N{active_overlay?}
N --> |是| O[GFX_blitHardwareHints]
N --> |否| P{left_button?}
P --> |是| M
```

**图表来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)
- [api.c](file://workspace/lib/libcommon/api.c#L1510-L1855)

**章节来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)
- [api.c](file://workspace/lib/libcommon/api.c#L1510-L1855)

## 非阻塞式UI调用实践

### 覆盖层的非阻塞特性
`sysui`模块的核心设计原则是**非阻塞**。以`SysUI_ShowOverlay`为例，它只是一个状态设置函数，调用后立即返回，不会等待用户交互或超时。

**超时机制**
覆盖层的自动隐藏由`SysUI_Update`函数中的超时逻辑实现：
```c
if (g_sysui_ctx.active_overlay != SYSUI_OVERLAY_NONE) {
    if (setting_adjusted) {
        g_sysui_ctx.overlay_display_start_time = now; // 重置计时器
    }
    else if (now - g_sysui_ctx.overlay_display_start_time > OVERLAY_TIMEOUT_MS) {
        g_sysui_ctx.active_overlay = SYSUI_OVERLAY_NONE; // 超时，关闭覆盖层
    }
}
```
这个逻辑在每一帧的主循环中被调用，通过比较当前时间与`overlay_display_start_time`的差值来判断是否超时。这种方式避免了使用`sleep`或`delay`导致的主线程阻塞。

### 与Settings工具的集成
`settings`工具（位于`workspace/tools/settings`）是`sysui`的一个典型使用者。当用户在`settings`中调整音量或亮度时，它会调用`SysUI_ShowOverlay`来显示一个临时的视觉反馈。这个反馈会持续`OVERLAY_TIMEOUT_MS`（500毫秒）后自动消失，而`settings`工具本身可以继续处理其他用户输入，无需等待。

### 与Bootlogo的集成
`bootlogo`工具（位于`workspace/tools/bootlogo`）在启动时显示一个Logo。它可能使用`sysui`来显示一个简单的“正在启动...”的提示，或者在启动完成后调用`SysUI_SetTitle`来更新主界面的标题。由于`sysui`的非阻塞特性，`bootlogo`可以在显示Logo的同时，后台进行其他初始化工作。

### 与Network插件的集成
`network`插件是`sysui`非阻塞特性的另一个优秀实践。它在`plugin_run`主循环中，通过`SysUI_Update`来处理所有系统级的输入。如果`SysUI_Update`返回`true`，说明系统UI正在处理输入（如用户正在调节亮度），插件会跳过自己的菜单输入处理，从而确保系统功能的优先响应。这完美地展示了如何在不阻塞主循环的情况下，实现多层级的、优先级分明的输入处理。

**章节来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)
- [network.cpp](file://workspace/plugins/network/network.cpp#L70-L75)

## 多线程同步机制与风险

### 当前同步机制分析
根据现有代码分析，`sysui`模块**本身并未实现显式的线程同步机制**（如互斥锁、信号量）。其设计假设所有UI相关的函数调用都发生在**同一个主线程**中，通常是主事件循环线程。

- **数据竞争风险**: `g_sysui_ctx`是一个全局静态变量。如果从另一个线程（例如一个后台下载线程）直接调用`SysUI_SetTitle`或`SysUI_ShowOverlay`，则可能与主线程的`SysUI_Render`或`SysUI_Update`函数同时访问`g_sysui_ctx`，导致数据竞争和未定义行为。
- **SDL线程安全**: SDL的图形渲染函数（如`SDL_BlitSurface`）通常不是线程安全的。在非主线程中直接调用`sysui`的渲染函数可能会导致图形上下文损坏或程序崩溃。

### 潜在死锁风险
由于`sysui`模块内部没有使用锁，因此它本身不会引入死锁。然而，如果开发者在多线程环境中错误地使用它，可能会间接导致死锁。

**风险场景**:
1.  开发者在主线程中持有一个锁`A`，然后调用`sysui`函数。
2.  同时，在另一个线程中，开发者试图通过某种方式（例如，一个不安全的回调）调用`sysui`函数，而这个调用又需要获取锁`A`。
3.  由于`sysui`的某些内部操作（虽然目前没有）可能需要等待主线程的渲染完成，这可能会形成循环等待，导致死锁。

### 最佳实践与建议
为了安全地在多线程环境中使用`sysui`，应遵循以下最佳实践：

1.  **UI操作主线程化**: 所有对`sysui` API的调用都应确保在主线程（UI线程）中执行。
2.  **使用消息队列**: 在工作线程中，不应直接调用`sysui`函数。相反，应将UI更新请求（如“更新标题为X”）放入一个线程安全的消息队列中。
3.  **主线程轮询**: 在主线程的事件循环中，定期检查该消息队列。如果发现有新的UI更新请求，则从队列中取出并安全地调用相应的`sysui`函数。

通过这种方式，可以确保UI状态的更新是原子的、一致的，并且完全在UI线程的上下文中进行，从而避免了数据竞争和潜在的死锁。

**章节来源**
- [sysui.c](file://workspace/lib/libcommon/sysui.c#L6-L205)