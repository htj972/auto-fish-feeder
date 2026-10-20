#pragma once

// 仅供桌面测试：替换按键模块用到的Arduino输入和时钟接口。
using uint32_t = unsigned int;
using uint8_t = unsigned char;
using String = const char *;
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT_PULLUP = 2;
uint32_t millis();
int digitalRead(int pin);
void pinMode(int pin, int mode);
constexpr int ADC_11db = 3;
int analogRead(int pin);
uint32_t analogReadMilliVolts(int pin);
void analogReadResolution(int bits);
void analogSetPinAttenuation(int pin, int attenuation);
uint32_t getCpuFrequencyMhz();
bool setCpuFrequencyMhz(uint32_t mhz);

inline int min(int a, int b) {
    return a < b ? a : b;
}
