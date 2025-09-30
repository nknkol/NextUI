# 游戏时间跟踪服务 (gametimectl)

<cite>
**本文档中引用的文件**  
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c) - *核心服务逻辑*
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h) - *数据库API接口*
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c) - *数据库操作实现*
- [defines.h](file://workspace/lib/libcommon/defines.h) - *路径常量定义*
- [gametime.c](file://workspace/plugins/gametime/gametime.c) - *新增的UI插件*
</cite>

## 更新摘要
**变更内容**   
- 新增了关于`gametime`插件的详细分析，阐述其如何在UI层与`gametimectl`服务形成闭环。
- 更新了“架构概述”和“详细组件分析”部分，以反映新的功能集成。
- 在“依赖分析”中增加了对`plugin.h`和`sysui.h`的引用。
- 修正了部分函数描述，使其与代码实现完全一致。

### 目录
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
游戏时间跟踪服务（gametimectl）是一个用于记录用户游戏会话时长的系统级工具。该服务在游戏启动时开始计时，在游戏退出时计算并持久化会话时长。它通过命令行接口提供功能，包括开始、停止、恢复和列出游戏活动。所有数据均存储在本地SQLite数据库中，支持对历史游戏记录的查询和统计。本技术文档将深入分析其信号监听机制、进程生命周期钩子以及与数据库的交互逻辑。此外，文档还新增了对`gametime`插件的分析，该插件利用`gametimectl`的服务数据，在用户界面（UI）层提供了一个可视化的历史记录查看器，形成了一个从数据采集到展示的完整功能闭环。

## 项目结构
游戏时间跟踪服务是NextUI项目的一部分，位于`workspace/system/gametimectl/`目录下。该服务作为一个独立的可执行程序实现，其核心逻辑由`gametimectl.c`文件构成，并依赖于`libgametimedb`库进行数据库操作。`libgametimedb`库本身位于`workspace/lib/libgametimedb/`目录，提供了对SQLite数据库的封装。服务的编译由`makefile`管理，并链接了`libcommon`、`libgametimedb`和`libsqlite3`等库。此外，新引入的`gametime`插件位于`workspace/plugins/gametime/`目录，它通过`libgametimedb`库读取数据，并利用`SysUI`框架渲染一个独立的UI界面，为用户提供直观的游戏时间统计。

``mermaid
graph TB
subgraph "gametimectl"
A[gametimectl.c]
B[makefile]
end
subgraph "libgametimedb"
C[gametimedb.c]
D[gametimedb.h]
end
subgraph "libcommon"
E[utils.h]
F[defines.h]
end
A --> C
A --> D
C --> E
C --> F
subgraph "gametime插件"
G[gametime.c]
end
G --> C
G --> H[sysui.h]
G --> I[plugin.h]
```

**图示来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [defines.h](file://workspace/lib/libcommon/defines.h)
- [gametime.c](file://workspace/plugins/gametime/gametime.c)

**本节来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c)
- [gametime.c](file://workspace/plugins/gametime/gametime.c)

## 核心组件
游戏时间跟踪服务的核心组件包括命令行解析器、数据库交互接口和时间记录逻辑。`gametimectl.c`中的`main`函数负责解析命令行参数，并根据不同的子命令调用相应的函数，如`play_activity_start`、`play_activity_stop`等。这些函数的实现位于`libgametimedb`库中，它们通过`sqlite3` API与数据库进行交互。关键的数据结构包括`ROM`（代表一个游戏）和`PlayActivity`（代表一次游戏会话），它们在`gametimedb.h`中定义。此外，`gametime`插件作为核心功能的延伸，也是一个重要的组件。它实现了`NextUI_Plugin`接口，通过`play_activity_find_all`等函数从数据库获取数据，并使用`SysUI`框架渲染一个包含游戏图标、总时长、平均时长和游玩次数的列表界面。

**本节来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L68)
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h#L1-L44)
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L0-L44)

## 架构概述
游戏时间跟踪服务采用分层架构。最上层是命令行接口（CLI），由`gametimectl`可执行文件提供。中间层是业务逻辑层，由`libgametimedb`库实现，它封装了所有与游戏时间跟踪相关的操作。最底层是数据持久层，使用SQLite数据库存储`rom`和`play_activity`两张表。当游戏启动时，`gametimectl start`命令被调用，`libgametimedb`库会向`play_activity`表插入一条新记录，其`play_time`字段为空，表示会话正在进行。当游戏退出时，`gametimectl stop`命令被调用，库会更新该记录，计算并填充`play_time`字段。新增的`gametime`插件位于一个独立的UI层，它通过`libgametimedb`库读取聚合后的数据，并利用`SysUI`框架为用户提供一个交互式的图形界面，从而完成了从数据采集到用户展示的完整闭环。

``mermaid
graph TD
CLI[命令行接口<br/>gametimectl] --> BLL[业务逻辑层<br/>libgametimedb]
BLL --> DAL[数据持久层<br/>SQLite]
DAL --> DB[(game_logs.sqlite)]
BLL --> UI[UI展示层<br/>gametime插件]
UI --> SysUI[UI框架<br/>SysUI]
```

**图示来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L68)
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L1-L470)
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L353-L402)

