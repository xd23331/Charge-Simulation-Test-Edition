此模块为文件处理模块，主要包括文件的读取与保存
- fileio.h — 接口与数据结构声明
- fileio.c — CSV 格式读写实现

### 核心数据结构

DataFiles（一组数据文件路径）

- nodes_file：道路节点文件路径
- roads_file：道路连接文件路径
- stations_file：充电站文件路径
- chargers_file：充电桩文件路径
- vehicles_file：车辆文件路径

LoadError（加载错误信息）

- file [256]：出错的文件名
- line：出错行号（0 表示文件无法打开）
- message [128]：错误描述

### 接口说明

int fileio_load_all(Graph *graph, EntityStore *store,
                    const DataFiles *files, LoadError *error);

加载全部基础数据。原子操作：先加载到临时结构，五个文件全部成功后才替换当前 graph 和 store；任一文件失败则保留原数据不变，并通过 error 参数报告出错位置。成功返回 0，失败返回 -1。

int fileio_save_all(const Graph *graph, const EntityStore *store,
                    const DataFiles *files);

保存路网、站点、充电桩和车辆的初始配置数据到五个 CSV 文件。注意保存的是初始状态（initial_node_id、initial_battery_percent、initial_state），不是仿真运行中的当前状态。

int fileio_save_vehicles(const EntityStore *store, const char *filename);

将车辆当前运行状态保存为 CSV，包含 id、node_id、battery_percent、state、station_id、charger_id。

### 输入文件格式

所有文件均为 CSV 格式，第一行为表头（自动跳过），空行自动跳过。

#### 1. 道路节点文件（nodes_file）

id,x,y
1,100.00,200.00
2,300.00,200.00

#### 2. 道路连接文件（roads_file）

from,to,distance_km,travel_minutes,two_way
1,2,5.500,10.000,1

two_way=1 时自动添加反向边（B→A），two_way=0 仅单向。

#### 3. 充电站文件（stations_file）

id,node_id,name
101,5,东部充电站

name 支持中文，读取到行尾。node_id 必须已在节点文件中定义。

#### 4. 充电桩文件（chargers_file）

id,station_id,power_kw,state
1001,101,120.00,0

state 取值：0 = 空闲（CHARGER_FREE），2 = 故障（CHARGER_FAULT）。
注意：加载时只接受 0 或 2，不允许初始状态为忙碌（1）。

#### 5. 车辆文件（vehicles_file）

id,node_id,battery_percent,target_percent,capacity_kwh,urgency
1,3,45.00,80.00,60.00,1

node_id 必须已在节点文件中定义。

### 输出文件说明

fileio_save_all 输出五个文件，内容为初始配置快照：

- 节点文件：id, x, y
- 道路文件：from, to, distance_km, travel_minutes, two_way（始终为 0，因为内部存储为有向边）
- 充电站文件：id, node_id, name
- 充电桩文件：id, station_id, power_kw, state（保存 initial_state，忙碌状态按空闲保存）
- 车辆文件：id, node_id（initial_node_id）, battery_percent（initial_battery_percent）, target_percent, capacity_kwh, urgency

fileio_save_vehicles 输出单个文件，内容为车辆当前运行状态：

id,node_id,battery_percent,state,station_id,charger_id

### 使用要点

1. 原子加载：fileio_load_all 要么全部成功替换，要么全部失败保留原样，不会出现部分加载的中间状态。
2. 加载顺序有依赖：节点 → 道路 → 充电站 → 充电桩 → 车辆。后加载的实体引用的编号必须已存在。
3. fileio_save_all 保存的是初始配置，不是当前运行状态；如需保存当前状态用 fileio_save_vehicles。
4. 所有文件为纯文本 CSV，可用 Excel 直接打开编辑。
5. 错误定位：加载失败时，error->file 指明出错文件，error->line 指明行号（0 = 文件打不开），error->message 给出原因（如 "invalid node data"、"cannot open file"）。