#pragma once
#include <Arduino.h>
void beginNetwork();
void tickNetwork();
void startAP();
bool networkApActive();
bool networkSetupVisible();
String networkApName();
String networkApPassword();
String networkApAddress();
int networkSignalLevel();
bool networkConnected();
String networkSsid();
String networkAddress();
void connectNetwork(const String &ssid, const String &password);
bool startNetworkScan();
String networkScanJson();
String networkConnectionJson();