## 详细组件分析

### 命令行接口分析
`gametimectl`的命令行接口由`main`函数实现。它支持`start`、`stop`、`resume`、`stop_all`和`list`五个子命令。`main`函数通过遍历`argv`数组来解析这些命令，并调用相应的处理函数。例如，当检测到`start`命令时，它会调用`play_activity_start(argv[++i])`，其中`argv[++i]`是紧随其后的ROM路径。

``mermaid
flowchart TD
Start([开始]) --> ParseArgs["解析命令行参数"]
ParseArgs --> CheckStart{"命令是 'start'?"}
CheckStart --> |是| CallStart["调用 play_activity_start(rom_path)"]
CheckStart --> |否| CheckStop{"命令是 'stop'?"}
CheckStop --> |是| CallStop["调用 play_activity_stop(rom_path)"]
CheckStop --> |否| CheckResume{"命令是 'resume'?"}
CheckResume --> |是| CallResume["调用 play_activity_resume()"]
CheckResume --> |否| CheckList{"命令是 'list'?"}
CheckList --> |是| CallList["调用 play_activity_list_all()"]
CheckList --> |否| PrintUsage["打印用法"]
PrintUsage --> End([结束])
CallStart --> End
CallStop --> End
CallList --> End
CallResume --> End
```

**图示来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L68)

**本节来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L1-L68)

### 数据库交互分析
数据库交互的核心是`libgametimedb`库。`play_activity_db_open`函数负责打开或创建位于`SHARED_USERDATA_PATH/game_logs.sqlite`的数据库文件。如果数据库不存在，它会创建`rom`和`play_activity`两张表。`rom`表存储游戏元数据，`play_activity`表存储每次游戏会话的开始时间（`created_at`）和时长（`play_time`）。`play_time`字段的初始值为NULL，表示会话正在进行。

``mermaid
erDiagram
ROM {
int id PK
text type
text name
text file_path UK
text image_path
int created_at
int updated_at
}
PLAY_ACTIVITY {
int rom_id FK
int play_time
int created_at
int updated_at
}
ROM ||--o{ PLAY_ACTIVITY : "has"
```

**图示来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L30-L50)
- [gametimedb.h](file://workspace/lib/libgametimedb/gametimedb.h#L10-L24)

#### 开始游戏会话
`play_activity_start`函数负责记录游戏会话的开始。它首先调用`play_activity_transaction_rom_find_by_file_path`来确保`rom`表中存在对应游戏的记录。如果不存在，则会创建一条新记录。然后，它向`play_activity`表插入一条新记录，其中`rom_id`指向该游戏，`play_time`为NULL，`created_at`为当前时间戳。这表示一个正在进行的游戏会话。

**本节来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L350-L358)

#### 停止游戏会话
`play_activity_stop`函数负责结束一个游戏会话。它通过`rom_file_path`找到对应的`rom_id`，然后执行一个UPDATE语句，将`play_time`字段设置为`(当前时间戳 - created_at)`，从而计算出本次会话的时长（以秒为单位）。同时，`updated_at`字段也会被更新。这个机制确保了只有`play_time`为NULL的记录才会被更新，防止了重复计时。

**本节来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L378-L385)

#### 恢复游戏会话
`play_activity_resume`函数允许用户恢复上一个被停止的游戏会话。它通过读取`/tmp/next`文件来获取上一个运行的游戏的命令行。`_get_active_rom_path`函数解析该文件，提取出ROM路径。然后，它检查`play_activity`表中是否存在一个`play_time`不为NULL且`rom_id`匹配的记录。如果存在，则认为这是一个可以恢复的会话，并调用`play_activity_start`来创建一个新的会话记录。

**本节来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L360-L370)
- [defines.h](file://workspace/lib/libcommon/defines.h#L20)

#### 查询历史记录
`play_activity_list_all`函数用于查询和显示所有游戏的历史记录。它首先调用`play_activity_find_all`，该函数执行一个复杂的SQL查询，将`rom`表和`play_activity`表进行LEFT JOIN，并按`rom.id`分组。查询结果包括游戏的总游玩次数、总时长、平均时长、首次和最后一次游玩时间。`play_activity_list_all`函数将这些数据格式化后输出到控制台，并计算所有游戏的总游玩时长。

**本节来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L387-L470)

