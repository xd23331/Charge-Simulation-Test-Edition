/*
 * 模块8：GTK 图形界面与示意路网绘制模块
 *
 * 使用 GTK 4 实现单一主窗口：操作区域、信息显示区域和地图绘制区域。
 * 使用 DrawingArea 与 Cairo 绘制固定二维示意路网，展示道路节点、充电站、
 * 车辆出发位置和推荐路线，并通过统一接口连接后端算法与仿真状态。
 * 定时控制（开始、暂停、单步、重置）由模块9的定时控制器统一管理。
 */

#include <gtk/gtk.h>

#include "../module1_graph/graph.h"
#include "../module3_entity/entity.h"
#include "../module5_recommend/recommend.h"
#include "../module6_simulation/simulation.h"
#include "../module7_fileio/fileio.h"
#include "../module9_timer/timer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define TIMER_INTERVAL_MS 220

/* 一辆车在界面中使用的行驶路线（用于在路网上显示移动）。 */
typedef struct {
    int vehicle_id;
    int active;
    double start_time;
    PathResult path;
} VisualRoute;

/* 地图坐标变换：统一比例，使线段长度能反映道路距离。 */
typedef struct {
    double scale;
    double ox;
    double oy;
} MapTransform;

/* 前端应用保存的后端数据与界面控件句柄。 */
typedef struct {
    Graph graph;
    EntityStore store;
    Simulation simulation;
    SimTimer timer;
    RecommendationList recommendations;
    Recommendation selected;
    int loaded;
    int has_selected;

    GtkWidget *vehicle_entry;
    GtkWidget *charger_entry;
    GtkWidget *log_view;
    GtkWidget *status_label;
    GtkWidget *drawing_area;
    PangoFontDescription *font;

    VisualRoute routes[MAX_VEHICLES];
} UiApp;

/* 清除界面保存的车辆移动路线。 */
static void clear_visual_routes(UiApp *app)
{
    memset(app->routes, 0, sizeof(app->routes));
}

/* 查找一辆车对应的界面移动路线。 */
static VisualRoute *find_visual_route(const UiApp *app, int vehicle_id)
{
    int i;
    for (i = 0; i < MAX_VEHICLES; ++i) {
        if (app->routes[i].active &&
            app->routes[i].vehicle_id == vehicle_id) {
            return (VisualRoute *)&app->routes[i];
        }
    }
    return NULL;
}

/* 保存车辆提交请求时选择的推荐路线。 */
static void save_visual_route(UiApp *app, int vehicle_id,
                              const PathResult *path, double start_time)
{
    VisualRoute *route = find_visual_route(app, vehicle_id);
    int i;

    if (route == NULL) {
        for (i = 0; i < MAX_VEHICLES; ++i) {
            if (!app->routes[i].active) {
                route = &app->routes[i];
                break;
            }
        }
    }
    if (route != NULL) {
        route->vehicle_id = vehicle_id;
        route->active = 1;
        route->start_time = start_time;
        route->path = *path;
    }
}

/* 把程序工作目录切换到项目根目录，方便从 build 目录或双击运行。
 * 使用宽字符 Windows API，避免中文路径下 C 运行时 chdir 编码不匹配。 */
static void set_project_directory(void)
{
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;

    /* 截断文件名，得到所在目录 */
    wchar_t *last_slash = wcsrchr(exe_path, L'\\');
    if (last_slash == NULL) return;
    *last_slash = L'\0';

    /* 若所在目录名为 "build"，则再截断一级，得到项目根 */
    wchar_t *dir_start = wcsrchr(exe_path, L'\\');
    if (dir_start != NULL && wcscmp(dir_start + 1, L"build") == 0) {
        *dir_start = L'\0';
    }

    SetCurrentDirectoryW(exe_path);
}

/* 在日志文本框中追加一行文字并滚动到底部。 */
static void log_line(UiApp *app, const char *format, ...)
{
    char buf[1024];
    char line[1032];
    GtkTextBuffer *buffer;
    GtkTextIter end;
    va_list args;

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    snprintf(line, sizeof(line), "%s\n", buf);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->log_view));
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, line, -1);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->log_view), &end,
                                 0.0, FALSE, 0.0, 0.0);
}

/* 清空日志文本框。 */
static void clear_log(UiApp *app)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(app->log_view));
    gtk_text_buffer_set_text(buffer, "", -1);
}

/* 从单行输入框读取一个整数。 */
static int read_number(GtkEntry *entry)
{
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    return atoi(text);
}

/* 返回便于显示的车辆状态名称。 */
static const char *state_name(VehicleState state)
{
    switch (state) {
        case VEHICLE_IDLE: return "空闲";
        case VEHICLE_TRAVELING: return "前往充电站";
        case VEHICLE_WAITING: return "排队等待";
        case VEHICLE_CHARGING: return "正在充电";
        case VEHICLE_FINISHED: return "充电完成";
        default: return "未知状态";
    }
}

/* 返回便于显示的充电桩状态名称。 */
static const char *charger_state_name(ChargerState state)
{
    switch (state) {
        case CHARGER_FREE: return "空闲";
        case CHARGER_BUSY: return "充电中";
        case CHARGER_FAULT: return "故障";
        default: return "未知";
    }
}

/* 把推荐模块中的英文失败原因转换为中文显示。 */
static const char *reason_name(const char *reason)
{
    if (strcmp(reason, "road unreachable") == 0) {
        return "道路不可达";
    }
    if (strcmp(reason, "no working charger") == 0) {
        return "没有可工作的充电桩";
    }
    if (strcmp(reason, "battery not enough") == 0) {
        return "剩余电量不足以到达";
    }
    if (strcmp(reason, "station unavailable") == 0) {
        return "充电站当前不可用";
    }
    return reason;
}

