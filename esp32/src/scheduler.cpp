#include "scheduler.h"
#include "app_state.h"
#include "motor.h"
#include "settings_store.h"

bool clockReady(time_t now) {
    return now > 1704067200;
}

String clockText(time_t now, const char *format) {
    if (!clockReady(now)) {
        return "--:--";
    }
    struct tm local{};
    localtime_r(&now, &local);
    char buf[32];
    strftime(buf, sizeof(buf), format, &local);
    return buf;
}

bool nextScheduledFeed(time_t now, time_t &when, uint32_t &duration) {
    when = 0;
    duration = 0;
    if (!clockReady(now)) {
        return false;
    }
    struct tm local{};
    localtime_r(&now, &local);
    time_t best = 0;
    for (int i = 0; i < TASK_COUNT; ++i) {
        const auto &t = settings.tasks[i];
        if (!t.enabled) {
            continue;
        }
        struct tm next = local;
        next.tm_hour = t.hour;
        next.tm_min = t.minute;
        next.tm_sec = 0;
        time_t candidate = mktime(&next);
        if (candidate <= now || lastDay[i] >= local.tm_year * 366 + local.tm_yday) {
            next.tm_mday++;
            candidate = mktime(&next);
        }
        if (!best || candidate < best) {
            best = candidate;
            duration = t.duration;
        }
    }
    when = best;
    return best != 0;
}

String nextTask(time_t now) {
    if (!clockReady(now)) {
        return "Wait for NTP";
    }
    time_t when;
    uint32_t duration;
    return nextScheduledFeed(now, when, duration) ? clockText(when, "%m-%d %H:%M") : "None";
}

void tickSchedule() {
    time_t now = time(nullptr);
    if (!clockReady(now)) {
        return;
    }
    struct tm local{};
    localtime_r(&now, &local);
    int32_t day = local.tm_year * 366 + local.tm_yday;
    for (int i = 0; i < TASK_COUNT; ++i) {
        const auto &t = settings.tasks[i];
        if (!t.enabled || t.hour != local.tm_hour || t.minute != local.tm_min ||
            lastDay[i] >= day) {
            continue;
        }
        // 启动电机前持久化当日标记，避免同一分钟重启后重复投喂。
        // 标记写入后即使电机忙或突然断电，也不补喂，优先避免重复投喂。
        if (!saveTaskDay(i, day)) {
            errorText = "Schedule storage failed";
            continue;
        }
        if (!startFeed(t.duration)) {
            errorText = "Task skipped: motor busy/unavailable";
        }
    }
}

void beginClock() {
    configTzTime("CST-8", "ntp.aliyun.com", "pool.ntp.org", "time.cloudflare.com");
}
