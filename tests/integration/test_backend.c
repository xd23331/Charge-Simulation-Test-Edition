#include "../../src/module1_graph/graph.h"
#include "../../src/module3_entity/entity.h"
#include "../../src/module4_charger/charger.h"
#include "../../src/module5_recommend/recommend.h"
#include "../../src/module6_simulation/simulation.h"
#include "../../src/module7_fileio/fileio.h"

#include <assert.h>
#include <stdio.h>

/* 测试数据加载、推荐和完整充电流程。 */
int main(void)
{
    Graph graph;
    EntityStore store;
    Simulation simulation;
    RecommendationList list;
    const Recommendation *best;
    Vehicle *vehicle;
    LoadError error;
    DataFiles files = {
        "data/input/nodes.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };
    int steps = 0;
    int saw_queue = 0;
    int i;

    graph_init(&graph);
    entity_init(&store);
    assert(fileio_load_all(&graph, &store, &files, &error) == 0);
    assert(graph.node_count == 20);
    assert(store.station_count == 5);
    assert(store.vehicle_count == 20);
    assert(entity_find_charger(&store, 1003) != NULL);

    vehicle = entity_find_vehicle(&store, 201);
    assert(vehicle != NULL);
    assert(recommend_all(&graph, &store, vehicle, &list) == 0);
    best = recommend_best(&list);
    assert(best != NULL);

    assert(simulation_init(&simulation, &graph, &store, 1.0) == 0);
    assert(simulation_submit(&simulation, vehicle->id, best) == 0);
    while (simulation_has_active(&simulation) && steps < 500) {
        assert(simulation_step(&simulation) == 0);
        ++steps;
    }
    assert(steps < 500);
    assert(vehicle->state == VEHICLE_FINISHED);
    assert(vehicle->battery_percent == vehicle->target_percent);

    /* 批量提交样例车辆，确认数据能够形成排队并最终完成。 */
    simulation_reset(&simulation);
    for (i = 0; i < store.vehicle_count; ++i) {
        vehicle = entity_find_vehicle(&store, store.vehicle_ids[i]);
        assert(vehicle != NULL);
        assert(recommend_all(&graph, &store, vehicle, &list) == 0);
        best = recommend_best(&list);
        assert(best != NULL);
        assert(simulation_submit(&simulation, vehicle->id, best) == 0);
    }
    steps = 0;
    while (simulation_has_active(&simulation) && steps < 1000) {
        assert(simulation_step(&simulation) == 0);
        for (i = 0; i < store.station_count; ++i) {
            Station *station = entity_find_station(
                &store, store.station_ids[i]);
            if (station != NULL && station->queue_size > 0) {
                saw_queue = 1;
            }
        }
        ++steps;
    }
    assert(saw_queue);
    assert(steps < 1000);

    graph_free(&graph);
    entity_free(&store);
    printf("test_backend passed\n");
    return 0;
}

