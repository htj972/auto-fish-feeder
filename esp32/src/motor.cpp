#include "motor.h"
#include "app_state.h"
#include "board_config.h"
#include <atomic>
#include <esp_timer.h>
#include <freertos/semphr.h>

namespace {
esp_timer_handle_t stopTimer = nullptr;
portMUX_TYPE motorMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool feeding = false;
bool pinsReady = false;
uint32_t feedSequence = 0;
int64_t stopAt = 0;
SemaphoreHandle_t controlMutex = nullptr;
std::atomic<bool> timerFailed{false};
} // namespace

static void stopMotor(void *) {
    // 定时器回调与主循环共享运行状态；回调不分配内存、不写 NVS。
    portENTER_CRITICAL(&motorMux);
    // 忽略上一轮停止后已排队的旧回调，不能把新启动的投喂提前停止。
    if (feeding && esp_timer_get_time() >= stopAt) {
        digitalWrite(MOTOR_PIN, MOTOR_ACTIVE_HIGH ? LOW : HIGH);
        feeding = false;
    }
    portEXIT_CRITICAL(&motorMux);
}

bool startFeed(uint32_t duration) {
    if (!pinsReady || !stopTimer || !controlMutex || duration < 100 || duration > MAX_MOTOR_MS) {
        return false;
    }
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    portENTER_CRITICAL(&motorMux);
    if (feeding) {
        portEXIT_CRITICAL(&motorMux);
        xSemaphoreGive(controlMutex);
        return false;
    }
    feeding = true;
    ++feedSequence;
    stopAt = esp_timer_get_time() + int64_t(duration) * 1000;
    portEXIT_CRITICAL(&motorMux);
    digitalWrite(MOTOR_PIN, MOTOR_ACTIVE_HIGH ? HIGH : LOW);
    // 独立定时器停止电机，避免网页处理耗时影响本次投喂时长。
    if (esp_timer_start_once(stopTimer, uint64_t(duration) * 1000) != ESP_OK) {
        portENTER_CRITICAL(&motorMux);
        digitalWrite(MOTOR_PIN, MOTOR_ACTIVE_HIGH ? LOW : HIGH);
        feeding = false;
        portEXIT_CRITICAL(&motorMux);
        timerFailed = true;
        xSemaphoreGive(controlMutex);
        return false;
    }
    xSemaphoreGive(controlMutex);
    return true;
}

void beginMotor() {
    controlMutex = xSemaphoreCreateMutex();
    if (!controlMutex) {
        errorText = "Motor mutex failed";
    }
    pinsReady =
        MOTOR_PIN == 0 || MOTOR_PIN == 1 || (MOTOR_PIN >= 3 && MOTOR_PIN <= 7) || MOTOR_PIN == 10;
    // 先设置关断电平再切换为输出；复位期间仍需硬件默认关断电阻。
    if (pinsReady) {
        digitalWrite(MOTOR_PIN, MOTOR_ACTIVE_HIGH ? LOW : HIGH);
        pinMode(MOTOR_PIN, OUTPUT);
    } else {
        errorText = "Motor GPIO not configured";
    }
    esp_timer_create_args_t timerArgs{};
    timerArgs.callback = stopMotor;
    timerArgs.name = "motor_stop";
    if (esp_timer_create(&timerArgs, &stopTimer) != ESP_OK) {
        stopTimer = nullptr;
        errorText = "Motor timer failed";
    }
}

void stopFeedIfSequence(uint32_t sequence) {
    if (!controlMutex) {
        return;
    }
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    if (sequence != 0 && sequence != motorFeedSequence()) {
        xSemaphoreGive(controlMutex);
        return;
    }
    if (stopTimer) {
        esp_timer_stop(stopTimer);
    }
    portENTER_CRITICAL(&motorMux);
    if (pinsReady) {
        digitalWrite(MOTOR_PIN, MOTOR_ACTIVE_HIGH ? LOW : HIGH);
    }
    feeding = false;
    portEXIT_CRITICAL(&motorMux);
    xSemaphoreGive(controlMutex);
}

void stopFeed() {
    stopFeedIfSequence(0);
}

bool motorIsFeeding() {
    portENTER_CRITICAL(&motorMux);
    const bool active = feeding;
    portEXIT_CRITICAL(&motorMux);
    return active;
}

uint32_t motorFeedSequence() {
    portENTER_CRITICAL(&motorMux);
    const uint32_t sequence = feedSequence;
    portEXIT_CRITICAL(&motorMux);
    return sequence;
}

void tickMotor() {
    if (timerFailed.exchange(false)) {
        errorText = "Motor timer failed";
    }
}
