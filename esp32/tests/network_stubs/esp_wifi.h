#pragma once
constexpr int WIFI_REASON_AUTH_FAIL = 202;
constexpr int WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT = 15;
constexpr int WIFI_REASON_HANDSHAKE_TIMEOUT = 204;
constexpr int WIFI_REASON_NO_AP_FOUND = 201;

inline void esp_wifi_scan_stop() {
}
