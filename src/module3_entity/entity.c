#include "entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 计算整数编号所在的哈希桶。 */
static int hash_index(int key)
{
    int value = key % HASH_SIZE;
    return value < 0 ? value + HASH_SIZE : value;
}

/* 在指定哈希表中查找对象。 */
static void *hash_find(HashNode *const *table, int key)
{
    HashNode *node = table[hash_index(key)];

    while (node != NULL) {
        if (node->key == key) {
            return node->value;
        }
        node = node->next;
    }
    return NULL;
}

/* 向指定哈希表插入一个对象。 */
static int hash_add(HashNode **table, int key, void *value)
{
    int index;
    HashNode *node;

    if (hash_find(table, key) != NULL) {
        return -1;
    }
    node = (HashNode *)malloc(sizeof(HashNode));
    if (node == NULL) {
        return -1;
    }
    index = hash_index(key);
    node->key = key;
    node->value = value;
    node->next = table[index];
    table[index] = node;
    return 0;
}

/* 从指定哈希表删除索引节点但不释放对象。 */
static void *hash_remove(HashNode **table, int key)
{
    int index = hash_index(key);
    HashNode *node = table[index];
    HashNode *previous = NULL;

    while (node != NULL) {
        if (node->key == key) {
            void *value = node->value;
            if (previous == NULL) {
                table[index] = node->next;
            } else {
                previous->next = node->next;
            }
            free(node);
            return value;
        }
        previous = node;
        node = node->next;
    }
    return NULL;
}

/* 从编号数组中移除一个编号。 */
static void remove_id(int *ids, int *count, int id)
{
    int i;

    for (i = 0; i < *count; ++i) {
        if (ids[i] == id) {
            ids[i] = ids[*count - 1];
            --(*count);
            return;
        }
    }
}

/* 只释放哈希表中的索引节点。 */
static void hash_clear(HashNode **table)
{
    int i;

    for (i = 0; i < HASH_SIZE; ++i) {
        HashNode *node = table[i];
        while (node != NULL) {
            HashNode *next = node->next;
            free(node);
            node = next;
        }
        table[i] = NULL;
    }
}

/* 初始化空的实体仓库。 */
void entity_init(EntityStore *store)
{
    if (store == NULL) {
        return;
    }
    memset(store, 0, sizeof(EntityStore));
}

/* 创建并添加一辆车辆。 */
int entity_add_vehicle(EntityStore *store, const Graph *graph,
                       int id, int node_id,
                       double battery_percent, double target_percent,
                       double capacity_kwh, int urgency)
{
    Vehicle *vehicle;

    if (store == NULL || graph == NULL ||
        graph_find_node(graph, node_id) < 0 ||
        store->vehicle_count >= MAX_VEHICLES || id < 0 ||
        battery_percent < 0.0 || battery_percent > 100.0 ||
        target_percent < battery_percent || target_percent > 100.0 ||
        capacity_kwh <= 0.0 || urgency < 1 || urgency > 3) {
        return -1;
    }

    vehicle = (Vehicle *)calloc(1, sizeof(Vehicle));
    if (vehicle == NULL) {
        return -1;
    }
    vehicle->id = id;
    vehicle->node_id = node_id;
    vehicle->initial_node_id = node_id;
    vehicle->battery_percent = battery_percent;
    vehicle->initial_battery_percent = battery_percent;
    vehicle->target_percent = target_percent;
    vehicle->capacity_kwh = capacity_kwh;
    vehicle->urgency = urgency;
    vehicle->state = VEHICLE_IDLE;
    vehicle->station_id = -1;
    vehicle->charger_id = -1;

    if (hash_add(store->vehicles, id, vehicle) != 0) {
        free(vehicle);
        return -1;
    }
    store->vehicle_ids[store->vehicle_count++] = id;
    return 0;
}

/* 根据编号查找车辆。 */
Vehicle *entity_find_vehicle(const EntityStore *store, int id)
{
    if (store == NULL) {
        return NULL;
    }
    return (Vehicle *)hash_find(store->vehicles, id);
}

/* 修改未参与当前任务的车辆基础数据。 */
int entity_update_vehicle(EntityStore *store, const Graph *graph,
                          int id, int node_id,
                          double battery_percent, double target_percent,
                          double capacity_kwh, int urgency)
{
    Vehicle *vehicle = entity_find_vehicle(store, id);

    if (graph == NULL || graph_find_node(graph, node_id) < 0 ||
        vehicle == NULL || vehicle->request != NULL ||
        (vehicle->state != VEHICLE_IDLE && vehicle->state != VEHICLE_FINISHED) ||
        battery_percent < 0.0 || battery_percent > 100.0 ||
        target_percent < battery_percent || target_percent > 100.0 ||
        capacity_kwh <= 0.0 || urgency < 1 || urgency > 3) {
        return -1;
    }
    vehicle->node_id = node_id;
    vehicle->initial_node_id = node_id;
    vehicle->battery_percent = battery_percent;
    vehicle->initial_battery_percent = battery_percent;
    vehicle->target_percent = target_percent;
    vehicle->capacity_kwh = capacity_kwh;
    vehicle->urgency = urgency;
    vehicle->state = VEHICLE_IDLE;
    return 0;
}

/* 删除未参与当前任务的车辆。 */
int entity_remove_vehicle(EntityStore *store, int id)
{
    Vehicle *vehicle;

    if (store == NULL) {
        return -1;
    }
    vehicle = entity_find_vehicle(store, id);
    if (vehicle == NULL || vehicle->request != NULL ||
        (vehicle->state != VEHICLE_IDLE && vehicle->state != VEHICLE_FINISHED)) {
        return -1;
    }
    hash_remove(store->vehicles, id);
    remove_id(store->vehicle_ids, &store->vehicle_count, id);
    free(vehicle);
    return 0;
}

