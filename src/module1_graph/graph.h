#ifndef GRAPH_H
#define GRAPH_H

#include "../common/model.h"

/* 道路边，记录终点、距离和预计通行时间。 */
typedef struct Edge {
    int to_index;
    double distance_km;
    double travel_minutes;
    struct Edge *next;
} Edge;

/* 道路节点，保存编号、绘图坐标和邻接边。 */
typedef struct {
    int id;
    double x;
    double y;
    Edge *edges;
} GraphNode;

/* 路网，使用固定上限的节点数组和邻接表。 */
typedef struct {
    GraphNode nodes[MAX_GRAPH_NODES];
    int node_count;
} Graph;

/* 最短路径结果，保存路径、总时间和总距离。 */
typedef struct {
    int reachable;
    int node_ids[MAX_PATH_NODES];
    int path_length;
    double total_minutes;
    double total_distance_km;
} PathResult;

/* 单源Dijkstra结果，保存起点到全部节点的最短时间和前驱。 */
typedef struct {
    int start_index;
    int node_count;
    double distances[MAX_GRAPH_NODES];
    int previous[MAX_GRAPH_NODES];
} DijkstraResult;

/* 初始化一个空路网。 */
void graph_init(Graph *graph);

/* 添加一个道路节点，成功返回0。 */
int graph_add_node(Graph *graph, int id, double x, double y);

/* 修改道路节点的绘图坐标。 */
int graph_update_node(Graph *graph, int id, double x, double y);

/* 删除道路节点及所有与它相连的道路。 */
int graph_remove_node(Graph *graph, int id);

/* 添加一条单向道路，成功返回0。 */
int graph_add_edge(Graph *graph, int from_id, int to_id,
                   double distance_km, double travel_minutes);

/* 修改一条已有道路的距离和通行时间。 */
int graph_update_edge(Graph *graph, int from_id, int to_id,
                      double distance_km, double travel_minutes);

/* 删除一条单向道路。 */
int graph_remove_edge(Graph *graph, int from_id, int to_id);

/* 根据节点编号查找数组下标，未找到返回-1。 */
int graph_find_node(const Graph *graph, int node_id);

/* 从一个起点执行一次Dijkstra并保存到全部节点的结果。 */
int graph_dijkstra_all(const Graph *graph, int start_id,
                       DijkstraResult *result);

/* 从单源Dijkstra结果中恢复到指定终点的路径。 */
int graph_path_from_dijkstra(const Graph *graph,
                             const DijkstraResult *dijkstra,
                             int end_id, PathResult *result);

/* 计算两个节点之间按通行时间最短的路径。 */
int graph_shortest_path(const Graph *graph, int start_id, int end_id,
                        PathResult *result);

/* 根据行驶距离估算车辆耗电量。 */
double graph_calc_energy(double distance_km, double kwh_per_km);

/* 释放路网中所有道路边。 */
void graph_free(Graph *graph);

#endif

