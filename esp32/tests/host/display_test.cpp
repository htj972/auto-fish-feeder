#include <ctime>
time_t displayTestTime(time_t *);
#define time displayTestTime
#include "../../src/oled_display.cpp"
#undef time

namespace {
uint32_t testNow = 100;
time_t testTime = 1800000000;
bool charging = false;
bool arming = false;
bool setup = false;
bool online = false;
int percent = 78;
uint32_t held = 0;
bool ipRequested = false;
String setupPassword = "ABC12345";

void step(uint32_t ms = 250) {
    testNow += ms;
    tickDisplay();
}
} // namespace

Settings settings{};
String errorText;

uint8_t savedOledBrightness() {
    return 56;
}

time_t displayTestTime(time_t *) {
    return testTime;
}

uint32_t millis() {
    return testNow;
}

bool networkConnected() {
    return online;
}

int networkSignalLevel() {
    return online ? 3 : 0;
}

bool networkSetupVisible() {
    return setup;
}

String networkApName() {
    return "FISH";
}

String networkApPassword() {
    return setupPassword;
}

String networkApAddress() {
    return "192.168.4.1";
}

String networkAddress() {
    return "192.168.1.123";
}

bool buttonIsArming() {
    return arming;
}

bool buttonIsFeeding() {
    return false;
}

uint32_t buttonHoldElapsed() {
    return held;
}

bool motorIsFeeding() {
    return false;
}

int batteryPercent() {
    return percent;
}

bool batteryIsCharging() {
    return charging;
}

bool takeButtonIpRequest() {
    const bool request = ipRequested;
    ipRequested = false;
    return request;
}

String clockText(time_t now, const char *format) {
    char text[32];
    const tm *local = gmtime(&now);
    strftime(text, sizeof(text), format, local);
    return text;
}

bool nextScheduledFeed(time_t, time_t &when, uint32_t &duration) {
    when = testTime + 3600;
    duration = 5000;
    return true;
}

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    settings.tasks[0].enabled = 1;
    beginDisplay();
    CHECK(displayBrightness() == 56 && oled.commandCalls == 2 && oled.lastCommand == 143);
    tickDisplay();
    CHECK(oled.displayCalls == 1);
    for (int i = 0; i < 100; ++i) {
        step();
    }
    CHECK(oled.displayCalls == 1);
    charging = true;
    step();
    CHECK(oled.displayCalls == 2);
    const uint8_t bolt[] = {0x06, 0x0C, 0x1F, 0x06, 0x0C};
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const bool pixel = oled.getBuffer()[92 + x] & (1 << (1 + y));
            CHECK(pixel == bool(bolt[y] & (0x10 >> x)));
        }
    }
    for (int i = 0; i < 100; ++i) {
        step();
    }
    CHECK(oled.displayCalls == 2);
    percent = 79;
    step();
    CHECK(oled.displayCalls == 3);
    testTime += 60;
    step();
    CHECK(oled.displayCalls == 4);
    settings.tasks[1].enabled = 1;
    step();
    CHECK(oled.displayCalls == 5);
    arming = true;
    held = 500;
    step(50);
    CHECK(oled.displayCalls == 6);
    step(50);
    CHECK(oled.displayCalls == 6);
    held = 1000;
    step(50);
    CHECK(oled.displayCalls == 7);
    arming = false;
    step(50);
    CHECK(oled.displayCalls == 8);
    setup = true;
    step(50);
    CHECK(oled.displayCalls == 9);
    for (int i = 0; i < 20; ++i) {
        step(500);
    }
    CHECK(oled.displayCalls == 9);
    setup = false;
    ipRequested = true;
    step(50);
    CHECK(oled.displayCalls == 10);
    step(4999);
    CHECK(oled.displayCalls == 10);
    step(50); // 页面切换允许一次50ms显示评估周期。
    CHECK(oled.displayCalls == 11);
    setup = true;
    setupPassword = "ABCDEFGHIJKLMNOP12345678";
    step(500);
    const unsigned beforeScroll = oled.displayCalls;
    for (int i = 0; i < 8; ++i) {
        step(500);
    }
    CHECK(oled.displayCalls > beforeScroll);
    const unsigned beforeBrightness = oled.displayCalls;
    CHECK(setDisplayBrightness(1) && oled.lastCommand == 3);
    CHECK(setDisplayBrightness(100) && oled.lastCommand == 255);
    CHECK(displayBrightness() == 100);
    const unsigned beforeInvalid = oled.commandCalls;
    CHECK(!setDisplayBrightness(0) && !setDisplayBrightness(101));
    CHECK(oled.commandCalls == beforeInvalid && oled.displayCalls == beforeBrightness);
    return 0;
}