/* 创建并添加一个充电站。 */
int entity_add_station(EntityStore *store, const Graph *graph,
                       int id, int node_id,
                       const char *name)
{
    Station *station;

    if (store == NULL || graph == NULL ||
        graph_find_node(graph, node_id) < 0 ||
        name == NULL || id < 0 ||
        store->station_count >= MAX_STATIONS) {
        return -1;
    }
    station = (Station *)calloc(1, sizeof(Station));
    if (station == NULL) {
        return -1;
    }
    station->id = id;
    station->node_id = node_id;
    snprintf(station->name, NAME_LEN, "%s", name);

    if (hash_add(store->stations, id, station) != 0) {
        free(station);
        return -1;
    }
    store->station_ids[store->station_count++] = id;
    return 0;
}

/* 根据编号查找充电站。 */
Station *entity_find_station(const EntityStore *store, int id)
{
    if (store == NULL) {
        return NULL;
    }
    return (Station *)hash_find(store->stations, id);
}

/* 判断一个站点是否仍被车辆任务引用。 */
static int station_is_in_use(const EntityStore *store, int station_id)
{
    int i;

    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL && vehicle->request != NULL &&
            vehicle->station_id == station_id) {
            return 1;
        }
    }
    return 0;
}

/* 修改当前没有业务任务的充电站。 */
int entity_update_station(EntityStore *store, const Graph *graph,
                          int id, int node_id,
                          const char *name)
{
    Station *station = entity_find_station(store, id);

    if (graph == NULL || graph_find_node(graph, node_id) < 0 ||
        station == NULL || name == NULL || station->queue_size != 0 ||
        station_is_in_use(store, id)) {
        return -1;
    }
    station->node_id = node_id;
    snprintf(station->name, NAME_LEN, "%s", name);
    return 0;
}

/* 删除不含充电桩和等待车辆的站点。 */
int entity_remove_station(EntityStore *store, int id)
{
    Station *station;

    if (store == NULL) {
        return -1;
    }
    station = entity_find_station(store, id);
    if (station == NULL || station->chargers != NULL || station->queue_size != 0 ||
        station_is_in_use(store, id)) {
        return -1;
    }
    hash_remove(store->stations, id);
    remove_id(store->station_ids, &store->station_count, id);
    free(station);
    return 0;
}

/* 删除没有被车辆或充电站引用的道路节点。 */
int entity_remove_graph_node(Graph *graph, const EntityStore *store,
                             int node_id)
{
    int i;

    if (graph == NULL || store == NULL) {
        return -1;
    }
    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL &&
            (vehicle->node_id == node_id ||
             vehicle->initial_node_id == node_id)) {
            return -1;
        }
    }
    for (i = 0; i < store->station_count; ++i) {
        Station *station = entity_find_station(store, store->station_ids[i]);
        if (station != NULL && station->node_id == node_id) {
            return -1;
        }
    }
    return graph_remove_node(graph, node_id);
}

/* 把充电桩加入全局哈希索引。 */
int entity_register_charger(EntityStore *store, Charger *charger)
{
    if (store == NULL || charger == NULL ||
        store->charger_count >= MAX_CHARGERS) {
        return -1;
    }
    if (hash_add(store->chargers, charger->id, charger) != 0) {
        return -1;
    }
    store->charger_ids[store->charger_count++] = charger->id;
    return 0;
}

/* 从全局哈希索引移除充电桩。 */
int entity_unregister_charger(EntityStore *store, int charger_id)
{
    if (store == NULL || hash_remove(store->chargers, charger_id) == NULL) {
        return -1;
    }
    remove_id(store->charger_ids, &store->charger_count, charger_id);
    return 0;
}

/* 根据编号查找充电桩。 */
Charger *entity_find_charger(const EntityStore *store, int id)
{
    if (store == NULL) {
        return NULL;
    }
    return (Charger *)hash_find(store->chargers, id);
}

/* 把充电请求加入全局哈希索引。 */
int entity_register_request(EntityStore *store, ChargeRequest *request)
{
    if (store == NULL || request == NULL ||
        store->request_count >= MAX_REQUESTS) {
        return -1;
    }
    if (hash_add(store->requests, request->id, request) != 0) {
        return -1;
    }
    store->request_ids[store->request_count++] = request->id;
    return 0;
}

/* 根据编号查找充电请求。 */
ChargeRequest *entity_find_request(const EntityStore *store, int id)
{
    if (store == NULL) {
        return NULL;
    }
    return (ChargeRequest *)hash_find(store->requests, id);
}

/* 从全局哈希索引移除请求但不释放请求。 */
int entity_unregister_request(EntityStore *store, int request_id)
{
    if (store == NULL || hash_remove(store->requests, request_id) == NULL) {
        return -1;
    }
    remove_id(store->request_ids, &store->request_count, request_id);
    return 0;
}

/* 释放仓库拥有的所有实体和索引节点。 */
void entity_free(EntityStore *store)
{
    int i;

    if (store == NULL) {
        return;
    }

    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL) {
            free(vehicle->request);
            vehicle->request = NULL;
        }
    }
    for (i = 0; i < store->station_count; ++i) {
        Station *station = entity_find_station(store, store->station_ids[i]);
        if (station != NULL) {
            Charger *charger = station->chargers;
            while (charger != NULL) {
                Charger *next = charger->next;
                free(charger);
                charger = next;
            }
            free(station);
        }
    }
    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        free(vehicle);
    }

    hash_clear(store->vehicles);
    hash_clear(store->stations);
    hash_clear(store->chargers);
    hash_clear(store->requests);
    store->vehicle_count = 0;
    store->station_count = 0;
    store->charger_count = 0;
    store->request_count = 0;
}

