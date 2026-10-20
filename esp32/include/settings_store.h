#pragma once
#include <Arduino.h>
constexpr int TASK_COUNT = 8;

// NVS 以原始字节保存这些结构，调整字段布局时必须升级版本并处理迁移。
struct Task {
    uint8_t enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t reserved;
    uint32_t duration;
};

struct Settings {
    uint32_t version;
    uint32_t manualMs;
    Task tasks[TASK_COUNT];
};

extern Settings settings;
extern int32_t lastDay[TASK_COUNT];
void beginSettings();
bool saveConfiguration(const Settings &next);
bool saveTaskDay(int index, int32_t day);
String savedSsid();
String savedPassword();
bool saveWifiCredentials(const String &ssid, const String &password);
uint8_t savedOledBrightness();
bool saveOledBrightness(uint8_t percent);
