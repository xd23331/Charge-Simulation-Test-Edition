#include "../../src/module1_graph/graph.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

/* 判断两个浮点数是否足够接近。 */
static int close_enough(double a, double b)
{
    return fabs(a - b) < 0.0001;
}

/* 测试最短路径、路径恢复和不可达判断。 */
int main(void)
{
    Graph graph;
    PathResult result;
    DijkstraResult dijkstra;

    graph_init(&graph);
    assert(graph_add_node(&graph, 1, 0, 0) == 0);
    assert(graph_add_node(&graph, 2, 1, 0) == 0);
    assert(graph_add_node(&graph, 3, 2, 0) == 0);
    assert(graph_add_node(&graph, 4, 3, 0) == 0);
    assert(graph_add_edge(&graph, 1, 2, 2, 4) == 0);
    assert(graph_add_edge(&graph, 2, 3, 3, 6) == 0);
    assert(graph_add_edge(&graph, 1, 3, 10, 20) == 0);
    assert(graph_add_edge(&graph, 1, 3, 10, 20) != 0);

    assert(graph_shortest_path(&graph, 1, 3, &result) == 0);
    assert(result.reachable == 1);
    assert(result.path_length == 3);
    assert(result.node_ids[0] == 1 && result.node_ids[2] == 3);
    assert(close_enough(result.total_minutes, 10.0));
    assert(close_enough(result.total_distance_km, 5.0));

    assert(graph_dijkstra_all(&graph, 1, &dijkstra) == 0);
    assert(graph_path_from_dijkstra(&graph, &dijkstra, 3, &result) == 0);
    assert(result.reachable == 1);
    assert(close_enough(result.total_minutes, 10.0));
    assert(graph_path_from_dijkstra(&graph, &dijkstra, 4, &result) == 0);
    assert(result.reachable == 0);

    assert(graph_shortest_path(&graph, 1, 4, &result) == 0);
    assert(result.reachable == 0);

    assert(graph_update_node(&graph, 2, 5, 6) == 0);
    assert(graph_update_edge(&graph, 1, 3, 4, 5) == 0);
    assert(graph_shortest_path(&graph, 1, 3, &result) == 0);
    assert(close_enough(result.total_minutes, 5.0));
    assert(graph_remove_edge(&graph, 1, 3) == 0);
    assert(graph_remove_node(&graph, 2) == 0);
    assert(graph_shortest_path(&graph, 1, 3, &result) == 0);
    assert(result.reachable == 0);
    graph_free(&graph);
    printf("test_graph passed\n");
    return 0;
}

