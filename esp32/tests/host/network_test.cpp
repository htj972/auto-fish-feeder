#include "../../src/network.cpp"

uint32_t testNow = 100;
String storedSsid = "OLD", storedPassword = "old-password";
String errorText;

uint32_t millis() {
    return testNow;
}

String savedSsid() {
    return storedSsid;
}

String savedPassword() {
    return storedPassword;
}

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    beginNetwork();
    CHECK(WiFi.sta && !WiFi.ap && !WiFi.autoReconnect);
    WiFi.state = WL_CONNECTED;
    tickNetwork();
    WiFi.state = WL_IDLE_STATUS;
    testNow = 25000;
    tickNetwork();
    CHECK(networkApActive() && dns.active);

    // 断网后提交新配置：保留AP，先停旧STA，等待后才启动新连接。
    storedSsid = "NEW";
    storedPassword = "new-password";
    const unsigned oldAttempts = WiFi.attempts;
    connectNetwork(storedSsid, storedPassword);
    CHECK(!WiFi.sta && WiFi.ap && connectionPending);
    testNow += 199;
    tickNetwork();
    CHECK(WiFi.attempts == oldAttempts && networkApActive());
    testNow += 1;
    tickNetwork();
    CHECK(WiFi.attempts == oldAttempts + 1 && WiFi.sta && WiFi.ap);
    CHECK(WiFi.ssid == "NEW" && WiFi.password == "new-password");
    CHECK(!startNetworkScan());

    // 连接超时后热点仍可用；定时重试重新加载新凭据。
    testNow += 20001;
    tickNetwork();
    CHECK(networkApActive() && !waitingForConnection);
    WiFi.ssid = "STALE";
    testNow += 10000;
    tickNetwork();
    CHECK(connectionPending && !WiFi.sta && WiFi.ap);
    testNow += 200;
    tickNetwork();
    CHECK(WiFi.ssid == "NEW" && WiFi.password == "new-password");
    WiFi.state = WL_CONNECTED;
    tickNetwork();
    CHECK(networkApActive());
    CHECK(networkConnectionJson().find("connected") != String::npos);
    testNow += 5000;
    tickNetwork();
    CHECK(!networkApActive() && !dns.active && WiFi.sta && !WiFi.ap);
    // 驱动认证失败原因通过主循环反馈，不能把所有失败都说成密码错误。
    connectNetwork(storedSsid, storedPassword);
    testNow += 200;
    tickNetwork();
    WiFi.eventCallback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, {{WIFI_REASON_AUTH_FAIL}});
    tickNetwork();
    CHECK(networkConnectionJson().find("auth_failed") != String::npos);
    CHECK(networkApActive());
    connectNetwork(storedSsid, storedPassword);
    testNow += 200;
    tickNetwork();
    WiFi.eventCallback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, {{WIFI_REASON_NO_AP_FOUND}});
    tickNetwork();
    CHECK(networkConnectionJson().find("not_found") != String::npos);
    return 0;
}
