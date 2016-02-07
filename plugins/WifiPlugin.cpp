#include "WifiPlugin.h"


/*
 * Virtual
 */

WifiPlugin::WifiPlugin() : Plugin(1, 1) {
  loadConfig();
}

String WifiPlugin::getName() {
  return "wifi";
}

int8_t WifiPlugin::getSensorByAddr(const char* addr_c) {
  if (strcmp(addr_c, "wlan") == 0)
    return 0;
  return -1;
}

bool WifiPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  strcpy(addr_c, "wlan");
  return true;
}

float WifiPlugin::getValue(int8_t sensor) {
  return WiFi.RSSI();
}

/**
 * Loop (idle -> uploading)
 */
void WifiPlugin::loop() {
  if (_status == PLUGIN_IDLE && elapsed(10*1000)) {
    _status = PLUGIN_UPLOADING;
    if (WiFi.status() == WL_CONNECTED) {
      upload();
    }
    _status = PLUGIN_IDLE;
  }
}
