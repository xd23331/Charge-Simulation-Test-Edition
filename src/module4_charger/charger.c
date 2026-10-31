#include "charger.h"

#include <stdlib.h>

/* 在指定站点添加一个充电桩。 */
int charger_add(EntityStore *store, int station_id, int charger_id,
                double power_kw, ChargerState state)
{
    Station *station;
    Charger *charger;

    if (store == NULL || power_kw <= 0.0 ||
        (state != CHARGER_FREE && state != CHARGER_FAULT)) {
        return -1;
    }
    station = entity_find_station(store, station_id);
    if (station == NULL) {
        return -1;
    }

    charger = (Charger *)calloc(1, sizeof(Charger));
    if (charger == NULL) {
        return -1;
    }
    charger->id = charger_id;
    charger->power_kw = power_kw;
    charger->initial_state = state;
    charger->state = state;

    if (entity_register_charger(store, charger) != 0) {
        free(charger);
        return -1;
    }
    charger->next = station->chargers;
    station->chargers = charger;
    return 0;
}

/* 从指定站点删除一个未使用的充电桩。 */
int charger_remove(EntityStore *store, int station_id, int charger_id)
{
    Station *station;
    Charger *charger;
    Charger *previous = NULL;

    if (store == NULL) {
        return -1;
    }
    station = entity_find_station(store, station_id);
    if (station == NULL) {
        return -1;
    }

    charger = station->chargers;
    while (charger != NULL && charger->id != charger_id) {
        previous = charger;
        charger = charger->next;
    }
    if (charger == NULL || charger->state == CHARGER_BUSY) {
        return -1;
    }

    if (previous == NULL) {
        station->chargers = charger->next;
    } else {
        previous->next = charger->next;
    }
    entity_unregister_charger(store, charger_id);
    free(charger);
    return 0;
}

/* 查找站点中的第一个空闲充电桩。 */
Charger *charger_find_free(Station *station)
{
    Charger *charger;

    if (station == NULL) {
        return NULL;
    }
    charger = station->chargers;
    while (charger != NULL) {
        if (charger->state == CHARGER_FREE) {
            return charger;
        }
        charger = charger->next;
    }
    return NULL;
}

/* 修改充电桩的基础状态，并将其作为以后重置时的初始状态。 */
int charger_set_state(Charger *charger, ChargerState state)
{
    if (charger == NULL || charger->state == CHARGER_BUSY ||
        (state != CHARGER_FREE && state != CHARGER_FAULT)) {
        return -1;
    }
    charger->initial_state = state;
    charger->state = state;
    return 0;
}

/* 修改未在工作的充电桩功率。 */
int charger_set_power(Charger *charger, double power_kw)
{
    if (charger == NULL || charger->state == CHARGER_BUSY || power_kw <= 0.0) {
        return -1;
    }
    charger->power_kw = power_kw;
    return 0;
}

/* 把一条充电请求分配给空闲充电桩。 */
int charger_assign(Charger *charger, ChargeRequest *request)
{
    if (charger == NULL || request == NULL || request->vehicle == NULL ||
        charger->state != CHARGER_FREE || charger->power_kw <= 0.0) {
        return -1;
    }
    charger->request = request;
    charger->remaining_minutes = request->needed_kwh / charger->power_kw * 60.0;
    charger->state = CHARGER_BUSY;
    request->state = REQUEST_CHARGING;
    request->vehicle->state = VEHICLE_CHARGING;
    request->vehicle->charger_id = charger->id;
    return 0;
}

/* 推进充电进度，完成时返回1。 */
int charger_update(Charger *charger, double minutes)
{
    ChargeRequest *request;
    Vehicle *vehicle;
    double charged_kwh;

    if (charger == NULL || charger->state != CHARGER_BUSY ||
        charger->request == NULL || minutes < 0.0) {
        return 0;
    }
    request = charger->request;
    vehicle = request->vehicle;
    charged_kwh = charger->power_kw * minutes / 60.0;
    if (charged_kwh > request->needed_kwh) {
        charged_kwh = request->needed_kwh;
    }

    request->needed_kwh -= charged_kwh;
    vehicle->battery_percent += charged_kwh / vehicle->capacity_kwh * 100.0;
    if (vehicle->battery_percent > vehicle->target_percent) {
        vehicle->battery_percent = vehicle->target_percent;
    }
    charger->remaining_minutes = request->needed_kwh
                               / charger->power_kw * 60.0;
    return request->needed_kwh <= 0.000001;
}

/* 完成当前任务、移除请求并释放充电桩。 */
int charger_finish(EntityStore *store, Charger *charger)
{
    ChargeRequest *request;
    Vehicle *vehicle;

    if (store == NULL || charger == NULL || charger->state != CHARGER_BUSY ||
        charger->request == NULL) {
        return -1;
    }
    request = charger->request;
    vehicle = request->vehicle;
    vehicle->battery_percent = vehicle->target_percent;
    vehicle->state = VEHICLE_FINISHED;
    vehicle->charger_id = -1;
    vehicle->request = NULL;

    entity_unregister_request(store, request->id);
    free(request);
    charger->request = NULL;
    charger->remaining_minutes = 0.0;
    charger->state = CHARGER_FREE;
    return 0;
}

