#include "cpu_power.h"
#include "app_state.h"
#include <Arduino.h>
#ifndef FEEDER_HOST_TEST
#include <esp32-hal-cpu.h>
#endif

void beginCpuPower() {
    // 编译目标与运行时都固定80MHz，不再因Web、扫描或投喂升频。
    if (getCpuFrequencyMhz() != 80 && !setCpuFrequencyMhz(80)) {
        errorText = "CPU frequency change failed";
        return;
    }
#ifndef FEEDER_HOST_TEST
    Serial.printf("CPU fixed: %lu MHz\n", static_cast<unsigned long>(getCpuFrequencyMhz()));
#endif
}
