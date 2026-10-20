#include "network.h"
#include "app_state.h"
#include "settings_store.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <algorithm>
#include <esp_wifi.h>

namespace {
DNSServer dns;
bool apActive = false;
bool setupVisible = false;
bool waitingForConnection = false;
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
    apName = "FishFeeder-" + String(uint32_t(ESP.getEfuseMac()), HEX);
    // Stable unique setup password, shown through USB CDC; change for production.
    char deviceId[9];
    snprintf(deviceId, sizeof(deviceId), "%08lx",
             static_cast<unsigned long>(uint32_t(ESP.getEfuseMac())));
    apPassword = "Fish" + String(deviceId);
    WiFi.persistent(false);
    WiFi.setHostname("fishfeeder");
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    String ssid = savedSsid(), password = savedPassword();
    if (!ssid.isEmpty()) {
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
    waitingForConnection = true;
    WiFi.begin(ssid.c_str(), password.c_str());
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

void tickNetwork() {
    tickScan();
    if (apActive) {
        dns.processNextRequest();
    }
    String ssid = savedSsid();
    if (WiFi.status() != WL_CONNECTED && millis() > 20000) {
        startAP();
    }
    if (scanState != ScanState::Running && !ssid.isEmpty() && WiFi.status() != WL_CONNECTED &&
        millis() - lastReconnect > 30000) {
        lastReconnect = millis();
        WiFi.reconnect();
    }
    static bool connected = false;
    bool online = WiFi.status() == WL_CONNECTED;
    if (online && !connected) {
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
    if (!online && waitingForConnection && millis() - lastReconnect > 20000) {
        waitingForConnection = false;
    }
    connected = online;
}
