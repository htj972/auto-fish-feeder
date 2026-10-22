#include "settings_store.h"
#include "app_state.h"
#include "board_config.h"
#include <Preferences.h>
Settings settings{};
int32_t lastDay[TASK_COUNT]{};

namespace {
Preferences prefs;
bool storageReady = false;
String ssid, password;
// 56%映射为143，接近128×32 SSD1306原先的0x8F默认对比度。
uint8_t oledBrightness = 56;
uint8_t oledIdleSeconds = OLED_IDLE_MS / 1000;
} // namespace

bool validSettings(const Settings &s) {
    if (s.version != 1 || s.manualMs < 100 || s.manualMs > MAX_MOTOR_MS) {
        return false;
    }
    for (const auto &t : s.tasks) {
        if (t.enabled > 1 || t.hour > 23 || t.minute > 59 || t.duration < 100 ||
            t.duration > MAX_MOTOR_MS) {
            return false;
        }
    }
    return true;
}

void beginSettings() {
    settings.version = 1;
    settings.manualMs = DEFAULT_FEED_MS;
    for (auto &t : settings.tasks) {
        t.duration = DEFAULT_FEED_MS;
    }
    storageReady = prefs.begin("fishfeeder", false);
    // 仅接受长度、版本和字段范围均匹配的存储内容，否则保留默认值。
    Settings loaded{};
    if (storageReady) {
        const uint8_t storedIdle = prefs.getUChar("oled_idle", OLED_IDLE_MS / 1000);
        oledIdleSeconds = storedIdle == 0 || (storedIdle >= OLED_IDLE_MIN_SECONDS &&
                                              storedIdle <= OLED_IDLE_MAX_SECONDS)
                              ? storedIdle
                              : OLED_IDLE_MS / 1000;
        const uint8_t storedBrightness = prefs.getUChar("oled_level", 56);
        if (storedBrightness >= 1 && storedBrightness <= 100) {
            oledBrightness = storedBrightness;
        }
        if (prefs.getBytesLength("settings") == sizeof(loaded)) {
            prefs.getBytes("settings", &loaded, sizeof(loaded));
            if (validSettings(loaded)) {
                settings = loaded;
            }
        }
        ssid = prefs.getString("ssid");
        password = prefs.getString("password");
        for (int i = 0; i < TASK_COUNT; ++i) {
            String key = "day" + String(i);
            lastDay[i] = prefs.getInt(key.c_str(), -1);
        }
    } else {
        errorText = "NVS storage failed";
    }
}

bool saveConfiguration(const Settings &next) {
    if (!validSettings(next) || !storageReady ||
        prefs.putBytes("settings", &next, sizeof(next)) != sizeof(next)) {
        return false;
    }
    settings = next;
    return true;
}

bool saveTaskDay(int index, int32_t day) {
    if (index < 0 || index >= TASK_COUNT || !storageReady) {
        return false;
    }
    String key = "day" + String(index);
    if (prefs.putInt(key.c_str(), day) != sizeof(int32_t)) {
        return false;
    }
    lastDay[index] = day;
    return true;
}

String savedSsid() {
    return ssid;
}

String savedPassword() {
    return password;
}

bool saveWifiCredentials(const String &nextSsid, const String &nextPassword) {
    if (!storageReady || prefs.putString("ssid", nextSsid) != nextSsid.length() ||
        prefs.putString("password", nextPassword) != nextPassword.length()) {
        return false;
    }
    ssid = nextSsid;
    password = nextPassword;
    return true;
}

uint8_t savedOledBrightness() {
    return oledBrightness;
}

bool saveOledBrightness(uint8_t percent) {
    if (percent < 1 || percent > 100 || !storageReady) {
        return false;
    }
    if (percent != oledBrightness && prefs.putUChar("oled_level", percent) != sizeof(uint8_t)) {
        return false;
    }
    oledBrightness = percent;
    return true;
}

uint8_t savedOledIdleSeconds() {
    return oledIdleSeconds;
}

bool saveOledIdleSeconds(uint8_t seconds) {
    if (!storageReady ||
        (seconds != 0 && (seconds < OLED_IDLE_MIN_SECONDS || seconds > OLED_IDLE_MAX_SECONDS))) {
        return false;
    }
    if (seconds != oledIdleSeconds && prefs.putUChar("oled_idle", seconds) != sizeof(uint8_t)) {
        return false;
    }
    oledIdleSeconds = seconds;
    return true;
}
