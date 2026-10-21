#include "oled_display.h"
#include "app_state.h"
#include "board_config.h"
#include "button.h"
#include "motor.h"
#include "network.h"
#include "power_monitor.h"
#include "scheduler.h"
#include "settings_store.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <string.h>

namespace {
Adafruit_SSD1306 oled(128, 32, &Wire, -1);
bool oledReady = false;
bool displaySleeping = false;
uint32_t lastActivity = 0;
bool displayWakeRequested = false;
uint8_t brightnessPercent = 56;
uint32_t lastDisplay = 0;
uint32_t ipShownAt = 0;
uint32_t ipShowDuration = 0;
bool wasConnected = false;
String lastFrameKey;
uint32_t lastEvaluation = 0;
bool hasFrame = false;
uint8_t previousFrame[128 * 32 / 8];
enum class Page { Home, Arming, Feeding, Ip, Setup, Alert };
Page lastPage = Page::Home;

void drawText(int x, int y, const String &text, uint8_t scale = 1) {
    for (unsigned i = 0; i < text.length(); ++i) {
        if (text[i] == ':') {
            // 窄冒号为主体右侧的小字号时长留出空间。
            oled.fillRect(x, y + scale, scale, 2 * scale, SSD1306_WHITE);
            oled.fillRect(x, y + 4 * scale, scale, 2 * scale, SSD1306_WHITE);
            x += 2 * scale;
        } else {
            oled.drawChar(x, y, text[i], SSD1306_WHITE, SSD1306_BLACK, scale);
            x += 6 * scale;
        }
    }
}

void drawCentered(int y, const String &text, uint8_t scale = 1) {
    drawText((128 - int(text.length()) * 6 * scale + scale) / 2, y, text, scale);
}

void drawStatus(time_t now) {
    // 与预览一致的45°扇形图标：根据RSSI逐级显示内、中、外三道圆弧。
    const uint8_t wifi[] = {0xE0, 0x18, 0x04, 0xE2, 0x12, 0xC9, 0x29, 0xA9};
    const int signal = networkSignalLevel();
    if (signal == 0) {
        drawText(1, 0, "--");
    } else {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const int distanceSquared = x * x + (7 - y) * (7 - y);
                const int limit = signal == 1 ? 9 : (signal == 2 ? 25 : 100);
                if ((wifi[y] & (0x80 >> x)) && distanceSquared <= limit) {
                    oled.drawPixel(x + 1, y, SSD1306_WHITE);
                }
            }
        }
    }
    drawText(42, 0, clockText(now, "%H:%M"));
    oled.drawRect(88, 0, 14, 7, SSD1306_WHITE);
    oled.fillRect(102, 2, 1, 3, SSD1306_WHITE);
    const int percent = batteryPercent();
    if (percent >= 0) {
        if (batteryIsCharging()) {
            // 5×5静态闪电，取消充电时周期填充动画，亮度保持原设置。
            const uint8_t bolt[] = {0x06, 0x0C, 0x1F, 0x06, 0x0C};
            for (int y = 0; y < 5; ++y) {
                for (int x = 0; x < 5; ++x) {
                    if (bolt[y] & (0x10 >> x)) {
                        oled.drawPixel(92 + x, 1 + y, SSD1306_WHITE);
                    }
                }
            }
        } else {
            oled.fillRect(90, 2, (percent * 10 + 50) / 100, 3, SSD1306_WHITE);
        }
    }
    const String label = percent >= 0 ? String(percent) + "%" : "--";
    drawText(128 - int(label.length()) * 6, 0, label);
}

String setupRowValue(const String &value) {
    // 保留标签，长SSID/密码以完整字符往返滚动，不截断凭据。
    constexpr unsigned visibleChars = 16;
    unsigned offset = 0;
    if (value.length() > visibleChars) {
        const unsigned travel = value.length() - visibleChars;
        const unsigned phase = (millis() / 500) % (2 * travel + 6);
        if (phase > 2 && phase <= travel + 2) {
            offset = phase - 2;
        } else if (phase > travel + 2 && phase <= travel + 5) {
            offset = travel;
        } else if (phase > travel + 5) {
            offset = 2 * travel + 6 - phase;
        }
    }
    return value.substring(offset, offset + visibleChars);
}

