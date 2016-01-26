#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "SimplePlugin.h"


/*
 * Virtual
 */

SimplePlugin::SimplePlugin() : _devices() {
  _devs = 1;
  File configFile = SPIFFS.open("/" + getName() + ".config", "r");
  if (configFile.size() == sizeof(_devices))
    configFile.read((uint8_t*)&_devices, sizeof(_devices));
  configFile.close();
}

bool SimplePlugin::getUuid(char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  strcpy(uuid_c, _devices.uuid);
  return true;
}

bool SimplePlugin::setUuid(const char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  if (strlen(_devices.uuid) + strlen(uuid_c) != 36) // erase before update
    return false;
  strcpy(_devices.uuid, uuid_c);
  return saveConfig();
}

void SimplePlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= _devs)
    return;
  Plugin::getSensorJson(json, sensor);
  (*json)[F("value")] = getValue(0);
}

bool SimplePlugin::saveConfig() {
  File configFile = SPIFFS.open("/" + getName() + ".config", "w");
  if (!configFile)
    return false;
  configFile.write((uint8_t*)&_devices, sizeof(_devices));
  configFile.close();
  return true;
}
