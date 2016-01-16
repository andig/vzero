#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "AnalogPlugin.h"


/*
 * Virtual
 */

AnalogPlugin::AnalogPlugin() {
  devs = 1;
}

String AnalogPlugin::getName() {
  return "analog";
}

int8_t AnalogPlugin::getSensors() {
  return devs;
}

int8_t AnalogPlugin::getSensorByAddr(const char* addr_c) {
  if (addr_c == "a0")
    return 0;
  return -1;
}

bool AnalogPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  addr_c = "a0";
  return true;
}

bool AnalogPlugin::getUuid(char* uuid_c, int8_t sensor) {
  return false;
}

bool AnalogPlugin::setUuid(const char* uuid_c, int8_t sensor) {
  return false;
}

float AnalogPlugin::getValue(int8_t sensor) {
  return analogRead(A0) / 1023.0;
}

void AnalogPlugin::getPluginJson(JsonObject* json) {
  JsonArray& sensorlist = json->createNestedArray("sensors");
  JsonObject& data = sensorlist.createNestedObject();
  getSensorJson(&data, 0);
}

void AnalogPlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= devs)
    return;
  (*json)["addr"] = "a0";
  (*json)["uuid"] = "";
  (*json)["value"] = getValue(0);
}

/**
 * Loop
 */
void AnalogPlugin::loop() {
}