/* 把文件模块中的常见英文错误转换为中文显示。 */
static const char *file_error_name(const char *message)
{
    if (strcmp(message, "cannot open file") == 0) {
        return "无法打开文件";
    }
    if (strcmp(message, "invalid node data") == 0) {
        return "道路节点数据不合法";
    }
    if (strcmp(message, "invalid road data") == 0) {
        return "道路数据不合法";
    }
    if (strcmp(message, "invalid station data") == 0) {
        return "充电站数据不合法";
    }
    if (strcmp(message, "invalid charger data") == 0) {
        return "充电桩数据不合法";
    }
    if (strcmp(message, "invalid vehicle data") == 0) {
        return "车辆数据不合法";
    }
    return message;
}

/* 把样例数据中的站点英文名转换为中文显示。 */
static const char *station_name(const Station *station)
{
    if (strcmp(station->name, "East Station") == 0) {
        return "东部充电站";
    }
    if (strcmp(station->name, "Central Station") == 0) {
        return "中心充电站";
    }
    if (strcmp(station->name, "South Station") == 0) {
        return "南部充电站";
    }
    if (strcmp(station->name, "West Station") == 0) {
        return "西部充电站";
    }
    if (strcmp(station->name, "North Station") == 0) {
        return "北部充电站";
    }
    return station->name;
}

/* 刷新顶部状态栏。 */
static void update_status(UiApp *app)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "仿真时间：%.0f 分钟    运行状态：%s",
             app->simulation.current_time,
             sim_timer_is_running(&app->timer) ? "运行中" : "已暂停");
    gtk_label_set_text(GTK_LABEL(app->status_label), buf);
}

/* 输出当前仿真时间和全部车辆状态。 */
static void log_status(UiApp *app)
{
    int i;

    if (!app->loaded) {
        return;
    }
    log_line(app, "当前仿真时间：%.0f 分钟", app->simulation.current_time);
    for (i = 0; i < simulation_vehicle_count(&app->simulation); ++i) {
        const Vehicle *vehicle = simulation_vehicle_at(&app->simulation, i);
        log_line(app, "  车辆 %d：%s，电量 %.1f%%，充电站 %d，充电桩 %d",
                 vehicle->id, state_name(vehicle->state),
                 vehicle->battery_percent, vehicle->station_id,
                 vehicle->charger_id);
    }
    update_status(app);
}

/* 前向声明：收集当前全部故障充电桩编号。 */
static int collect_faulty_chargers(UiApp *app, int *ids, int max_ids);

/* 加载样例CSV数据并初始化仿真。 */
static void load_data(UiApp *app)
{
    DataFiles files = {
        "data/input/nodes.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };
    LoadError error;

    sim_timer_stop(&app->timer);
    if (app->loaded) {
        simulation_reset(&app->simulation);
        graph_free(&app->graph);
        entity_free(&app->store);
    }
    graph_init(&app->graph);
    entity_init(&app->store);
    clear_log(app);
    app->loaded = 0;
    app->has_selected = 0;
    clear_visual_routes(app);

    if (fileio_load_all(&app->graph, &app->store, &files, &error) != 0) {
        log_line(app, "数据加载失败：%s，第 %d 行，%s",
                 error.file, error.line, file_error_name(error.message));
        gtk_widget_queue_draw(app->drawing_area);
        return;
    }
    if (simulation_init(&app->simulation, &app->graph, &app->store, 1.0) != 0) {
        log_line(app, "仿真初始化失败。可检查数据之间的关联是否正确。");
        return;
    }
    app->loaded = 1;
    log_line(app, "数据加载成功：%d 个道路节点，%d 个充电站，%d 辆车。",
             app->graph.node_count, app->store.station_count,
             app->store.vehicle_count);
    {
        int faulty_ids[16];
        int faulty_count = collect_faulty_chargers(app, faulty_ids, 16);
        if (faulty_count > 0) {
            int k;
            log_line(app, "注意：初始数据中有 %d 个故障充电桩：", faulty_count);
            for (k = 0; k < faulty_count; ++k) {
                log_line(app, "  - 充电桩 %d（可在“充电桩编号”输入 %d 后点“修复设备”恢复）",
                         faulty_ids[k], faulty_ids[k]);
            }
        }
    }
    log_status(app);
    gtk_widget_queue_draw(app->drawing_area);
}

/* 为输入的车辆计算并显示全部站点推荐。 */
static void recommend_vehicle(UiApp *app)
{
    int vehicle_id = read_number(GTK_ENTRY(app->vehicle_entry));
    Vehicle *vehicle;
    const Recommendation *best;
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    vehicle = entity_find_vehicle(&app->store, vehicle_id);
    if (vehicle == NULL) {
        log_line(app, "没有找到编号为 %d 的车辆。", vehicle_id);
        return;
    }
    if (recommend_all(&app->graph, &app->store, vehicle,
                      &app->recommendations) != 0) {
        log_line(app, "充电站推荐计算失败。");
        return;
    }
    log_line(app, "车辆 %d 的充电站推荐结果：", vehicle_id);
    for (i = 0; i < app->recommendations.count; ++i) {
        Recommendation *item = &app->recommendations.items[i];
        if (item->valid) {
            log_line(app, "  ★ 充电站 %d：行驶 %.1f 分钟，等待 %.1f 分钟，"
                          "充电 %.1f 分钟，总计 %.1f 分钟",
                     item->station_id, item->travel_minutes,
                     item->wait_minutes, item->charge_minutes,
                     item->total_minutes);
        } else {
            log_line(app, "  充电站 %d：不可选，原因：%s",
                     item->station_id, reason_name(item->reason));
        }
    }
    best = recommend_best(&app->recommendations);
    if (best == NULL) {
        app->has_selected = 0;
        log_line(app, "当前没有可用的充电站。");
    } else {
        app->selected = *best;
        app->has_selected = 1;
        log_line(app, "系统推荐：充电站 %d（地图中的红色路线为推荐路线）。",
                 best->station_id);
    }
    gtk_widget_queue_draw(app->drawing_area);
}

/* 提交最近一次推荐得到的充电请求。 */
static void submit_request(UiApp *app)
{
    int vehicle_id = read_number(GTK_ENTRY(app->vehicle_entry));
    const Vehicle *vehicle;

    if (!app->loaded || !app->has_selected) {
        log_line(app, "请先为车辆计算充电站推荐。");
        return;
    }
    if (simulation_submit(&app->simulation, vehicle_id,
                          &app->selected) != 0) {
        log_line(app, "车辆 %d 的充电请求提交失败。", vehicle_id);
        return;
    }
    vehicle = entity_find_vehicle(&app->store, vehicle_id);
    save_visual_route(app, vehicle_id, &app->selected.path,
                      app->simulation.current_time);
    log_line(app, "车辆 %d 已提交请求 %d，目标为充电站 %d。",
             vehicle_id, vehicle->request->id, app->selected.station_id);
    app->has_selected = 0;
    log_status(app);
    gtk_widget_queue_draw(app->drawing_area);
}

/* 为全部空闲车辆计算推荐并批量提交充电请求。 */
static void submit_all_requests(UiApp *app)
{
    int submitted = 0;
    int failed = 0;
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    sim_timer_stop(&app->timer);
    for (i = 0; i < app->store.vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            &app->store, app->store.vehicle_ids[i]);
        RecommendationList list;
        const Recommendation *best;

        if (vehicle == NULL || vehicle->state != VEHICLE_IDLE) {
            continue;
        }
        if (recommend_all(&app->graph, &app->store, vehicle, &list) != 0) {
            ++failed;
            continue;
        }
        best = recommend_best(&list);
        if (best == NULL ||
            simulation_submit(&app->simulation, vehicle->id, best) != 0) {
            ++failed;
            continue;
        }
        save_visual_route(app, vehicle->id, &best->path,
                          app->simulation.current_time);
        ++submitted;
    }
    app->has_selected = 0;
    log_line(app, "批量提交完成：成功 %d 辆，失败 %d 辆。",
             submitted, failed);
    log_line(app, "点击“运行到底”可观察车辆到站、排队和充电过程。");
    gtk_widget_queue_draw(app->drawing_area);
}

