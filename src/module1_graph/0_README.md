此文件主要内容为图的建立
- graph.h — 数据结构与接口声明
- graph.c — 全部实现（含内部最小堆）
### 核心数据结构

Edge（单向道路边）
- to_index：终点节点在数组中的下标
- distance_km：距离（公里）
- travel_minutes：预计通行时间（分钟）
- next：邻接表下一条边

GraphNode（道路节点）
- id：节点业务编号
- x, y：绘图坐标
- edges：邻接边链表头指针

Graph（路网）
- nodes [MAX_GRAPH_NODES]：节点数组
- node_count：当前节点数

PathResult（最短路径结果）
- reachable：是否可达（1/0）
- node_ids [MAX_PATH_NODES]：路径上的节点编号序列
- path_length：路径节点数
- total_minutes：总通行时间
- total_distance_km：总距离

DijkstraResult（单源最短路径结果）
- start_index：起点数组下标
- node_count：节点总数
- distances [MAX_GRAPH_NODES]：到各节点的最短时间
- previous [MAX_GRAPH_NODES]：各节点的前驱下标

### 接口说明

void graph_init(Graph *graph);
初始化为空路网，node_count 置 0，所有边指针置 NULL。

void graph_free(Graph *graph);
释放所有道路边的内存，node_count 置 0。

#### 节点管理

int graph_add_node(Graph *graph, int id, double x, double y);
添加一个道路节点。id 不能重复，不能超过 MAX_GRAPH_NODES。成功返回 0，失败返回 -1。

int graph_update_node(Graph *graph, int id, double x, double y);
修改节点的绘图坐标。节点不存在返回 -1。

int graph_remove_node(Graph *graph, int id);
删除节点，同时自动删除所有从该节点出发和指向该节点的道路，并修正剩余边的 to_index。

int graph_find_node(const Graph *graph, int node_id);
根据节点编号查找数组下标，未找到返回 -1。
#### 道路管理（均为单向）
int graph_add_edge(Graph *graph, int from_id, int to_id,
double distance_km, double travel_minutes);
添加一条单向道路。distance_km 和 travel_minutes 必须大于 0，起止节点必须存在，重复边会被拒绝。

int graph_update_edge(Graph *graph, int from_id, int to_id,
double distance_km, double travel_minutes);
修改已有道路的距离和通行时间。

int graph_remove_edge(Graph *graph, int from_id, int to_id);
删除一条单向道路。

#### 路径计算

int graph_dijkstra_all(const Graph *graph, int start_id,
DijkstraResult *result);

从起点执行一次 Dijkstra，计算到全部节点的最短通行时间和前驱关系。适合需要查询多个终点的场景。

int graph_path_from_dijkstra(const Graph *graph,
const DijkstraResult *dijkstra,
int end_id, PathResult *result);

从已有的 Dijkstra 结果中恢复到指定终点的完整路径（节点序列、总时间、总距离）。终点不可达时 result->reachable = 0。

int graph_shortest_path(const Graph *graph, int start_id, int end_id,
PathResult *result);

便捷封装，内部调用 graph_dijkstra_all + graph_path_from_dijkstra。仅查询单个终点时使用。
#### 工具函数

double graph_calc_energy(double distance_km, double kwh_per_km);

根据行驶距离和百公里能耗率估算耗电量，返回 kWh。

