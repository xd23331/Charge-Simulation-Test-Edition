#include "../../src/module1_graph/graph.h"
#include "../../src/module3_entity/entity.h"
#include "../../src/module7_fileio/fileio.h"

#include <assert.h>
#include <stdio.h>

/* 测试全部基础数据的加载、保存和再次加载。 */
int main(void)
{
    Graph first_graph;
    Graph second_graph;
    EntityStore first_store;
    EntityStore second_store;
    LoadError error;
    DataFiles input = {
        "data/input/nodes.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };
    DataFiles output = {
        "data/output/test_nodes.csv",
        "data/output/test_roads.csv",
        "data/output/test_stations.csv",
        "data/output/test_chargers.csv",
        "data/output/test_vehicles.csv"
    };
    DataFiles invalid = {
        "data/input/not_found.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };

    graph_init(&first_graph);
    graph_init(&second_graph);
    entity_init(&first_store);
    entity_init(&second_store);
    assert(fileio_load_all(&first_graph, &first_store, &input, &error) == 0);
    assert(fileio_load_all(&first_graph, &first_store, &invalid, &error) != 0);
    assert(first_graph.node_count == 20);
    assert(first_store.vehicle_count == 20);
    assert(fileio_save_all(&first_graph, &first_store, &output) == 0);
    assert(fileio_load_all(&second_graph, &second_store, &output, &error) == 0);
    assert(second_graph.node_count == first_graph.node_count);
    assert(second_store.vehicle_count == first_store.vehicle_count);
    assert(second_store.station_count == first_store.station_count);
    assert(second_store.charger_count == first_store.charger_count);

    graph_free(&first_graph);
    graph_free(&second_graph);
    entity_free(&first_store);
    entity_free(&second_store);
    printf("test_fileio passed\n");
    return 0;
}

