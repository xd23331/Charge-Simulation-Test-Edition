#include "../../src/module2_scheduler/scheduler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 测试最大堆能够优先返回紧急车辆。 */
int main(void)
{
    Station station;
    Vehicle normal;
    Vehicle urgent;
    ChargeRequest first;
    ChargeRequest second;
    Charger busy_charger;
    Charger free_charger;

    memset(&station, 0, sizeof(station));
    memset(&normal, 0, sizeof(normal));
    memset(&urgent, 0, sizeof(urgent));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&busy_charger, 0, sizeof(busy_charger));
    memset(&free_charger, 0, sizeof(free_charger));

    normal.id = 1;
    normal.battery_percent = 40;
    normal.urgency = 1;
    urgent.id = 2;
    urgent.battery_percent = 20;
    urgent.urgency = 3;

    first.vehicle = &normal;
    first.arrival_time = 0;
    first.sequence = 1;
    second.vehicle = &urgent;
    second.arrival_time = 1;
    second.sequence = 2;

    assert(scheduler_push(&station, &first, 5) == 0);
    assert(scheduler_push(&station, &second, 5) == 0);
    assert(scheduler_pop(&station) == &second);
    assert(scheduler_pop(&station) == &first);

    busy_charger.power_kw = 60;
    busy_charger.state = CHARGER_BUSY;
    busy_charger.remaining_minutes = 30;
    busy_charger.next = &free_charger;
    free_charger.power_kw = 40;
    free_charger.state = CHARGER_FREE;
    station.chargers = &busy_charger;
    assert(scheduler_estimate_wait(&station, 20) == 0.0);
    printf("test_scheduler passed\n");
    return 0;
}

