#include "../../src/module3_entity/entity.h"

#include <assert.h>
#include <stdio.h>

/* 测试车辆和站点的添加、查找、修改与删除。 */
int main(void)
{
    EntityStore store;
    Graph graph;
    Vehicle *vehicle;
    Station *station;
    ChargeRequest request = {0};

    graph_init(&graph);
    entity_init(&store);
    assert(graph_add_node(&graph, 10, 0, 0) == 0);
    assert(graph_add_node(&graph, 11, 1, 0) == 0);
    assert(graph_add_node(&graph, 20, 2, 0) == 0);
    assert(graph_add_node(&graph, 21, 3, 0) == 0);
    assert(graph_add_node(&graph, 30, 4, 0) == 0);
    assert(entity_add_vehicle(&store, &graph, 1, 99,
                              30, 80, 60, 2) != 0);
    assert(entity_add_vehicle(&store, &graph, 1, 10,
                              30, 80, 60, 2) == 0);
    assert(entity_add_vehicle(&store, &graph, 1, 10,
                              30, 80, 60, 2) != 0);
    vehicle = entity_find_vehicle(&store, 1);
    assert(vehicle != NULL);
    assert(entity_update_vehicle(&store, &graph, 1, 99,
                                 40, 90, 70, 3) != 0);
    assert(entity_update_vehicle(&store, &graph, 1, 11,
                                 40, 90, 70, 3) == 0);
    assert(vehicle->node_id == 11 && vehicle->urgency == 3);

    assert(entity_add_station(&store, &graph, 101, 99, "Invalid") != 0);
    assert(entity_add_station(&store, &graph, 101, 20, "Station A") == 0);
    station = entity_find_station(&store, 101);
    assert(station != NULL);
    assert(entity_update_station(&store, &graph, 101, 99,
                                 "Invalid") != 0);
    assert(entity_update_station(&store, &graph, 101, 21,
                                 "Station B") == 0);
    assert(station->node_id == 21);
    assert(entity_remove_graph_node(&graph, &store, 11) != 0);
    assert(entity_remove_graph_node(&graph, &store, 21) != 0);

    request.id = 500;
    request.vehicle = vehicle;
    request.station = station;
    assert(entity_register_request(&store, &request) == 0);
    assert(entity_find_request(&store, 500) == &request);
    assert(entity_unregister_request(&store, 500) == 0);

    assert(entity_remove_vehicle(&store, 1) == 0);
    assert(entity_remove_station(&store, 101) == 0);
    assert(entity_remove_graph_node(&graph, &store, 30) == 0);
    assert(store.vehicle_count == 0 && store.station_count == 0);
    entity_free(&store);
    graph_free(&graph);
    printf("test_entity passed\n");
    return 0;
}

