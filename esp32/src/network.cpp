#include "network.h"
#include "app_state.h"
#include "settings_store.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <algorithm>
#include <atomic>
#include <esp_wifi.h>

namespace {
DNSServer dns;
bool apActive = false;
bool setupVisible = false;
bool waitingForConnection = false;
bool connectionPending = false;
String targetSsid, targetPassword;
String apName, apPassword;
uint32_t lastReconnect = 0;
enum class ScanState { Idle, Running, Ready, Failed };
ScanState scanState = ScanState::Idle;

struct ScanEntry {
    String ssid;
    int rssi;
    bool secure;
};

ScanEntry scanEntries[24];
int scanCount = 0;
uint32_t scanStarted = 0;
bool mdnsActive = false;
std::atomic<uint8_t> disconnectReason{0};
const char *connectionState = "idle";
uint32_t connectedAt = 0;
bool connectedPreviously = false;
constexpr uint32_t AP_CLOSE_DELAY_MS = 5000;

String jsonString(const String &value) {
    String out = "\"";
    for (unsigned i = 0; i < value.length(); ++i) {
        const uint8_t ch = value[i];
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += char(ch);
        } else if (ch < 32) {
            char escaped[7];
            snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
            out += escaped;
        } else {
            out += char(ch);
        }
    }
    return out + "\"";
}

void tickScan() {
    if (scanState != ScanState::Running) {
        return;
    }
    const int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) {
        if (millis() - scanStarted > 15000) {
            esp_wifi_scan_stop();
            WiFi.scanDelete();
            scanState = ScanState::Failed;
        }
        return;
    }
    scanCount = 0;
    if (found < 0) {
        scanState = ScanState::Failed;
        WiFi.scanDelete();
        return;
    }
    for (int i = 0; i < found; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) {
            continue;
        }
        const int rssi = WiFi.RSSI(i);
        int slot = -1;
        for (int j = 0; j < scanCount; ++j) {
            if (scanEntries[j].ssid == ssid) {
                slot = j;
                break;
            }
        }
        if (slot >= 0 && scanEntries[slot].rssi >= rssi) {
            continue;
        }
        if (slot < 0) {
            if (scanCount < 24) {
                slot = scanCount++;
            } else {
                slot = 0;
                for (int j = 1; j < scanCount; ++j) {
                    if (scanEntries[j].rssi < scanEntries[slot].rssi) {
                        slot = j;
                    }
                }
                if (scanEntries[slot].rssi >= rssi) {
                    continue;
                }
            }
        }
        scanEntries[slot] = {ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN};
    }
    std::sort(scanEntries, scanEntries + scanCount, [](const ScanEntry &a, const ScanEntry &b) {
        return a.rssi > b.rssi;
    });
    WiFi.scanDelete();
    scanState = ScanState::Ready;
}
} // namespace

void startAP() {
    setupVisible = true;
    if (apActive) {
        return;
    }
    if (!WiFi.softAP(apName.c_str(), apPassword.c_str())) {
        errorText = "AP failed";
        return;
    }
    apActive = true;
    dns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Setup AP: %s  password: %s  http://192.168.4.1\n", apName.c_str(),
                  apPassword.c_str());
}

void beginNetwork() {
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            // WiFi回调不修改String或操作Web，主循环消费失败原因。
            disconnectReason = info.wifi_sta_disconnected.reason;
        }
    });
    apName = "FishFeeder-" + String(uint32_t(ESP.getEfuseMac()), HEX);
    // Stable unique setup password, shown through USB CDC; change for production.
    char deviceId[9];
    snprintf(deviceId, sizeof(deviceId), "%08lx",
             static_cast<unsigned long>(uint32_t(ESP.getEfuseMac())));
    apPassword = "Fish" + String(deviceId);
    WiFi.persistent(false);
    WiFi.setHostname("fishfeeder");
    // 正常运行只开启STA；需要配网时softAP()再启用热点。
    WiFi.mode(WIFI_STA);
    // 重试由本模块统一管理，避免旧网络自动重连干扰用户提交的新配置。
    WiFi.setAutoReconnect(false);
    lastReconnect = millis();
    String ssid = savedSsid(), password = savedPassword();
    if (!ssid.isEmpty()) {
        connectionState = "connecting";
        targetSsid = ssid;
        waitingForConnection = true;
        WiFi.begin(ssid.c_str(), password.c_str());
    } else {
        startAP();
    }
}

void connectNetwork(const String &ssid, const String &password) {
    if (scanState == ScanState::Running) {
        esp_wifi_scan_stop();
        WiFi.scanDelete();
        scanState = ScanState::Idle;
    }
    startAP();
    targetSsid = ssid;
    targetPassword = password;
    waitingForConnection = true;
    connectionState = "connecting";
    connectedPreviously = false;
    connectionPending = true;
    // 只重置STA，保留AP和配网页面；下一轮延后连接，让旧连接事件先处理完。
    WiFi.enableSTA(false);
    lastReconnect = millis();
}

bool networkApActive() {
    return apActive;
}

