#ifndef SIMULATION_H
#define SIMULATION_H

#include "../module1_graph/graph.h"
#include "../module3_entity/entity.h"
#include "../module5_recommend/recommend.h"

/* 后端仿真环境，保存统一时间和业务数据地址。 */
typedef struct {
    Graph *graph;
    EntityStore *store;
    double current_time;
    double step_minutes;
    int next_request_id;
} Simulation;

/* 初始化仿真环境。 */
int simulation_init(Simulation *simulation, Graph *graph,
                    EntityStore *store, double step_minutes);

/* 根据一条推荐结果提交车辆充电请求。 */
int simulation_submit(Simulation *simulation, int vehicle_id,
                      const Recommendation *recommendation);

/* 取消仍在行驶或等待中的充电请求。 */
int simulation_cancel(Simulation *simulation, int request_id);

/* 把充电桩设为故障，正在充电的车辆重新进入等待队列。 */
int simulation_break_charger(Simulation *simulation, int charger_id);

/* 把故障充电桩恢复为空闲状态。 */
int simulation_repair_charger(Simulation *simulation, int charger_id);

/* 推进一个固定时间步。 */
int simulation_step(Simulation *simulation);

/* 清除进行中的任务并恢复初始状态。 */
void simulation_reset(Simulation *simulation);

/* 判断是否还有车辆处于行驶、等待或充电状态。 */
int simulation_has_active(const Simulation *simulation);

/* 返回仿真中的车辆数量。 */
int simulation_vehicle_count(const Simulation *simulation);

/* 按顺序返回只读车辆数据。 */
const Vehicle *simulation_vehicle_at(const Simulation *simulation, int index);

/* 返回仿真中的站点数量。 */
int simulation_station_count(const Simulation *simulation);

/* 按顺序返回只读站点数据。 */
const Station *simulation_station_at(const Simulation *simulation, int index);

/* 返回指定站点的充电桩数量。 */
int simulation_charger_count(const Simulation *simulation, int station_id);

/* 按链表顺序返回指定站点中的只读充电桩。 */
const Charger *simulation_charger_at(const Simulation *simulation,
                                     int station_id, int index);

/* 返回指定站点当前等待车辆数量。 */
int simulation_waiting_count(const Simulation *simulation, int station_id);

/* 按堆数组顺序返回指定站点中的只读等待车辆。 */
const Vehicle *simulation_waiting_vehicle_at(const Simulation *simulation,
                                             int station_id, int index);

/* 返回当前仍然有效的充电请求数量。 */
int simulation_request_count(const Simulation *simulation);

/* 按编号列表顺序返回一条只读充电请求。 */
const ChargeRequest *simulation_request_at(const Simulation *simulation,
                                           int index);

#endif

