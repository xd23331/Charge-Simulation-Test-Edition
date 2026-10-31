#include "graph.h"

#include <float.h>
#include <stdlib.h>

/* 最小堆元素，保存节点下标和当前最短时间。 */
typedef struct {
    int node_index;
    double distance;
} MinHeapItem;

/* Dijkstra 使用的简单二叉最小堆。 */
typedef struct {
    MinHeapItem items[MAX_GRAPH_NODES * MAX_GRAPH_NODES];
    int size;
} MinHeap;

/* 交换最小堆中的两个元素。 */
static void heap_swap(MinHeapItem *a, MinHeapItem *b)
{
    MinHeapItem temp = *a;
    *a = *b;
    *b = temp;
}

/* 向最小堆加入一个候选节点。 */
static int heap_push(MinHeap *heap, int node_index, double distance)
{
    int index;
    int parent;

    if (heap->size >= MAX_GRAPH_NODES * MAX_GRAPH_NODES) {
        return -1;
    }

    index = heap->size++;
    heap->items[index].node_index = node_index;
    heap->items[index].distance = distance;

    while (index > 0) {
        parent = (index - 1) / 2;
        if (heap->items[parent].distance <= heap->items[index].distance) {
            break;
        }
        heap_swap(&heap->items[parent], &heap->items[index]);
        index = parent;
    }
    return 0;
}

/* 从最小堆取出距离最小的元素。 */
static int heap_pop(MinHeap *heap, MinHeapItem *item)
{
    int index = 0;

    if (heap->size == 0) {
        return -1;
    }

    *item = heap->items[0];
    heap->items[0] = heap->items[--heap->size];

    while (1) {
        int left = index * 2 + 1;
        int right = left + 1;
        int smallest = index;

        if (left < heap->size &&
            heap->items[left].distance < heap->items[smallest].distance) {
            smallest = left;
        }
        if (right < heap->size &&
            heap->items[right].distance < heap->items[smallest].distance) {
            smallest = right;
        }
        if (smallest == index) {
            break;
        }
        heap_swap(&heap->items[index], &heap->items[smallest]);
        index = smallest;
    }
    return 0;
}

/* 根据一条路径统计道路总距离。 */
static double path_distance(const Graph *graph, const int *indices, int count)
{
    double total = 0.0;
    int i;

    for (i = 0; i + 1 < count; ++i) {
        Edge *edge = graph->nodes[indices[i]].edges;
        while (edge != NULL && edge->to_index != indices[i + 1]) {
            edge = edge->next;
        }
        if (edge != NULL) {
            total += edge->distance_km;
        }
    }
    return total;
}

/* 在一个节点的邻接表中查找指定道路。 */
static Edge *find_edge(Graph *graph, int from_index, int to_index)
{
    Edge *edge = graph->nodes[from_index].edges;

    while (edge != NULL) {
        if (edge->to_index == to_index) {
            return edge;
        }
        edge = edge->next;
    }
    return NULL;
}

/* 删除所有指向指定节点的道路并修正节点下标。 */
static void remove_incoming_edges(Graph *graph, int removed_index)
{
    int i;

    for (i = 0; i < graph->node_count; ++i) {
        Edge *edge;
        Edge *previous = NULL;
        if (i == removed_index) {
            continue;
        }
        edge = graph->nodes[i].edges;
        while (edge != NULL) {
            Edge *next = edge->next;
            if (edge->to_index == removed_index) {
                if (previous == NULL) {
                    graph->nodes[i].edges = next;
                } else {
                    previous->next = next;
                }
                free(edge);
            } else {
                if (edge->to_index > removed_index) {
                    --edge->to_index;
                }
                previous = edge;
            }
            edge = next;
        }
    }
}

/* 把前驱数组转换为从起点到终点的节点序列。 */
static int build_path(const Graph *graph, int start, int end,
                      const int *previous, PathResult *result)
{
    int reversed[MAX_PATH_NODES];
    int count = 0;
    int current = end;
    int i;

    while (current != -1 && count < MAX_PATH_NODES) {
        reversed[count++] = current;
        if (current == start) {
            break;
        }
        current = previous[current];
    }

    if (count == 0 || reversed[count - 1] != start) {
        return -1;
    }

    result->path_length = count;
    for (i = 0; i < count; ++i) {
        int index = reversed[count - i - 1];
        result->node_ids[i] = graph->nodes[index].id;
    }

    for (i = 0; i < count; ++i) {
        reversed[i] = graph_find_node(graph, result->node_ids[i]);
    }
    result->total_distance_km = path_distance(graph, reversed, count);
    return 0;
}