void drawSetupRow(int y, const char *label, const String &value) {
    drawText(0, y, label);
    drawText(30, y, setupRowValue(value));
}

void drawHome(time_t now) {
    // 统计已启用的每日任务总数，不随当天已完成任务减少。
    unsigned enabledTasks = 0;
    for (const auto &task : settings.tasks) {
        if (task.enabled) {
            ++enabledTasks;
        }
    }
    const String taskLabel = String(enabledTasks) + (enabledTasks == 1 ? " TASK" : " TASKS");
    drawText(128 - int(taskLabel.length()) * 6, 12, taskLabel);

    time_t when;
    uint32_t duration;
    const bool hasTask = nextScheduledFeed(now, when, duration);
    drawText(5, 10, hasTask ? clockText(when, "%H:%M") : "--:--", 3);
    if (hasTask) {
        const String seconds =
            duration % 1000 == 0 ? String(duration / 1000) : String(duration / 1000.0f, 1);
        const String label = "+" + seconds + "s";
        drawText(128 - int(label.length()) * 6, 24, label);
    }
}
} // namespace

void beginDisplay() {
    if (OLED_SDA >= 0 && OLED_SCL >= 0) {
        Wire.begin(OLED_SDA, OLED_SCL);
        Wire.setTimeOut(50);
        Wire.beginTransmission(OLED_ADDRESS);
        if (Wire.endTransmission() == 0) {
            oledReady = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, true, false);
        }
        if (oledReady) {
            oled.setTextSize(1);
            oled.setTextColor(SSD1306_WHITE);
            oled.setTextWrap(false);
            setDisplayBrightness(savedOledBrightness());
            lastActivity = millis();
        } else {
            errorText = "OLED not detected";
        }
    }
}

uint8_t displayBrightness() {
    return brightnessPercent;
}

void wakeDisplay() {
    // Web与显示均在主循环执行，留到下一次显示周期刷新画面后开屏。
    displayWakeRequested = true;
}

bool setDisplayBrightness(uint8_t percent) {
    if (!oledReady || percent < 1 || percent > 100) {
        return false;
    }
    // 仅修改全屏对比度寄存器，不强制刷新帧，不改变按需刷新策略。
    const uint8_t contrast = (uint16_t(percent) * 255 + 50) / 100;
    oled.ssd1306_command(SSD1306_SETCONTRAST);
    oled.ssd1306_command(contrast);
    brightnessPercent = percent;
    return true;
}

