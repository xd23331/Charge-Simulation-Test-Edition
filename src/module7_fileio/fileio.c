#include "fileio.h"

#include "../module4_charger/charger.h"

#include <stdio.h>
#include <string.h>

/* 记录一次文件读取错误。 */
static void set_error(LoadError *error, const char *file,
                      int line, const char *message)
{
    if (error == NULL) {
        return;
    }
    snprintf(error->file, sizeof(error->file), "%s", file);
    error->line = line;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

/* 判断一行是否为空行。 */
static int line_is_empty(const char *line)
{
    while (*line != '\0') {
        if (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n') {
            return 0;
        }
        ++line;
    }
    return 1;
}

/* 读取道路节点文件。 */
static int load_nodes(Graph *graph, const char *filename, LoadError *error)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int line_number = 0;

    if (file == NULL) {
        set_error(error, filename, 0, "cannot open file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int id;
        double x;
        double y;
        ++line_number;
        if (line_number == 1 || line_is_empty(line)) {
            continue;
        }
        if (sscanf(line, "%d,%lf,%lf", &id, &x, &y) != 3 ||
            graph_add_node(graph, id, x, y) != 0) {
            set_error(error, filename, line_number, "invalid node data");
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}

/* 读取道路连接文件。 */
static int load_roads(Graph *graph, const char *filename, LoadError *error)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int line_number = 0;

    if (file == NULL) {
        set_error(error, filename, 0, "cannot open file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int from;
        int to;
        int two_way;
        double distance;
        double minutes;
        ++line_number;
        if (line_number == 1 || line_is_empty(line)) {
            continue;
        }
        if (sscanf(line, "%d,%d,%lf,%lf,%d", &from, &to, &distance,
                   &minutes, &two_way) != 5 ||
            graph_add_edge(graph, from, to, distance, minutes) != 0 ||
            (two_way && graph_add_edge(graph, to, from, distance, minutes) != 0)) {
            set_error(error, filename, line_number, "invalid road data");
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}

/* 读取充电站文件。 */
static int load_stations(EntityStore *store, const Graph *graph,
                         const char *filename, LoadError *error)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int line_number = 0;

    if (file == NULL) {
        set_error(error, filename, 0, "cannot open file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int id;
        int node_id;
        char name[NAME_LEN];
        ++line_number;
        if (line_number == 1 || line_is_empty(line)) {
            continue;
        }
        if (sscanf(line, "%d,%d,%63[^\r\n]", &id, &node_id, name) != 3 ||
            graph_find_node(graph, node_id) < 0 ||
            entity_add_station(store, graph, id, node_id, name) != 0) {
            set_error(error, filename, line_number, "invalid station data");
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}

/* 读取充电桩文件。 */
static int load_chargers(EntityStore *store, const char *filename,
                         LoadError *error)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int line_number = 0;

    if (file == NULL) {
        set_error(error, filename, 0, "cannot open file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int id;
        int station_id;
        int state;
        double power;
        ++line_number;
        if (line_number == 1 || line_is_empty(line)) {
            continue;
        }
        if (sscanf(line, "%d,%d,%lf,%d", &id, &station_id, &power,
                   &state) != 4 ||
            (state != CHARGER_FREE && state != CHARGER_FAULT) ||
            charger_add(store, station_id, id, power, (ChargerState)state) != 0) {
            set_error(error, filename, line_number, "invalid charger data");
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}

/* 读取车辆文件。 */
static int load_vehicles(EntityStore *store, const Graph *graph,
                         const char *filename, LoadError *error)
{
    FILE *file = fopen(filename, "r");
    char line[256];
    int line_number = 0;

    if (file == NULL) {
        set_error(error, filename, 0, "cannot open file");
        return -1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        int id;
        int node_id;
        int urgency;
        double battery;
        double target;
        double capacity;
        ++line_number;
        if (line_number == 1 || line_is_empty(line)) {
            continue;
        }
        if (sscanf(line, "%d,%d,%lf,%lf,%lf,%d", &id, &node_id,
                   &battery, &target, &capacity, &urgency) != 6 ||
            graph_find_node(graph, node_id) < 0 ||
            entity_add_vehicle(store, graph, id, node_id, battery, target,
                               capacity, urgency) != 0) {
            set_error(error, filename, line_number, "invalid vehicle data");
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    return 0;
}

/* 加载全部基础数据，全部成功后才替换当前模型。 */
int fileio_load_all(Graph *graph, EntityStore *store,
                    const DataFiles *files, LoadError *error)
{
    Graph new_graph;
    EntityStore new_store;
    int failed;

    if (graph == NULL || store == NULL || files == NULL) {
        return -1;
    }
    graph_init(&new_graph);
    entity_init(&new_store);

    failed = load_nodes(&new_graph, files->nodes_file, error) != 0
          || load_roads(&new_graph, files->roads_file, error) != 0
          || load_stations(&new_store, &new_graph,
                           files->stations_file, error) != 0
          || load_chargers(&new_store, files->chargers_file, error) != 0
          || load_vehicles(&new_store, &new_graph,
                           files->vehicles_file, error) != 0;

    if (failed) {
        graph_free(&new_graph);
        entity_free(&new_store);
        return -1;
    }

    graph_free(graph);
    entity_free(store);
    *graph = new_graph;
    *store = new_store;
    return 0;
}

/* 保存道路节点数据。 */
static int save_nodes(const Graph *graph, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL) {
        return -1;
    }
    fprintf(file, "id,x,y\n");
    for (i = 0; i < graph->node_count; ++i) {
        fprintf(file, "%d,%.2f,%.2f\n", graph->nodes[i].id,
                graph->nodes[i].x, graph->nodes[i].y);
    }
    fclose(file);
    return 0;
}

/* 保存所有有向道路数据。 */
static int save_roads(const Graph *graph, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL) {
        return -1;
    }
    fprintf(file, "from,to,distance_km,travel_minutes,two_way\n");
    for (i = 0; i < graph->node_count; ++i) {
        const Edge *edge = graph->nodes[i].edges;
        while (edge != NULL) {
            fprintf(file, "%d,%d,%.3f,%.3f,0\n",
                    graph->nodes[i].id,
                    graph->nodes[edge->to_index].id,
                    edge->distance_km, edge->travel_minutes);
            edge = edge->next;
        }
    }
    fclose(file);
    return 0;
}

/* 保存充电站基础数据。 */
static int save_stations(const EntityStore *store, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL) {
        return -1;
    }
    fprintf(file, "id,node_id,name\n");
    for (i = 0; i < store->station_count; ++i) {
        Station *station = entity_find_station(store, store->station_ids[i]);
        if (station != NULL) {
            fprintf(file, "%d,%d,%s\n", station->id,
                    station->node_id, station->name);
        }
    }
    fclose(file);
    return 0;
}

/* 保存充电桩基础数据，忙碌状态按空闲状态保存。 */
static int save_chargers(const EntityStore *store, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL) {
        return -1;
    }
    fprintf(file, "id,station_id,power_kw,state\n");
    for (i = 0; i < store->station_count; ++i) {
        Station *station = entity_find_station(store, store->station_ids[i]);
        Charger *charger = station == NULL ? NULL : station->chargers;
        while (charger != NULL) {
            fprintf(file, "%d,%d,%.2f,%d\n", charger->id,
                    station->id, charger->power_kw,
                    charger->initial_state);
            charger = charger->next;
        }
    }
    fclose(file);
    return 0;
}

/* 保存车辆的初始配置数据。 */
static int save_vehicle_inputs(const EntityStore *store, const char *filename)
{
    FILE *file = fopen(filename, "w");
    int i;

    if (file == NULL) {
        return -1;
    }
    fprintf(file, "id,node_id,battery_percent,target_percent,capacity_kwh,urgency\n");
    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL) {
            fprintf(file, "%d,%d,%.2f,%.2f,%.2f,%d\n",
                    vehicle->id, vehicle->initial_node_id,
                    vehicle->initial_battery_percent,
                    vehicle->target_percent, vehicle->capacity_kwh,
                    vehicle->urgency);
        }
    }
    fclose(file);
    return 0;
}

/* 保存路网、站点、充电桩和车辆基础数据。 */
int fileio_save_all(const Graph *graph, const EntityStore *store,
                    const DataFiles *files)
{
    if (graph == NULL || store == NULL || files == NULL) {
        return -1;
    }
    if (save_nodes(graph, files->nodes_file) != 0 ||
        save_roads(graph, files->roads_file) != 0 ||
        save_stations(store, files->stations_file) != 0 ||
        save_chargers(store, files->chargers_file) != 0 ||
        save_vehicle_inputs(store, files->vehicles_file) != 0) {
        return -1;
    }
    return 0;
}

/* 把车辆当前状态保存到CSV文件。 */
int fileio_save_vehicles(const EntityStore *store, const char *filename)
{
    FILE *file;
    int i;

    if (store == NULL || filename == NULL) {
        return -1;
    }
    file = fopen(filename, "w");
    if (file == NULL) {
        return -1;
    }
    fprintf(file, "id,node_id,battery_percent,state,station_id,charger_id\n");
    for (i = 0; i < store->vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(store, store->vehicle_ids[i]);
        if (vehicle != NULL) {
            fprintf(file, "%d,%d,%.2f,%d,%d,%d\n",
                    vehicle->id, vehicle->node_id, vehicle->battery_percent,
                    vehicle->state, vehicle->station_id, vehicle->charger_id);
        }
    }
    fclose(file);
    return 0;
}