/* 初始化一个空路网。 */
void graph_init(Graph *graph)
{
    int i;

    if (graph == NULL) {
        return;
    }
    graph->node_count = 0;
    for (i = 0; i < MAX_GRAPH_NODES; ++i) {
        graph->nodes[i].edges = NULL;
    }
}

/* 添加一个道路节点，成功返回0。 */
int graph_add_node(Graph *graph, int id, double x, double y)
{
    GraphNode *node;

    if (graph == NULL || id < 0 || graph_find_node(graph, id) >= 0) {
        return -1;
    }
    if (graph->node_count >= MAX_GRAPH_NODES) {
        return -1;
    }

    node = &graph->nodes[graph->node_count++];
    node->id = id;
    node->x = x;
    node->y = y;
    node->edges = NULL;
    return 0;
}

/* 修改道路节点的绘图坐标。 */
int graph_update_node(Graph *graph, int id, double x, double y)
{
    int index = graph_find_node(graph, id);

    if (index < 0) {
        return -1;
    }
    graph->nodes[index].x = x;
    graph->nodes[index].y = y;
    return 0;
}

/* 删除道路节点及所有与它相连的道路。 */
int graph_remove_node(Graph *graph, int id)
{
    int index;
    int i;
    Edge *edge;

    if (graph == NULL) {
        return -1;
    }
    index = graph_find_node(graph, id);
    if (index < 0) {
        return -1;
    }

    edge = graph->nodes[index].edges;
    while (edge != NULL) {
        Edge *next = edge->next;
        free(edge);
        edge = next;
    }
    graph->nodes[index].edges = NULL;
    remove_incoming_edges(graph, index);

    for (i = index; i + 1 < graph->node_count; ++i) {
        graph->nodes[i] = graph->nodes[i + 1];
    }
    --graph->node_count;
    graph->nodes[graph->node_count].edges = NULL;
    return 0;
}

/* 添加一条单向道路，成功返回0。 */
int graph_add_edge(Graph *graph, int from_id, int to_id,
                   double distance_km, double travel_minutes)
{
    int from;
    int to;
    Edge *edge;

    if (graph == NULL || distance_km <= 0.0 || travel_minutes <= 0.0) {
        return -1;
    }

    from = graph_find_node(graph, from_id);
    to = graph_find_node(graph, to_id);
    if (from < 0 || to < 0 || find_edge(graph, from, to) != NULL) {
        return -1;
    }

    edge = (Edge *)malloc(sizeof(Edge));
    if (edge == NULL) {
        return -1;
    }
    edge->to_index = to;
    edge->distance_km = distance_km;
    edge->travel_minutes = travel_minutes;
    edge->next = graph->nodes[from].edges;
    graph->nodes[from].edges = edge;
    return 0;
}

/* 修改一条已有道路的距离和通行时间。 */
int graph_update_edge(Graph *graph, int from_id, int to_id,
                      double distance_km, double travel_minutes)
{
    int from;
    int to;
    Edge *edge;

    if (graph == NULL || distance_km <= 0.0 || travel_minutes <= 0.0) {
        return -1;
    }
    from = graph_find_node(graph, from_id);
    to = graph_find_node(graph, to_id);
    if (from < 0 || to < 0) {
        return -1;
    }
    edge = find_edge(graph, from, to);
    if (edge == NULL) {
        return -1;
    }
    edge->distance_km = distance_km;
    edge->travel_minutes = travel_minutes;
    return 0;
}

/* 删除一条单向道路。 */
int graph_remove_edge(Graph *graph, int from_id, int to_id)
{
    int from;
    int to;
    Edge *edge;
    Edge *previous = NULL;

    if (graph == NULL) {
        return -1;
    }
    from = graph_find_node(graph, from_id);
    to = graph_find_node(graph, to_id);
    if (from < 0 || to < 0) {
        return -1;
    }
    edge = graph->nodes[from].edges;
    while (edge != NULL && edge->to_index != to) {
        previous = edge;
        edge = edge->next;
    }
    if (edge == NULL) {
        return -1;
    }
    if (previous == NULL) {
        graph->nodes[from].edges = edge->next;
    } else {
        previous->next = edge->next;
    }
    free(edge);
    return 0;
}

