#include "recommend.h"

#include <stdio.h>
#include <string.h>

/* 统计站点可用充电桩数量和平均功率。 */
static int station_average_power(const Station *station, double *average_power)
{
    const Charger *charger = station->chargers;
    double total = 0.0;
    int count = 0;

    while (charger != NULL) {
        if (charger->state != CHARGER_FAULT) {
            total += charger->power_kw;
            ++count;
        }
        charger = charger->next;
    }
    *average_power = count == 0 ? 0.0 : total / count;
    return count;
}

/* 填写一个不可推荐站点的原因。 */
static void set_invalid(Recommendation *item, const char *reason)
{
    item->valid = 0;
    item->score = 1e30;
    snprintf(item->reason, NAME_LEN, "%s", reason);
}

/* 计算车辆到达站点后的充电需求。 */
static double needed_energy(const Vehicle *vehicle, double arrival_energy)
{
    double target_energy = vehicle->capacity_kwh
                         * vehicle->target_percent / 100.0;
    double needed = target_energy - arrival_energy;
    return needed > 0.0 ? needed : 0.0;
}

/* 计算到站电量较低时使用的简单风险分。 */
static double battery_risk(double arrival_percent)
{
    if (arrival_percent >= 15.0) {
        return 0.0;
    }
    return (15.0 - arrival_percent) * 2.0;
}

/* 估算已经前往同一站点车辆带来的额外等待时间。 */
static double incoming_wait(const EntityStore *store, const Station *station,
                            double average_power, int charger_count)
{
    double total_minutes = 0.0;
    int i;

    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL && vehicle->state == VEHICLE_TRAVELING &&
            vehicle->station_id == station->id && vehicle->request != NULL) {
            total_minutes += vehicle->request->needed_kwh
                           / average_power * 60.0;
        }
    }
    return total_minutes / charger_count;
}

/* 计算一个候选站点的推荐结果。 */
static void evaluate_station(const Graph *graph, const EntityStore *store,
                             const Vehicle *vehicle,
                             Station *station,
                             const DijkstraResult *dijkstra,
                             Recommendation *item)
{
    double current_energy;
    double arrival_energy;
    double average_power;
    double needed;
    int charger_count;

    memset(item, 0, sizeof(Recommendation));
    item->station_id = station->id;

    if (graph_path_from_dijkstra(graph, dijkstra, station->node_id,
                                 &item->path) != 0 || !item->path.reachable) {
        set_invalid(item, "road unreachable");
        return;
    }
    charger_count = station_average_power(station, &average_power);
    if (charger_count == 0) {
        set_invalid(item, "no working charger");
        return;
    }

    item->travel_energy_kwh = graph_calc_energy(
        item->path.total_distance_km, ENERGY_PER_KM);
    current_energy = vehicle->capacity_kwh
                   * vehicle->battery_percent / 100.0;
    arrival_energy = current_energy - item->travel_energy_kwh;
    if (arrival_energy < 0.0) {
        set_invalid(item, "battery not enough");
        return;
    }

    needed = needed_energy(vehicle, arrival_energy);
    item->wait_minutes = scheduler_estimate_wait(station, needed);
    if (item->wait_minutes < 0.0) {
        set_invalid(item, "station unavailable");
        return;
    }
    if (item->wait_minutes > item->path.total_minutes) {
        item->wait_minutes -= item->path.total_minutes;
    } else {
        item->wait_minutes = 0.0;
    }
    item->wait_minutes += incoming_wait(store, station, average_power,
                                        charger_count);

    item->valid = 1;
    item->travel_minutes = item->path.total_minutes;
    item->charge_minutes = needed / average_power * 60.0;
    item->arrival_battery_percent = arrival_energy
                                  / vehicle->capacity_kwh * 100.0;
    item->total_minutes = item->travel_minutes
                        + item->wait_minutes
                        + item->charge_minutes;
    item->score = item->total_minutes
                + battery_risk(item->arrival_battery_percent);
    snprintf(item->reason, NAME_LEN, "total time %.1f min",
             item->total_minutes);
}

/* 判断推荐a是否应该排在推荐b之前。 */
static int recommendation_before(const Recommendation *a,
                                 const Recommendation *b)
{
    if (a->valid != b->valid) {
        return a->valid > b->valid;
    }
    if (a->score != b->score) {
        return a->score < b->score;
    }
    return a->station_id < b->station_id;
}

/* 使用插入排序整理数量较少的推荐结果。 */
static void sort_recommendations(RecommendationList *list)
{
    int i;

    for (i = 1; i < list->count; ++i) {
        Recommendation value = list->items[i];
        int j = i - 1;
        while (j >= 0 && recommendation_before(&value, &list->items[j])) {
            list->items[j + 1] = list->items[j];
            --j;
        }
        list->items[j + 1] = value;
    }
}

/* 计算一辆车到所有充电站的推荐结果。 */
int recommend_all(const Graph *graph, const EntityStore *store,
                  const Vehicle *vehicle, RecommendationList *list)
{
    DijkstraResult dijkstra;
    int i;

    if (graph == NULL || store == NULL || vehicle == NULL || list == NULL) {
        return -1;
    }
    if (graph_dijkstra_all(graph, vehicle->node_id, &dijkstra) != 0) {
        return -1;
    }
    list->count = 0;
    for (i = 0; i < store->station_count; ++i) {
        Station *station = entity_find_station(store, store->station_ids[i]);
        if (station != NULL) {
            evaluate_station(graph, store, vehicle, station, &dijkstra,
                             &list->items[list->count++]);
        }
    }
    sort_recommendations(list);
    return 0;
}

/* 返回列表中排名最高的有效推荐，没有结果时返回NULL。 */
const Recommendation *recommend_best(const RecommendationList *list)
{
    if (list == NULL || list->count == 0 || !list->items[0].valid) {
        return NULL;
    }
    return &list->items[0];
}

