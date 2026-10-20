#include "power_monitor.h"
#include "board_config.h"

namespace {
float batteryVolts = 0;
uint32_t batteryAdcMv = 0;
bool batteryOverrange = false;
uint8_t chargeState = 0;
int percent = -1;
uint8_t missingSamples = 0;
bool sampled = false;

int estimatePercent(float voltage) {
    // 单节锂电静态电压近似曲线，不代表库仑计测得的真实剩余容量。
    const float volts[] = {3.0f, 3.3f, 3.5f, 3.6f, 3.7f, 3.8f, 3.9f, 4.0f, 4.1f, 4.2f};
    const int levels[] = {0, 5, 10, 20, 35, 50, 65, 80, 90, 100};
    if (voltage <= volts[0]) {
        return 0;
    }
    for (unsigned i = 1; i < sizeof(volts) / sizeof(volts[0]); ++i) {
        if (voltage <= volts[i]) {
            return levels[i - 1] + int((voltage - volts[i - 1]) / (volts[i] - volts[i - 1]) *
                                       (levels[i] - levels[i - 1]));
        }
    }
    return 100;
}
} // namespace

const char *chargeText() {
    switch (chargeState) {
    case 1:
        return "Charging";
    case 2:
        return "Full";
    case 3:
        return "Check charger";
    default:
        return "Idle/unknown";
    }
}

void tickPower() {
    static uint32_t lastSample = 0;
    if (millis() - lastSample < 1000) {
        return;
    }
    lastSample = millis();
    bool charging = digitalRead(CHARGE_PIN) == LOW;
    bool done = digitalRead(CHARGE_DONE_PIN) == LOW;
    // 两个开漏输出均高时状态不确定，不能据此判断是否接入充电电源。
    const uint8_t previousChargeState = chargeState;
    chargeState = charging ? (done ? 3 : 1) : (done ? 2 : 0);
    if (BATTERY_ADC_ENABLED) {
        uint32_t sum = 0, rawSum = 0;
        for (int i = 0; i < 16; ++i) {
            sum += analogReadMilliVolts(BATTERY_PIN);
            rawSum += analogRead(BATTERY_PIN);
        }
        batteryAdcMv = (sum + 8) / 16;
        // ESP32-C3 的 11 dB ADC 有效量程约 2.5 V，保留裕量并检测饱和。
        batteryOverrange = batteryAdcMv >= 2400 || rawSum / 16 >= 4000;
        batteryVolts = batteryAdcMv * 0.001f * (BATTERY_R_TOP + BATTERY_R_BOTTOM) /
                       BATTERY_R_BOTTOM * BATTERY_CALIBRATION;
        sampled = true;
        missingSamples = batteryVolts < 1.0f ? min(int(missingSamples) + 1, 3) : 0;
        if (batteryOverrange || missingSamples >= 3 || batteryVolts < 2.5f) {
            percent = -1;
        } else if (chargeState != 1 || previousChargeState != 1 || percent < 0) {
            // 充电用静态闪电标记，百分比保留进入充电时的电压估计值。
            percent = chargeState == 2 ? 100 : estimatePercent(batteryVolts);
        }
    }
}

void beginPower() {
    pinMode(CHARGE_PIN, INPUT_PULLUP);
    pinMode(CHARGE_DONE_PIN, INPUT_PULLUP);
    if (BATTERY_ADC_ENABLED) {
        analogReadResolution(12);
        analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    }
}

float batteryVoltage() {
    return batteryVolts;
}

bool batteryIsOverrange() {
    return batteryOverrange;
}

int chargeLevel() {
    return digitalRead(CHARGE_PIN);
}

int chargeDoneLevel() {
    return digitalRead(CHARGE_DONE_PIN);
}

bool batteryIsPresent() {
    // 仅凭BAT电压无法可靠区分无电池与充电器对BAT电容供电的情况。
    return BATTERY_ADC_ENABLED && sampled && missingSamples < 3 && batteryVolts >= 2.5f &&
           !batteryOverrange;
}

bool batteryIsCharging() {
    return chargeState == 1 && batteryIsPresent();
}

int batteryPercent() {
    return batteryIsPresent() ? percent : -1;
}