/* 定时回调：推进一个时间步并刷新界面，返回是否继续运行。 */
static gboolean on_timer_tick(gpointer user_data)
{
    UiApp *app = user_data;

    if (!app->loaded) {
        return FALSE;
    }
    simulation_step(&app->simulation);
    update_status(app);
    gtk_widget_queue_draw(app->drawing_area);
    return simulation_has_active(&app->simulation);
}

/* 立即推进一个仿真时间步（单步运行）。 */
static void run_single_step(UiApp *app)
{
    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    sim_timer_stop(&app->timer);
    simulation_step(&app->simulation);
    log_status(app);
    gtk_widget_queue_draw(app->drawing_area);
}

/* 开始固定步数或连续运行。 */
static void start_timed_run(UiApp *app, int steps, int until_done)
{
    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    if (!simulation_has_active(&app->simulation)) {
        log_line(app, "当前没有正在执行的充电任务。");
        return;
    }
    if (!sim_timer_start(&app->timer, steps, until_done,
                         TIMER_INTERVAL_MS)) {
        log_line(app, "无法启动定时仿真。");
        return;
    }
    if (until_done) {
        log_line(app, "开始连续运行，车辆将沿推荐路线逐步移动。");
    } else {
        log_line(app, "开始运行 %d 个时间步。", steps);
    }
    update_status(app);
}

/* 暂停定时仿真。 */
static void pause_run(UiApp *app)
{
    sim_timer_stop(&app->timer);
    update_status(app);
    log_line(app, "定时运行已暂停。");
}

/* 收集当前全部故障充电桩编号，返回数量。 */
static int collect_faulty_chargers(UiApp *app, int *ids, int max_ids)
{
    int i;
    int count = 0;

    for (i = 0; i < app->store.station_count && count < max_ids; ++i) {
        const Station *station = entity_find_station(
            &app->store, app->store.station_ids[i]);
        int j;
        int charger_count;

        if (station == NULL) {
            continue;
        }
        charger_count = simulation_charger_count(&app->simulation,
                                                  station->id);
        for (j = 0; j < charger_count && count < max_ids; ++j) {
            const Charger *charger = simulation_charger_at(
                &app->simulation, station->id, j);
            if (charger != NULL && charger->state == CHARGER_FAULT) {
                ids[count++] = charger->id;
            }
        }
    }
    return count;
}

/* 把输入编号的充电桩设为故障。 */
static void break_charger(UiApp *app)
{
    int charger_id = read_number(GTK_ENTRY(app->charger_entry));

    if (!app->loaded ||
        simulation_break_charger(&app->simulation, charger_id) != 0) {
        log_line(app, "无法将充电桩 %d 设置为故障（编号不存在或已是故障/充电中）。",
                 charger_id);
        return;
    }
    log_line(app, "充电桩 %d 已设置为故障。", charger_id);
    gtk_widget_queue_draw(app->drawing_area);
}