/* 根据节点编号查找数组下标，未找到返回-1。 */
int graph_find_node(const Graph *graph, int node_id)
{
    int i;

    if (graph == NULL) {
        return -1;
    }
    for (i = 0; i < graph->node_count; ++i) {
        if (graph->nodes[i].id == node_id) {
            return i;
        }
    }
    return -1;
}

/* 从一个起点执行一次Dijkstra并保存全部节点结果。 */
int graph_dijkstra_all(const Graph *graph, int start_id,
                       DijkstraResult *result)
{
    int visited[MAX_GRAPH_NODES] = {0};
    MinHeap heap = {{{0}}, 0};
    MinHeapItem item;
    int start;
    int i;

    if (graph == NULL || result == NULL) {
        return -1;
    }
    start = graph_find_node(graph, start_id);
    if (start < 0) {
        return -1;
    }

    result->start_index = start;
    result->node_count = graph->node_count;
    for (i = 0; i < graph->node_count; ++i) {
        result->distances[i] = DBL_MAX;
        result->previous[i] = -1;
    }
    result->distances[start] = 0.0;
    if (heap_push(&heap, start, 0.0) != 0) {
        return -1;
    }

    while (heap_pop(&heap, &item) == 0) {
        Edge *edge;
        if (visited[item.node_index]) {
            continue;
        }
        visited[item.node_index] = 1;

        edge = graph->nodes[item.node_index].edges;
        while (edge != NULL) {
            double next = item.distance + edge->travel_minutes;
            if (next < result->distances[edge->to_index]) {
                result->distances[edge->to_index] = next;
                result->previous[edge->to_index] = item.node_index;
                if (heap_push(&heap, edge->to_index, next) != 0) {
                    return -1;
                }
            }
            edge = edge->next;
        }
    }
    return 0;
}

/* 从单源Dijkstra结果中恢复一条终点路径。 */
int graph_path_from_dijkstra(const Graph *graph,
                             const DijkstraResult *dijkstra,
                             int end_id, PathResult *result)
{
    int end;

    if (graph == NULL || dijkstra == NULL || result == NULL ||
        dijkstra->node_count != graph->node_count ||
        dijkstra->start_index < 0 ||
        dijkstra->start_index >= graph->node_count) {
        return -1;
    }
    result->reachable = 0;
    result->path_length = 0;
    result->total_minutes = 0.0;
    result->total_distance_km = 0.0;

    end = graph_find_node(graph, end_id);
    if (end < 0) {
        return -1;
    }
    if (dijkstra->distances[end] == DBL_MAX) {
        return 0;
    }
    if (build_path(graph, dijkstra->start_index, end,
                   dijkstra->previous, result) != 0) {
        return -1;
    }
    result->reachable = 1;
    result->total_minutes = dijkstra->distances[end];
    return 0;
}

/* 计算两个节点之间按通行时间最短的路径。 */
int graph_shortest_path(const Graph *graph, int start_id, int end_id,
                        PathResult *result)
{
    DijkstraResult dijkstra;

    if (graph_dijkstra_all(graph, start_id, &dijkstra) != 0) {
        return -1;
    }
    return graph_path_from_dijkstra(graph, &dijkstra, end_id, result);
}

/* 根据行驶距离估算车辆耗电量。 */
double graph_calc_energy(double distance_km, double kwh_per_km)
{
    if (distance_km < 0.0 || kwh_per_km < 0.0) {
        return 0.0;
    }
    return distance_km * kwh_per_km;
}

/* 释放路网中所有道路边。 */
void graph_free(Graph *graph)
{
    int i;

    if (graph == NULL) {
        return;
    }
    for (i = 0; i < graph->node_count; ++i) {
        Edge *edge = graph->nodes[i].edges;
        while (edge != NULL) {
            Edge *next = edge->next;
            free(edge);
            edge = next;
        }
        graph->nodes[i].edges = NULL;
    }
    graph->node_count = 0;
}

