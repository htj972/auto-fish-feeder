#pragma once
#include <Arduino.h>
void beginPower();
void tickPower();
const char *chargeText();
float batteryVoltage();
bool batteryIsOverrange();
int chargeLevel();
int chargeDoneLevel();
bool batteryIsPresent();
bool batteryIsCharging();
int batteryPercent();
