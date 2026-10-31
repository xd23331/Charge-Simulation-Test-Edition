#include <windows.h>

#include "../module1_graph/graph.h"
#include "../module3_entity/entity.h"
#include "../module5_recommend/recommend.h"
#include "../module6_simulation/simulation.h"
#include "../module7_fileio/fileio.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_LOAD 101
#define ID_RECOMMEND 102
#define ID_SUBMIT 103
#define ID_STEP 104
#define ID_RUN10 105
#define ID_RUNALL 106
#define ID_BREAK 107
#define ID_REPAIR 108
#define ID_RESET 109
#define ID_CHARGER_STATUS 110
#define ID_SUBMIT_ALL 111
#define ID_PAUSE 112
#define ID_SPEED1 113
#define ID_SPEED2 114
#define ID_SPEED5 115
#define ID_BG_TOGGLE 116
#define ID_VEHICLE 201
#define ID_CHARGER 202
#define ID_LOG 203
#define ID_VEHICLE_INFO 204
#define ID_STATION_INFO 205
#define ID_STATS_INFO 206
#define ID_TITLE_LABEL 210
#define ID_GROUP_DATA 211
#define ID_GROUP_SIM 212
#define ID_GROUP_DEVICE 213
#define ID_VEHICLE_LABEL 214
#define ID_STATION_LABEL 215
#define ID_STATS_LABEL 216
#define ID_LOG_LABEL 217
#define ID_CHARGER_LABEL 218
#define ID_ANIMATION_TIMER 301
#define ANIMATION_INTERVAL_MS 350

/* 一辆车在测试界面中使用的行驶路线。 */
typedef struct {
    int vehicle_id;
    int active;
    double start_time;
    PathResult path;
} VisualRoute;

/* 测试窗口使用的后端数据和控件句柄。 */
typedef struct {
    Graph graph;
    EntityStore store;
    Simulation simulation;
    RecommendationList recommendations;
    Recommendation selected;
    int loaded;
    int has_selected;
    HWND window;
    HWND vehicle_edit;
    HWND charger_edit;
    HWND log_edit;
    HWND vehicle_info;
    HWND station_info;
    HWND status_label;
    HWND pause_button;
    HWND stats_info;
    HWND bg_toggle_button;
    HFONT font_title;
    HFONT font_section;
    HFONT font_normal;
    HFONT font_small;
    HBITMAP bg_bmp;
    int bg_w;
    int bg_h;
    int show_background;
    HBITMAP dim_cache_bmp;
    int dim_cache_w;
    int dim_cache_h;
    HBITMAP bg_stretch_cache;
    int bg_cache_w;
    int bg_cache_h;
    VisualRoute routes[MAX_VEHICLES];
    int timer_running;
    int timer_steps_left;
    int timer_until_done;
    int timer_paused;
    int speed_multiplier;
    int selected_node_id;
    int selected_vehicle_id;
    int refresh_counter;
    /* 可拖动分割条布局 */
    int left_panel_width;
    int vehicle_info_h;
    int station_info_h;
    int stats_info_h;
    int log_info_h;
    int drag_mode;       /* 0=无, 1=垂直, 2=水平 */
    int drag_index;       /* 水平分割条索引 */
    int drag_start_mouse; /* 拖动起始鼠标坐标 */
    int drag_start_value; /* 拖动起始值 */
    double map_zoom;      /* 地图缩放比例 */
} TestApp;

static TestApp app;

/* 清除界面保存的车辆移动路线。 */
static void clear_visual_routes(void)
{
    memset(app.routes, 0, sizeof(app.routes));
}

/* 查找一辆车对应的界面移动路线。 */
static VisualRoute *find_visual_route(int vehicle_id)
{
    int i;

    for (i = 0; i < MAX_VEHICLES; ++i) {
        if (app.routes[i].active &&
            app.routes[i].vehicle_id == vehicle_id) {
            return &app.routes[i];
        }
    }
    return NULL;
}

