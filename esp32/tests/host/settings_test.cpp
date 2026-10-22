#include "../../src/settings_store.cpp"
#include <stdint.h>

String errorText = "";

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    beginSettings();
    CHECK(savedOledBrightness() == 56);
    CHECK(savedOledIdleSeconds() == 30);
    Settings next = settings;
    next.tasks[0].enabled = 1;
    next.tasks[0].hour = 8;
    next.tasks[0].duration = 5000;
    CHECK(saveConfiguration(next));
    CHECK(saveOledBrightness(1));
    beginSettings();
    CHECK(savedOledBrightness() == 1);
    CHECK(settings.tasks[0].enabled == 1 && settings.tasks[0].duration == 5000);
    CHECK(saveOledBrightness(100));
    CHECK(!saveOledBrightness(0) && !saveOledBrightness(101));
    beginSettings();
    CHECK(savedOledBrightness() == 100);
    CHECK(settings.tasks[0].hour == 8);
    CHECK(saveOledIdleSeconds(15));
    beginSettings();
    CHECK(savedOledIdleSeconds() == 15);
    CHECK(saveOledIdleSeconds(90));
    CHECK(!saveOledIdleSeconds(14) && !saveOledIdleSeconds(91));
    beginSettings();
    CHECK(savedOledIdleSeconds() == 90);
    CHECK(saveOledIdleSeconds(0));
    beginSettings();
    CHECK(savedOledIdleSeconds() == 0 && settings.tasks[0].hour == 8);
    return 0;
}