/* 恢复输入编号的故障充电桩；若编号不对则列出当前故障桩。 */
static void repair_charger(UiApp *app)
{
    int charger_id = read_number(GTK_ENTRY(app->charger_entry));
    int faulty_ids[16];
    int faulty_count;
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    if (simulation_repair_charger(&app->simulation, charger_id) == 0) {
        log_line(app, "充电桩 %d 已修复，可以继续使用。", charger_id);
        gtk_widget_queue_draw(app->drawing_area);
        return;
    }

    /* 修复失败：列出当前全部故障充电桩，引导用户输入正确编号。 */
    faulty_count = collect_faulty_chargers(app, faulty_ids, 16);
    if (faulty_count == 0) {
        log_line(app, "充电桩 %d 不是故障状态，当前没有故障充电桩。",
                 charger_id);
        return;
    }
    log_line(app, "充电桩 %d 不是故障状态，无法修复。", charger_id);
    log_line(app, "当前故障充电桩：");
    for (i = 0; i < faulty_count; ++i) {
        log_line(app, "  - 充电桩 %d（在上方“充电桩编号”输入此编号后再点“修复设备”）",
                 faulty_ids[i]);
    }
}

/* 重置全部仿真状态但保留已经加载的基础数据。 */
static void reset_simulation(UiApp *app)
{
    if (!app->loaded) {
        return;
    }
    sim_timer_stop(&app->timer);
    simulation_reset(&app->simulation);
    app->has_selected = 0;
    clear_visual_routes(app);
    log_line(app, "仿真状态已重置。");
    log_status(app);
    gtk_widget_queue_draw(app->drawing_area);
}

/* 在日志中列出全部车辆信息（车辆查看）。 */
static void log_vehicle_list(UiApp *app)
{
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    log_line(app, "全部车辆信息：");
    for (i = 0; i < simulation_vehicle_count(&app->simulation); ++i) {
        const Vehicle *vehicle = simulation_vehicle_at(&app->simulation, i);
        log_line(app, "  车辆 %d：节点 %d，电量 %.1f%%，目标 %.1f%%，"
                      "容量 %.0f kWh，紧急度 %d，状态：%s",
                 vehicle->id, vehicle->node_id, vehicle->battery_percent,
                 vehicle->target_percent, vehicle->capacity_kwh,
                 vehicle->urgency, state_name(vehicle->state));
    }
}

/* 在日志中列出全部充电站信息（充电站查看）。 */
static void log_station_list(UiApp *app)
{
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    log_line(app, "全部充电站信息：");
    for (i = 0; i < simulation_station_count(&app->simulation); ++i) {
        const Station *station = simulation_station_at(&app->simulation, i);
        log_line(app, "  充电站 %d（%s）：道路节点 %d，充电桩 %d 个，"
                      "等待车辆 %d 辆",
                 station->id, station_name(station), station->node_id,
                 simulation_charger_count(&app->simulation, station->id),
                 station->queue_size);
    }
}

/* 在日志中列出每个充电站及其全部充电桩的实时状态。 */
static void log_charger_status(UiApp *app)
{
    int i;

    if (!app->loaded) {
        log_line(app, "请先加载数据。");
        return;
    }
    log_line(app, "各充电站的充电桩状态：");
    for (i = 0; i < app->store.station_count; ++i) {
        const Station *station = entity_find_station(
            &app->store, app->store.station_ids[i]);
        int j;
        int count;

        if (station == NULL) {
            continue;
        }
        count = simulation_charger_count(&app->simulation, station->id);
        log_line(app, "充电站 %d（%s），等待车辆 %d 辆：",
                 station->id, station_name(station), station->queue_size);
        for (j = 0; j < count; ++j) {
            const Charger *charger = simulation_charger_at(
                &app->simulation, station->id, j);
            if (charger->state == CHARGER_BUSY &&
                charger->request != NULL &&
                charger->request->vehicle != NULL) {
                log_line(app, "  充电桩 %d：%.0f kW，%s，车辆 %d，剩余 %.1f 分钟",
                         charger->id, charger->power_kw,
                         charger_state_name(charger->state),
                         charger->request->vehicle->id,
                         charger->remaining_minutes);
            } else {
                log_line(app, "  充电桩 %d：%.0f kW，%s",
                         charger->id, charger->power_kw,
                         charger_state_name(charger->state));
            }
        }
    }
}

/* ---- 以下为使用 Cairo 绘制固定二维示意路网的函数 ---- */

/* 计算路网到绘图区的统一坐标变换。 */
static void compute_transform(const UiApp *app, double width, double height,
                              MapTransform *transform)
{
    double max_x = 1.0;
    double max_y = 1.0;
    double scale_x;
    double scale_y;
    int i;

    for (i = 0; i < app->graph.node_count; ++i) {
        if (app->graph.nodes[i].x > max_x) {
            max_x = app->graph.nodes[i].x;
        }
        if (app->graph.nodes[i].y > max_y) {
            max_y = app->graph.nodes[i].y;
        }
    }
    scale_x = (width - 140) / max_x;
    scale_y = (height - 160) / max_y;
    transform->scale = scale_x < scale_y ? scale_x : scale_y;
    if (transform->scale < 0.5) {
        transform->scale = 0.5;
    }
    if (transform->scale > 2.2) {
        transform->scale = 2.2;
    }
    transform->ox = (width - max_x * transform->scale) / 2.0;
    transform->oy = (height - max_y * transform->scale) / 2.0 + 24;
}

/* 把道路节点坐标转换为绘图区中的窗口坐标。 */
static void node_xy(const UiApp *app, const GraphNode *node,
                    const MapTransform *transform, double *x, double *y)
{
    (void)app;
    *x = transform->ox + node->x * transform->scale;
    *y = transform->oy + node->y * transform->scale;
}