/* 保存车辆提交请求时选择的推荐路线。 */
static void save_visual_route(int vehicle_id, const PathResult *path,
                              double start_time)
{
    VisualRoute *route = find_visual_route(vehicle_id);
    int i;

    if (route == NULL) {
        for (i = 0; i < MAX_VEHICLES; ++i) {
            if (!app.routes[i].active) {
                route = &app.routes[i];
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

/* 把程序工作目录切换到项目根目录，方便双击运行。 */
static void set_project_directory(void)
{
    char path[MAX_PATH];
    char *slash;

    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) {
        return;
    }
    slash = strrchr(path, '\\');
    if (slash == NULL) {
        return;
    }
    *slash = '\0';
    slash = strrchr(path, '\\');
    if (slash != NULL && strcmp(slash + 1, "build") == 0) {
        *slash = '\0';
    }
    SetCurrentDirectoryA(path);
}

/* 创建指定大小和粗细的微软雅黑字体。 */
static HFONT make_font(int height, int weight)
{
    return CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH, "Microsoft YaHei");
}

/* 创建界面使用的全部字体。 */
static void create_fonts(void)
{
    app.font_title = make_font(32, FW_BOLD);
    app.font_section = make_font(22, FW_BOLD);
    app.font_normal = make_font(20, FW_NORMAL);
    app.font_small = make_font(19, FW_NORMAL);
}

/* 销毁界面字体。 */
static void destroy_fonts(void)
{
    DeleteObject(app.font_title);
    DeleteObject(app.font_section);
    DeleteObject(app.font_normal);
    DeleteObject(app.font_small);
}

/* 加载背景图。 */
static void load_background(void)
{
    app.bg_bmp = (HBITMAP)LoadImageA(NULL, "assets/background.bmp",
                                       IMAGE_BITMAP, 0, 0,
                                       LR_LOADFROMFILE);
    if (app.bg_bmp != NULL) {
        BITMAP bm;
        GetObject(app.bg_bmp, sizeof(bm), &bm);
        app.bg_w = bm.bmWidth;
        app.bg_h = bm.bmHeight;
    }
}

/* 绘制窗口背景图，左侧控制区用白色覆盖，地图区保留半透明背景。
   使用缓存位图避免频繁 StretchBlt/CreateCompatibleBitmap。 */
static void draw_background(HDC dc, int width, int height)
{
    RECT left_rect = {0, 0, app.left_panel_width, height};
    HBRUSH white_brush = CreateSolidBrush(RGB(255, 255, 255));
    RECT map_rect = {app.left_panel_width + 5, 15, width - 15, height - 15};
    int map_w = map_rect.right - map_rect.left;
    int map_h = map_rect.bottom - map_rect.top;
    BLENDFUNCTION blend;
    HDC mem_dc;
    HBITMAP old_bmp;

    /* 背景图：预拉伸缓存，窗口大小不变时直接 BitBlt */
    if (app.bg_bmp != NULL && app.show_background) {
        if (app.bg_stretch_cache == NULL ||
            app.bg_cache_w != width || app.bg_cache_h != height) {
            HDC src_dc = CreateCompatibleDC(dc);
            HDC dst_dc = CreateCompatibleDC(dc);
            HBITMAP old_src, old_dst;
            if (app.bg_stretch_cache != NULL) {
                DeleteObject(app.bg_stretch_cache);
            }
            app.bg_stretch_cache = CreateCompatibleBitmap(dc, width, height);
            app.bg_cache_w = width;
            app.bg_cache_h = height;
            old_src = SelectObject(src_dc, app.bg_bmp);
            old_dst = SelectObject(dst_dc, app.bg_stretch_cache);
            StretchBlt(dst_dc, 0, 0, width, height,
                       src_dc, 0, 0, app.bg_w, app.bg_h, SRCCOPY);
            SelectObject(src_dc, old_src);
            SelectObject(dst_dc, old_dst);
            DeleteDC(src_dc);
            DeleteDC(dst_dc);
        }
        mem_dc = CreateCompatibleDC(dc);
        old_bmp = SelectObject(mem_dc, app.bg_stretch_cache);
        BitBlt(dc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);
        SelectObject(mem_dc, old_bmp);
        DeleteDC(mem_dc);
    } else {
        FillRect(dc, &(RECT){0, 0, width, height}, white_brush);
    }
    /* 左侧控制面板区域用纯白色覆盖，保证控件文字清晰。 */
    FillRect(dc, &left_rect, white_brush);
    DeleteObject(white_brush);

    /* 地图区域半透明覆盖层：缓存位图，窗口大小不变时直接 AlphaBlend */
    if (app.dim_cache_bmp == NULL ||
        app.dim_cache_w != map_w || app.dim_cache_h != map_h) {
        HDC tmp_dc = CreateCompatibleDC(dc);
        HBITMAP old_tmp;
        HBRUSH gray_brush;
        RECT fill_rect = {0, 0, map_w, map_h};
        if (app.dim_cache_bmp != NULL) {
            DeleteObject(app.dim_cache_bmp);
        }
        app.dim_cache_bmp = CreateCompatibleBitmap(dc, map_w, map_h);
        app.dim_cache_w = map_w;
        app.dim_cache_h = map_h;
        old_tmp = SelectObject(tmp_dc, app.dim_cache_bmp);
        gray_brush = CreateSolidBrush(RGB(235, 238, 242));
        FillRect(tmp_dc, &fill_rect, gray_brush);
        DeleteObject(gray_brush);
        SelectObject(tmp_dc, old_tmp);
        DeleteDC(tmp_dc);
    }
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 140;
    blend.AlphaFormat = 0;
    mem_dc = CreateCompatibleDC(dc);
    old_bmp = SelectObject(mem_dc, app.dim_cache_bmp);
    AlphaBlend(dc, map_rect.left, map_rect.top, map_w, map_h,
               mem_dc, 0, 0, map_w, map_h, blend);
    SelectObject(mem_dc, old_bmp);
    DeleteDC(mem_dc);
}

/* 更新顶部状态栏文字。 */
static void update_status(const char *text)
{
    if (app.status_label != NULL) {
        SetWindowTextA(app.status_label, text);
    }
}

static void refresh_stats(void);

/* 根据当前仿真状态刷新顶部状态栏。 */
static void refresh_status_bar(void)
{
    char text[128];
    const char *state = app.timer_paused ? "已暂停" :
                        (app.timer_running ? "运行中" : "已暂停");
    snprintf(text, sizeof(text), "仿真时间：%.0f 分钟    状态：%s    速度：%dx",
             app.simulation.current_time, state, app.speed_multiplier);
    update_status(text);
    refresh_stats();
}

/* 在日志文本框末尾追加一行文字，带仿真时间戳。 */
static void log_line(const char *format, ...)
{
    char text[512];
    char line[560];
    int length;
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (app.loaded) {
        snprintf(line, sizeof(line), "[%.0f分] %s\r\n",
                 app.simulation.current_time, text);
    } else {
        snprintf(line, sizeof(line), "%s\r\n", text);
    }

    length = GetWindowTextLengthA(app.log_edit);
    SendMessageA(app.log_edit, EM_SETSEL, length, length);
    SendMessageA(app.log_edit, EM_REPLACESEL, FALSE, (LPARAM)line);
}

/* 向指定的只读多行编辑框追加一行文字。 */
static const char *state_name(VehicleState state);
static const char *charger_state_name(ChargerState state);
static const char *station_name(const Station *station);

static void append_line_to(HWND edit, const char *format, ...)
{
    char text[512];
    char line[520];
    int length;
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    snprintf(line, sizeof(line), "%s\r\n", text);

    length = GetWindowTextLengthA(edit);
    SendMessageA(edit, EM_SETSEL, length, length);
    SendMessageA(edit, EM_REPLACESEL, FALSE, (LPARAM)line);
}

/* 刷新车辆信息区域：清空后写入当前仿真时间和全部车辆状态。 */
static void refresh_vehicle_info(void)
{
    int i;

    if (!app.loaded) {
        SetWindowTextA(app.vehicle_info, "");
        return;
    }
    SetWindowTextA(app.vehicle_info, "");
    append_line_to(app.vehicle_info, "仿真时间：%.0f 分钟",
                    app.simulation.current_time);
    for (i = 0; i < simulation_vehicle_count(&app.simulation); ++i) {
        const Vehicle *vehicle = simulation_vehicle_at(&app.simulation, i);
        append_line_to(app.vehicle_info,
                       "车%d %s 电量%.0f%% 站%d 桩%d",
                       vehicle->id, state_name(vehicle->state),
                       vehicle->battery_percent, vehicle->station_id,
                       vehicle->charger_id);
    }
}

/* 刷新充电站信息区域：清空后写入全部站点和充电桩状态。 */
static void refresh_station_info(void)
{
    int i;

    if (!app.loaded) {
        SetWindowTextA(app.station_info, "");
        return;
    }
    SetWindowTextA(app.station_info, "");
    for (i = 0; i < app.store.station_count; ++i) {
        const Station *station = entity_find_station(
            &app.store, app.store.station_ids[i]);
        const Charger *charger;

        if (station == NULL) {
            continue;
        }
        append_line_to(app.station_info, "站%d %s 等待%d辆",
                       station->id, station_name(station),
                       station->queue_size);
        charger = station->chargers;
        while (charger != NULL) {
            if (charger->state == CHARGER_BUSY &&
                charger->request != NULL &&
                charger->request->vehicle != NULL) {
                append_line_to(app.station_info,
                               "  桩%d %.0fkW %s 车%d 剩%.0f分",
                               charger->id, charger->power_kw,
                               charger_state_name(charger->state),
                               charger->request->vehicle->id,
                               charger->remaining_minutes);
            } else {
                append_line_to(app.station_info, "  桩%d %.0fkW %s",
                               charger->id, charger->power_kw,
                               charger_state_name(charger->state));
            }
            charger = charger->next;
        }
    }
}

/* 刷新统计信息区域。 */
static void refresh_stats(void)
{
    int finished = 0, traveling = 0, waiting = 0, charging = 0, idle = 0;
    double total_wait = 0.0;
    int wait_count = 0;
    int total_chargers = 0, busy_chargers = 0;
    int i;

    if (!app.loaded || app.stats_info == NULL) {
        return;
    }
    for (i = 0; i < simulation_vehicle_count(&app.simulation); ++i) {
        const Vehicle *v = simulation_vehicle_at(&app.simulation, i);
        switch (v->state) {
            case VEHICLE_IDLE: ++idle; break;
            case VEHICLE_TRAVELING: ++traveling; break;
            case VEHICLE_WAITING: ++waiting; break;
            case VEHICLE_CHARGING: ++charging; break;
            case VEHICLE_FINISHED:
                ++finished;
                if (v->request != NULL && v->request->wait_time >= 0) {
                    total_wait += v->request->wait_time;
                    ++wait_count;
                }
                break;
        }
    }
    for (i = 0; i < app.store.station_count; ++i) {
        const Station *st = entity_find_station(&app.store,
                                                  app.store.station_ids[i]);
        const Charger *ch;
        if (st == NULL) continue;
        ch = st->chargers;
        while (ch != NULL) {
            ++total_chargers;
            if (ch->state == CHARGER_BUSY) ++busy_chargers;
            ch = ch->next;
        }
    }
    SetWindowTextA(app.stats_info, "");
    append_line_to(app.stats_info,
        "完成%d 行驶%d 等待%d 充电%d 空闲%d | 平均等待%.1f分 | 桩利用率%.0f%%",
        finished, traveling, waiting, charging, idle,
        wait_count > 0 ? total_wait / wait_count : 0.0,
        total_chargers > 0 ?
        (double)busy_chargers / total_chargers * 100.0 : 0.0);
}

/* 从单行输入框读取一个整数。 */
static int read_number(HWND edit)
{
    char text[32];
    GetWindowTextA(edit, text, sizeof(text));
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

/* 样例数据中的站点英文名只在测试界面中转换为中文。 */
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

/* 刷新车辆信息区域的显示。 */
static void log_status(void)
{
    refresh_vehicle_info();
}

/* 刷新充电站信息区域的显示。 */
static void log_charger_status(void)
{
    if (!app.loaded) {
        log_line("请先加载数据。");
        return;
    }
    refresh_station_info();
    log_line("已刷新各充电站的充电桩状态。");
}

/* 停止测试界面的定时仿真。 */
static void stop_timed_run(int show_status)
{
    if (!app.timer_running && !app.timer_paused) {
        return;
    }
    KillTimer(app.window, ID_ANIMATION_TIMER);
    app.timer_running = 0;
    app.timer_steps_left = 0;
    app.timer_until_done = 0;
    app.timer_paused = 0;
    if (app.pause_button != NULL) {
        SetWindowTextA(app.pause_button, "暂停");
    }
    if (show_status) {
        log_status();
        refresh_station_info();
    }
    refresh_status_bar();
}

/* 加载样例CSV数据并初始化仿真。 */
static void load_data(void)
{
    DataFiles files = {
        "data/input/nodes.csv",
        "data/input/roads.csv",
        "data/input/stations.csv",
        "data/input/chargers.csv",
        "data/input/vehicles.csv"
    };
    LoadError error;

    stop_timed_run(0);
    if (app.loaded) {
        simulation_reset(&app.simulation);
        graph_free(&app.graph);
        entity_free(&app.store);
    }
    graph_init(&app.graph);
    entity_init(&app.store);
    SetWindowTextA(app.log_edit, "");
    SetWindowTextA(app.vehicle_info, "");
    SetWindowTextA(app.station_info, "");
    app.loaded = 0;
    app.has_selected = 0;
    app.speed_multiplier = 1;
    app.show_background = 1;
    app.selected_node_id = -1;
    app.selected_vehicle_id = -1;
    clear_visual_routes();

    if (fileio_load_all(&app.graph, &app.store, &files, &error) != 0) {
        log_line("数据加载失败：%s，第 %d 行，%s",
                 error.file, error.line, file_error_name(error.message));
        return;
    }
    if (simulation_init(&app.simulation, &app.graph, &app.store, 1.0) != 0) {
        log_line("仿真初始化失败。可检查数据之间的关联是否正确。");
        return;
    }
    app.loaded = 1;
    log_line("数据加载成功：%d 个道路节点，%d 个充电站，%d 辆车。",
             app.graph.node_count, app.store.station_count,
             app.store.vehicle_count);
    log_status();
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 为输入的车辆计算并显示全部站点推荐。 */
static void recommend_vehicle(void)
{
    int vehicle_id = read_number(app.vehicle_edit);
    Vehicle *vehicle;
    const Recommendation *best;
    int i;

    if (!app.loaded) {
        log_line("请先加载数据。");
        return;
    }
    vehicle = entity_find_vehicle(&app.store, vehicle_id);
    if (vehicle == NULL) {
        log_line("没有找到编号为 %d 的车辆。", vehicle_id);
        return;
    }
    if (recommend_all(&app.graph, &app.store, vehicle,
                      &app.recommendations) != 0) {
        log_line("充电站推荐计算失败。");
        return;
    }

    log_line("车辆 %d 的充电站推荐结果：", vehicle_id);
    for (i = 0; i < app.recommendations.count; ++i) {
        Recommendation *item = &app.recommendations.items[i];
        if (item->valid) {
            log_line("  充电站 %d：行驶 %.1f 分钟，等待 %.1f 分钟，"
                     "充电 %.1f 分钟，总计 %.1f 分钟",
                     item->station_id, item->travel_minutes,
                     item->wait_minutes, item->charge_minutes,
                     item->total_minutes);
        } else {
            log_line("  充电站 %d：不可选，原因：%s",
                     item->station_id, reason_name(item->reason));
        }
    }
    best = recommend_best(&app.recommendations);
    if (best == NULL) {
        app.has_selected = 0;
        log_line("当前没有可用的充电站。");
    } else {
        app.selected = *best;
        app.has_selected = 1;
        log_line("系统推荐：充电站 %d（地图中的红线为推荐路线）。",
                 best->station_id);
    }
    InvalidateRect(app.window, NULL, TRUE);
}

/* 提交最近一次推荐得到的充电请求。 */
static void submit_request(void)
{
    int vehicle_id = read_number(app.vehicle_edit);

    if (!app.loaded || !app.has_selected) {
        log_line("请先为车辆计算充电站推荐。");
        return;
    }
    if (simulation_submit(&app.simulation, vehicle_id, &app.selected) != 0) {
        log_line("车辆 %d 的充电请求提交失败。", vehicle_id);
        return;
    }
    save_visual_route(vehicle_id, &app.selected.path,
                      app.simulation.current_time);
    log_line("车辆 %d 已提交请求 %d，目标为充电站 %d。",
             vehicle_id,
             entity_find_vehicle(&app.store, vehicle_id)->request->id,
             app.selected.station_id);
    app.has_selected = 0;
    log_status();
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 为全部空闲车辆计算推荐并批量提交充电请求。 */
static void submit_all_requests(void)
{
    int submitted = 0;
    int failed = 0;
    int i;

    if (!app.loaded) {
        log_line("请先加载数据。");
        return;
    }
    stop_timed_run(0);
    for (i = 0; i < app.store.vehicle_count; ++i) {
        Vehicle *vehicle = entity_find_vehicle(
            &app.store, app.store.vehicle_ids[i]);
        RecommendationList list;
        const Recommendation *best;

        if (vehicle == NULL || vehicle->state != VEHICLE_IDLE) {
            continue;
        }
        if (recommend_all(&app.graph, &app.store, vehicle, &list) != 0) {
            ++failed;
            continue;
        }
        best = recommend_best(&list);
        if (best == NULL ||
            simulation_submit(&app.simulation, vehicle->id, best) != 0) {
            ++failed;
            continue;
        }
        save_visual_route(vehicle->id, &best->path,
                          app.simulation.current_time);
        ++submitted;
    }
    app.has_selected = 0;
    log_line("批量提交完成：成功 %d 辆，失败 %d 辆。",
             submitted, failed);
    log_line("点击“运行到底”可观察车辆到站、排队和充电过程。");
    refresh_vehicle_info();
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 推进指定次数的仿真时间步。 */
static void run_steps(int count)
{
    int i;

    if (!app.loaded) {
        log_line("请先加载数据。");
        return;
    }
    stop_timed_run(0);
    for (i = 0; i < count; ++i) {
        simulation_step(&app.simulation);
    }
    log_status();
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 启动定时仿真，使地图可以逐步显示车辆移动。 */
static void start_timed_run(int steps, int until_done)
{
    int interval;

    if (!app.loaded) {
        log_line("请先加载数据。");
        return;
    }
    if (!simulation_has_active(&app.simulation)) {
        log_line("当前没有正在执行的充电任务。");
        return;
    }
    stop_timed_run(0);
    app.timer_steps_left = steps;
    app.timer_until_done = until_done;
    app.timer_paused = 0;
    /* 把焦点移到主窗口，避免按钮/编辑框有焦点时被频繁更新触发系统提示音 */
    SetFocus(app.window);
    interval = ANIMATION_INTERVAL_MS / app.speed_multiplier;
    if (interval < 100) interval = 100;
    app.timer_running = SetTimer(app.window, ID_ANIMATION_TIMER,
                                 interval, NULL) != 0;
    if (!app.timer_running) {
        log_line("无法启动定时动画。");
    } else if (until_done) {
        log_line("开始连续运行（%dx速）。", app.speed_multiplier);
    } else {
        log_line("开始运行 %d 步（%dx速）。", steps, app.speed_multiplier);
    }
    if (app.pause_button != NULL) {
        SetWindowTextA(app.pause_button, "暂停");
    }
    refresh_status_bar();
}

/* 暂停或继续当前定时仿真。 */
static void toggle_pause(void)
{
    int interval;

    if (!app.timer_running && !app.timer_paused) {
        log_line("当前没有正在运行的仿真。");
        return;
    }
    if (app.timer_paused) {
        interval = ANIMATION_INTERVAL_MS / app.speed_multiplier;
        if (interval < 100) interval = 100;
        if (SetTimer(app.window, ID_ANIMATION_TIMER, interval, NULL) != 0) {
            app.timer_paused = 0;
            app.timer_running = 1;
            SetWindowTextA(app.pause_button, "暂停");
            log_line("仿真继续运行。");
        }
    } else {
        KillTimer(app.window, ID_ANIMATION_TIMER);
        app.timer_paused = 1;
        app.timer_running = 0;
        SetWindowTextA(app.pause_button, "继续");
        log_line("仿真已暂停。");
    }
    refresh_status_bar();
}

/* 设置仿真倍速。 */
static void set_speed(int multiplier)
{
    app.speed_multiplier = multiplier;
    if (app.timer_running && !app.timer_paused) {
        int interval = ANIMATION_INTERVAL_MS / multiplier;
        if (interval < 100) interval = 100;
        KillTimer(app.window, ID_ANIMATION_TIMER);
        SetTimer(app.window, ID_ANIMATION_TIMER, interval, NULL);
    }
    log_line("仿真速度设为 %dx。", multiplier);
    refresh_status_bar();
}

/* 切换背景图显示。 */
static void toggle_background(void)
{
    app.show_background = !app.show_background;
    if (app.bg_toggle_button != NULL) {
        SetWindowTextA(app.bg_toggle_button,
                       app.show_background ? "隐藏背景" : "显示背景");
    }
    InvalidateRect(app.window, NULL, TRUE);
}

/* 定时推进一个时间步并刷新地图。 */
static void timed_run_step(void)
{
    int finished;
    RECT client;
    RECT map_area;

    if (!app.timer_running || !app.loaded) {
        return;
    }
    simulation_step(&app.simulation);
    if (!app.timer_until_done) {
        --app.timer_steps_left;
    }
    /* 每2步才更新编辑框，避免频繁SetWindowText触发系统提示音 */
    ++app.refresh_counter;
    if (app.refresh_counter >= 2) {
        app.refresh_counter = 0;
        refresh_vehicle_info();
        refresh_station_info();
        refresh_status_bar();
    }
    /* 只重绘地图区域 */
    GetClientRect(app.window, &client);
    map_area.left = app.left_panel_width + 5;
    map_area.top = 15;
    map_area.right = client.right - 15;
    map_area.bottom = client.bottom - 15;
    InvalidateRect(app.window, &map_area, FALSE);
    finished = !simulation_has_active(&app.simulation) ||
               (!app.timer_until_done && app.timer_steps_left <= 0);
    if (finished) {
        refresh_vehicle_info();
        refresh_station_info();
        refresh_status_bar();
        log_line("定时运行结束。");
        stop_timed_run(1);
    }
}

/* 设置输入编号的充电桩为故障。 */
static void break_charger(void)
{
    int charger_id = read_number(app.charger_edit);
    if (!app.loaded ||
        simulation_break_charger(&app.simulation, charger_id) != 0) {
        log_line("无法将充电桩 %d 设置为故障，请检查编号或当前状态。",
                 charger_id);
        return;
    }
    log_line("充电桩 %d 已设置为故障。", charger_id);
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 恢复输入编号的故障充电桩。 */
static void repair_charger(void)
{
    int charger_id = read_number(app.charger_edit);
    if (!app.loaded ||
        simulation_repair_charger(&app.simulation, charger_id) != 0) {
        log_line("无法修复充电桩 %d，请检查编号或当前状态。", charger_id);
        return;
    }
    log_line("充电桩 %d 已修复，可以继续使用。", charger_id);
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 重置全部仿真状态但保留已经加载的基础数据。 */
static void reset_simulation(void)
{
    if (!app.loaded) {
        return;
    }
    stop_timed_run(0);
    simulation_reset(&app.simulation);
    app.has_selected = 0;
    clear_visual_routes();
    log_line("仿真状态已重置。");
    log_status();
    refresh_station_info();
    refresh_status_bar();
    InvalidateRect(app.window, NULL, TRUE);
}

/* 把路网坐标转换为绘图区中的窗口坐标。 */
static POINT node_point(const GraphNode *node, const RECT *area)
{
    POINT point;
    double scale = 1.35 * app.map_zoom;
    /* 横纵方向使用同一比例，使线段长度能反映道路距离。 */
    point.x = area->left + 30 + (int)(node->x * scale);
    point.y = area->top + 20 + (int)(node->y * scale);
    return point;
}

/* 查找两个相邻节点之间的道路数据。 */
static const Edge *find_route_edge(int from_index, int to_index)
{
    const Edge *edge = app.graph.nodes[from_index].edges;

    while (edge != NULL) {
        if (edge->to_index == to_index) {
            return edge;
        }
        edge = edge->next;
    }
    return NULL;
}

/* 根据仿真时间计算车辆在推荐路线上的显示位置。 */
static POINT moving_vehicle_point(const Vehicle *vehicle, const RECT *area)
{
    VisualRoute *route = find_visual_route(vehicle->id);
    int node_index = graph_find_node(&app.graph, vehicle->node_id);
    POINT fallback = node_point(&app.graph.nodes[node_index], area);
    double elapsed;
    int i;

    if (vehicle->state != VEHICLE_TRAVELING || route == NULL ||
        route->path.path_length < 2) {
        return fallback;
    }
    elapsed = app.simulation.current_time - route->start_time;
    if (elapsed < 0.0) {
        elapsed = 0.0;
    }
    for (i = 0; i + 1 < route->path.path_length; ++i) {
        int from_index = graph_find_node(
            &app.graph, route->path.node_ids[i]);
        int to_index = graph_find_node(
            &app.graph, route->path.node_ids[i + 1]);
        const Edge *edge;

        if (from_index < 0 || to_index < 0) {
            return fallback;
        }
        edge = find_route_edge(from_index, to_index);
        if (edge == NULL || edge->travel_minutes <= 0.0) {
            return fallback;
        }
        if (elapsed <= edge->travel_minutes) {
            POINT from = node_point(&app.graph.nodes[from_index], area);
            POINT to = node_point(&app.graph.nodes[to_index], area);
            double ratio = elapsed / edge->travel_minutes;
            POINT current;

            current.x = from.x + (int)((to.x - from.x) * ratio);
            current.y = from.y + (int)((to.y - from.y) * ratio);
            return current;
        }
        elapsed -= edge->travel_minutes;
    }
    node_index = graph_find_node(
        &app.graph, route->path.node_ids[route->path.path_length - 1]);
    if (node_index >= 0) {
        return node_point(&app.graph.nodes[node_index], area);
    }
    return fallback;
}

/* 判断一个路网节点是否放置了充电站。 */
static const Station *station_on_node(int node_id)
{
    int i;
    for (i = 0; i < app.store.station_count; ++i) {
        const Station *station = entity_find_station(
            &app.store, app.store.station_ids[i]);
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

/* 绘制全部道路。 */
static void draw_edges(HDC dc, const RECT *area)
{
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(170, 170, 170));
    HPEN old_pen = SelectObject(dc, pen);
    HFONT font = CreateFontA(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH, "Microsoft YaHei");
    HFONT old_font = SelectObject(dc, font);
    int i;

    SetTextColor(dc, RGB(90, 90, 90));
    SetBkColor(dc, RGB(248, 248, 248));
    SetBkMode(dc, OPAQUE);
    for (i = 0; i < app.graph.node_count; ++i) {
        Edge *edge = app.graph.nodes[i].edges;
        POINT from = node_point(&app.graph.nodes[i], area);
        while (edge != NULL) {
            /* 当前样例道路均为双向，只绘制一次，避免线条和文字重复。 */
            if (i < edge->to_index) {
                POINT to = node_point(&app.graph.nodes[edge->to_index], area);
                char label[32];
                int label_x;
                int label_y;

                MoveToEx(dc, from.x, from.y, NULL);
                LineTo(dc, to.x, to.y);
                snprintf(label, sizeof(label), "%.1f公里", edge->distance_km);
                label_x = (from.x + to.x) / 2 + 4;
                label_y = (from.y + to.y) / 2 - 13;
                TextOutA(dc, label_x, label_y, label, (int)strlen(label));
            }
            edge = edge->next;
        }
    }
    SelectObject(dc, old_font);
    DeleteObject(font);
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

/* 用红色绘制最近一次推荐路径。 */
static void draw_selected_path(HDC dc, const RECT *area)
{
    HPEN pen;
    HPEN old_pen;
    int i;

    if (!app.has_selected || !app.selected.valid) {
        return;
    }
    pen = CreatePen(PS_SOLID, 4, RGB(220, 50, 50));
    old_pen = SelectObject(dc, pen);
    for (i = 0; i + 1 < app.selected.path.path_length; ++i) {
        int from_index = graph_find_node(
            &app.graph, app.selected.path.node_ids[i]);
        int to_index = graph_find_node(
            &app.graph, app.selected.path.node_ids[i + 1]);
        POINT from = node_point(&app.graph.nodes[from_index], area);
        POINT to = node_point(&app.graph.nodes[to_index], area);
        MoveToEx(dc, from.x, from.y, NULL);
        LineTo(dc, to.x, to.y);
    }
    SelectObject(dc, old_pen);
    DeleteObject(pen);
}

/* 显示车辆路线：选中车辆用粗蓝色实线高亮，其他行驶车辆用浅色虚线。 */
static void draw_vehicle_routes(HDC dc, const RECT *area)
{
    HPEN pen_normal = CreatePen(PS_DASH, 1, RGB(200, 160, 100));
    HPEN pen_selected = CreatePen(PS_SOLID, 4, RGB(30, 100, 220));
    HPEN old_pen;
    int i;

    for (i = 0; i < MAX_VEHICLES; ++i) {
        VisualRoute *route = &app.routes[i];
        Vehicle *vehicle;
        int j;
        int is_selected;

        if (!route->active) {
            continue;
        }
        vehicle = entity_find_vehicle(&app.store, route->vehicle_id);
        if (vehicle == NULL || vehicle->state != VEHICLE_TRAVELING) {
            continue;
        }
        is_selected = (app.selected_vehicle_id == vehicle->id);
        old_pen = SelectObject(dc, is_selected ? pen_selected : pen_normal);
        for (j = 0; j + 1 < route->path.path_length; ++j) {
            int from_index = graph_find_node(
                &app.graph, route->path.node_ids[j]);
            int to_index = graph_find_node(
                &app.graph, route->path.node_ids[j + 1]);
            if (from_index >= 0 && to_index >= 0) {
                POINT from = node_point(&app.graph.nodes[from_index], area);
                POINT to = node_point(&app.graph.nodes[to_index], area);
                MoveToEx(dc, from.x, from.y, NULL);
                LineTo(dc, to.x, to.y);
            }
        }
        SelectObject(dc, old_pen);
    }
    DeleteObject(pen_normal);
    DeleteObject(pen_selected);
}

/* 绘制节点、站点名称和节点编号。充电站按负载着色。 */
static void draw_nodes(HDC dc, const RECT *area)
{
    int i;

    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < app.graph.node_count; ++i) {
        const GraphNode *node = &app.graph.nodes[i];
        const Station *station = station_on_node(node->id);
        POINT point = node_point(node, area);
        COLORREF node_color;
        HBRUSH brush;
        HBRUSH old_brush;
        char label[96];
        SIZE size;
        int free_count = 0, busy_count = 0, fault_count = 0;
        int total_count;
        double load_rate;

        /* 先统计站点充电桩状态，用于着色 */
        if (station != NULL) {
            count_station_chargers(station, &free_count,
                                   &busy_count, &fault_count);
        }
        total_count = free_count + busy_count + fault_count;
        load_rate = total_count > 0 ? (double)busy_count / total_count : 0.0;

        /* 节点着色：非站点蓝色；站点按负载/故障着色 */
        if (station == NULL) {
            node_color = RGB(60, 130, 220);
        } else if (fault_count > 0) {
            node_color = RGB(220, 50, 50);  /* 有故障桩：红色 */
        } else if (load_rate >= 0.60) {
            node_color = RGB(220, 80, 30);  /* 高负载：橙红色 */
        } else if (load_rate >= 0.30) {
            node_color = RGB(240, 180, 30); /* 中等负载：黄色 */
        } else {
            node_color = RGB(40, 170, 90);  /* 低负载：绿色 */
        }

        brush = CreateSolidBrush(node_color);
        old_brush = SelectObject(dc, brush);
        Ellipse(dc, point.x - 9, point.y - 9, point.x + 9, point.y + 9);
        SelectObject(dc, old_brush);
        DeleteObject(brush);
        SetTextColor(dc, RGB(20, 20, 20));
        snprintf(label, sizeof(label), "%d", node->id);
        GetTextExtentPoint32A(dc, label, (int)strlen(label), &size);
        TextOutA(dc, point.x - size.cx / 2, point.y - size.cy / 2,
                 label, (int)strlen(label));
        if (station == NULL) {
            continue;
        } else {
            snprintf(label, sizeof(label), "站%d %s",
                     station->id, station_name(station));
            GetTextExtentPoint32A(dc, label, (int)strlen(label), &size);
            TextOutA(dc, point.x - size.cx / 2, point.y + 12,
                     label, (int)strlen(label));
            snprintf(label, sizeof(label), "空%d 充%d 故%d 等待%d",
                     free_count, busy_count, fault_count,
                     station->queue_size);
            GetTextExtentPoint32A(dc, label, (int)strlen(label), &size);
            TextOutA(dc, point.x - size.cx / 2, point.y + 28,
                     label, (int)strlen(label));
        }
    }
}

/* 绘制车辆当前位置和编号，按SOC着色：绿(充足)/黄(偏低)/红(紧急)。 */
static void draw_vehicles(HDC dc, const RECT *area)
{
    int i;

    for (i = 0; i < app.store.vehicle_count; ++i) {
        const Vehicle *vehicle = entity_find_vehicle(
            &app.store, app.store.vehicle_ids[i]);
        int node_index = graph_find_node(&app.graph, vehicle->node_id);
        if (node_index >= 0) {
            POINT point = moving_vehicle_point(vehicle, area);
            COLORREF vcolor;
            HBRUSH brush;
            HBRUSH old_brush;
            char label[32];
            int slot = 0;
            int j;
            int vehicle_x;
            int vehicle_y;

            if (vehicle->battery_percent >= 50.0) {
                vcolor = RGB(40, 170, 90);
            } else if (vehicle->battery_percent >= 20.0) {
                vcolor = RGB(240, 180, 30);
            } else {
                vcolor = RGB(220, 50, 50);
            }
            if (app.selected_vehicle_id == vehicle->id) {
                vcolor = RGB(120, 60, 220);
            }
            brush = CreateSolidBrush(vcolor);
            old_brush = SelectObject(dc, brush);

            for (j = 0; j < i; ++j) {
                const Vehicle *previous = entity_find_vehicle(
                    &app.store, app.store.vehicle_ids[j]);
                if (previous != NULL && previous->node_id == vehicle->node_id) {
                    ++slot;
                }
            }
            if (vehicle->state == VEHICLE_TRAVELING) {
                vehicle_x = point.x - 9;
                vehicle_y = point.y - 7;
            } else {
                vehicle_x = point.x - 27 + slot * 60;
                vehicle_y = point.y + 47;
            }
            /* 绘制小车形状：车身 + 车顶 */
            Rectangle(dc, vehicle_x, vehicle_y + 4,
                      vehicle_x + 18, vehicle_y + 14);
            Rectangle(dc, vehicle_x + 4, vehicle_y,
                      vehicle_x + 14, vehicle_y + 6);
            SelectObject(dc, old_brush);
            DeleteObject(brush);
            snprintf(label, sizeof(label), "车%d", vehicle->id);
            TextOutA(dc, vehicle_x + 21, vehicle_y + 2,
                     label, (int)strlen(label));
        }
    }
}

/* 在地图上标注推荐候选站点：绿=最优、黄=备选、红=不可达。 */
static void draw_recommendation_stations(HDC dc, const RECT *area)
{
    int i;
    const Recommendation *best = recommend_best(&app.recommendations);

    if (app.recommendations.count <= 0) {
        return;
    }
    for (i = 0; i < app.recommendations.count; ++i) {
        const Recommendation *rec = &app.recommendations.items[i];
        const Station *station = entity_find_station(&app.store, rec->station_id);
        int node_index;
        POINT point;
        COLORREF ring_color;
        HBRUSH brush;
        HPEN pen;
        HPEN old_pen;
        HBRUSH old_brush;

        if (station == NULL) continue;
        node_index = graph_find_node(&app.graph, station->node_id);
        if (node_index < 0) continue;
        point = node_point(&app.graph.nodes[node_index], area);

        if (!rec->valid) {
            ring_color = RGB(200, 50, 50);
        } else if (best != NULL && rec->station_id == best->station_id) {
            ring_color = RGB(40, 170, 90);
        } else {
            ring_color = RGB(240, 180, 30);
        }
        pen = CreatePen(PS_SOLID, 3, ring_color);
        brush = (HBRUSH)GetStockObject(NULL_BRUSH);
        old_pen = SelectObject(dc, pen);
        old_brush = SelectObject(dc, brush);
        Ellipse(dc, point.x - 14, point.y - 14, point.x + 14, point.y + 14);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
        DeleteObject(pen);
    }
}

/* 在地图右下角绘制固定图例面板，分三组：车辆电量、充电站、推荐站点。 */
static void draw_legend(HDC dc, const RECT *area)
{
    int legend_w = 230;
    int legend_h = 220;
    int lx = area->right - legend_w - 12;
    int ly = area->bottom - legend_h - 12;
    RECT bg_rect = {lx, ly, lx + legend_w, ly + legend_h};
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    HFONT title_font, section_font, item_font;
    HFONT old_font;
    HPEN line_pen;
    int line_y;

    FillRect(dc, &bg_rect, bg_brush);
    DeleteObject(bg_brush);
    FrameRect(dc, &bg_rect, GetStockObject(GRAY_BRUSH));

    title_font = CreateFontA(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                              DEFAULT_PITCH, "Microsoft YaHei");
    section_font = CreateFontA(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH, "Microsoft YaHei");
    item_font = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH, "Microsoft YaHei");
    line_pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));

    SetBkMode(dc, TRANSPARENT);

    /* 主标题 */
    old_font = SelectObject(dc, title_font);
    SetTextColor(dc, RGB(30, 30, 30));
    TextOutA(dc, lx + 10, ly + 6, "图例", (int)strlen("图例"));
    SelectObject(dc, line_pen);
    MoveToEx(dc, lx + 8, ly + 28, NULL);
    LineTo(dc, lx + legend_w - 8, ly + 28);
    line_y = ly + 34;

    /* ===== 第一组：车辆电量 ===== */
    SelectObject(dc, section_font);
    SetTextColor(dc, RGB(60, 60, 60));
    TextOutA(dc, lx + 10, line_y, "车辆电量", (int)strlen("车辆电量"));
    line_y += 20;
    SelectObject(dc, item_font);
    SetTextColor(dc, RGB(40, 40, 40));
    {
        HBRUSH b1 = CreateSolidBrush(RGB(40, 170, 90));
        HBRUSH b2 = CreateSolidBrush(RGB(240, 180, 30));
        HBRUSH b3 = CreateSolidBrush(RGB(220, 50, 50));
        HBRUSH old;
        old = SelectObject(dc, b1);
        Rectangle(dc, lx + 14, line_y, lx + 26, line_y + 12);
        SelectObject(dc, old);
        TextOutA(dc, lx + 32, line_y, "充足  ≥50%", (int)strlen("充足  ≥50%"));
        line_y += 18;
        old = SelectObject(dc, b2);
        Rectangle(dc, lx + 14, line_y, lx + 26, line_y + 12);
        SelectObject(dc, old);
        TextOutA(dc, lx + 32, line_y, "偏低  20%-50%", (int)strlen("偏低  20%-50%"));
        line_y += 18;
        old = SelectObject(dc, b3);
        Rectangle(dc, lx + 14, line_y, lx + 26, line_y + 12);
        SelectObject(dc, old);
        TextOutA(dc, lx + 32, line_y, "紧急  <20%", (int)strlen("紧急  <20%"));
        DeleteObject(b1); DeleteObject(b2); DeleteObject(b3);
    }
    line_y += 20;

    /* ===== 第二组：充电站 ===== */
    SelectObject(dc, section_font);
    SetTextColor(dc, RGB(60, 60, 60));
    TextOutA(dc, lx + 10, line_y, "充电站负载", (int)strlen("充电站负载"));
    line_y += 20;
    SelectObject(dc, item_font);
    SetTextColor(dc, RGB(40, 40, 40));
    {
        HBRUSH c1 = CreateSolidBrush(RGB(40, 170, 90));
        HBRUSH c2 = CreateSolidBrush(RGB(240, 180, 30));
        HBRUSH c3 = CreateSolidBrush(RGB(220, 80, 30));
        HBRUSH c4 = CreateSolidBrush(RGB(220, 50, 50));
        HBRUSH oldc;
        oldc = SelectObject(dc, c1);
        Rectangle(dc, lx + 14, line_y, lx + 26, line_y + 12);
        SelectObject(dc, c2);
        Rectangle(dc, lx + 66, line_y, lx + 78, line_y + 12);
        SelectObject(dc, c3);
        Rectangle(dc, lx + 118, line_y, lx + 130, line_y + 12);
        SelectObject(dc, c4);
        Rectangle(dc, lx + 170, line_y, lx + 182, line_y + 12);
        SelectObject(dc, oldc);
        DeleteObject(c1); DeleteObject(c2); DeleteObject(c3); DeleteObject(c4);
    }
    TextOutA(dc, lx + 30, line_y, "空闲", (int)strlen("空闲"));
    TextOutA(dc, lx + 82, line_y, "中等", (int)strlen("中等"));
    TextOutA(dc, lx + 134, line_y, "繁忙", (int)strlen("繁忙"));
    TextOutA(dc, lx + 186, line_y, "故障", (int)strlen("故障"));
    line_y += 20;

    /* ===== 第三组：推荐站点 ===== */
    SelectObject(dc, section_font);
    SetTextColor(dc, RGB(60, 60, 60));
    TextOutA(dc, lx + 10, line_y, "推荐站点", (int)strlen("推荐站点"));
    line_y += 20;
    SelectObject(dc, item_font);
    SetTextColor(dc, RGB(40, 40, 40));
    {
        HPEN p1 = CreatePen(PS_SOLID, 3, RGB(40, 170, 90));
        HPEN p2 = CreatePen(PS_SOLID, 3, RGB(240, 180, 30));
        HPEN p3 = CreatePen(PS_SOLID, 3, RGB(200, 50, 50));
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH oldb;
        HPEN oldp;
        oldp = SelectObject(dc, p1); oldb = SelectObject(dc, nb);
        Ellipse(dc, lx + 12, line_y, lx + 26, line_y + 14);
        SelectObject(dc, p2);
        Ellipse(dc, lx + 80, line_y, lx + 94, line_y + 14);
        SelectObject(dc, p3);
        Ellipse(dc, lx + 148, line_y, lx + 162, line_y + 14);
        SelectObject(dc, oldp); SelectObject(dc, oldb);
        DeleteObject(p1); DeleteObject(p2); DeleteObject(p3);
    }
    TextOutA(dc, lx + 30, line_y, "最优", (int)strlen("最优"));
    TextOutA(dc, lx + 98, line_y, "备选", (int)strlen("备选"));
    TextOutA(dc, lx + 166, line_y, "不可达", (int)strlen("不可达"));
    line_y += 22;

    /* 底部提示 */
    SelectObject(dc, item_font);
    SetTextColor(dc, RGB(120, 120, 120));
    TextOutA(dc, lx + 10, line_y, "点击节点/车辆查看详情",
             (int)strlen("点击节点/车辆查看详情"));

    SelectObject(dc, old_font);
    DeleteObject(title_font);
    DeleteObject(section_font);
    DeleteObject(item_font);
    DeleteObject(line_pen);
}

/* 绘制路网测试区域。 */
static void draw_map(HDC dc)
{
    RECT client;
    RECT area;
    GetClientRect(app.window, &client);
    area.left = app.left_panel_width + 5;
    area.top = 15;
    area.right = client.right - 15;
    area.bottom = client.bottom - 15;
    FrameRect(dc, &area, GetStockObject(GRAY_BRUSH));
    if (!app.loaded) {
        const char *message = "请加载数据后查看道路示意图。";
        TextOutA(dc, 450, 50, message, (int)strlen(message));
        return;
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(70, 70, 70));
    {
        HFONT legend_font = CreateFontA(-16, 0, 0, 0, FW_NORMAL, FALSE,
                                         FALSE, FALSE, GB2312_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         DEFAULT_QUALITY, DEFAULT_PITCH,
                                         "Microsoft YaHei");
        HFONT old_font = SelectObject(dc, legend_font);
        const char *legend = "道路标注单位：公里，线段长度按道路距离比例绘制";
        TextOutA(dc, area.left + 12, area.top + 8,
                 legend, (int)strlen(legend));
        SelectObject(dc, old_font);
        DeleteObject(legend_font);
    }
    /* 右上角显示当前缩放比例 */
    {
        char zoom_text[32];
        HFONT zf = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                GB2312_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH, "Microsoft YaHei");
        HFONT old_zf = SelectObject(dc, zf);
        SetTextColor(dc, RGB(60, 60, 60));
        snprintf(zoom_text, sizeof(zoom_text), "缩放 %.0f%%  (Ctrl+滚轮)",
                 app.map_zoom * 100);
        TextOutA(dc, area.right - 180, area.top + 8,
                 zoom_text, (int)strlen(zoom_text));
        SelectObject(dc, old_zf);
        DeleteObject(zf);
    }
    draw_edges(dc, &area);
    draw_selected_path(dc, &area);
    draw_vehicle_routes(dc, &area);
    draw_recommendation_stations(dc, &area);
    draw_nodes(dc, &area);
    draw_vehicles(dc, &area);
    draw_legend(dc, &area);
}

/* 处理地图区域的鼠标点击，选中节点或车辆并显示详情。 */
static void handle_map_click(int x, int y)
{
    RECT client;
    RECT area;
    int i;
    int found = 0;

    if (!app.loaded) return;
    GetClientRect(app.window, &client);
    area.left = app.left_panel_width + 5;
    area.top = 15;
    area.right = client.right - 15;
    area.bottom = client.bottom - 15;
    if (x < area.left || x > area.right || y < area.top || y > area.bottom) {
        return;
    }

    /* 检查车辆点击 */
    for (i = 0; i < app.store.vehicle_count; ++i) {
        const Vehicle *vehicle = entity_find_vehicle(
            &app.store, app.store.vehicle_ids[i]);
        int node_index = graph_find_node(&app.graph, vehicle->node_id);
        if (node_index >= 0) {
            POINT point = moving_vehicle_point(vehicle, &area);
            int vx, vy;
            int slot = 0, j;
            for (j = 0; j < i; ++j) {
                const Vehicle *prev = entity_find_vehicle(
                    &app.store, app.store.vehicle_ids[j]);
                if (prev != NULL && prev->node_id == vehicle->node_id) ++slot;
            }
            if (vehicle->state == VEHICLE_TRAVELING) {
                vx = point.x; vy = point.y;
            } else {
                vx = point.x - 18 + slot * 58;
                vy = point.y + 54;
            }
            if (abs(x - vx) < 20 && abs(y - vy) < 20) {
                app.selected_vehicle_id = vehicle->id;
                app.selected_node_id = -1;
                log_line("选中车辆 %d：状态%s，电量%.0f%%，目标%.0f%%，"
                         "容量%.0fkWh，当前站%d桩%d",
                         vehicle->id, state_name(vehicle->state),
                         vehicle->battery_percent, vehicle->target_percent,
                         vehicle->capacity_kwh, vehicle->station_id,
                         vehicle->charger_id);
                found = 1;
                break;
            }
        }
    }

    /* 检查节点点击 */
    if (!found) {
        for (i = 0; i < app.graph.node_count; ++i) {
            const GraphNode *node = &app.graph.nodes[i];
            POINT point = node_point(node, &area);
            if (abs(x - point.x) < 14 && abs(y - point.y) < 14) {
                const Station *station = station_on_node(node->id);
                app.selected_node_id = node->id;
                app.selected_vehicle_id = -1;
                if (station != NULL) {
                    int fc, bc, ftc;
                    count_station_chargers(station, &fc, &bc, &ftc);
                    log_line("选中站点 %d（节点%d）：%s，空闲%d 充电%d 故障%d 等待%d",
                             station->id, node->id, station_name(station),
                             fc, bc, ftc, station->queue_size);
                } else {
                    Edge *edge = node->edges;
                    int edge_count = 0;
                    Edge *tmp = node->edges;
                    while (tmp != NULL) { ++edge_count; tmp = tmp->next; }
                    log_line("选中道路节点 %d（坐标%.1f,%.1f），连接%d条道路",
                             node->id, node->x, node->y, edge_count);
                    while (edge != NULL) {
                        const GraphNode *to = &app.graph.nodes[edge->to_index];
                        log_line("  -> 节点%d：%.1f公里，%.1f分钟",
                                 to->id, edge->distance_km, edge->travel_minutes);
                        edge = edge->next;
                    }
                }
                found = 1;
                break;
            }
        }
    }

    if (!found) {
        app.selected_node_id = -1;
        app.selected_vehicle_id = -1;
    }
    InvalidateRect(app.window, NULL, TRUE);
}

/* 创建控件并设置指定字体。 */
static HWND create_control(const char *class_name, const char *text,
                           DWORD style, int x, int y, int width, int height,
                           int id, HWND parent, HFONT font)
{
    HWND control = CreateWindowExA(
        0, class_name, text, style, x, y, width, height,
        parent, (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);
    SendMessageA(control, WM_SETFONT, (WPARAM)font, TRUE);
    return control;
}

/* ===== 可拖动分割条布局常量 ===== */
#define PANEL_MIN_W 320
#define PANEL_MAX_W 680
#define INFO_MIN_H 45
#define SPLITTER_SIZE 6
#define FIXED_TOP_H 440  /* 顶部固定区域高度（到车辆信息标签） */

/* 根据当前布局变量更新所有控件的位置和大小。 */
static void update_layout(void)
{
    int w = app.left_panel_width;
    int info_w = w - 30;
    int group_w = w - 20;
    int title_w = w - 30;
    int y = FIXED_TOP_H;
    HWND h;

    /* 标题和状态栏 */
    h = GetDlgItem(app.window, ID_TITLE_LABEL);
    if (h) MoveWindow(h, 15, 6, title_w, 44, TRUE);
    if (app.status_label) MoveWindow(app.status_label, 15, 54, title_w, 32, TRUE);

    /* 三个分组框 */
    h = GetDlgItem(app.window, ID_GROUP_DATA);
    if (h) MoveWindow(h, 10, 94, group_w, 128, TRUE);
    h = GetDlgItem(app.window, ID_GROUP_SIM);
    if (h) MoveWindow(h, 10, 230, group_w, 112, TRUE);
    h = GetDlgItem(app.window, ID_GROUP_DEVICE);
    if (h) MoveWindow(h, 10, 350, group_w, 80, TRUE);

    /* 右侧跟随面板宽度的按钮 */
    h = GetDlgItem(app.window, ID_RECOMMEND);
    if (h) MoveWindow(h, w - 98, 121, 78, 38, TRUE);
    h = GetDlgItem(app.window, ID_RESET);
    if (h) MoveWindow(h, w - 98, 258, 78, 34, TRUE);
    h = GetDlgItem(app.window, ID_REPAIR);
    if (h) MoveWindow(h, w - 98, 373, 78, 36, TRUE);

    /* 车辆信息 */
    h = GetDlgItem(app.window, ID_VEHICLE_LABEL);
    if (h) MoveWindow(h, 15, y, 140, 26, TRUE);
    if (app.vehicle_info) MoveWindow(app.vehicle_info, 15, y + 26, info_w, app.vehicle_info_h, TRUE);
    y += 26 + app.vehicle_info_h + 8;

    /* 充电站信息 */
    h = GetDlgItem(app.window, ID_STATION_LABEL);
    if (h) MoveWindow(h, 15, y, 140, 26, TRUE);
    if (app.station_info) MoveWindow(app.station_info, 15, y + 26, info_w, app.station_info_h, TRUE);
    y += 26 + app.station_info_h + 8;

    /* 统计信息 */
    h = GetDlgItem(app.window, ID_STATS_LABEL);
    if (h) MoveWindow(h, 15, y, 140, 24, TRUE);
    if (app.stats_info) MoveWindow(app.stats_info, 15, y + 24, info_w, app.stats_info_h, TRUE);
    y += 24 + app.stats_info_h + 8;

    /* 操作日志：自动填充剩余空间 */
    {
        RECT client;
        int log_h;
        GetClientRect(app.window, &client);
        log_h = client.bottom - y - 24 - 10;
        if (log_h < INFO_MIN_H) log_h = INFO_MIN_H;
        app.log_info_h = log_h;
        h = GetDlgItem(app.window, ID_LOG_LABEL);
        if (h) MoveWindow(h, 15, y, 140, 24, TRUE);
        if (app.log_edit) MoveWindow(app.log_edit, 15, y + 24, info_w, log_h, TRUE);
    }
}

/* 检测鼠标位置是否在分割条上，返回拖动模式和索引。 */
static int hit_test_splitter(int x, int y, int *index)
{
    int cy = FIXED_TOP_H + 26;
    int thresholds[3];
    int i;

    /* 垂直分割条 */
    if (x >= app.left_panel_width - 3 && x <= app.left_panel_width + 3) {
        *index = 0;
        return 1; /* 垂直 */
    }

    /* 水平分割条：三个信息表之间 */
    thresholds[0] = cy + app.vehicle_info_h;
    thresholds[1] = thresholds[0] + 8 + 26 + app.station_info_h;
    thresholds[2] = thresholds[1] + 8 + 24 + app.stats_info_h;

    for (i = 0; i < 3; ++i) {
        if (y >= thresholds[i] - 3 && y <= thresholds[i] + 3) {
            *index = i;
            return 2; /* 水平 */
        }
    }
    return 0;
}

/* 创建测试页面上的输入框、按钮和信息显示区。 */
static void create_controls(HWND window)
{
    /* 初始化布局默认值 */
    app.left_panel_width = 380;
    app.vehicle_info_h = 92;
    app.station_info_h = 92;
    app.stats_info_h = 38;
    app.log_info_h = 100;
    app.drag_mode = 0;
    app.map_zoom = 1.0;

    /* 标题 */
    create_control("STATIC", "城市新能源汽车充电资源调度仿真系统",
                   WS_CHILD | WS_VISIBLE | SS_CENTER,
                   15, 6, 350, 44, ID_TITLE_LABEL, window, app.font_title);

    /* 状态栏 */
    app.status_label = create_control(
        "STATIC", "仿真时间：0 分钟    状态：已暂停",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        15, 54, 350, 32, 0, window, app.font_normal);

    /* ===== 分组1：数据操作 ===== */
    create_control("BUTTON", "数据操作",
                   WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                   10, 94, 360, 128, ID_GROUP_DATA, window, app.font_section);
    create_control("STATIC", "车辆编号：", WS_CHILD | WS_VISIBLE,
                   20, 124, 80, 32, 0, window, app.font_normal);
    app.vehicle_edit = create_control(
        "EDIT", "201", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        105, 122, 75, 36, ID_VEHICLE, window, app.font_normal);
    create_control("BUTTON", "加载数据", WS_CHILD | WS_VISIBLE,
                   185, 121, 78, 38, ID_LOAD, window, app.font_normal);
    create_control("BUTTON", "站点推荐", WS_CHILD | WS_VISIBLE,
                   268, 121, 78, 38, ID_RECOMMEND, window, app.font_normal);
    create_control("BUTTON", "提交请求", WS_CHILD | WS_VISIBLE,
                   20, 168, 78, 38, ID_SUBMIT, window, app.font_normal);
    create_control("BUTTON", "批量提交", WS_CHILD | WS_VISIBLE,
                   105, 168, 88, 38, ID_SUBMIT_ALL, window, app.font_normal);

    /* ===== 分组2：仿真控制 ===== */
    create_control("BUTTON", "仿真控制",
                   WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                   10, 230, 360, 112, ID_GROUP_SIM, window, app.font_section);
    create_control("BUTTON", "单步运行", WS_CHILD | WS_VISIBLE,
                   20, 258, 75, 34, ID_STEP, window, app.font_normal);
    create_control("BUTTON", "运行10步", WS_CHILD | WS_VISIBLE,
                   100, 258, 80, 34, ID_RUN10, window, app.font_normal);
    create_control("BUTTON", "运行到底", WS_CHILD | WS_VISIBLE,
                   185, 258, 75, 34, ID_RUNALL, window, app.font_normal);
    create_control("BUTTON", "重置仿真", WS_CHILD | WS_VISIBLE,
                   265, 258, 78, 34, ID_RESET, window, app.font_normal);
    app.pause_button = create_control("BUTTON", "暂停", WS_CHILD | WS_VISIBLE,
                   20, 300, 75, 34, ID_PAUSE, window, app.font_normal);
    create_control("BUTTON", "1x速", WS_CHILD | WS_VISIBLE,
                   100, 300, 52, 34, ID_SPEED1, window, app.font_normal);
    create_control("BUTTON", "2x速", WS_CHILD | WS_VISIBLE,
                   158, 300, 52, 34, ID_SPEED2, window, app.font_normal);
    create_control("BUTTON", "5x速", WS_CHILD | WS_VISIBLE,
                   216, 300, 52, 34, ID_SPEED5, window, app.font_normal);

    /* ===== 分组3：设备管理 ===== */
    create_control("BUTTON", "设备管理",
                   WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                   10, 350, 360, 80, ID_GROUP_DEVICE, window, app.font_section);
    create_control("STATIC", "充电桩编号：", WS_CHILD | WS_VISIBLE,
                   20, 376, 88, 30, ID_CHARGER_LABEL, window, app.font_normal);
    app.charger_edit = create_control(
        "EDIT", "1005", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        112, 374, 70, 34, ID_CHARGER, window, app.font_normal);
    create_control("BUTTON", "设为故障", WS_CHILD | WS_VISIBLE,
                   187, 373, 78, 36, ID_BREAK, window, app.font_normal);
    create_control("BUTTON", "修复设备", WS_CHILD | WS_VISIBLE,
                   270, 373, 78, 36, ID_REPAIR, window, app.font_normal);
    create_control("BUTTON", "查看桩状态", WS_CHILD | WS_VISIBLE,
                   20, 412, 115, 28, ID_CHARGER_STATUS, window, app.font_normal);
    app.bg_toggle_button = create_control("BUTTON", "隐藏背景",
                   WS_CHILD | WS_VISIBLE,
                   145, 412, 105, 28, ID_BG_TOGGLE, window, app.font_small);

    /* ===== 车辆信息区（高92，约6行） ===== */
    create_control("STATIC", "车辆信息", WS_CHILD | WS_VISIBLE,
                   15, 440, 140, 26, ID_VEHICLE_LABEL, window, app.font_section);
    app.vehicle_info = create_control(
        "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        15, 466, 350, 92, ID_VEHICLE_INFO, window, app.font_small);

    /* ===== 充电站信息区（高92，约6行） ===== */
    create_control("STATIC", "充电站信息", WS_CHILD | WS_VISIBLE,
                   15, 566, 140, 26, ID_STATION_LABEL, window, app.font_section);
    app.station_info = create_control(
        "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        15, 592, 350, 92, ID_STATION_INFO, window, app.font_small);

    /* ===== 统计信息区（高38，约2行） ===== */
    create_control("STATIC", "运行统计", WS_CHILD | WS_VISIBLE,
                   15, 692, 140, 24, ID_STATS_LABEL, window, app.font_section);
    app.stats_info = create_control(
        "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER |
        ES_MULTILINE | ES_READONLY,
        15, 718, 350, 38, ID_STATS_INFO, window, app.font_small);

    /* ===== 操作日志区（高100，约7行） ===== */
    create_control("STATIC", "操作日志", WS_CHILD | WS_VISIBLE,
                   15, 764, 140, 24, ID_LOG_LABEL, window, app.font_section);
    app.log_edit = create_control(
        "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        15, 790, 350, 100, ID_LOG, window, app.font_small);

    /* 根据布局变量更新所有控件位置 */
    update_layout();
}

/* 根据按钮编号调用对应的后端测试操作。 */
static void handle_command(int id)
{
    switch (id) {
        case ID_LOAD: load_data(); break;
        case ID_RECOMMEND: recommend_vehicle(); break;
        case ID_SUBMIT: submit_request(); break;
        case ID_SUBMIT_ALL: submit_all_requests(); break;
        case ID_STEP: run_steps(1); break;
        case ID_RUN10: start_timed_run(10, 0); break;
        case ID_RUNALL: start_timed_run(0, 1); break;
        case ID_PAUSE: toggle_pause(); break;
        case ID_SPEED1: set_speed(1); break;
        case ID_SPEED2: set_speed(2); break;
        case ID_SPEED5: set_speed(5); break;
        case ID_BG_TOGGLE: toggle_background(); break;
        case ID_BREAK: break_charger(); break;
        case ID_REPAIR: repair_charger(); break;
        case ID_CHARGER_STATUS: log_charger_status(); break;
        case ID_RESET: reset_simulation(); break;
        default: break;
    }
}

/* Win32窗口过程，负责控件消息和地图重绘。 */
static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                    WPARAM w_param, LPARAM l_param)
{
    (void)l_param;
    switch (message) {
        case WM_CREATE:
            app.window = window;
            create_fonts();
            load_background();
            create_controls(window);
            load_data();
            return 0;
        case WM_COMMAND:
            if (HIWORD(w_param) == BN_CLICKED) {
                handle_command(LOWORD(w_param));
                return 0;
            }
            return DefWindowProcA(window, message, w_param, l_param);
        case WM_KEYDOWN:
            if (w_param == VK_SPACE) {
                toggle_pause();
                return 0;
            } else if (w_param == 'R' || w_param == 'r') {
                reset_simulation();
                return 0;
            } else if (w_param == VK_RIGHT) {
                run_steps(1);
                return 0;
            }
            return DefWindowProcA(window, message, w_param, l_param);
        case WM_CHAR:
            if (w_param == ' ' || w_param == 'r' || w_param == 'R') {
                return 0;
            }
            return DefWindowProcA(window, message, w_param, l_param);
        case WM_SETCURSOR: {
            int x = LOWORD(GetMessagePos() & 0xFFFF);
            int y = HIWORD((GetMessagePos() >> 16) & 0xFFFF);
            POINT pt = {x, y};
            int idx;
            ScreenToClient(window, &pt);
            if (app.drag_mode == 1) {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            } else if (app.drag_mode == 2) {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            } else if (hit_test_splitter(pt.x, pt.y, &idx) == 1) {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            } else if (hit_test_splitter(pt.x, pt.y, &idx) == 2) {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
                return TRUE;
            }
            return DefWindowProcA(window, message, w_param, l_param);
        }
        case WM_LBUTTONDOWN: {
            int x = LOWORD(l_param);
            int y = HIWORD(l_param);
            int idx;
            int mode = hit_test_splitter(x, y, &idx);
            if (mode > 0) {
                app.drag_mode = mode;
                app.drag_index = idx;
                app.drag_start_mouse = (mode == 1) ? x : y;
                app.drag_start_value = (mode == 1) ? app.left_panel_width :
                    (idx == 0 ? app.vehicle_info_h :
                     idx == 1 ? app.station_info_h : app.stats_info_h);
                SetCapture(window);
                return 0;
            }
            handle_map_click(x, y);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = LOWORD(l_param);
            int y = HIWORD(l_param);
            if (app.drag_mode == 1) {
                int new_w = app.drag_start_value + (x - app.drag_start_mouse);
                if (new_w < PANEL_MIN_W) new_w = PANEL_MIN_W;
                if (new_w > PANEL_MAX_W) new_w = PANEL_MAX_W;
                app.left_panel_width = new_w;
                update_layout();
                InvalidateRect(window, NULL, TRUE);
            } else if (app.drag_mode == 2) {
                int new_h = app.drag_start_value + (y - app.drag_start_mouse);
                if (new_h < INFO_MIN_H) new_h = INFO_MIN_H;
                if (app.drag_index == 0) app.vehicle_info_h = new_h;
                else if (app.drag_index == 1) app.station_info_h = new_h;
                else app.stats_info_h = new_h;
                update_layout();
                InvalidateRect(window, NULL, TRUE);
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (app.drag_mode > 0) {
                app.drag_mode = 0;
                ReleaseCapture();
                InvalidateRect(window, NULL, TRUE);
            }
            return 0;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(w_param);
            /* 仅在 Ctrl 键按下时缩放地图，避免普通滚动误触 */
            if (GetKeyState(VK_CONTROL) >= 0) {
                return DefWindowProcA(window, message, w_param, l_param);
            }
            if (delta > 0) {
                app.map_zoom *= 1.15;
                if (app.map_zoom > 3.0) app.map_zoom = 3.0;
            } else {
                app.map_zoom /= 1.15;
                if (app.map_zoom < 0.5) app.map_zoom = 0.5;
            }
            InvalidateRect(window, NULL, TRUE);
            return 0;
        }
        case WM_TIMER:
            if (w_param == ID_ANIMATION_TIMER) {
                timed_run_step();
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            RECT client;
            GetClientRect(window, &client);
            draw_background(dc, client.right, client.bottom);
            draw_map(dc);
            /* 绘制可拖动分割条 */
            {
                HBRUSH sb = CreateSolidBrush(RGB(180, 180, 180));
                int cy = FIXED_TOP_H + 26;
                int ths[3];
                int i;
                /* 垂直分割条 */
                FillRect(dc, &(RECT){app.left_panel_width - 2, 0,
                         app.left_panel_width + 2, client.bottom}, sb);
                /* 水平分割条 */
                ths[0] = cy + app.vehicle_info_h;
                ths[1] = ths[0] + 8 + 26 + app.station_info_h;
                ths[2] = ths[1] + 8 + 24 + app.stats_info_h;
                for (i = 0; i < 3; ++i) {
                    FillRect(dc, &(RECT){10, ths[i] - 2,
                             app.left_panel_width - 10, ths[i] + 2}, sb);
                }
                DeleteObject(sb);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            stop_timed_run(0);
            if (app.loaded) {
                simulation_reset(&app.simulation);
                graph_free(&app.graph);
                entity_free(&app.store);
            }
            destroy_fonts();
            if (app.bg_bmp != NULL) {
                DeleteObject(app.bg_bmp);
            }
            if (app.dim_cache_bmp != NULL) {
                DeleteObject(app.dim_cache_bmp);
            }
            if (app.bg_stretch_cache != NULL) {
                DeleteObject(app.bg_stretch_cache);
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(window, message, w_param, l_param);
    }
}

/* Windows程序入口，注册窗口类并进入消息循环。 */
int APIENTRY WinMain(HINSTANCE instance, HINSTANCE previous,
                     LPSTR command_line, int show_command)
{
    const char *class_name = "BackendTestWindow";
    WNDCLASSA window_class;
    HWND window;
    MSG message;

    (void)previous;
    (void)command_line;
    (void)show_command;
    set_project_directory();
    memset(&app, 0, sizeof(app));
    memset(&window_class, 0, sizeof(window_class));
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&window_class)) {
        return 1;
    }
    window = CreateWindowExA(
        0, class_name, "城市新能源汽车充电资源调度仿真系统",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        1400, 900, NULL, NULL, instance, NULL);
    if (window == NULL) {
        return 1;
    }
    ShowWindow(window, SW_SHOWMAXIMIZED);
    UpdateWindow(window);

    while (GetMessageA(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return (int)message.wParam;
}

