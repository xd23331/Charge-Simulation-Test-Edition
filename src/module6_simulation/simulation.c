#include "simulation.h"

#include "../module2_scheduler/scheduler.h"
#include "../module4_charger/charger.h"

#include <stdlib.h>

/* 更新已经到达所选站点的车辆。 */
static void update_arrivals(Simulation *simulation)
{
    int i;

    for (i = 0; i < simulation->store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            simulation->store, simulation->store->vehicle_ids[i]);
        if (vehicle != NULL && vehicle->state == VEHICLE_TRAVELING &&
            vehicle->arrival_time <= simulation->current_time) {
            ChargeRequest *request = vehicle->request;
            vehicle->node_id = request->station->node_id;
            if (scheduler_push(request->station, request,
                               simulation->current_time) == 0) {
                request->state = REQUEST_WAITING;
                vehicle->state = VEHICLE_WAITING;
            }
        }
    }
}

/* 更新所有正在工作的充电桩。 */
static void update_charging(Simulation *simulation)
{
    int i;

    for (i = 0; i < simulation->store->station_count; ++i) {
        Station *station = entity_find_station(
            simulation->store, simulation->store->station_ids[i]);
        Charger *charger = station == NULL ? NULL : station->chargers;

        while (charger != NULL) {
            if (charger_update(charger, simulation->step_minutes)) {
                charger_finish(simulation->store, charger);
            }
            charger = charger->next;
        }
    }
}

/* 为一个站点的空闲充电桩分配等待车辆。 */
static void assign_station(Simulation *simulation, Station *station)
{
    Charger *charger;

    scheduler_rebuild(station, simulation->current_time);
    charger = charger_find_free(station);
    while (charger != NULL && station->queue_size > 0) {
        ChargeRequest *request = scheduler_pop(station);
        if (charger_assign(charger, request) != 0) {
            scheduler_push(station, request, simulation->current_time);
            break;
        }
        charger = charger_find_free(station);
    }
}

/* 为所有站点执行一次等待车辆分配。 */
static void assign_waiting(Simulation *simulation)
{
    int i;

    for (i = 0; i < simulation->store->station_count; ++i) {
        Station *station = entity_find_station(
            simulation->store, simulation->store->station_ids[i]);
        if (station != NULL) {
            assign_station(simulation, station);
        }
    }
}

/* 初始化仿真环境。 */
int simulation_init(Simulation *simulation, Graph *graph,
                    EntityStore *store, double step_minutes)
{
    if (simulation == NULL || graph == NULL || store == NULL ||
        step_minutes <= 0.0) {
        return -1;
    }
    simulation->graph = graph;
    simulation->store = store;
    simulation->current_time = 0.0;
    simulation->step_minutes = step_minutes;
    simulation->next_request_id = 1;
    return 0;
}

/* 根据一条推荐结果提交车辆充电请求。 */
int simulation_submit(Simulation *simulation, int vehicle_id,
                      const Recommendation *recommendation)
{
    Vehicle *vehicle;
    Station *station;
    ChargeRequest *request;
    double current_energy;
    double arrival_energy;
    double target_energy;

    if (simulation == NULL || recommendation == NULL ||
        !recommendation->valid) {
        return -1;
    }
    vehicle = entity_find_vehicle(simulation->store, vehicle_id);
    station = entity_find_station(simulation->store,
                                  recommendation->station_id);
    if (vehicle == NULL || station == NULL ||
        (vehicle->state != VEHICLE_IDLE &&
         vehicle->state != VEHICLE_FINISHED)) {
        return -1;
    }

    current_energy = vehicle->capacity_kwh
                   * vehicle->battery_percent / 100.0;
    arrival_energy = current_energy - recommendation->travel_energy_kwh;
    if (arrival_energy < 0.0) {
        return -1;
    }

    request = (ChargeRequest *)calloc(1, sizeof(ChargeRequest));
    if (request == NULL) {
        return -1;
    }
    target_energy = vehicle->capacity_kwh
                  * vehicle->target_percent / 100.0;
    request->id = simulation->next_request_id++;
    request->vehicle = vehicle;
    request->station = station;
    request->request_time = simulation->current_time;
    request->arrival_time = simulation->current_time
                          + recommendation->travel_minutes;
    request->needed_kwh = target_energy - arrival_energy;
    if (request->needed_kwh < 0.0) {
        request->needed_kwh = 0.0;
    }
    request->sequence = request->id;
    request->state = REQUEST_TRAVELING;
    request->travel_energy_kwh = recommendation->travel_energy_kwh;

    if (entity_register_request(simulation->store, request) != 0) {
        free(request);
        return -1;
    }

    vehicle->battery_percent = arrival_energy / vehicle->capacity_kwh * 100.0;
    vehicle->state = VEHICLE_TRAVELING;
    vehicle->station_id = station->id;
    vehicle->arrival_time = request->arrival_time;
    vehicle->request = request;
    return 0;
}