/* 查找两个相邻节点之间的道路数据。 */
static const Edge *find_route_edge(const UiApp *app, int from_index,
                                   int to_index)
{
    const Edge *edge = app->graph.nodes[from_index].edges;

    while (edge != NULL) {
        if (edge->to_index == to_index) {
            return edge;
        }
        edge = edge->next;
    }
    return NULL;
}

/* 在指定位置绘制一段文字。 */
static void draw_text(cairo_t *cr, const UiApp *app, double x, double y,
                      const char *text, double size, double r, double g,
                      double b)
{
    PangoLayout *layout = gtk_widget_create_pango_layout(
        GTK_WIDGET(app->drawing_area), text);
    PangoFontDescription *font = pango_font_description_copy(app->font);

    pango_font_description_set_size(font, size * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}

/* 在指定位置居中绘制一段文字，返回文字包围盒高度。 */
static double draw_text_centered(cairo_t *cr, const UiApp *app,
                                 double center_x, double center_y,
                                 const char *text, double size,
                                 double r, double g, double b)
{
    PangoLayout *layout = gtk_widget_create_pango_layout(
        GTK_WIDGET(app->drawing_area), text);
    PangoFontDescription *font = pango_font_description_copy(app->font);
    int width;
    int height;

    pango_font_description_set_size(font, size * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    pango_layout_get_pixel_size(layout, &width, &height);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, center_x - width / 2.0, center_y - height / 2.0);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
    return height;
}

/* 绘制带白色半透明背景的文字，避免被线条遮挡。 */
static void draw_text_with_bg(cairo_t *cr, const UiApp *app, double x,
                               double y, const char *text, double size,
                               double r, double g, double b)
{
    PangoLayout *layout = gtk_widget_create_pango_layout(
        GTK_WIDGET(app->drawing_area), text);
    PangoFontDescription *font = pango_font_description_copy(app->font);
    int tw;
    int th;

    pango_font_description_set_size(font, size * PANGO_SCALE);
    pango_layout_set_font_description(layout, font);
    pango_layout_get_pixel_size(layout, &tw, &th);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
    cairo_rectangle(cr, x - 2, y - 1, tw + 4, th + 2);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);
}

/* 绘制汽车符号（朝上的三角形，带白色描边）。 */
static void draw_car_symbol(cairo_t *cr, double x, double y, double size,
                            double r, double g, double b)
{
    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y - size);
    cairo_line_to(cr, x - size * 0.85, y + size * 0.65);
    cairo_line_to(cr, x + size * 0.85, y + size * 0.65);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

/* 在圆圈中心绘制闪电符号（白色）。 */
static void draw_lightning(cairo_t *cr, double x, double y, double size)
{
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr, x + size * 0.25, y - size * 0.55);
    cairo_line_to(cr, x - size * 0.30, y + size * 0.10);
    cairo_line_to(cr, x - size * 0.02, y + size * 0.10);
    cairo_line_to(cr, x - size * 0.20, y + size * 0.55);
    cairo_line_to(cr, x + size * 0.35, y - size * 0.12);
    cairo_line_to(cr, x + size * 0.05, y - size * 0.12);
    cairo_close_path(cr);
    cairo_fill(cr);
}

/* 绘制全部道路及距离标注。 */
static void draw_edges(cairo_t *cr, const UiApp *app,
                       const MapTransform *transform)
{
    int i;

    cairo_set_source_rgb(cr, 0.66, 0.66, 0.66);
    cairo_set_line_width(cr, 2.0);
    for (i = 0; i < app->graph.node_count; ++i) {
        const Edge *edge = app->graph.nodes[i].edges;
        double from_x;
        double from_y;

        node_xy(app, &app->graph.nodes[i], transform, &from_x, &from_y);
        while (edge != NULL) {
            /* 当前样例道路均为双向，只绘制一次，避免线条和文字重复。 */
            if (i < edge->to_index) {
                double to_x;
                double to_y;
                char label[32];

                node_xy(app, &app->graph.nodes[edge->to_index],
                        transform, &to_x, &to_y);
                cairo_move_to(cr, from_x, from_y);
                cairo_line_to(cr, to_x, to_y);
                cairo_stroke(cr);
                snprintf(label, sizeof(label), "%.1f公里", edge->distance_km);
                draw_text_with_bg(cr, app, (from_x + to_x) / 2.0 + 3,
                                  (from_y + to_y) / 2.0 - 14, label, 8.5,
                                  0.30, 0.30, 0.30);
            }
            edge = edge->next;
        }
    }
}

