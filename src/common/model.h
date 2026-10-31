#ifndef MODEL_H
#define MODEL_H

#define NAME_LEN 64
#define MAX_VEHICLES 100
#define MAX_STATIONS 30
#define MAX_CHARGERS 100
#define MAX_REQUESTS 200
#define MAX_GRAPH_NODES 100
#define MAX_PATH_NODES 100

/* 车辆在仿真过程中的状态。 */
typedef enum {
    VEHICLE_IDLE,
    VEHICLE_TRAVELING,
    VEHICLE_WAITING,
    VEHICLE_CHARGING,
    VEHICLE_FINISHED
} VehicleState;

/* 充电桩当前的工作状态。 */
typedef enum {
    CHARGER_FREE,
    CHARGER_BUSY,
    CHARGER_FAULT
} ChargerState;

/* 充电请求在业务流程中的状态。 */
typedef enum {
    REQUEST_TRAVELING,
    REQUEST_WAITING,
    REQUEST_CHARGING
} RequestState;

typedef struct Vehicle Vehicle;
typedef struct Station Station;
typedef struct Charger Charger;
typedef struct ChargeRequest ChargeRequest;

/* 车辆数据，保存电量、位置和当前业务状态。 */
struct Vehicle {
    int id;
    int node_id;
    int initial_node_id;
    double battery_percent;
    double initial_battery_percent;
    double target_percent;
    double capacity_kwh;
    int urgency;
    VehicleState state;
    int station_id;
    int charger_id;
    double arrival_time;
    ChargeRequest *request;
};

/* 充电请求，连接一辆车和它选择的充电站。 */
struct ChargeRequest {
    int id;
    Vehicle *vehicle;
    Station *station;
    double request_time;
    double arrival_time;
    double wait_time;
    double priority;
    double needed_kwh;
    double travel_energy_kwh;
    int sequence;
    RequestState state;
};

/* 充电桩节点，同时也是站点充电桩单链表的节点。 */
struct Charger {
    int id;
    double power_kw;
    ChargerState initial_state;
    ChargerState state;
    ChargeRequest *request;
    double remaining_minutes;
    Charger *next;
};

/* 充电站数据，包含充电桩链表和等待车辆最大堆。 */
struct Station {
    int id;
    int node_id;
    char name[NAME_LEN];
    Charger *chargers;
    ChargeRequest *queue[MAX_VEHICLES];
    int queue_size;
};

#endif