/* 取消仍在行驶或等待中的充电请求。 */
int simulation_cancel(Simulation *simulation, int request_id)
{
    ChargeRequest *request;
    Vehicle *vehicle;

    if (simulation == NULL || simulation->store == NULL) {
        return -1;
    }
    request = entity_find_request(simulation->store, request_id);
    if (request == NULL || request->state == REQUEST_CHARGING) {
        return -1;
    }
    vehicle = request->vehicle;
    if (request->state == REQUEST_WAITING &&
        scheduler_remove(request->station, request_id,
                         simulation->current_time) == NULL) {
        return -1;
    }
    if (request->state == REQUEST_TRAVELING) {
        vehicle->battery_percent += request->travel_energy_kwh
                                  / vehicle->capacity_kwh * 100.0;
        if (vehicle->battery_percent > 100.0) {
            vehicle->battery_percent = 100.0;
        }
    }

    entity_unregister_request(simulation->store, request_id);
    vehicle->request = NULL;
    vehicle->state = VEHICLE_IDLE;
    vehicle->station_id = -1;
    vehicle->charger_id = -1;
    vehicle->arrival_time = 0.0;
    free(request);
    return 0;
}

/* 把充电桩设为故障，正在充电的车辆重新进入等待队列。 */
int simulation_break_charger(Simulation *simulation, int charger_id)
{
    Charger *charger;
    ChargeRequest *request;

    if (simulation == NULL || simulation->store == NULL) {
        return -1;
    }
    charger = entity_find_charger(simulation->store, charger_id);
    if (charger == NULL || charger->state == CHARGER_FAULT) {
        return -1;
    }
    if (charger->state == CHARGER_FREE) {
        charger->state = CHARGER_FAULT;
        return 0;
    }

    request = charger->request;
    if (request == NULL || request->station->queue_size >= MAX_VEHICLES) {
        return -1;
    }
    charger->request = NULL;
    charger->remaining_minutes = 0.0;
    charger->state = CHARGER_FAULT;
    request->state = REQUEST_WAITING;
    request->vehicle->state = VEHICLE_WAITING;
    request->vehicle->charger_id = -1;
    if (scheduler_push(request->station, request,
                       simulation->current_time) != 0) {
        return -1;
    }
    return 0;
}

/* 把故障充电桩恢复为空闲状态。 */
int simulation_repair_charger(Simulation *simulation, int charger_id)
{
    Charger *charger;

    if (simulation == NULL || simulation->store == NULL) {
        return -1;
    }
    charger = entity_find_charger(simulation->store, charger_id);
    if (charger == NULL || charger->state != CHARGER_FAULT) {
        return -1;
    }
    charger->state = CHARGER_FREE;
    return 0;
}

/* 推进一个固定时间步。 */
int simulation_step(Simulation *simulation)
{
    if (simulation == NULL || simulation->store == NULL) {
        return -1;
    }
    simulation->current_time += simulation->step_minutes;
    update_arrivals(simulation);
    update_charging(simulation);
    assign_waiting(simulation);
    return 0;
}

/* 清除进行中的任务并恢复初始状态。 */
void simulation_reset(Simulation *simulation)
{
    int i;

    if (simulation == NULL || simulation->store == NULL) {
        return;
    }
    for (i = 0; i < simulation->store->station_count; ++i) {
        Station *station = entity_find_station(
            simulation->store, simulation->store->station_ids[i]);
        Charger *charger = station == NULL ? NULL : station->chargers;
        if (station != NULL) {
            station->queue_size = 0;
        }
        while (charger != NULL) {
            charger->request = NULL;
            charger->remaining_minutes = 0.0;
            charger->state = charger->initial_state;
            charger = charger->next;
        }
    }
    for (i = 0; i < simulation->store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            simulation->store, simulation->store->vehicle_ids[i]);
        if (vehicle != NULL) {
            if (vehicle->request != NULL) {
                entity_unregister_request(simulation->store,
                                          vehicle->request->id);
            }
            free(vehicle->request);
            vehicle->request = NULL;
            vehicle->node_id = vehicle->initial_node_id;
            vehicle->battery_percent = vehicle->initial_battery_percent;
            vehicle->state = VEHICLE_IDLE;
            vehicle->station_id = -1;
            vehicle->charger_id = -1;
            vehicle->arrival_time = 0.0;
        }
    }
    simulation->current_time = 0.0;
    simulation->next_request_id = 1;
}

