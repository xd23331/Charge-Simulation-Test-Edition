# 城市新能源汽车充电资源调度仿真系统

本项目使用 C11 实现。已完成模块1至模块7的后端代码、模块8（GTK 图形界面与路网绘制）和模块9（定时控制）的前端代码、样例数据、命令行演示和基础测试。

## 当前实现功能

- 模块1：邻接表、道路增删改、二叉最小堆、单源 Dijkstra 和路径恢复。
- 模块2：二叉最大堆、动态优先级和按充电桩任务估算等待时间。
- 模块3：拉链哈希表、车辆/站点/充电桩/请求索引、实体增查改删、节点引用校验和受保护的道路节点删除。
- 模块4：充电桩单链表、初始状态记录、功率修改、逐步充电、分配、释放和故障状态。
- 模块5：候选站点过滤、在途负载估算、评分和排序。
- 模块6：车辆到达、排队、充电、完成、请求取消、运行中故障和恢复的固定时间步仿真，并提供车辆、站点、充电桩、等待队列和请求的统一只读查询接口。
- 模块7：CSV 数据加载、校验、全部基础数据保存和车辆状态保存。
- 模块8：GTK 4 单一主窗口图形界面，使用 DrawingArea 与 Cairo 绘制固定二维示意路网，展示道路节点、充电站、车辆出发位置和推荐路线，支持数据加载、车辆/站点查看、充电请求提交、站点推荐以及开始、暂停、单步、重置等操作。
- 模块9：基于 g_timeout_add 的定时控制器，统一管理仿真开始、暂停、单步和重置，定时回调只调用统一仿真入口并触发界面刷新。

## 编译与测试

项目提供 `Makefile`，使用支持 C11 的 GCC 编译：

```text
make all
make test
```

演示程序生成在 `build/backend_demo.exe`，测试程序生成在 `build` 目录。

当前包含模块1至模块7的独立测试和一组完整流程集成测试。

## Win32 后端测试页面

`src/test_ui` 中提供了一个额外的 Win32 API 测试页面，仅用于检查后端，不属于正式模块8。它支持数据加载、站点推荐、请求提交、单步与连续仿真、充电桩故障与恢复、仿真重置和简单路网绘制。道路旁会显示距离，线段长度与道路距离采用统一比例；站点旁会显示充电桩状态数量，也可以点击“查看桩状态”查看每个充电桩的详细信息。

```text
make gui
build/backend_test_gui.exe
```

页面默认加载 20 辆样例车辆，并使用车辆 `201` 和充电桩 `1005` 作为单车操作的默认编号。可以先点击“站点推荐”再点击“提交请求”，也可以点击“批量提交”一次提交全部空闲车辆，以观察多站点分流和排队。“运行10步”和“运行到底”采用定时刷新，车辆会沿橙色虚线标出的推荐路线逐步移动。站点旁的“等待”数字表示当前排队车辆数。“设为故障”和“修复设备”用于测试充电桩故障与恢复，“重置仿真”用于恢复初始状态。

## 正式前端（模块8 GTK 界面 + 模块9 定时控制）

正式前端由 `src/module8_ui/gtk_ui.c` 和 `src/module9_timer/timer.c` 组成，使用 GTK 4 开发，需要在 msys2 的 ucrt64 环境中编译和运行。

### 编译

在 msys2 ucrt64 终端中进入项目根目录执行：

```text
make gui
```

或在任意 shell 中直接使用 gcc（需 `pkg-config` 可用，且 gtk4 开发包已安装）：

```text
gcc -std=c11 -Wall -Wextra -pedantic -Isrc \
    $(pkg-config --cflags gtk4) \
    src/module8_ui/gtk_ui.c src/module9_timer/timer.c \
    src/module1_graph/graph.c src/module2_scheduler/scheduler.c \
    src/module3_entity/entity.c src/module4_charger/charger.c \
    src/module5_recommend/recommend.c src/module6_simulation/simulation.c \
    src/module7_fileio/fileio.c \
    $(pkg-config --libs gtk4) \
    -o build/charging_system_gui.exe
```

编译产物为 `build/charging_system_gui.exe`。

### 运行

`build/` 目录下已附带全部 GTK 运行库 DLL（共 59 个），因此**直接双击 `build/charging_system_gui.exe` 即可运行**，无需安装 msys2 或配置环境变量。

也可以使用以下方式运行：

1. 双击项目根目录下的 `start_gui.bat`（备用启动脚本，会自动把 msys2 的 `ucrt64/bin` 加入 PATH）。
2. 在 msys2 ucrt64 终端中，进入项目根目录后运行：
   ```text
   ./build/charging_system_gui.exe
   ```

> 注意：如果重新编译了 exe，需要确保 `build/` 目录下的 DLL 文件仍然存在；若被删除，可从 `C:\msys64\ucrt64\bin` 重新复制，或使用 `start_gui.bat` 启动。

程序启动后自动加载 `data/input` 下的样例数据并在路网上绘制。界面主要操作：

- **加载数据**：重新读取样例 CSV 数据。
- **站点推荐**：为输入编号的车辆计算全部候选充电站并排序，红色路线标出推荐路径，日志显示各站行驶/等待/充电时间及不可选原因。
- **提交请求 / 批量提交**：提交单车或全部空闲车辆的充电请求。
- **单步运行 / 运行10步 / 开始运行 / 暂停**：定时控制模块统一驱动的仿真推进。
- **重置仿真**：恢复初始状态。
- **设为故障 / 修复设备**：测试充电桩故障与恢复。
- **车辆列表 / 站点列表 / 查看桩状态**：查看各业务实体信息。

## 目录结构

```text
src/
├─ app/                     程序入口
├─ common/                  公共结构体、枚举和常量
├─ module1_graph/           模块1：道路图与 Dijkstra
├─ module2_scheduler/       模块2：优先队列与动态调度
├─ module3_entity/          模块3：哈希表与实体管理
├─ module4_charger/         模块4：充电桩链表与资源管理
├─ module5_recommend/       模块5：充电站综合推荐
├─ module6_simulation/      模块6：动态仿真
├─ module7_fileio/          模块7：文件读写
├─ module8_ui/              模块8：GTK 图形界面与路网绘制
└─ module9_timer/           模块9：定时控制

tests/
├─ module1_graph/           模块1测试
├─ module2_scheduler/       模块2测试
├─ module3_entity/          模块3测试
├─ module4_charger/         模块4测试
├─ module5_recommend/       模块5测试
├─ module6_simulation/      模块6测试
├─ module7_fileio/          模块7测试
├─ module8_ui/              模块8测试
├─ module9_timer/           模块9测试
└─ integration/             整体联调测试

data/
├─ input/                   输入数据
└─ output/                  保存数据和测试输出

assets/                     界面资源
build/                      编译输出
```

