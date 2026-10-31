#ifndef ENTITY_H
#define ENTITY_H

#include "../common/model.h"
#include "../module1_graph/graph.h"

#define HASH_SIZE 101

/* 哈希链表节点，保存整数编号和对象地址。 */
typedef struct HashNode {
    int key;
    void *value;
    struct HashNode *next;
} HashNode;

/* 所有业务实体的哈希索引和编号列表。 */
typedef struct {
    HashNode *vehicles[HASH_SIZE];
    HashNode *stations[HASH_SIZE];
    HashNode *chargers[HASH_SIZE];
    HashNode *requests[HASH_SIZE];
    int vehicle_ids[MAX_VEHICLES];
    int station_ids[MAX_STATIONS];
    int charger_ids[MAX_CHARGERS];
    int request_ids[MAX_REQUESTS];
    int vehicle_count;
    int station_count;
    int charger_count;
    int request_count;
} EntityStore;

/* 初始化空的实体仓库。 */
void entity_init(EntityStore *store);

/* 创建并添加一辆车辆。 */
int entity_add_vehicle(EntityStore *store, const Graph *graph,
                       int id, int node_id,
                       double battery_percent, double target_percent,
                       double capacity_kwh, int urgency);

/* 根据编号查找车辆。 */
Vehicle *entity_find_vehicle(const EntityStore *store, int id);

/* 修改未参与当前任务的车辆基础数据。 */
int entity_update_vehicle(EntityStore *store, const Graph *graph,
                          int id, int node_id,
                          double battery_percent, double target_percent,
                          double capacity_kwh, int urgency);

/* 删除未参与当前任务的车辆。 */
int entity_remove_vehicle(EntityStore *store, int id);

/* 创建并添加一个充电站。 */
int entity_add_station(EntityStore *store, const Graph *graph,
                       int id, int node_id,
                       const char *name);

/* 根据编号查找充电站。 */
Station *entity_find_station(const EntityStore *store, int id);

/* 修改当前没有业务任务的充电站。 */
int entity_update_station(EntityStore *store, const Graph *graph,
                          int id, int node_id,
                          const char *name);

/* 删除不含充电桩和等待车辆的站点。 */
int entity_remove_station(EntityStore *store, int id);

/* 删除没有被车辆或充电站引用的道路节点。 */
int entity_remove_graph_node(Graph *graph, const EntityStore *store,
                             int node_id);

/* 把充电桩加入全局哈希索引。 */
int entity_register_charger(EntityStore *store, Charger *charger);

/* 从全局哈希索引移除充电桩。 */
int entity_unregister_charger(EntityStore *store, int charger_id);

/* 根据编号查找充电桩。 */
Charger *entity_find_charger(const EntityStore *store, int id);

/* 把充电请求加入全局哈希索引。 */
int entity_register_request(EntityStore *store, ChargeRequest *request);

/* 根据编号查找充电请求。 */
ChargeRequest *entity_find_request(const EntityStore *store, int id);

/* 从全局哈希索引移除请求但不释放请求。 */
int entity_unregister_request(EntityStore *store, int request_id);

/* 释放仓库拥有的所有实体和索引节点。 */
void entity_free(EntityStore *store);

#endif

