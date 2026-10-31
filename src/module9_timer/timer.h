#ifndef TIMER_H
#define TIMER_H

#include <glib.h>

/* 每个时间步执行的真实工作回调，返回是否继续运行。 */
typedef gboolean (*SimTimerTick)(gpointer user_data);

/* 仿真定时控制器，封装 g_timeout_add 的周期触发。 */
typedef struct {
    gboolean running;
    gboolean until_done;
    gint steps_left;
    guint source_id;
    guint interval_ms;
    SimTimerTick tick;
    gpointer user_data;
} SimTimer;

/* 初始化定时控制器，tick 为每个时间步调用的工作入口。 */
void sim_timer_init(SimTimer *timer, SimTimerTick tick, gpointer user_data);

/* 开始周期运行。steps 为本次运行的步数，until_done 为是否一直运行到无任务。 */
gboolean sim_timer_start(SimTimer *timer, gint steps, gboolean until_done,
                         guint interval_ms);

/* 停止定时运行（暂停），保留当前仿真状态。 */
void sim_timer_stop(SimTimer *timer);

/* 返回定时器当前是否在运行。 */
gboolean sim_timer_is_running(const SimTimer *timer);

#endif