/* 用红色绘制最近一次推荐路径。 */
static void draw_selected_path(cairo_t *cr, const UiApp *app,
                               const MapTransform *transform)
{
    int i;

    if (!app->has_selected || !app->selected.valid) {
        return;
    }
    cairo_set_source_rgb(cr, 0.86, 0.20, 0.20);
    cairo_set_line_width(cr, 4.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (i = 0; i + 1 < app->selected.path.path_length; ++i) {
        int from_index = graph_find_node(
            &app->graph, app->selected.path.node_ids[i]);
        int to_index = graph_find_node(
            &app->graph, app->selected.path.node_ids[i + 1]);
        double from_x;
        double from_y;
        double to_x;
        double to_y;

        if (from_index < 0 || to_index < 0) {
            continue;
        }
        node_xy(app, &app->graph.nodes[from_index], transform,
                &from_x, &from_y);
        node_xy(app, &app->graph.nodes[to_index], transform, &to_x, &to_y);
        cairo_move_to(cr, from_x, from_y);
        cairo_line_to(cr, to_x, to_y);
    }
    cairo_stroke(cr);
}

/* 用橙色虚线显示正在行驶车辆的路线。 */
static void draw_vehicle_routes(cairo_t *cr, const UiApp *app,
                                const MapTransform *transform)
{
    double dashes[2] = {8.0, 5.0};
    int i;

    cairo_set_source_rgb(cr, 0.92, 0.53, 0.12);
    cairo_set_line_width(cr, 2.0);
    cairo_set_dash(cr, dashes, 2, 0.0);
    for (i = 0; i < MAX_VEHICLES; ++i) {
        const VisualRoute *route = &app->routes[i];
        const Vehicle *vehicle;
        int j;

        if (!route->active) {
            continue;
        }
        vehicle = entity_find_vehicle(&app->store, route->vehicle_id);
        if (vehicle == NULL || vehicle->state != VEHICLE_TRAVELING) {
            continue;
        }
        for (j = 0; j + 1 < route->path.path_length; ++j) {
            int from_index = graph_find_node(
                &app->graph, route->path.node_ids[j]);
            int to_index = graph_find_node(
                &app->graph, route->path.node_ids[j + 1]);
            double from_x;
            double from_y;
            double to_x;
            double to_y;

            if (from_index < 0 || to_index < 0) {
                continue;
            }
            node_xy(app, &app->graph.nodes[from_index], transform,
                    &from_x, &from_y);
            node_xy(app, &app->graph.nodes[to_index], transform,
                    &to_x, &to_y);
            cairo_move_to(cr, from_x, from_y);
            cairo_line_to(cr, to_x, to_y);
        }
    }
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0.0);
}

/* 判断一个路网节点是否放置了充电站。 */
static const Station *station_on_node(const UiApp *app, int node_id)
{
    int i;

    for (i = 0; i < app->store.station_count; ++i) {
        const Station *station = entity_find_station(
            &app->store, app->store.station_ids[i]);
        if (station != NULL && station->node_id == node_id) {
            return station;
        }
    }
    return NULL;
}

/* 统计一个站点中三种状态的充电桩数量。 */
static void count_station_chargers(const Station *station, int *free_count,
                                   int *busy_count, int *fault_count)
{
    const Charger *charger = station->chargers;

    *free_count = 0;
    *busy_count = 0;
    *fault_count = 0;
    while (charger != NULL) {
        if (charger->state == CHARGER_FREE) {
            ++*free_count;
        } else if (charger->state == CHARGER_BUSY) {
            ++*busy_count;
        } else if (charger->state == CHARGER_FAULT) {
            ++*fault_count;
        }
        charger = charger->next;
    }
}

/* 绘制节点、站点名称、编号和状态。 */
static void draw_nodes(cairo_t *cr, const UiApp *app,
                       const MapTransform *transform)
{
    int i;

    for (i = 0; i < app->graph.node_count; ++i) {
        const GraphNode *node = &app->graph.nodes[i];
        const Station *station = station_on_node(app, node->id);
        double x;
        double y;
        char label[96];

        node_xy(app, node, transform, &x, &y);
        if (station == NULL) {
            /* 普通道路节点：蓝色小圆。 */
            cairo_set_source_rgb(cr, 0.24, 0.51, 0.86);
            cairo_arc(cr, x, y, 7.0, 0.0, 2.0 * G_PI);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            snprintf(label, sizeof(label), "%d", node->id);
            draw_text_centered(cr, app, x, y, label, 8.5, 1.0, 1.0, 1.0);
        } else {
            /* 充电站：较大绿色圆 + 白色闪电符号。 */
            cairo_set_source_rgb(cr, 0.13, 0.62, 0.32);
            cairo_arc(cr, x, y, 12.0, 0.0, 2.0 * G_PI);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
            cairo_set_line_width(cr, 1.5);
            cairo_stroke(cr);
            draw_lightning(cr, x, y, 10.0);
            /* 节点编号放在圆圈下方。 */
            snprintf(label, sizeof(label), "%d", node->id);
            draw_text_centered(cr, app, x, y + 20, label, 8.0,
                               0.10, 0.30, 0.15);
            /* 站点名称和桩状态放在节点上方，避免与车辆重叠。 */
            {
                int free_count;
                int busy_count;
                int fault_count;

                count_station_chargers(station, &free_count,
                                       &busy_count, &fault_count);
                snprintf(label, sizeof(label), "空%d 充%d 故%d 等待%d",
                         free_count, busy_count, fault_count,
                         station->queue_size);
                draw_text_centered(cr, app, x, y - 34, label, 8.5,
                                   0.15, 0.35, 0.18);
            }
            snprintf(label, sizeof(label), "站%d %s",
                     station->id, station_name(station));
            draw_text_centered(cr, app, x, y - 20, label, 10.0,
                               0.04, 0.18, 0.06);
        }
    }
}

/* 根据仿真时间计算车辆在推荐路线上的显示位置。 */
static void moving_vehicle_point(const UiApp *app, const Vehicle *vehicle,
                                 const MapTransform *transform,
                                 double *x, double *y)
{
    const VisualRoute *route = find_visual_route(app, vehicle->id);
    double elapsed;
    int node_index;
    int i;

    node_index = graph_find_node(&app->graph, vehicle->node_id);
    if (node_index >= 0) {
        node_xy(app, &app->graph.nodes[node_index], transform, x, y);
    }
    if (vehicle->state != VEHICLE_TRAVELING || route == NULL ||
        route->path.path_length < 2) {
        return;
    }
    elapsed = app->simulation.current_time - route->start_time;
    if (elapsed < 0.0) {
        elapsed = 0.0;
    }
    for (i = 0; i + 1 < route->path.path_length; ++i) {
        int from_index = graph_find_node(
            &app->graph, route->path.node_ids[i]);
        int to_index = graph_find_node(
            &app->graph, route->path.node_ids[i + 1]);
        const Edge *edge;

        if (from_index < 0 || to_index < 0) {
            return;
        }
        edge = find_route_edge(app, from_index, to_index);
        if (edge == NULL || edge->travel_minutes <= 0.0) {
            return;
        }
        if (elapsed <= edge->travel_minutes) {
            double from_x;
            double from_y;
            double to_x;
            double to_y;
            double ratio;

            node_xy(app, &app->graph.nodes[from_index], transform,
                    &from_x, &from_y);
            node_xy(app, &app->graph.nodes[to_index], transform,
                    &to_x, &to_y);
            ratio = elapsed / edge->travel_minutes;
            *x = from_x + (to_x - from_x) * ratio;
            *y = from_y + (to_y - from_y) * ratio;
            return;
        }
        elapsed -= edge->travel_minutes;
    }
}

