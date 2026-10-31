#include "../module1_graph/graph.h"
#include "../module3_entity/entity.h"
#include "../module5_recommend/recommend.h"
#include "../module6_simulation/simulation.h"
#include "../module7_fileio/fileio.h"

#include <stdio.h>

/* 把车辆状态转换为便于阅读的文字。 */
static const char *vehicle_state_name(VehicleState state)
{
    switch (state) {
        case VEHICLE_IDLE: return "idle";
        case VEHICLE_TRAVELING: return "traveling";
        case VEHICLE_WAITING: return "waiting";
        case VEHICLE_CHARGING: return "charging";
        case VEHICLE_FINISHED: return "finished";
        default: return "unknown";
    }
}

/* 输出一辆车的全部站点推荐结果。 */
static void print_recommendations(const Vehicle *vehicle,
                                  const RecommendationList *list)
{
    int i;

    printf("\nVehicle %d recommendations:\n", vehicle->id);
    for (i = 0; i < list->count; ++i) {
        const Recommendation *item = &list->items[i];
        if (item->valid) {
            printf("  Station %d: travel %.1f, wait %.1f, charge %.1f, total %.1f\n",
                   item->station_id, item->travel_minutes,
                   item->wait_minutes, item->charge_minutes,
                   item->total_minutes);
        } else {
            printf("  Station %d: unavailable (%s)\n",
                   item->station_id, item->reason);
        }
    }
}

/* 为所有空闲车辆选择排名第一的站点并提交请求。 */
static void submit_demo_requests(Simulation *simulation)
{
    int i;

    for (i = 0; i < simulation->store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            simulation->store, simulation->store->vehicle_ids[i]);
        RecommendationList list;
        const Recommendation *best;

        if (vehicle == NULL || vehicle->state != VEHICLE_IDLE) {
            continue;
        }
        recommend_all(simulation->graph, simulation->store, vehicle, &list);
        print_recommendations(vehicle, &list);
        best = recommend_best(&list);
        if (best != NULL) {
            simulation_submit(simulation, vehicle->id, best);
            printf("  Selected station %d\n", best->station_id);
        }
    }
}

/* 输出当前仿真时间和车辆状态。 */
static void print_status(const Simulation *simulation)
{
    int i;

    printf("\nTime %.0f minutes\n", simulation->current_time);
    for (i = 0; i < simulation->store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            simulation->store, simulation->store->vehicle_ids[i]);
        if (vehicle != NULL) {
            printf("  Vehicle %d: %-10s battery %.1f%%\n",
                   vehicle->id, vehicle_state_name(vehicle->state),
                   vehicle->battery_percent);
        }
    }
}

/* 程序入口：加载样例数据并运行一次后端仿真。 */
int main(void)
{
    Graph graph;
    EntityStore store;
    Simulation simulation;
    LoadError error;
    DataFiles files = {
        "data/input/nodes.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };
    int steps = 0;

    graph_init(&graph);
    entity_init(&store);
    if (fileio_load_all(&graph, &store, &files, &error) != 0) {
        printf("Load failed: %s line %d, %s\n",
               error.file, error.line, error.message);
        graph_free(&graph);
        entity_free(&store);
        return 1;
    }
    if (simulation_init(&simulation, &graph, &store, 1.0) != 0) {
        graph_free(&graph);
        entity_free(&store);
        return 1;
    }

    submit_demo_requests(&simulation);
    print_status(&simulation);
    while (simulation_has_active(&simulation) && steps < 500) {
        simulation_step(&simulation);
        ++steps;
        if (steps % 10 == 0 || !simulation_has_active(&simulation)) {
            print_status(&simulation);
        }
    }

    fileio_save_vehicles(&store, "data/output/vehicles_result.csv");
    graph_free(&graph);
    entity_free(&store);
    return 0;
}

