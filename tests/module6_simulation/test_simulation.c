#include "../../src/module1_graph/graph.h"
#include "../../src/module3_entity/entity.h"
#include "../../src/module4_charger/charger.h"
#include "../../src/module5_recommend/recommend.h"
#include "../../src/module6_simulation/simulation.h"

#include <assert.h>
#include <stdio.h>

/* 测试完整仿真流程和重置功能。 */
int main(void)
{
    Graph graph;
    EntityStore store;
    Simulation simulation;
    RecommendationList list;
    Vehicle *vehicle;
    int request_id;
    double battery_before;
    int steps = 0;

    graph_init(&graph);
    entity_init(&store);
    assert(graph_add_node(&graph, 1, 0, 0) == 0);
    assert(graph_add_node(&graph, 2, 1, 0) == 0);
    assert(graph_add_edge(&graph, 1, 2, 2, 2) == 0);
    assert(entity_add_station(&store, &graph, 10, 2, "Station") == 0);
    assert(charger_add(&store, 10, 100, 60, CHARGER_FREE) == 0);
    assert(charger_add(&store, 10, 101, 60, CHARGER_FAULT) == 0);
    assert(entity_add_vehicle(&store, &graph, 1, 1,
                              20, 80, 60, 2) == 0);

    vehicle = entity_find_vehicle(&store, 1);
    assert(recommend_all(&graph, &store, vehicle, &list) == 0);
    assert(simulation_init(&simulation, &graph, &store, 1) == 0);
    assert(simulation_charger_count(&simulation, 10) == 2);
    assert(simulation_charger_at(&simulation, 10, 0) != NULL);
    assert(simulation_charger_at(&simulation, 10, 2) == NULL);
    assert(simulation_submit(&simulation, 1, recommend_best(&list)) == 0);
    assert(simulation_request_count(&simulation) == 1);
    assert(simulation_request_at(&simulation, 0) == vehicle->request);
    request_id = vehicle->request->id;
    assert(entity_find_request(&store, request_id) != NULL);
    assert(simulation_cancel(&simulation, request_id) == 0);
    assert(vehicle->state == VEHICLE_IDLE && store.request_count == 0);

    assert(simulation_submit(&simulation, 1, recommend_best(&list)) == 0);
    while (vehicle->state != VEHICLE_CHARGING && steps++ < 10) {
        simulation_step(&simulation);
    }
    assert(vehicle->state == VEHICLE_CHARGING);
    battery_before = vehicle->battery_percent;
    simulation_step(&simulation);
    assert(vehicle->battery_percent > battery_before);
    assert(simulation_break_charger(&simulation, 100) == 0);
    assert(vehicle->state == VEHICLE_WAITING);
    assert(simulation_waiting_count(&simulation, 10) == 1);
    assert(simulation_waiting_vehicle_at(&simulation, 10, 0) == vehicle);
    assert(simulation_repair_charger(&simulation, 100) == 0);
    simulation_step(&simulation);
    assert(vehicle->state == VEHICLE_CHARGING);

    while (simulation_has_active(&simulation) && steps++ < 100) {
        simulation_step(&simulation);
    }
    assert(vehicle->state == VEHICLE_FINISHED);
    assert(simulation_repair_charger(&simulation, 101) == 0);
    assert(simulation_break_charger(&simulation, 100) == 0);
    simulation_reset(&simulation);
    assert(vehicle->state == VEHICLE_IDLE);
    assert(vehicle->node_id == 1);
    assert(vehicle->battery_percent == 20);
    assert(entity_find_charger(&store, 100)->state == CHARGER_FREE);
    assert(entity_find_charger(&store, 101)->state == CHARGER_FAULT);

    graph_free(&graph);
    entity_free(&store);
    printf("test_simulation passed\n");
    return 0;
}

