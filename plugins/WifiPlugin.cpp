#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "WifiPlugin.h"


/*
 * Virtual
 */

WifiPlugin::WifiPlugin() {
  devs = 1;
}

String WifiPlugin::getName() {
  return "wifi";
}

int8_t WifiPlugin::getSensors() {
  return devs;
}

int8_t WifiPlugin::getSensorByAddr(const char* addr_c) {
  if (addr_c == "wlan")
    return 0;
  return -1;
}

bool WifiPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  addr_c = "wlan";
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

void WifiPlugin::getPluginJson(JsonObject* json) {
  JsonArray& sensorlist = json->createNestedArray("sensors");
  JsonObject& data = sensorlist.createNestedObject();
  getSensorJson(&data, 0);
}

void WifiPlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= devs)
    return;
  (*json)["addr"] = "wlan";
  (*json)["uuid"] = "";
  (*json)["value"] = WiFi.RSSI();
}

/**
 * Loop
 */
void WifiPlugin::loop() {
}
