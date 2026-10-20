#include "../../src/power_monitor.cpp"

namespace {
uint32_t testNow = 0;
uint32_t adcMv = 1900;
int raw = 3000;
bool charging = false;
bool done = false;

void sample() {
    testNow += 1000;
    tickPower();
}
} // namespace

uint32_t millis() {
    return testNow;
}

int digitalRead(int pin) {
    return (pin == CHARGE_PIN ? charging : done) ? LOW : HIGH;
}

void pinMode(int, int) {
}

void analogReadResolution(int) {
}

void analogSetPinAttenuation(int, int) {
}

int analogRead(int) {
    return raw;
}

uint32_t analogReadMilliVolts(int) {
    return adcMv;
}

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    beginPower();
    CHECK(batteryPercent() == -1);
    sample();
    CHECK(batteryIsPresent() && batteryPercent() >= 49 && batteryPercent() <= 50);
    charging = true;
    sample();
    const int lockedPercent = batteryPercent();
    adcMv = 2050;
    sample();
    CHECK(batteryIsCharging() && batteryPercent() == lockedPercent);
    charging = false;
    done = true;
    sample();
    CHECK(!batteryIsCharging() && batteryPercent() == 100);
    adcMv = 0;
    raw = 0;
    sample();
    sample();
    sample();
    CHECK(!batteryIsPresent() && batteryPercent() == -1);
    adcMv = 2500;
    raw = 4095;
    sample();
    CHECK(batteryIsOverrange() && batteryPercent() == -1);
    return 0;
}
