#ifndef RECOMMEND_H
#define RECOMMEND_H

#include "../module1_graph/graph.h"
#include "../module2_scheduler/scheduler.h"
#include "../module3_entity/entity.h"

#define ENERGY_PER_KM 0.18

/* 单个充电站的推荐计算结果。 */
typedef struct {
    int station_id;
    int valid;
    char reason[NAME_LEN];
    PathResult path;
    double travel_minutes;
    double wait_minutes;
    double charge_minutes;
    double total_minutes;
    double travel_energy_kwh;
    double arrival_battery_percent;
    double score;
} Recommendation;

/* 全部候选站点的推荐结果列表。 */
typedef struct {
    Recommendation items[MAX_STATIONS];
    int count;
} RecommendationList;

/* 计算一辆车到所有充电站的推荐结果。 */
int recommend_all(const Graph *graph, const EntityStore *store,
                  const Vehicle *vehicle, RecommendationList *list);

/* 返回列表中排名最高的有效推荐，没有结果时返回NULL。 */
const Recommendation *recommend_best(const RecommendationList *list);

#endif

