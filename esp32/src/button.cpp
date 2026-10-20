#include "button.h"
#include "app_state.h"
#include "board_config.h"
#include "motor.h"
#include "network.h"
#include <atomic>
#ifndef FEEDER_HOST_TEST
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {
bool rawLast = false;
std::atomic<bool> pressed{false};
std::atomic<bool> bootHold{false};
std::atomic<bool> triggered{false};
std::atomic<bool> manualActive{false};
uint32_t changed = 0;
std::atomic<uint32_t> down{0};
std::atomic<uint32_t> manualSequence{0};
std::atomic<bool> setupRequest{false};
std::atomic<bool> ipRequest{false};
std::atomic<bool> feedFailed{false};
bool firstClick = false;
uint32_t firstRelease = 0;
constexpr uint32_t CLICK_MAX_MS = 350;
} // namespace

void beginButton() {
    if (BUTTON_PIN >= 0) {
        pinMode(BUTTON_PIN, INPUT_PULLUP);
        // 上电已按下的这一轮操作只用于配网，不触发投喂。
        bootHold = digitalRead(BUTTON_PIN) == LOW;
    }
}

void tickButton() {
    if (BUTTON_PIN < 0) {
        return;
    }
    const bool raw = digitalRead(BUTTON_PIN) == LOW;
    const uint32_t now = millis();
    if (manualActive && (!motorIsFeeding() || motorFeedSequence() != manualSequence)) {
        manualActive = false;
    }
    if (raw != rawLast) {
        rawLast = raw;
        changed = now;
    }
    // 松手立即撤销本次操作，不等待释放消抖，也不停止其他来源的投喂。
    if (!raw) {
        if (manualActive) {
            stopFeedIfSequence(manualSequence);
        }
        if (pressed && !bootHold && !triggered && now - down <= CLICK_MAX_MS && !motorIsFeeding()) {
            if (firstClick && now - firstRelease <= CLICK_MAX_MS) {
                ipRequest = true;
                firstClick = false;
            } else {
                firstClick = true;
                firstRelease = now;
            }
        }
        manualActive = false;
        pressed = false;
        triggered = false;
        bootHold = false;
        return;
    }
    if (!pressed && now - changed >= 40) {
        pressed = true;
        down = changed;
    }
    if (!pressed || triggered) {
        return;
    }
    if (now - down > CLICK_MAX_MS) {
        firstClick = false;
    }
    if (bootHold) {
        if (now - down >= LONG_PRESS_MS) {
            triggered = true;
            setupRequest = true;
        }
        return;
    }
    if (now - down >= MANUAL_HOLD_MS) {
        triggered = true;
        // 持续按住也最多运行30秒；超时或Web停止后，必须松手再按才能重启。
        manualActive = startFeed(MAX_MOTOR_MS);
        manualSequence = motorFeedSequence();
        if (!manualActive) {
            feedFailed = true;
        }
    }
}

bool buttonIsArming() {
    return pressed && !bootHold && !triggered;
}

bool buttonIsFeeding() {
    return manualActive && motorIsFeeding() && motorFeedSequence() == manualSequence;
}

uint32_t buttonHoldElapsed() {
    return buttonIsArming() ? millis() - down : 0;
}

void tickButtonActions() {
    // WiFi和String报警只由主循环操作，按键任务不调用可能阻塞的网络接口。
    if (setupRequest.exchange(false)) {
        startAP();
    }
    if (feedFailed.exchange(false)) {
        errorText = "Manual feed unavailable or motor busy";
    }
}

bool takeButtonIpRequest() {
    return ipRequest.exchange(false);
}

void beginButtonTask() {
#ifndef FEEDER_HOST_TEST
    // 独立5ms采样，避免Web请求、OLED传输或WiFi处理把2秒长按拖成3秒。
    const BaseType_t result = xTaskCreate(
        [](void *) {
            TickType_t last = xTaskGetTickCount();
            for (;;) {
                tickButton();
                vTaskDelayUntil(&last, pdMS_TO_TICKS(5));
            }
        },
        "button", 3072, nullptr, 3, nullptr);
    if (result != pdPASS) {
        errorText = "Button task failed";
    }
#endif
}
