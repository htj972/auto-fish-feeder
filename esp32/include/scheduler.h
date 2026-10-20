#pragma once
#include <Arduino.h>
#include <time.h>
void beginClock();
bool clockReady(time_t now);
String clockText(time_t now, const char *format);
String nextTask(time_t now);
bool nextScheduledFeed(time_t now, time_t &when, uint32_t &duration);
void tickSchedule();
