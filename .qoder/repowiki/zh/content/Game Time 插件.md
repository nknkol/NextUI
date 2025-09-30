# Game Time 插件

<cite>
**本文档中引用的文件**   
- [gametime.c](file://workspace/plugins/gametime/gametime.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)
- [makefile](file://workspace/plugins/gametime/makefile)
- [plugin.h](file://workspace/lib/libcommon/plugin.h)
- [sysui.h](file://workspace/lib/libcommon/sysui.h)
- [defines.h](file://workspace/lib/libcommon/defines.h)
</cite>

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构概述](#架构概述)
5. [详细组件分析](#详细组件分析)
6. [依赖分析](#依赖分析)
7. [性能考虑](#性能考虑)
8. [故障排除指南](#故障排除指南)
9. [结论](#结论)

## 简介
Game Time 插件是一个用于跟踪和显示用户游戏时间的系统组件。该插件从游戏启动时开始记录每个游戏的游玩时间，并将数据持久化到 SQLite 数据库中。用户可以通过插件界面查看总游戏时间、各游戏的游玩统计信息，包括总时长、平均时长和游玩次数。插件提供了直观的图形化界面，显示游戏缩略图和相关统计数据，使用户能够轻松了解自己的游戏习惯。该系统由多个协同工作的组件构成，包括插件前端、数据库后端和控制程序，共同实现了完整的游戏时间追踪功能。

## 项目结构
Game Time 插件的实现分布在多个目录中，遵循了清晰的模块化架构。核心功能被分解为独立的组件，每个组件负责特定的职责。插件的用户界面和主逻辑位于 `workspace/plugins/gametime/` 目录下，而数据存储和访问逻辑则封装在 `workspace/lib/libgametimedb/` 库中。系统级的控制程序位于 `workspace/system/gametimectl/`，用于从外部启动和停止时间追踪。这种分离确保了代码的可维护性和可扩展性。

``mermaid
graph TB
subgraph "插件层"
GT["gametime.c"]
end
subgraph "库层"
GTDB["libgametimedb"]
end
subgraph "系统层"
GTC["gametimectl.c"]
end
GT --> GTDB
GTC --> GTDB
```

**Diagram sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)

**Section sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)

## 核心组件
Game Time 系统的核心由三个主要组件构成：插件前端、数据库库和控制程序。插件前端 (`gametime.c`) 负责提供用户界面，展示游戏时间统计数据。它从数据库中获取数据，并使用 SDL 图形库渲染列表、缩略图和文本信息。数据库库 (`libgametimedb`) 是系统的心脏，它定义了数据模型，管理 SQLite 数据库连接，并提供用于查询和更新游戏时间数据的 API。控制程序 (`gametimectl.c`) 作为一个命令行工具，在游戏启动和退出时被调用，以开始和停止时间追踪会话。这三个组件通过明确定义的接口进行通信，确保了系统的稳定性和可靠性。

**Section sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L1-L403)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L1-L471)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L69)

## 架构概述
Game Time 系统采用分层架构，清晰地分离了表示层、业务逻辑层和数据访问层。最上层是插件前端，它作为用户与系统交互的入口。它依赖于 SysUI 框架来处理输入和渲染 UI。中间层是 `libgametimedb` 库，它封装了所有与数据库相关的操作，为上层提供了一个干净的 API。最底层是 SQLite 数据库文件，它持久化存储所有的游戏活动记录。控制程序 `gametimectl` 作为系统服务，直接与数据库层交互，根据游戏的生命周期事件来更新数据。这种架构使得各个组件可以独立开发和测试。

``mermaid
graph TD
A[用户] --> B[Game Time 插件]
B --> C[SysUI 框架]
C --> D[输入/输出]
B --> E[libgametimedb]
F[gametimectl] --> E
E --> G[(game_logs.sqlite)]
style B fill:#f9f,stroke:#333
style E fill:#bbf,stroke:#333
style G fill:#9f9,stroke:#333
```

**Diagram sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)

## 详细组件分析

### 插件前端分析
Game Time 插件前端是一个典型的 SDL 应用程序，遵循初始化、运行循环和清理的生命周期。`plugin_init` 函数在插件加载时被调用，负责初始化 SDL 表面、加载游戏活动数据、预加载缩略图并设置 UI 布局。`plugin_run` 函数包含主事件循环，它持续轮询输入设备（如方向键和 B 键），并根据用户输入更新选中项和滚动位置。当需要重新渲染时，`renderList` 函数会被调用，它遍历游戏列表，为每个游戏项绘制圆角矩形背景、缩略图和统计信息（总时长、平均时长、游玩次数）。

``mermaid
flowchart TD
Start([插件启动]) --> Init["初始化 (plugin_init)"]
Init --> LoadData["加载游戏活动数据"]
LoadData --> Preload["预加载缩略图"]
Preload --> SetupUI["设置 UI 布局"]
SetupUI --> RunLoop["主循环 (plugin_run)"]
RunLoop --> PollInput["轮询输入"]
PollInput --> HandleInput{"输入已处理?"}
HandleInput --> |是| Dirty["标记为脏"]
HandleInput --> |否| CheckButtons["检查方向键/B键"]
CheckButtons --> Up{"上键?"}
Up --> |是| MoveUp["向上移动选择"]
CheckButtons --> Down{"下键?"}
Down --> |是| MoveDown["向下移动选择"]
CheckButtons --> B{"B键?"}
B --> |是| Quit["设置退出标志"]
MoveUp --> Dirty
MoveDown --> Dirty
Quit --> Dirty
Dirty --> Render{"需要渲染?"}
Render --> |是| Clear["清除屏幕"]
Render --> |是| SetTitle["设置标题"]
Render --> |是| DrawList["绘制游戏列表"]
Render --> |是| Flip["翻转屏幕"]
Render --> |否| Sync["同步"]
Sync --> RunLoop
Quit --> Cleanup["清理 (plugin_quit)"]
Cleanup --> End([插件退出])
```

**Diagram sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L200-L403)

**Section sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L1-L403)

### 数据库库分析
`libgametimedb` 库是 Game Time 系统的数据核心。它定义了三个关键的数据结构：`ROM`、`PlayActivity` 和 `PlayActivities`。`ROM` 结构体代表一个游戏，包含其 ID、类型、名称、文件路径和图片路径。`PlayActivity` 结构体代表一个游戏的游玩活动，包含指向 `ROM` 的指针以及总游玩时间、平均游玩时间、游玩次数和首次/最后游玩时间等统计信息。`PlayActivities` 是一个容器，包含一个 `PlayActivity` 指针数组和总数。

该库使用 SQLite 作为持久化存储，数据库文件位于 `/.userdata/shared/game_logs.sqlite`。它在首次使用时自动创建两个表：`rom` 表存储游戏元数据，`play_activity` 表存储每次游玩的时长记录。通过 `play_activity_find_all` 函数，库执行一个复杂的 SQL 查询，将两个表连接起来，按游戏 ID 分组，计算每个游戏的总时长、平均时长和游玩次数，并按总时长降序排列返回结果。`play_activity_start` 和 `play_activity_stop` 函数则负责在游戏开始和结束时创建和更新 `play_activity` 记录。

``mermaid
classDiagram
class ROM {
+int id
+char* type
+char* name
+char* file_path
+char* image_path
}
class PlayActivity {
+ROM* rom
+int play_count
+int play_time_total
+int play_time_average
+char* first_played_at
+char* last_played_at
}
class PlayActivities {
+PlayActivity** play_activity
+int count
+int play_time_total
}
class gametimedb {
+sqlite3* play_activity_db_open()
+void play_activity_db_close(sqlite3*)
+void free_play_activities(PlayActivities*)
+PlayActivities* play_activity_find_all()
+void play_activity_start(char*)
+void play_activity_stop(char*)
}
PlayActivities --> PlayActivity : "包含"
PlayActivity --> ROM : "引用"
gametimedb ..> PlayActivities : "创建"
gametimedb ..> PlayActivity : "管理"
gametimedb ..> ROM : "管理"
```

**Diagram sources**
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h#L1-L45)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L1-L471)

**Section sources**
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h#L1-L45)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L1-L471)

### 控制程序分析
`gametimectl` 是一个命令行工具，作为系统服务在游戏启动和退出时被调用。它的主要作用是通知 `libgametimedb` 库开始或停止对特定游戏的时间追踪。当用户启动一个游戏时，系统会执行 `gametimectl start /path/to/game.rom`，这会调用 `play_activity_start` 函数，在数据库中为该游戏创建一个新的游玩会话。当游戏退出时，系统会执行 `gametimectl stop /path/to/game.rom`，调用 `play_activity_stop` 函数，计算本次会话的持续时间并将其记录到数据库中。该程序还支持 `resume` 功能，允许用户从上次中断的地方继续追踪，以及 `list` 命令来查看所有记录。

``mermaid
sequenceDiagram
participant User as "用户"
participant System as "系统"
participant GametimeCtl as "gametimectl"
participant DB as "libgametimedb"
User->>System : 启动游戏
System->>GametimeCtl : gametimectl start game.rom
GametimeCtl->>DB : play_activity_start("game.rom")
DB-->>GametimeCtl : 开始追踪
GametimeCtl-->>System : 完成
System->>User : 运行游戏
User->>System : 退出游戏
System->>GametimeCtl : gametimectl stop game.rom
GametimeCtl->>DB : play_activity_stop("game.rom")
DB-->>GametimeCtl : 停止并记录
GametimeCtl-->>System : 完成
System->>User : 返回主界面
```

**Diagram sources**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L69)

**Section sources**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L69)

## 依赖分析
Game Time 插件的构建配置明确指出了其依赖关系。根据 `makefile`，插件被编译为一个共享库 (`.so`)，并链接了多个关键库：`-lmsettings` 用于系统设置，`-lgametimedb` 提供游戏时间数据功能，`-lsqlite3` 作为底层数据库引擎，以及 `-lwifimg` 和 `-lwifid` 用于网络和 WiFi 功能。插件通过 `plugin.h` 中定义的 `NextUI_Plugin` 结构体与主系统集成，该结构体要求插件导出 `init`、`run` 和 `quit` 三个生命周期函数。插件还依赖于 `sysui.h` 来使用统一的用户界面框架进行渲染和输入处理。

``mermaid
graph TD
GT["Game Time 插件"]
GT --> MSettings["-lmsettings"]
GT --> GameTimeDB["-lgametimedb"]
GT --> SQLite["-lsqlite3"]
GT --> WiFiImg["-lwifimg"]
GT --> WiFiD["-lwifid"]
GT --> PluginH["plugin.h"]
GT --> SysUIH["sysui.h"]
style GT fill:#f96,stroke:#333
```

**Diagram sources**
- [makefile](file://workspace/plugins/gametime/makefile#L1-L42)
- [plugin.h](file://workspace/lib/libcommon/plugin.h#L1-L21)
- [sysui.h](file://workspace/lib/libcommon/sysui.h#L1-L49)

**Section sources**
- [makefile](file://workspace/plugins/gametime/makefile#L1-L42)

## 性能考虑
Game Time 系统在性能方面进行了多项优化。首先，它在插件初始化时预加载所有游戏的缩略图 (`preloadRomImages`)，避免了在滚动列表时进行耗时的磁盘 I/O 操作，从而保证了 UI 的流畅性。其次，数据库操作被封装在库中，并且 `play_activity_find_all` 函数使用一个高效的 SQL 查询一次性获取所有所需数据，减少了与数据库的交互次数。对于缩略图的处理，代码会检查图像格式并进行必要的转换，然后将其缩放并应用圆角效果，这些操作在预加载阶段完成，减轻了主循环的负担。然而，一个潜在的性能问题是，预加载所有缩略图会消耗大量内存，尤其是在游戏库很大的情况下。

## 故障排除指南
如果 Game Time 插件无法正常工作，可以按照以下步骤进行排查：

1.  **检查数据库文件**：确认数据库文件 `/.userdata/shared/game_logs.sqlite` 是否存在且可读写。如果文件损坏，可以尝试删除它，系统会在下次启动时自动重建。
2.  **验证控制程序调用**：确保游戏启动器在启动和退出游戏时正确调用了 `gametimectl start` 和 `gametimectl stop` 命令。可以在系统日志中查找相关调用记录。
3.  **检查缩略图路径**：如果游戏缩略图不显示，检查游戏 ROM 文件的路径是否正确，并确认对应的 `.media` 文件夹中存在同名的 `.png` 图片文件。
4.  **查看插件日志**：检查系统日志中是否有来自 `gametime.c` 或 `gametimedb.c` 的错误信息，例如数据库打开失败或 SQL 执行错误。
5.  **内存问题**：如果设备在打开插件时卡顿或崩溃，可能是由于预加载了过多的缩略图导致内存不足。尝试减少游戏库中的游戏数量或优化缩略图的大小。

**Section sources**
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L1-L403)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L1-L471)

## 结论
Game Time 插件是一个功能完整、架构清晰的系统组件，成功地实现了游戏时间追踪的核心功能。它通过模块化的设计，将用户界面、数据逻辑和系统控制分离，提高了代码的可维护性。插件利用 SDL 和 SysUI 框架提供了直观的用户体验，并通过 SQLite 数据库实现了可靠的数据持久化。尽管存在预加载缩略图可能带来的内存压力，但整体设计是高效且实用的。该插件为用户提供了有价值的洞察，帮助他们了解自己的游戏行为，是 NextUI 系统中一个重要的增强功能。