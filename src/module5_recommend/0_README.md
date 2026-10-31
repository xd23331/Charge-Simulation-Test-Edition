### 文件清单

- recommend.h — 接口与数据结构声明
- recommend.c — 推荐算法实现

### 核心数据结构

Recommendation（单个充电站的推荐结果）

- station_id：充电站编号
- valid：是否有效（1 = 可达且可充电，0 = 不可用）
- reason [NAME_LEN]：有效时显示总时间，无效时说明原因
- path：到该站的最短路径详情（PathResult）
- travel_minutes：行驶时间
- wait_minutes：等待时间（含排队预估 + 在途同站车辆分摊）
- charge_minutes：充电时间
- total_minutes：总时间 = 行驶 + 等待 + 充电
- travel_energy_kwh：行驶耗电量（kWh）
- arrival_battery_percent：到达时预估电量百分比
- score：综合评分 = 总时间 + 低电量风险分

RecommendationList（全部候选站点列表）

- items [MAX_STATIONS]：推荐结果数组
- count：实际数量

### 接口说明

```
int recommend_all(const Graph *graph, const EntityStore *store,
                  const Vehicle *vehicle, RecommendationList *list);
```

计算一辆车到所有充电站的推荐结果，自动按评分排序后写入 list。成功返回 0，参数错误返回 -1。

```
const Recommendation *recommend_best(const RecommendationList *list);
```

返回排名第一的有效推荐。如果列表为空或第一名无效，返回 NULL。

### 推荐算法流程

1. 以车辆当前节点为起点执行一次 Dijkstra，算出到全部节点的最短时间。
2. 遍历所有充电站，逐个评估：
   - 路径不可达 → valid=0，reason="road unreachable"
   - 无可用充电桩（全部故障）→ valid=0，reason="no working charger"
   - 电量不够到达（当前电量 - 行驶耗电 < 0）→ valid=0，reason="battery not enough"
   - 调度器返回站点不可用 → valid=0，reason="station unavailable"
3. 对有效站点计算时间分解：
   - 行驶时间 = 路径总时间
   - 等待时间 = max (排队预估 - 行驶时间，0) + 正在前往该站车辆的分摊等待
   - 充电时间 = 需充电量 ÷ 站点平均功率 × 60
4. 综合评分 = 总时间 + 低电量风险分（到达电量 < 15% 时，每低 1% 加 2 分）
5. 排序规则：有效优先 → score 小的优先 → score 相同则 station_id 小的优先

### 使用要点

1. 固定能耗率 ENERGY_PER_KM = 0.18 kWh/km，即每公里耗电 0.18 度。
2. 无效结果也保留在列表中，排在有效结果之后，reason 字段说明原因，方便 UI 展示 "不可达" 站点。
3. 等待时间考虑了正在前往同一站点的车辆：会把它们的充电需求按桩数分摊计入等待。
4. recommend_best 只检查排序后的第一名，如果第一名无效直接返回 NULL，不会跳过找下一个有效项。
5. 依赖模块：module1_graph（路径计算）、module2_scheduler（排队预估）、module3_entity（实体数据）。