/* 返回车辆在当前状态的绘制颜色。 */
static void vehicle_color(const Vehicle *vehicle, double *r, double *g,
                          double *b)
{
    switch (vehicle->state) {
        case VEHICLE_IDLE:
            *r = 0.96; *g = 0.63; *b = 0.12;
            break;
        case VEHICLE_TRAVELING:
            *r = 0.20; *g = 0.35; *b = 0.78;
            break;
        case VEHICLE_WAITING:
            *r = 0.82; *g = 0.24; *b = 0.24;
            break;
        case VEHICLE_CHARGING:
            *r = 0.16; *g = 0.67; *b = 0.35;
            break;
        default:
            *r = 0.47; *g = 0.47; *b = 0.47;
            break;
    }
}

/* 绘制车辆当前位置和编号。 */
static void draw_vehicles(cairo_t *cr, const UiApp *app,
                          const MapTransform *transform)
{
    int i;

    for (i = 0; i < app->store.vehicle_count; ++i) {
        const Vehicle *vehicle = entity_find_vehicle(
            &app->store, app->store.vehicle_ids[i]);
        int node_index;
        double x;
        double y;
        double r;
        double g;
        double b;
        char label[32];
        int slot = 0;
        int j;

        if (vehicle == NULL) {
            continue;
        }
        node_index = graph_find_node(&app->graph, vehicle->node_id);
        if (node_index < 0) {
            continue;
        }
        moving_vehicle_point(app, vehicle, transform, &x, &y);
        for (j = 0; j < i; ++j) {
            const Vehicle *previous = entity_find_vehicle(
                &app->store, app->store.vehicle_ids[j]);
            if (previous != NULL && previous->node_id == vehicle->node_id) {
                ++slot;
            }
        }
        vehicle_color(vehicle, &r, &g, &b);
        if (vehicle->state == VEHICLE_TRAVELING) {
            /* 行驶中车辆：三角形符号位于路线上。 */
            draw_car_symbol(cr, x, y, 8.0, r, g, b);
            snprintf(label, sizeof(label), "车%d", vehicle->id);
            draw_text(cr, app, x + 10, y - 7, label, 8.5, r, g, b);
        } else {
            /* 静止车辆：三角形符号排在节点下方。 */
            double vx = x - 22.0 + slot * 48.0;
            double vy = y + 42.0;

            draw_car_symbol(cr, vx, vy, 7.0, r, g, b);
            snprintf(label, sizeof(label), "车%d", vehicle->id);
            draw_text(cr, app, vx + 9, vy - 5, label, 8.5, 0.10, 0.10, 0.10);
        }
    }
}

/* 绘制整个路网地图区域。 */
static void on_draw(GtkDrawingArea *area, cairo_t *cr, int width,
                    int height, gpointer user_data)
{
    UiApp *app = user_data;
    MapTransform transform;

    (void)area;
    cairo_set_source_rgb(cr, 0.97, 0.97, 0.97);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0.80, 0.80, 0.80);
    cairo_rectangle(cr, 0.5, 0.5, width - 1.0, height - 1.0);
    cairo_stroke(cr);

    if (!app->loaded) {
        draw_text(cr, app, 20, 30, "请点击“加载数据”查看道路示意图。",
                  13.0, 0.40, 0.40, 0.40);
        return;
    }

    compute_transform(app, (double)width, (double)height, &transform);
    draw_text(cr, app, 12, 8,
              "图例：●蓝=道路节点  ⚡绿圆=充电站  红粗线=推荐路线  橙虚线=行驶路线  ▲=车辆",
              9.5, 0.30, 0.30, 0.30);
    draw_edges(cr, app, &transform);
    draw_selected_path(cr, app, &transform);
    draw_vehicle_routes(cr, app, &transform);
    draw_nodes(cr, app, &transform);
    draw_vehicles(cr, app, &transform);
}

/* ---- 界面构建 ---- */

static void on_button_clicked(GtkWidget *widget, gpointer user_data)
{
    UiApp *app = user_data;
    const char *name = gtk_widget_get_name(widget);

    if (strcmp(name, "load") == 0) {
        load_data(app);
    } else if (strcmp(name, "recommend") == 0) {
        recommend_vehicle(app);
    } else if (strcmp(name, "submit") == 0) {
        submit_request(app);
    } else if (strcmp(name, "submit_all") == 0) {
        submit_all_requests(app);
    } else if (strcmp(name, "step") == 0) {
        run_single_step(app);
    } else if (strcmp(name, "run10") == 0) {
        start_timed_run(app, 10, 0);
    } else if (strcmp(name, "run") == 0) {
        start_timed_run(app, 0, 1);
    } else if (strcmp(name, "pause") == 0) {
        pause_run(app);
    } else if (strcmp(name, "reset") == 0) {
        reset_simulation(app);
    } else if (strcmp(name, "break") == 0) {
        break_charger(app);
    } else if (strcmp(name, "repair") == 0) {
        repair_charger(app);
    } else if (strcmp(name, "vehicles") == 0) {
        log_vehicle_list(app);
    } else if (strcmp(name, "stations") == 0) {
        log_station_list(app);
    } else if (strcmp(name, "charger_status") == 0) {
        log_charger_status(app);
    }
}

