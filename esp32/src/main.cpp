#include "app_state.h"
#include "button.h"
#include "cpu_power.h"
#include "motor.h"
#include "network.h"
#include "oled_display.h"
#include "power_monitor.h"
#include "scheduler.h"
#include "settings_store.h"
#include "web_ui.h"
#include <Arduino.h>

void setup() {
    Serial.begin(115200); // Native USB CDC; GPIO20/21 belong to TP4056.
    beginCpuPower();
    if (!validateBoardPins()) {
        Serial.println("Invalid or conflicting pins. Startup halted.");
        while (true) {
            delay(1000);
        }
    }
    beginMotor();
    beginButton();
    beginPower();
    beginSettings();
    beginDisplay();
    beginNetwork();
    beginClock();
    beginWeb();
    beginButtonTask();
    Serial.println("FishFeeder ESP32-C3-FH4 started.");
}

void loop() {
    tickPower();
    tickMotor();
    tickButtonActions();
    tickSchedule();
    tickDisplay();
    tickNetwork();
    tickWeb();
    delay(2);
}