bool networkSetupVisible() {
    return apActive && setupVisible;
}

String networkApName() {
    return apName;
}

String networkApPassword() {
    return apPassword;
}

String networkApAddress() {
    return WiFi.softAPIP().toString();
}

int networkSignalLevel() {
    if (!networkConnected()) {
        return 0;
    }
    const int rssi = WiFi.RSSI();
    return rssi >= -55 ? 3 : (rssi >= -70 ? 2 : 1);
}

bool networkConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String networkSsid() {
    return WiFi.SSID();
}

String networkAddress() {
    return WiFi.localIP().toString();
}

bool startNetworkScan() {
    if (scanState == ScanState::Running) {
        return true;
    }
    if (waitingForConnection ||
        (!networkConnected() && !savedSsid().isEmpty() && millis() - lastReconnect < 15000)) {
        return false;
    }
    WiFi.scanDelete();
    const int result = WiFi.scanNetworks(true, false, false, 120);
    scanStarted = millis();
    scanState = result == WIFI_SCAN_FAILED ? ScanState::Failed : ScanState::Running;
    return scanState == ScanState::Running;
}

String networkScanJson() {
    const char *state = scanState == ScanState::Running  ? "running"
                        : scanState == ScanState::Ready  ? "ready"
                        : scanState == ScanState::Failed ? "failed"
                                                         : "idle";
    String json = "{\"state\":\"" + String(state) + "\",\"networks\":[";
    if (scanState == ScanState::Ready) {
        for (int i = 0; i < scanCount; ++i) {
            if (i) {
                json += ',';
            }
            json += "{\"ssid\":" + jsonString(scanEntries[i].ssid) +
                    ",\"rssi\":" + String(scanEntries[i].rssi) +
                    ",\"secure\":" + (scanEntries[i].secure ? "true" : "false") + "}";
        }
    }
    return json + "]}";
}

String networkConnectionJson() {
    const bool online = networkConnected() && !connectionPending &&
                        (!waitingForConnection || WiFi.SSID() == targetSsid);
    return String("{\"state\":") + jsonString(online ? "connected" : connectionState) +
           ",\"ip\":" + jsonString(online ? networkAddress() : "") + "}";
}

void tickNetwork() {
    tickScan();
    if (apActive) {
        dns.processNextRequest();
    }
    if (connectionPending) {
        if (millis() - lastReconnect < 200) {
            return;
        }
        connectionPending = false;
        disconnectReason = 0;
        lastReconnect = millis();
        const wl_status_t result = WiFi.begin(targetSsid.c_str(), targetPassword.c_str());
        targetPassword = "";
        Serial.printf("WiFi connection requested, status=%d\n", int(result));
        if (result == WL_CONNECT_FAILED) {
            connectionState = "failed";
            waitingForConnection = false;
            errorText = "WiFi connection start failed; retry setup";
        }
    }
    String ssid = savedSsid();
    if (WiFi.status() != WL_CONNECTED && millis() > 20000) {
        startAP();
    }
    if (!waitingForConnection && scanState != ScanState::Running && !ssid.isEmpty() &&
        WiFi.status() != WL_CONNECTED && millis() - lastReconnect > 30000) {
        // 显式重新加载已保存凭据，不使用可能仍保留旧配置的reconnect()。
        connectNetwork(ssid, savedPassword());
        return;
    }
    bool online = WiFi.status() == WL_CONNECTED;
    if (online && !connectedPreviously) {
        connectedAt = millis();
        connectionState = "connected";
        Serial.println("Web: http://" + WiFi.localIP().toString());
        setupVisible = false;
        if (mdnsActive) {
            MDNS.end();
        }
        mdnsActive = MDNS.begin("fishfeeder");
        if (mdnsActive) {
            MDNS.addService("http", "tcp", 80);
        }
    }
    if (online && waitingForConnection && WiFi.SSID() == savedSsid()) {
        setupVisible = false;
        waitingForConnection = false;
    }
    // 等目标网络取得IP后关闭AP及配网DNS，保留STA连接和局域网Web。
    if (online && !waitingForConnection && apActive &&
        millis() - connectedAt >= AP_CLOSE_DELAY_MS && WiFi.mode(WIFI_STA)) {
        dns.stop();
        apActive = false;
        setupVisible = false;
        Serial.println("Setup AP stopped; use LAN Web address");
    }
    if (!online && waitingForConnection && millis() - lastReconnect > 20000) {
        waitingForConnection = false;
        connectionState = "timeout";
        Serial.printf("WiFi connection timed out, status=%d; setup AP remains active\n",
                      int(WiFi.status()));
    }
    const uint8_t reason = disconnectReason.exchange(0);
    if (!online && waitingForConnection) {
        if (reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
            reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            connectionState = "auth_failed";
            waitingForConnection = false;
        } else if (reason == WIFI_REASON_NO_AP_FOUND) {
            connectionState = "not_found";
            waitingForConnection = false;
        }
    }
    if (!online && connectedPreviously) {
        connectionState = "disconnected";
    }
    connectedPreviously = online;
}
