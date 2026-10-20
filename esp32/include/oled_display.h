#pragma once
#include <Arduino.h>
void beginDisplay();
void tickDisplay();
uint8_t displayBrightness();
bool setDisplayBrightness(uint8_t percent);
