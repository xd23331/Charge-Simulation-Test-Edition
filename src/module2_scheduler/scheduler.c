#include "scheduler.h"

#include <stddef.h>

/* 判断请求a是否应排在请求b之前。 */
static int request_before(const ChargeRequest *a, const ChargeRequest *b)
{
    if (a->priority > b->priority) {
        return 1;
    }
    if (a->priority < b->priority) {
        return 0;
    }
    if (a->arrival_time < b->arrival_time) {
        return 1;
    }
    if (a->arrival_time > b->arrival_time) {
        return 0;
    }
    return a->sequence < b->sequence;
}

/* 交换最大堆中的两个请求指针。 */
static void request_swap(ChargeRequest **a, ChargeRequest **b)
{
    ChargeRequest *temp = *a;
    *a = *b;
    *b = temp;
}

/* 让指定元素向堆顶移动到正确位置。 */
static void sift_up(Station *station, int index)
{
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (request_before(station->queue[parent], station->queue[index])) {
            break;
        }
        request_swap(&station->queue[parent], &station->queue[index]);
        index = parent;
    }
}

/* 让指定元素向堆底移动到正确位置。 */
static void sift_down(Station *station, int index)
{
    while (1) {
        int left = index * 2 + 1;
        int right = left + 1;
        int best = index;

        if (left < station->queue_size &&
            request_before(station->queue[left], station->queue[best])) {
            best = left;
        }
        if (right < station->queue_size &&
            request_before(station->queue[right], station->queue[best])) {
            best = right;
        }
        if (best == index) {
            break;
        }
        request_swap(&station->queue[index], &station->queue[best]);
        index = best;
    }
}

/* 收集各可用充电桩的功率和剩余工作时间。 */
static int collect_charger_loads(const Station *station, double *loads,
                                 double *powers)
{
    const Charger *charger = station->chargers;
    int count = 0;

    while (charger != NULL) {
        if (charger->state != CHARGER_FAULT) {
            powers[count] = charger->power_kw;
            loads[count] = charger->state == CHARGER_BUSY
                         ? charger->remaining_minutes : 0.0;
            ++count;
        }
        charger = charger->next;
    }
    return count;
}

/* 查找当前累计工作时间最短的充电桩。 */
static int find_lightest_charger(const double *loads, int count)
{
    int best = 0;
    int i;

    for (i = 1; i < count; ++i) {
        if (loads[i] < loads[best]) {
            best = i;
        }
    }
    return best;
}

/* 计算一条充电请求在当前时刻的动态优先级。 */
double scheduler_calc_priority(ChargeRequest *request, double current_time)
{
    double low_battery;

    if (request == NULL || request->vehicle == NULL) {
        return 0.0;
    }
    request->wait_time = current_time - request->arrival_time;
    if (request->wait_time < 0.0) {
        request->wait_time = 0.0;
    }
    low_battery = 100.0 - request->vehicle->battery_percent;
    request->priority = request->vehicle->urgency * 30.0
                      + low_battery * 0.5
                      + request->wait_time * 0.2;
    return request->priority;
}

/* 把一条请求加入站点的最大堆等待队列。 */
int scheduler_push(Station *station, ChargeRequest *request, double current_time)
{
    int index;

    if (station == NULL || request == NULL ||
        station->queue_size >= MAX_VEHICLES) {
        return -1;
    }
    scheduler_calc_priority(request, current_time);
    index = station->queue_size++;
    station->queue[index] = request;
    sift_up(station, index);
    return 0;
}

/* 取出站点中当前优先级最高的请求。 */
ChargeRequest *scheduler_pop(Station *station)
{
    ChargeRequest *request;

    if (station == NULL || station->queue_size == 0) {
        return NULL;
    }
    request = station->queue[0];
    station->queue[0] = station->queue[--station->queue_size];
    if (station->queue_size > 0) {
        sift_down(station, 0);
    }
    return request;
}

/* 按请求编号从等待队列中移除请求。 */
ChargeRequest *scheduler_remove(Station *station, int request_id,
                                double current_time)
{
    int i;

    if (station == NULL) {
        return NULL;
    }
    for (i = 0; i < station->queue_size; ++i) {
        if (station->queue[i]->id == request_id) {
            ChargeRequest *request = station->queue[i];
            station->queue[i] = station->queue[--station->queue_size];
            scheduler_rebuild(station, current_time);
            return request;
        }
    }
    return NULL;
}

/* 重新计算队列中所有请求的优先级并重建最大堆。 */
void scheduler_rebuild(Station *station, double current_time)
{
    int i;

    if (station == NULL) {
        return;
    }
    for (i = 0; i < station->queue_size; ++i) {
        scheduler_calc_priority(station->queue[i], current_time);
    }
    for (i = station->queue_size / 2 - 1; i >= 0; --i) {
        sift_down(station, i);
    }
}

/* 根据当前任务和等待车辆估算新车辆的等待时间。 */
double scheduler_estimate_wait(const Station *station, double needed_kwh)
{
    double loads[MAX_CHARGERS];
    double powers[MAX_CHARGERS];
    int charger_count;
    int i;

    if (station == NULL || needed_kwh < 0.0) {
        return -1.0;
    }
    charger_count = collect_charger_loads(station, loads, powers);
    if (charger_count == 0) {
        return -1.0;
    }

    for (i = 0; i < station->queue_size; ++i) {
        int best = find_lightest_charger(loads, charger_count);
        loads[best] += station->queue[i]->needed_kwh / powers[best] * 60.0;
    }

    return loads[find_lightest_charger(loads, charger_count)];
}