/* 判断是否还有车辆处于行驶、等待或充电状态。 */
int simulation_has_active(const Simulation *simulation)
{
    int i;

    if (simulation == NULL || simulation->store == NULL) {
        return 0;
    }
    for (i = 0; i < simulation->store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            simulation->store, simulation->store->vehicle_ids[i]);
        if (vehicle != NULL &&
            (vehicle->state == VEHICLE_TRAVELING ||
             vehicle->state == VEHICLE_WAITING ||
             vehicle->state == VEHICLE_CHARGING)) {
            return 1;
        }
    }
    return 0;
}

/* 返回仿真中的车辆数量。 */
int simulation_vehicle_count(const Simulation *simulation)
{
    return simulation == NULL || simulation->store == NULL
         ? 0 : simulation->store->vehicle_count;
}

/* 按顺序返回只读车辆数据。 */
const Vehicle *simulation_vehicle_at(const Simulation *simulation, int index)
{
    if (simulation == NULL || simulation->store == NULL || index < 0 ||
        index >= simulation->store->vehicle_count) {
        return NULL;
    }
    return entity_find_vehicle(simulation->store,
                               simulation->store->vehicle_ids[index]);
}

/* 返回仿真中的站点数量。 */
int simulation_station_count(const Simulation *simulation)
{
    return simulation == NULL || simulation->store == NULL
         ? 0 : simulation->store->station_count;
}

/* 按顺序返回只读站点数据。 */
const Station *simulation_station_at(const Simulation *simulation, int index)
{
    if (simulation == NULL || simulation->store == NULL || index < 0 ||
        index >= simulation->store->station_count) {
        return NULL;
    }
    return entity_find_station(simulation->store,
                               simulation->store->station_ids[index]);
}

/* 返回指定站点的充电桩数量。 */
int simulation_charger_count(const Simulation *simulation, int station_id)
{
    const Station *station;
    const Charger *charger;
    int count = 0;

    if (simulation == NULL || simulation->store == NULL) {
        return 0;
    }
    station = entity_find_station(simulation->store, station_id);
    if (station == NULL) {
        return 0;
    }
    charger = station->chargers;
    while (charger != NULL) {
        ++count;
        charger = charger->next;
    }
    return count;
}

/* 按链表顺序返回指定站点中的只读充电桩。 */
const Charger *simulation_charger_at(const Simulation *simulation,
                                     int station_id, int index)
{
    const Station *station;
    const Charger *charger;

    if (simulation == NULL || simulation->store == NULL || index < 0) {
        return NULL;
    }
    station = entity_find_station(simulation->store, station_id);
    if (station == NULL) {
        return NULL;
    }
    charger = station->chargers;
    while (charger != NULL && index > 0) {
        charger = charger->next;
        --index;
    }
    return charger;
}

/* 返回指定站点当前等待车辆数量。 */
int simulation_waiting_count(const Simulation *simulation, int station_id)
{
    const Station *station;

    if (simulation == NULL || simulation->store == NULL) {
        return 0;
    }
    station = entity_find_station(simulation->store, station_id);
    return station == NULL ? 0 : station->queue_size;
}

/* 按堆数组顺序返回指定站点中的只读等待车辆。 */
const Vehicle *simulation_waiting_vehicle_at(const Simulation *simulation,
                                             int station_id, int index)
{
    const Station *station;

    if (simulation == NULL || simulation->store == NULL || index < 0) {
        return NULL;
    }
    station = entity_find_station(simulation->store, station_id);
    if (station == NULL || index >= station->queue_size ||
        station->queue[index] == NULL) {
        return NULL;
    }
    return station->queue[index]->vehicle;
}

/* 返回当前仍然有效的充电请求数量。 */
int simulation_request_count(const Simulation *simulation)
{
    return simulation == NULL || simulation->store == NULL
         ? 0 : simulation->store->request_count;
}

/* 按编号列表顺序返回一条只读充电请求。 */
const ChargeRequest *simulation_request_at(const Simulation *simulation,
                                           int index)
{
    if (simulation == NULL || simulation->store == NULL || index < 0 ||
        index >= simulation->store->request_count) {
        return NULL;
    }
    return entity_find_request(
        simulation->store, simulation->store->request_ids[index]);
}

