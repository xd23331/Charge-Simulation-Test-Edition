#ifndef CHARGER_H
#define CHARGER_H

#include "../module3_entity/entity.h"

/* 在指定站点添加一个充电桩。 */
int charger_add(EntityStore *store, int station_id, int charger_id,
                double power_kw, ChargerState state);

/* 从指定站点删除一个未使用的充电桩。 */
int charger_remove(EntityStore *store, int station_id, int charger_id);

/* 查找站点中的第一个空闲充电桩。 */
Charger *charger_find_free(Station *station);

/* 修改空闲充电桩的状态。 */
int charger_set_state(Charger *charger, ChargerState state);

/* 修改未在工作的充电桩功率。 */
int charger_set_power(Charger *charger, double power_kw);

/* 把一条充电请求分配给空闲充电桩。 */
int charger_assign(Charger *charger, ChargeRequest *request);

/* 推进充电进度，完成时返回1。 */
int charger_update(Charger *charger, double minutes);

/* 完成当前任务、移除请求并释放充电桩。 */
int charger_finish(EntityStore *store, Charger *charger);

#endif

