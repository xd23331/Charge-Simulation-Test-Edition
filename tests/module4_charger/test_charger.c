#include "../../src/module4_charger/charger.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* 测试充电桩的添加、修改、分配、完成和删除。 */
int main(void)
{
    EntityStore store;
    Graph graph;
    Vehicle *vehicle;
    Charger *charger;
    ChargeRequest *request;

    graph_init(&graph);
    entity_init(&store);
    assert(graph_add_node(&graph, 1, 0, 0) == 0);
    assert(entity_add_station(&store, &graph, 101, 1, "Station") == 0);
    assert(entity_add_vehicle(&store, &graph, 201, 1,
                              20, 80, 60, 2) == 0);
    assert(charger_add(&store, 101, 1001, 60, CHARGER_FREE) == 0);

    charger = entity_find_charger(&store, 1001);
    vehicle = entity_find_vehicle(&store, 201);
    assert(charger != NULL && vehicle != NULL);
    assert(charger_set_power(charger, 80) == 0);
    assert(charger_set_state(charger, CHARGER_FAULT) == 0);
    assert(charger_find_free(entity_find_station(&store, 101)) == NULL);
    assert(charger_set_state(charger, CHARGER_FREE) == 0);

    request = (ChargeRequest *)calloc(1, sizeof(ChargeRequest));
    assert(request != NULL);
    request->id = 1;
    request->vehicle = vehicle;
    request->needed_kwh = 20;
    assert(entity_register_request(&store, request) == 0);
    vehicle->request = request;
    vehicle->state = VEHICLE_WAITING;
    assert(charger_assign(charger, request) == 0);
    assert(charger_set_power(charger, 100) != 0);
    assert(charger_update(charger, 5) == 0);
    assert(vehicle->battery_percent > 20);
    assert(charger_update(charger, 10) == 1);
    assert(charger_finish(&store, charger) == 0);
    assert(vehicle->state == VEHICLE_FINISHED);

    assert(charger_remove(&store, 101, 1001) == 0);
    entity_free(&store);
    graph_free(&graph);
    printf("test_charger passed\n");
    return 0;
}

