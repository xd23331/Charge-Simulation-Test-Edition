#include "../../src/module1_graph/graph.h"
#include "../../src/module3_entity/entity.h"
#include "../../src/module4_charger/charger.h"
#include "../../src/module5_recommend/recommend.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 测试推荐结果会考虑前往同一站点的车辆负载。 */
int main(void)
{
    Graph graph;
    EntityStore store;
    Vehicle *vehicle;
    Vehicle *incoming;
    Station *busy_station;
    ChargeRequest *request;
    RecommendationList list;

    graph_init(&graph);
    entity_init(&store);
    assert(graph_add_node(&graph, 1, 0, 0) == 0);
    assert(graph_add_node(&graph, 2, 1, 0) == 0);
    assert(graph_add_node(&graph, 3, 2, 0) == 0);
    assert(graph_add_edge(&graph, 1, 2, 2, 5) == 0);
    assert(graph_add_edge(&graph, 1, 3, 3, 8) == 0);
    assert(entity_add_station(&store, &graph, 10, 2, "Near") == 0);
    assert(entity_add_station(&store, &graph, 20, 3, "Far") == 0);
    assert(charger_add(&store, 10, 100, 60, CHARGER_FREE) == 0);
    assert(charger_add(&store, 20, 200, 60, CHARGER_FREE) == 0);
    assert(entity_add_vehicle(&store, &graph, 1, 1,
                              50, 80, 60, 1) == 0);
    assert(entity_add_vehicle(&store, &graph, 2, 1,
                              20, 90, 60, 2) == 0);

    vehicle = entity_find_vehicle(&store, 1);
    assert(recommend_all(&graph, &store, vehicle, &list) == 0);
    assert(recommend_best(&list)->station_id == 10);

    incoming = entity_find_vehicle(&store, 2);
    busy_station = entity_find_station(&store, 10);
    request = (ChargeRequest *)calloc(1, sizeof(ChargeRequest));
    assert(request != NULL);
    request->vehicle = incoming;
    request->station = busy_station;
    request->needed_kwh = 50;
    incoming->request = request;
    incoming->station_id = 10;
    incoming->state = VEHICLE_TRAVELING;

    assert(recommend_all(&graph, &store, vehicle, &list) == 0);
    assert(recommend_best(&list)->station_id == 20);

    assert(entity_update_vehicle(&store, &graph, 1, 1,
                                 0.1, 80, 60, 1) == 0);
    assert(recommend_all(&graph, &store, vehicle, &list) == 0);
    assert(recommend_best(&list) == NULL);

    graph_free(&graph);
    entity_free(&store);
    printf("test_recommend passed\n");
    return 0;
}

