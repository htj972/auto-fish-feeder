#pragma once

// GPIO numbers, not package pin numbers. Motor driver must default OFF in hardware.
constexpr int MOTOR_PIN = 10;
constexpr int BUTTON_PIN = 5; // Active low, button to GND.
constexpr int OLED_SDA = 4;
constexpr int OLED_SCL = 3;
constexpr int CHARGE_PIN = 20;      // TP4056 CHRG, open drain, active low.
constexpr int CHARGE_DONE_PIN = 21; // TP4056 STDBY, open drain, active low.
constexpr int BATTERY_PIN = 2;
// Battery+ -> 100k -> GPIO2 -> 100k -> GND. Requires physical resistor change.
constexpr bool BATTERY_ADC_ENABLED = true;
constexpr float BATTERY_R_TOP = 100000.0f;
constexpr float BATTERY_R_BOTTOM = 100000.0f;
constexpr float BATTERY_CALIBRATION = 1.0f;
constexpr bool MOTOR_ACTIVE_HIGH = true;
constexpr int OLED_ADDRESS = 0x3C;
constexpr unsigned OLED_IDLE_MS = 30000;
constexpr unsigned MAX_MOTOR_MS = 30000;
constexpr unsigned DEFAULT_FEED_MS = 2000;
constexpr unsigned LONG_PRESS_MS = 3000;
constexpr unsigned MANUAL_HOLD_MS = 2000;
static_assert(CHARGE_PIN != CHARGE_DONE_PIN && CHARGE_PIN != MOTOR_PIN &&
                  CHARGE_DONE_PIN != MOTOR_PIN,
              "Conflicting charger pins");
static_assert(!BATTERY_ADC_ENABLED ||
                  (4.2f * BATTERY_R_BOTTOM / (BATTERY_R_TOP + BATTERY_R_BOTTOM) < 3.3f),
              "Divider exceeds GPIO voltage limit");
static_assert(!BATTERY_ADC_ENABLED ||
                  (4.2f * BATTERY_R_BOTTOM / (BATTERY_R_TOP + BATTERY_R_BOTTOM) < 2.4f),
              "Divider exceeds usable ADC range");