static GtkWidget *make_button(UiApp *app, const char *name, const char *label)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_widget_set_name(button, name);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), app);
    return button;
}

/* 创建并返回带标签的单行输入框。 */
static GtkWidget *make_entry_row(GtkWidget *box, const char *label_text,
                                 int default_id)
{
    GtkWidget *label = gtk_label_new(label_text);
    GtkWidget *entry = gtk_entry_new();
    char text_buf[16];

    gtk_box_append(GTK_BOX(box), label);
    gtk_widget_set_size_request(entry, 72, -1);
    snprintf(text_buf, sizeof(text_buf), "%d", default_id);
    gtk_editable_set_text(GTK_EDITABLE(entry), text_buf);
    gtk_box_append(GTK_BOX(box), entry);
    gtk_widget_set_hexpand(entry, FALSE);
    return entry;
}

/* 构建单一主窗口的全部控件。 */
static GtkWidget *build_window(UiApp *app, GMainLoop *loop)
{
    GtkWidget *window;
    GtkWidget *root;
    GtkWidget *header;
    GtkWidget *row1;
    GtkWidget *row2;
    GtkWidget *row3;
    GtkWidget *body;
    GtkWidget *info_box;
    GtkWidget *info_scroll;
    GtkWidget *info_label;

    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window),
                         "城市新能源汽车充电资源调度仿真系统");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 760);
    g_signal_connect(window, "destroy", G_CALLBACK(g_main_loop_quit), loop);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(root, 8);
    gtk_widget_set_margin_end(root, 8);
    gtk_widget_set_margin_top(root, 8);
    gtk_widget_set_margin_bottom(root, 8);
    gtk_window_set_child(GTK_WINDOW(window), root);

    app->status_label = gtk_label_new("仿真时间：0 分钟");
    gtk_box_append(GTK_BOX(root), app->status_label);

    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(root), header);
    app->vehicle_entry = make_entry_row(header, "车辆编号：", 201);
    app->charger_entry = make_entry_row(header, "  充电桩编号：", 1005);

    row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(row1), make_button(app, "load", "加载数据"));
    gtk_box_append(GTK_BOX(row1), make_button(app, "recommend", "站点推荐"));
    gtk_box_append(GTK_BOX(row1), make_button(app, "submit", "提交请求"));
    gtk_box_append(GTK_BOX(row1), make_button(app, "submit_all", "批量提交"));
    gtk_box_append(GTK_BOX(root), row1);

    row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(row2), make_button(app, "step", "单步运行"));
    gtk_box_append(GTK_BOX(row2), make_button(app, "run10", "运行10步"));
    gtk_box_append(GTK_BOX(row2), make_button(app, "run", "开始运行"));
    gtk_box_append(GTK_BOX(row2), make_button(app, "pause", "暂停"));
    gtk_box_append(GTK_BOX(row2), make_button(app, "reset", "重置仿真"));
    gtk_box_append(GTK_BOX(root), row2);

    row3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(row3), make_button(app, "break", "设为故障"));
    gtk_box_append(GTK_BOX(row3), make_button(app, "repair", "修复设备"));
    gtk_box_append(GTK_BOX(row3), make_button(app, "vehicles", "车辆列表"));
    gtk_box_append(GTK_BOX(row3), make_button(app, "stations", "站点列表"));
    gtk_box_append(GTK_BOX(row3),
                   make_button(app, "charger_status", "查看桩状态"));
    gtk_box_append(GTK_BOX(root), row3);

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_vexpand(body, TRUE);
    gtk_box_append(GTK_BOX(root), body);

    info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_size_request(info_box, 420, -1);
    gtk_box_append(GTK_BOX(body), info_box);

    info_label = gtk_label_new("信息显示区域");
    gtk_box_append(GTK_BOX(info_box), info_label);

    info_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(info_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(info_scroll, TRUE);
    gtk_box_append(GTK_BOX(info_box), info_scroll);

    app->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->log_view),
                                GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(info_scroll),
                                  app->log_view);

    app->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(app->drawing_area, 760, 560);
    gtk_widget_set_hexpand(app->drawing_area, TRUE);
    gtk_widget_set_vexpand(app->drawing_area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->drawing_area),
                                   on_draw, app, NULL);
    gtk_box_append(GTK_BOX(body), app->drawing_area);

    return window;
}

/* 程序入口：初始化 GTK、加载数据并进入主循环。 */
int main(int argc, char **argv)
{
    UiApp app;
    GtkWidget *window;
    GMainLoop *loop;

    (void)argc;
    (void)argv;
    set_project_directory();

    gtk_init();

    memset(&app, 0, sizeof(app));
    app.font = pango_font_description_from_string("Microsoft YaHei 11");
    graph_init(&app.graph);
    entity_init(&app.store);
    sim_timer_init(&app.timer, on_timer_tick, &app);

    loop = g_main_loop_new(NULL, FALSE);
    window = build_window(&app, loop);
    gtk_widget_set_visible(window, TRUE);
    load_data(&app);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    sim_timer_stop(&app.timer);
    if (app.loaded) {
        simulation_reset(&app.simulation);
        graph_free(&app.graph);
        entity_free(&app.store);
    }
    pango_font_description_free(app.font);
    return 0;
}
