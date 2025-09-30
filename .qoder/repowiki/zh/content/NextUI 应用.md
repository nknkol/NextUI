# NextUI 应用

<cite>
**本文档中引用的文件**   
- [async.h](file://workspace/apps/nextui/include/async.h)
- [async.c](file://workspace/apps/nextui/async.c)
- [browser.h](file://workspace/apps/nextui/include/browser.h)
- [browser.c](file://workspace/apps/nextui/browser.c)
- [datastructures.h](file://workspace/apps/nextui/include/datastructures.h)
- [datastructures.c](file://workspace/apps/nextui/datastructures.c)
- [globals.h](file://workspace/apps/nextui/include/globals.h)
- [launcher.h](file://workspace/apps/nextui/include/launcher.h)
- [launcher.c](file://workspace/apps/nextui/launcher.c)
- [uimanager.h](file://workspace/apps/nextui/include/uimanager.h)
- [uimanager.c](file://workspace/apps/nextui/uimanager.c)
- [nextui.c](file://workspace/apps/nextui/nextui.c)
- [minarch.c](file://workspace/apps/minarch/minarch.c) - *新增截图与蓝牙音频支持*
- [makefile](file://workspace/apps/minarch/makefile) - *新增截图与蓝牙音频支持*
</cite>

## 更新摘要
**已做更改**   
- 在“核心组件”部分新增了对 MinArch 应用中截图和蓝牙音频功能的说明。
- 新增“MinArch 截图与蓝牙音频功能”章节，详细描述新功能的实现与触发方式。
- 更新了“项目结构”部分，以反映 MinArch 模块的增强功能。
- 所有标题、标签、引用和图表注释均已完全转换为中文。

## 目录
1. [项目结构](#项目结构)
2. [核心组件](#核心组件)
3. [异步处理模块](#异步处理模块)
4. [浏览器模块](#浏览器模块)
5. [数据结构](#数据结构)
6. [启动器模块](#启动器模块)
7. [UI管理器模块](#ui管理器模块)
8. [模块集成与工作流](#模块集成与工作流)
9. [常见问题排查](#常见问题排查)
10. [MinArch 截图与蓝牙音频功能](#minarch-截图与蓝牙音频功能)

## 项目结构

NextUI 应用是一个基于 C 语言和 SDL 库构建的嵌入式系统用户界面，主要用于游戏模拟器前端。其代码库遵循清晰的分层架构，将核心功能模块化。

``mermaid
graph TB
subgraph "NextUI 应用"
nextui_c["nextui.c\n(主程序入口)"]
async_h["async.h\n(异步处理)"]
async_c["async.c"]
browser_h["browser.h\n(文件浏览)"]
browser_c["browser.c"]
datastructures_h["datastructures.h\n(核心数据结构)"]
datastructures_c["datastructures.c"]
launcher_h["launcher.h\n(启动器)"]
launcher_c["launcher.c"]
uimanager_h["uimanager.h\n(UI管理)"]
uimanager_c["uimanager.c"]
globals_h["globals.h\n(全局变量)"]
end
nextui_c --> async_h
nextui_c --> browser_h
nextui_c --> datastructures_h
nextui_c --> launcher_h
nextui_c --> uimanager_h
nextui_c --> globals_h
async_c --> async_h
browser_c --> browser_h
datastructures_c --> datastructures_h
launcher_c --> launcher_h
uimanager_c --> uimanager_h
style nextui_c fill:#4CAF50,stroke:#388E3C
style async_h fill:#2196F3,stroke:#1976D2
style browser_h fill:#2196F3,stroke:#1976D2
style datastructures_h fill:#2196F3,stroke:#1976D2
style launcher_h fill:#2196F3,stroke:#1976D2
style uimanager_h fill:#2196F3,stroke:#1976D2
```

**图源**
- [nextui.c](file://workspace/apps/nextui/nextui.c)
- [async.h](file://workspace/apps/nextui/include/async.h)
- [browser.h](file://workspace/apps/nextui/include/browser.h)
- [datastructures.h](file://workspace/apps/nextui/include/datastructures.h)
- [launcher.h](file://workspace/apps/nextui/include/launcher.h)
- [uimanager.h](file://workspace/apps/nextui/include/uimanager.h)

**本节来源**
- [nextui.c](file://workspace/apps/nextui/nextui.c)
- [project_structure](file://)

## 核心组件

NextUI 应用的核心由五个相互协作的模块构成：异步处理（Async）、浏览器（Browser）、数据结构（Data Structures）、启动器（Launcher）和 UI 管理器（UI Manager）。这些模块通过一组全局变量和函数调用进行通信，共同管理用户界面的导航、资源加载和游戏启动。

每个模块都有明确的职责：
- **异步处理模块**：负责在后台线程中加载图像资源（如背景和缩略图），避免阻塞主 UI 线程，确保界面流畅。
- **浏览器模块**：提供文件系统浏览功能，能够扫描目录、识别游戏文件、管理最近游玩列表和收藏夹。
- **数据结构模块**：定义了应用中使用的核心数据结构，如动态数组、哈希表和文件条目，为其他模块提供基础支持。
- **启动器模块**：处理游戏和工具的启动逻辑，包括准备恢复状态、更新最近游玩列表和生成启动命令。
- **UI 管理器模块**：管理 UI 的导航状态，如目录堆栈和快速菜单，并协调用户交互。

此外，`minarch` 应用作为系统底层组件，新增了截图和蓝牙音频支持功能，进一步增强了用户体验。

**本节来源**
- [nextui.c](file://workspace/apps/nextui/nextui.c#L0-L199)
- [async.h](file://workspace/apps/nextui/include/async.h)
- [browser.h](file://workspace/apps/nextui/include/browser.h)
- [datastructures.h](file://workspace/apps/nextui/include/datastructures.h)
- [launcher.h](file://workspace/apps/nextui/include/launcher.h)
- [uimanager.h](file://workspace/apps/nextui/include/uimanager.h)
- [minarch.c](file://workspace/apps/minarch/minarch.c) - *新增功能*

## 异步处理模块

异步处理模块（`async.c` 和 `async.h`）是 NextUI 应用流畅性的关键。它通过创建后台工作线程来加载耗时的图像资源，从而避免了 UI 卡顿。

### 设计目的
该模块的主要目的是实现非阻塞的图像加载。当用户浏览文件系统时，应用需要加载每个目录的背景图和游戏的缩略图。如果这些操作在主线程中同步执行，会导致界面冻结。通过将这些任务放入后台队列，主线程可以继续响应用户输入。

### 实现细节
该模块的核心是一个基于线程和队列的系统：
1.  **任务队列**：定义了 `TaskNode` 链表结构来管理待处理的任务。存在两个独立的队列：`taskBGQueueHead` 用于背景图，`taskThumbQueueHead` 用于缩略图。
2.  **互斥锁与条件变量**：使用 `SDL_mutex` 和 `SDL_cond` 来保证多线程环境下的数据安全。`bgqueueMutex` 和 `bgqueueCond` 用于同步对背景队列的访问。
3.  **工作线程**：`BGLoadWorker` 函数是一个无限循环，它会阻塞在 `SDL_CondWait` 上，直到有新任务被加入队列。一旦被唤醒，它会从队列中取出一个任务，加载图像，并通过回调函数将结果返回给主线程。
4.  **资源加载**：使用 `IMG_Load` 函数加载图像文件，然后将其转换为 RGBA8888 格式，以确保与 SDL 的兼容性。

### API 接口
- `initImageLoaderPool(SDL_Surface* main_screen)`: 初始化异步加载系统，创建工作线程。
- `startLoadFolderBackground(const char* imagePath)`: 将一个加载目录背景图的任务加入队列。
- `startLoadThumb(const char* thumbpath)`: 将一个加载缩略图的任务加入队列。
- `animPill(AnimTask *task)`: 启动一个 UI 动画任务（如药丸状选择框的移动）。

``mermaid
sequenceDiagram
participant UI as "UI线程"
participant Async as "异步模块"
participant Worker as "后台工作线程"
UI->>Async : startLoadFolderBackground(path)
Note over Async : 将任务加入bgQueue<br/>并发送信号唤醒工作线程
Async->>Async : enqueueBGTask()
Async->>Worker : SDL_CondSignal(bgqueueCond)
loop 等待任务
Worker->>Worker : SDL_CondWait(bgqueueCond)
end
Worker->>Async : 取出任务
Worker->>Worker : IMG_Load(path)
Worker->>Worker : 转换图像格式
Worker->>UI : 调用回调函数传递结果
UI->>UI : 更新UI
```

**图源**
- [async.h](file://workspace/apps/nextui/include/async.h#L0-L85)
- [async.c](file://workspace/apps/nextui/async.c#L0-L199)

**本节来源**
- [async.h](file://workspace/apps/nextui/include/async.h)
- [async.c](file://workspace/apps/nextui/async.c)

## 浏览器模块

浏览器模块（`browser.c` 和 `browser.h`）负责与文件系统交互，为 UI 层提供数据支持。

### 设计目的
该模块旨在提供一个高效、灵活的文件浏览接口，能够识别不同类型的文件（游戏、工具、收藏夹等），并为 UI 层提供结构化的数据。

### 实现细节
该模块的核心是 `Directory` 和 `Entry` 数据结构：
- **Entry (条目)**: 代表文件系统中的一个项目，可以是目录、游戏包（.pak）、ROM 文件或快捷方式（DIP）。它包含路径、显示名称和类型等信息。
- **Directory (目录)**: 代表一个文件夹，包含一个 `Entry` 数组（`entries`）和当前的选中、起始、结束索引，用于实现分页浏览。

模块通过一系列函数来填充 `Directory` 对象：
- `Directory_index(Directory* self)`: 扫描 `self->path` 指定的目录，根据配置和文件类型创建 `Entry` 对象并添加到 `entries` 数组中。
- `hasEmu(char* emu_name)`: 检查指定名称的模拟器包（.pak）是否存在。
- `hasRecents()`: 读取 `RECENT_PATH` 文件，加载最近游玩的游戏列表。

### API 接口
- `Directory_new(char* path, int selected)`: 创建一个新的 `Directory` 对象。
- `Directory_index(Directory* self)`: 填充目录内容。
- `entryFromPakName(char* pak_name)`: 根据包名查找并创建一个 `Entry`。
- `hasRecents(void)`: 检查是否存在最近游玩记录。

**本节来源**
- [browser.h](file://workspace/apps/nextui/include/browser.h)
- [browser.c](file://workspace/apps/nextui/browser.c#L0-L199)

## 数据结构

数据结构模块（`datastructures.c` 和 `datastructures.h`）为整个应用提供了基础的数据容器。

### 核心数据结构
1.  **Array (动态数组)**:
    - **目的**: 一个通用的、可变大小的数组，用于存储指针。
    - **实现**: 使用 `malloc` 和 `realloc` 动态管理内存。当容量不足时，容量自动翻倍。
    - **API**: `Array_new()`, `Array_push()`, `Array_pop()`, `Array_free()`。

2.  **Hash (简单哈希表)**:
    - **目的**: 一个简单的键值对存储，用于配置或映射。
    - **实现**: 内部使用两个 `Array`，一个存储键，一个存储值，通过索引对应。
    - **API**: `Hash_new()`, `Hash_set()`, `Hash_get()`, `Hash_free()`。

3.  **Entry (文件条目)**:
    - **目的**: 表示文件系统中的一个项目。
    - **字段**: `path` (完整路径), `name` (显示名称), `type` (类型，如 `ENTRY_DIR`, `ENTRY_PAK`)。

4.  **Directory (目录)**:
    - **目的**: 表示一个包含多个 `Entry` 的文件夹。
    - **字段**: `path`, `name`, `entries` (Entry数组), `selected` (当前选中索引)。

``mermaid
classDiagram
class Array {
+int count
+int capacity
+void** items
+Array_new() Array
+Array_push(item) void
+Array_pop() void*
+Array_free() void
}
class Hash {
+Array* keys
+Array* values
+Hash_new() Hash
+Hash_set(key, value) void
+Hash_get(key) char*
+Hash_free() void
}
class Entry {
+char* path
+char* name
+int type
+Entry_new(path, type) Entry
+Entry_free() void
}
class Directory {
+char* path
+Array* entries
+int selected
+int start
+int end
+Directory_new(path, selected) Directory
+Directory_free() void
}
Array <|-- Hash : "keys 和 values"
Array <|-- Directory : "entries"
```

**图源**
- [datastructures.h](file://workspace/apps/nextui/include/datastructures.h#L0-L102)
- [datastructures.c](file://workspace/apps/nextui/datastructures.c#L0-L199)

**本节来源**
- [datastructures.h](file://workspace/apps/nextui/include/datastructures.h)
- [datastructures.c](file://workspace/apps/nextui/datastructures.c)

## 启动器模块

启动器模块（`launcher.c` 和 `launcher.h`）负责游戏和工具的启动流程。

### 设计目的
该模块封装了启动一个游戏或工具所需的所有逻辑，包括状态管理、命令生成和外部进程调用。

### 实现细节
1.  **启动流程**:
    - `openPak(char* path)`: 用于启动一个 `.pak` 包（通常是工具或模拟器）。它会生成一个执行 `launch.sh` 脚本的命令。
    - `openRom(char* path, char* last)`: 用于启动一个游戏 ROM。它会处理多盘游戏（M3U）和光盘镜像（CUE）等复杂情况。
2.  **恢复功能**:
    - `readyResume(Entry* entry)`: 检查指定游戏是否有保存的状态（存档），以决定是否显示“继续游戏”选项。
    - `autoResume(void)`: 在应用启动时，如果检测到 `AUTO_RESUME_PATH` 文件，会自动启动上次的游戏。
3.  **最近游玩列表**:
    - `addRecent(char* path, char* alias)`: 将游戏添加到最近游玩列表的顶部。
    - `saveRecents(void)`: 将内存中的最近游玩列表写入 `RECENT_PATH` 文件。

### API 接口
- `queueNext(char* cmd)`: 将启动命令写入 `/tmp/next` 文件，并设置 `quit=1`，通知主循环退出。
- `saveLast(char* path)`: 保存最后启动的项目路径。
- `GFX_showLauncherTransition(...)`: 显示启动时的图形过渡动画。

**本节来源**
- [launcher.h](file://workspace/apps/nextui/include/launcher.h)
- [launcher.c](file://workspace/apps/nextui/launcher.c#L0-L199)

## UI管理器模块

UI管理器模块（`uimanager.c` 和 `uimanager.h`）负责管理应用的导航状态和用户界面逻辑。

### 设计目的
该模块是 UI 状态的中枢，它维护了用户当前浏览的目录堆栈，并处理了快速菜单等高级导航功能。

### 实现细节
1.  **导航堆栈**:
    - 使用一个 `Array* stack` 来存储用户访问过的 `Directory` 对象。当用户进入一个子目录时，当前目录被压入堆栈；当用户返回时，从堆栈弹出。
    - `openDirectory(char* path, int auto_launch)`: 核心函数，用于打开一个新目录。它会创建 `Directory` 对象，调用 `Directory_index` 加载内容，并管理堆栈。
2.  **快速菜单**:
    - `getQuickEntries()`: 生成快速菜单中的快捷入口，如“最近游玩”、“游戏”、“工具”等。
    - `getQuickToggles()`: 生成快速菜单中的系统操作，如“Wi-Fi”、“重启”、“关机”等。
3.  **生命周期管理**:
    - `Menu_init()`: 初始化 UI 状态，包括堆栈、最近列表和快速菜单。
    - `Menu_quit()`: 清理所有分配的内存，防止内存泄漏。

### API 接口
- `Menu_init(void)`: 初始化 UI 管理器。
- `Menu_quit(void)`: 释放 UI 管理器资源。
- `openDirectory(char* path, int auto_launch)`: 导航到指定路径。
- `Entry_open(SDL_Surface* screen, int* dirty, Entry* self)`: 处理条目被打开的事件（例如，启动游戏或进入目录）。

**本节来源**
- [uimanager.h](file://workspace/apps/nextui/include/uimanager.h)
- [uimanager.c](file://workspace/apps/nextui/uimanager.c#L0-L199)

## 模块集成与工作流

各模块通过全局变量和函数调用紧密集成。一个典型的用户浏览和启动游戏的工作流如下：

1.  **初始化**: `nextui.c` 的 `main` 函数调用 `Menu_init()`，初始化 `stack`, `recents` 等全局变量，并调用 `initImageLoaderPool()` 启动异步线程。
2.  **浏览**: 用户操作触发 `openDirectory()`。该函数调用 `Directory_new()` 和 `Directory_index()` 来填充内容。`Directory_index()` 会调用 `hasRecents()` 等函数。UI 在渲染时，会调用 `startLoadFolderBackground()` 和 `startLoadThumb()` 来请求加载图像。
3.  **启动**: 用户选中一个游戏并按下确认键，触发 `Entry_open()`。该函数根据 `Entry` 的类型调用 `openRom()` 或 `openPak()`。`openRom()` 会调用 `readyResume()` 检查恢复状态，然后调用 `queueNext()` 生成启动命令并退出主循环。
4.  **异步加载**: 后台的 `BGLoadWorker` 线程持续运行，从队列中取出任务，加载图像，并通过回调更新 `folderbgbmp` 或 `thumbbmp`，同时设置 `folderbgchanged` 或 `thumbchanged` 标志，通知主循环需要重绘。

``mermaid
flowchart TD
A["main()"] --> B["Menu_init()"]
B --> C["初始化全局变量"]
B --> D["initImageLoaderPool()"]
D --> E["创建BGLoadWorker线程"]
A --> F["主循环"]
F --> G["用户输入"]
G --> H{"用户进入目录?"}
H --> |是| I["openDirectory()"]
I --> J["Directory_index()"]
J --> K["调用hasRecents()等"]
J --> L["startLoadFolderBackground()"]
L --> M["任务加入队列"]
M --> N["BGLoadWorker线程"]
N --> O["加载图像"]
O --> P["通过回调更新UI"]
H --> |否| Q{"用户启动游戏?"}
Q --> |是| R["Entry_open()"]
R --> S["openRom()"]
S --> T["readyResume()"]
S --> U["queueNext()"]
U --> V["写入/tmp/next"]
U --> W["quit=1"]
W --> X["主循环退出"]
```

**图源**
- [nextui.c](file://workspace/apps/nextui/nextui.c)
- [uimanager.c](file://workspace/apps/nextui/uimanager.c)
- [browser.c](file://workspace/apps/nextui/browser.c)
- [async.c](file://workspace/apps/nextui/async.c)
- [launcher.c](file://workspace/apps/nextui/launcher.c)

**本节来源**
- [nextui.c](file://workspace/apps/nextui/nextui.c#L0-L199)
- [uimanager.c](file://workspace/apps/nextui/uimanager.c)
- [browser.c](file://workspace/apps/nextui/browser.c)
- [async.c](file://workspace/apps/nextui/async.c)
- [launcher.c](file://workspace/apps/nextui/launcher.c)

## 常见问题排查

### 问题1：应用启动后立即退出
**可能原因**: 存在 `AUTO_RESUME_PATH` 文件，但指定的游戏或模拟器已不存在。
**排查步骤**:
1.  检查 `/tmp/auto_resume` 文件是否存在。
2.  如果存在，检查文件内容中的路径，确认对应的 ROM 和 `.pak` 文件是否还在。
3.  删除 `/tmp/auto_resume` 文件以禁用自动恢复功能。

### 问题2：界面卡顿或加载缓慢
**可能原因**: 异步图像加载模块未能正常工作，导致图像加载阻塞了主线程。
**排查步骤**:
1.  检查 `async.c` 中的 `BGLoadWorker` 线程是否成功创建。
2.  确认 `SDL_CondSignal` 和 `SDL_CondWait` 是否被正确调用。
3.  检查图像文件路径是否正确，`access()` 和 `IMG_Load()` 是否返回成功。

### 问题3：“最近游玩”列表为空
**可能原因**: `RECENT_PATH` 文件丢失或格式错误，或游戏启动时未正确调用 `addRecent()`。
**排查步骤**:
1.  检查 `~/.config/minui/recents.txt` 文件是否存在且可读。
2.  确认游戏启动后，`launcher.c` 中的 `addRecent()` 函数是否被调用。
3.  检查 `hasRecents()` 函数的逻辑，确保文件路径拼接正确。

**本节来源**
- [launcher.c](file://workspace/apps/nextui/launcher.c#L200-L366)
- [browser.c](file://workspace/apps/nextui/browser.c#L200-L721)
- [async.c](file://workspace/apps/nextui/async.c#L200-L466)

## MinArch 截图与蓝牙音频功能

### 设计目的
`minarch` 应用新增了两项核心功能：**截图功能**和**蓝牙音频支持**。截图功能允许用户通过快捷键将当前游戏画面保存至 SD 卡，便于分享或记录游戏瞬间。蓝牙音频支持则实现了对蓝牙音频设备的动态检测与自动切换，提升了音频输出的灵活性。

### 实现细节
1.  **截图功能**:
    - **触发机制**: 通过 `PAD_justPressed` 检测特定按键组合。根据代码分析，截图功能由 `BTN_Y` 键触发。
    - **图像捕获**: 调用 `GFX_GL_screenCapture` 函数从 GPU 获取当前帧的原始像素数据。
    - **图像处理**: 使用 `SDL_CreateRGBSurfaceWithFormatFrom` 和 `SDL_ConvertSurfaceFormat` 将原始数据转换为标准的 RGBA8888 格式。
    - **文件保存**: 在独立线程 `save_screenshot_thread` 中，使用 `SDL_RWFromFile` 和 `IMG_SavePNG` 将图像保存为 PNG 文件，路径为 `/mnt/SDCARD/Screenshots/`。
2.  **蓝牙音频支持**:
    - **事件监听**: 通过 `BT_registerDeviceWatcher` 注册 `onBluetoothAudioChanged` 回调函数，监听蓝牙设备的连接与断开事件。
    - **动态切换**: 在回调函数中，根据蓝牙状态动态修改 `AUDIODEV` 环境变量，将其设置为 `bluealsa`（蓝牙）或 `default`（本地），从而实现音频输出设备的无缝切换。
    - **线程安全**: 由于音频设备的重置必须在主线程（音频线程）中进行，因此通过设置 `resetAudio` 标志，由主循环在安全时机调用 `SND_resetAudio`。

### API 接口
- `save_screenshot_thread(void* data)`: 处理截图保存的后台线程函数。
- `onBluetoothAudioChanged(bool bluetooth, int watch_event)`: 蓝牙设备状态变化的回调函数。
- `GFX_GL_screenCapture(int* width, int* height)`: 获取当前屏幕的原始像素数据。

**本节来源**
- [minarch.c](file://workspace/apps/minarch/minarch.c#L6500-L7537)
- [makefile](file://workspace/apps/minarch/makefile)