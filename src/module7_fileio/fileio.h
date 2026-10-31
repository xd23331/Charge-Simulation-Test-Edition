#ifndef FILEIO_H
#define FILEIO_H

#include "../module1_graph/graph.h"
#include "../module3_entity/entity.h"

/* 一组输入数据文件路径。 */
typedef struct {
    const char *nodes_file;
    const char *roads_file;
    const char *stations_file;
    const char *chargers_file;
    const char *vehicles_file;
} DataFiles;

/* 文件读取错误，包含文件、行号和简单说明。 */
typedef struct {
    char file[256];
    int line;
    char message[128];
} LoadError;

/* 加载全部基础数据，全部成功后才替换当前模型。 */
int fileio_load_all(Graph *graph, EntityStore *store,
                    const DataFiles *files, LoadError *error);

/* 保存路网、站点、充电桩和车辆基础数据。 */
int fileio_save_all(const Graph *graph, const EntityStore *store,
                    const DataFiles *files);

/* 把车辆当前状态保存到CSV文件。 */
int fileio_save_vehicles(const EntityStore *store, const char *filename);

#endif

