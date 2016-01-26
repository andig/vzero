#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "AnalogPlugin.h"


/*
 * Virtual
 */

AnalogPlugin::AnalogPlugin() : devices(), devs(1) {
  File configFile = SPIFFS.open(F("/analog.config"), "r");
  if (configFile.size() == sizeof(devices)) {
    configFile.read((uint8_t*)&devices, sizeof(devices));
  }
  configFile.close();
}

String AnalogPlugin::getName() {
  return "analog";
}

int8_t AnalogPlugin::getSensors() {
  return devs;
}

int8_t AnalogPlugin::getSensorByAddr(const char* addr_c) {
  if (strcmp(addr_c, "a0") == 0)
    return 0;
  return -1;
}

bool AnalogPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  strcpy(addr_c, "a0");
  return true;
}

bool AnalogPlugin::getUuid(char* uuid_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  strcpy(uuid_c, devices.uuid);
  return true;
}

bool AnalogPlugin::setUuid(const char* uuid_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  if (strlen(devices.uuid) + strlen(uuid_c) != 36) // erase before update
    return false;
  strcpy(devices.uuid, uuid_c);
  return saveConfig();
}

float AnalogPlugin::getValue(int8_t sensor) {
  return analogRead(A0) / 1023.0;
}

void AnalogPlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= devs)
    return;
  Plugin::getSensorJson(json, sensor);
  (*json)[F("value")] = getValue(0);
}

/**
 * Loop
 */
void AnalogPlugin::loop() {
}