### gametime插件分析
`gametime`插件是一个独立的UI组件，它利用`gametimectl`服务产生的数据，为用户提供一个图形化的游戏时间统计视图。该插件遵循`NextUI_Plugin`接口，其核心是`plugin_init`、`plugin_run`和`plugin_quit`三个生命周期函数。在`plugin_init`阶段，插件初始化`SysUI`框架，调用`play_activity_find_all`从数据库加载所有游戏的统计信息，并预加载游戏图标。`plugin_run`函数负责主循环，它处理用户输入（如上下键滚动、B键退出），并调用`renderList`函数来渲染游戏列表。列表项包含游戏图标、名称、总时长、平均时长和游玩次数。总时长显示在界面顶部。时间数据通过`serializeTime`函数格式化为“Xh Ym”或“Zs”的可读形式。该插件的存在使得`gametimectl`的服务价值得到了最大化，形成了一个完整的“数据采集-存储-展示”闭环。

**本节来源**
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L0-L402)
- [plugin.h](file://workspace/lib/libcommon/plugin.h#L11-L16)
- [sysui.h](file://workspace/lib/libcommon/sysui.h)
- [utils.c](file://workspace/lib/libcommon/utils.c#L198-L250)

## 依赖分析
游戏时间跟踪服务依赖于多个内部和外部库。它直接依赖`libcommon`库，该库提供了`utils.h`和`defines.h`等通用工具和宏定义。`SHARED_USERDATA_PATH`等路径常量在`defines.h`中定义。它还依赖`libgametimedb`库来实现数据库操作。在编译时，它链接了`libsqlite3`库以使用SQLite功能。`makefile`文件清晰地列出了这些依赖关系。`gametime`插件在此基础上，新增了对`plugin.h`和`sysui.h`的依赖，以实现其作为UI插件的功能。

``mermaid
graph LR
gametimectl --> libgametimedb
gametimectl --> libcommon
libgametimedb --> libsqlite3
libcommon --> libc
gametime插件 --> libgametimedb
gametime插件 --> plugin.h
gametime插件 --> sysui.h
```

**图示来源**
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c)
- [makefile](file://workspace/system/gametimectl/makefile#L36)
- [gametime.c](file://workspace/plugins/gametime/gametime.c)

**本节来源**
- [makefile](file://workspace/system/gametimectl/makefile#L36)
- [defines.h](file://workspace/lib/libcommon/defines.h)
- [plugin.h](file://workspace/lib/libcommon/plugin.h)

## 性能考虑
该服务的性能主要受SQLite数据库操作的影响。由于游戏启动和退出是相对低频的事件，因此数据库的读写操作不会成为性能瓶颈。`play_activity_start`和`play_activity_stop`操作都是简单的INSERT和UPDATE语句，执行效率很高。`play_activity_list_all`操作涉及复杂的JOIN和GROUP BY，但由于游戏库的规模通常不会非常大，因此查询时间可以接受。数据库文件位于用户数据目录，确保了数据的持久性和访问速度。`gametime`插件在启动时会执行一次`play_activity_find_all`查询，这可能会在游戏库非常大时产生短暂的加载延迟，但后续的滚动和渲染操作是轻量级的，性能表现良好。

## 故障排除指南
当遇到游戏时间记录丢失或数据库损坏时，可以采取以下措施：
1.  **检查数据库文件**：确认`SHARED_USERDATA_PATH/game_logs.sqlite`文件是否存在且可读写。
2.  **验证命令调用**：确保游戏启动器正确调用了`gametimectl start [rom_path]`，并且游戏退出时调用了`gametimectl stop [rom_path]`。
3.  **检查日志**：查看`gametimectl`的输出日志，确认是否有错误信息，例如"Error: Missing rom_path argument"。
4.  **修复数据库**：如果怀疑数据库损坏，可以尝试使用`sqlite3`命令行工具打开并检查数据库。如果无法修复，可以安全地删除`game_logs.sqlite`文件，服务会在下次启动时自动创建一个新的数据库。
5.  **检查ROM路径**：确保传递给`start`和`stop`命令的`rom_path`参数完全一致，否则服务会将其视为两个不同的游戏。
6.  **插件问题**：如果`gametime`插件无法显示数据，请检查`play_activity_find_all`函数是否返回了有效数据，并确认`SysUI`框架初始化是否成功。

**本节来源**
- [gametimedb.c](file://workspace/lib/libgametimedb/gametimedb.c#L30-L50)
- [gametimectl.c](file://workspace/system/gametimectl/gametimectl.c#L10-L20)
- [gametime.c](file://workspace/plugins/gametime/gametime.c#L353-L402)

## 结论
游戏时间跟踪服务（gametimectl）是一个设计简洁、功能明确的工具。它通过命令行接口与系统其他部分集成，利用SQLite数据库可靠地存储游戏时间数据。其核心机制是通过在`play_activity`表中插入`play_time`为NULL的记录来标记会话开始，并通过更新该字段来计算和结束会话。这种设计有效地防止了重复计时，并支持对历史数据的灵活查询。该服务的模块化设计使其易于维护和扩展。随着`gametime`插件的引入，该服务的价值得到了进一步提升，形成了一个从底层数据采集到上层UI展示的完整生态系统，为用户提供了直观且有价值的游戏时间统计功能。