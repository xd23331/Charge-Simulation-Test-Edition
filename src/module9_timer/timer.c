#include "timer.h"

/* 定时回调：推进一个时间步，并根据运行模式决定是否结束。 */
static gboolean timer_timeout(gpointer data)
{
    SimTimer *timer = data;
    gboolean keep = TRUE;

    if (!timer->running) {
        return G_SOURCE_REMOVE;
    }
    if (timer->tick != NULL) {
        keep = timer->tick(timer->user_data);
    }
    if (timer->until_done) {
        /* 连续运行：直到回调返回否（无活动任务）才结束。 */
        if (keep) {
            return G_SOURCE_CONTINUE;
        }
        timer->running = FALSE;
        timer->source_id = 0;
        return G_SOURCE_REMOVE;
    }
    /* 固定步数运行：运行指定步数后结束。 */
    if (timer->steps_left > 0) {
        --timer->steps_left;
    }
    if (timer->steps_left > 0) {
        return G_SOURCE_CONTINUE;
    }
    timer->running = FALSE;
    timer->source_id = 0;
    return G_SOURCE_REMOVE;
}

void sim_timer_init(SimTimer *timer, SimTimerTick tick, gpointer user_data)
{
    timer->running = FALSE;
    timer->until_done = FALSE;
    timer->steps_left = 0;
    timer->source_id = 0;
    timer->interval_ms = 200;
    timer->tick = tick;
    timer->user_data = user_data;
}

gboolean sim_timer_start(SimTimer *timer, gint steps, gboolean until_done,
                         guint interval_ms)
{
    sim_timer_stop(timer);
    if (interval_ms > 0) {
        timer->interval_ms = interval_ms;
    }
    timer->until_done = until_done;
    timer->steps_left = steps;
    timer->running = TRUE;
    timer->source_id = g_timeout_add(timer->interval_ms, timer_timeout, timer);
    return timer->source_id != 0;
}

void sim_timer_stop(SimTimer *timer)
{
    if (timer->source_id != 0) {
        g_source_remove(timer->source_id);
    }
    timer->source_id = 0;
    timer->running = FALSE;
}

gboolean sim_timer_is_running(const SimTimer *timer)
{
    return timer->running;
}
