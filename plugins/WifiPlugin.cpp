#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "WifiPlugin.h"


/*
 * Virtual
 */

WifiPlugin::WifiPlugin() : devices(), devs(1) {
  File configFile = SPIFFS.open(F("/wifi.config"), "r");
  if (configFile.size() == sizeof(devices)) {
    configFile.read((uint8_t*)&devices, sizeof(devices));
  }
  configFile.close();
}

String WifiPlugin::getName() {
  return "wifi";
}

int8_t WifiPlugin::getSensors() {
  return devs;
}

int8_t WifiPlugin::getSensorByAddr(const char* addr_c) {
  if (strcmp(addr_c, "wlan") == 0)
    return 0;
  return -1;
}

bool WifiPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  strcpy(addr_c, "wlan");
  return true;
}

bool WifiPlugin::getUuid(char* uuid_c, int8_t sensor) {
  return false;
}

bool WifiPlugin::setUuid(const char* uuid_c, int8_t sensor) {
  return false;
}

float WifiPlugin::getValue(int8_t sensor) {
  return analogRead(A0) / 1023.0;
}

void WifiPlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= devs)
    return;
  Plugin::getSensorJson(json, sensor);
  (*json)[F("value")] = WiFi.RSSI();
}

/**
 * Loop
 */
void WifiPlugin::loop() {
}
