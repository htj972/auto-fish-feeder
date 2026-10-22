#pragma once
#include "Arduino.h"
constexpr int WIFI_STA = 1, WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2;
constexpr int WIFI_AUTH_OPEN = 0;

enum wl_status_t { WL_IDLE_STATUS, WL_CONNECTED, WL_CONNECT_FAILED };

using WiFiEvent_t = int;
constexpr int ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 1;

struct WiFiEventInfo_t {
    struct {
        uint8_t reason;
    } wifi_sta_disconnected;
};

struct TestIp {
    String toString() {
        return "192.168.1.100";
    }
};

struct TestWiFi {
    void (*eventCallback)(WiFiEvent_t, WiFiEventInfo_t) = nullptr;

    void onEvent(void (*callback)(WiFiEvent_t, WiFiEventInfo_t)) {
        eventCallback = callback;
    }

    bool sta = true, ap = false, autoReconnect = true;
    unsigned attempts = 0;
    wl_status_t state = WL_IDLE_STATUS;
    String ssid, password;

    bool mode(int) {
        sta = true;
        ap = false;
        return true;
    }

    bool enableSTA(bool enabled) {
        sta = enabled;
        state = WL_IDLE_STATUS;
        return true;
    }

    bool softAP(const char *, const char *) {
        ap = true;
        return true;
    }

    void persistent(bool) {
    }

    void setHostname(const char *) {
    }

    void setAutoReconnect(bool enabled) {
        autoReconnect = enabled;
    }

    wl_status_t begin(const char *s, const char *p) {
        sta = true;
        ssid = s;
        password = p;
        ++attempts;
        state = WL_IDLE_STATUS;
        return state;
    }

    wl_status_t status() {
        return state;
    }

    String SSID(int = 0) {
        return ssid;
    }

    int RSSI(int = 0) {
        return -50;
    }

    int encryptionType(int) {
        return 1;
    }

    TestIp softAPIP() {
        return {};
    }

    TestIp localIP() {
        return {};
    }

    void scanDelete() {
    }

    int scanComplete() {
        return WIFI_SCAN_RUNNING;
    }

    int scanNetworks(bool, bool, bool, int) {
        return WIFI_SCAN_RUNNING;
    }
} inline WiFi;