void tickDisplay() {
    if (!oledReady || millis() - lastDisplay < 50) {
        return;
    }
    lastDisplay = millis();
    const bool buttonActivity = takeButtonDisplayActivity();
    if (buttonActivity || displayWakeRequested) {
        displayWakeRequested = false;
        lastActivity = lastDisplay;
        if (displaySleeping) {
            // 先在关屏状态更新画面，最后开屏，避免闪现旧时间。
            hasFrame = false;
        }
    }
    if (lastDisplay - lastActivity >= OLED_IDLE_MS) {
        if (!displaySleeping) {
            // SSD1306硬件关屏，停止面板驱动；无需持续发送黑色画面。
            oled.ssd1306_command(SSD1306_DISPLAYOFF);
            displaySleeping = true;
        }
        wasConnected = networkConnected();
        return;
    }
    const bool connected = networkConnected();
    if (connected && !wasConnected) {
        ipShownAt = millis();
        ipShowDuration = 8000;
    }
    wasConnected = connected;
    if (takeButtonIpRequest()) {
        ipShownAt = millis();
        ipShowDuration = 5000;
    }
    time_t now = time(nullptr);
    const bool arming = buttonIsArming();
    const Page page = arming                                                    ? Page::Arming
                      : motorIsFeeding()                                        ? Page::Feeding
                      : ipShowDuration && millis() - ipShownAt < ipShowDuration ? Page::Ip
                      : networkSetupVisible()                                   ? Page::Setup
                      : !errorText.isEmpty()                                    ? Page::Alert
                                                                                : Page::Home;
    if (arming) {
        ipShowDuration = 0;
    }
    // 动态进度每50ms评估，滚动每500ms，其余页面每250ms检查内容是否改变。
    const uint32_t interval = page == Page::Arming ? 50 : page == Page::Setup ? 500 : 250;
    if (hasFrame && page == lastPage && millis() - lastEvaluation < interval) {
        return;
    }
    lastPage = page;
    lastEvaluation = millis();
    String frameKey = String(int(page));
    if (page == Page::Setup) {
        frameKey += "|" + setupRowValue(networkApName()) + "|" +
                    setupRowValue(networkApPassword()) + "|" + setupRowValue(networkApAddress());
    } else {
        frameKey += "|" + clockText(now, "%H:%M") + "|" + String(networkSignalLevel()) + "|" +
                    String(batteryPercent()) + "|" + String(batteryIsCharging());
        if (page == Page::Home) {
            time_t when;
            uint32_t duration;
            const bool next = nextScheduledFeed(now, when, duration);
            frameKey += "|" + (next ? clockText(when, "%H:%M") : "--:--") + "|" + String(duration);
            for (const auto &task : settings.tasks) {
                frameKey += task.enabled ? '1' : '0';
            }
        } else if (page == Page::Arming) {
            frameKey += "|" + String(112 * min(buttonHoldElapsed(), uint32_t(MANUAL_HOLD_MS)) /
                                     MANUAL_HOLD_MS);
        } else if (page == Page::Feeding) {
            frameKey += "|" + String(buttonIsFeeding());
        } else if (page == Page::Ip) {
            frameKey += "|" + (connected ? networkAddress() : "OFFLINE");
        }
    }
    if (hasFrame && frameKey == lastFrameKey) {
        return;
    }
    lastFrameKey = frameKey;
    oled.clearDisplay();
    if (page == Page::Arming) {
        drawStatus(now);
        drawCentered(12, "HOLD 2s");
        for (int x = 8; x < 120; x += 2) {
            oled.drawPixel(x, 27, SSD1306_WHITE);
        }
        const uint32_t held = min(buttonHoldElapsed(), uint32_t(MANUAL_HOLD_MS));
        oled.fillRect(8, 25, 112 * held / MANUAL_HOLD_MS, 5, SSD1306_WHITE);
    } else if (page == Page::Feeding) {
        drawStatus(now);
        drawCentered(10, "FEEDING", 2);
        drawCentered(25, buttonIsFeeding() ? "RELEASE TO STOP" : "AUTO / WEB");
    } else if (page == Page::Ip) {
        drawStatus(now);
        drawCentered(12, connected ? "IP ADDRESS" : "OFFLINE");
        if (connected) {
            drawCentered(24, networkAddress());
        }
    } else if (page == Page::Setup) {
        drawSetupRow(1, "SSID:", networkApName());
        drawSetupRow(12, "PASS:", networkApPassword());
        drawSetupRow(23, "WEB:", networkApAddress());
    } else {
        drawStatus(now);
        if (page == Page::Alert) {
            drawCentered(10, "ALERT", 2);
            drawCentered(25, "SEE WEB");
        } else {
            drawHome(now);
        }
    }
    // 最终逐字节确认像素变化，连RSSI抖动或设置变化造成的相同画面也不重复传输。
    const uint8_t *frame = oled.getBuffer();
    if (!hasFrame || memcmp(previousFrame, frame, sizeof(previousFrame)) != 0) {
        oled.display();
        memcpy(previousFrame, frame, sizeof(previousFrame));
    }
    hasFrame = true;
    if (displaySleeping) {
        oled.ssd1306_command(SSD1306_DISPLAYON);
        displaySleeping = false;
    }
}
