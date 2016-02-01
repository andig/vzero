#include <FS.h>
#include "SimplePlugin.h"


/*
 * Virtual
 */

SimplePlugin::SimplePlugin() : _devices() {
  _devs = 1;
  File configFile = SPIFFS.open("/" + getName() + ".config", "r");
  DEBUG_PLUGIN("[%s] sizes %d %d \n", getName().c_str(), configFile.size(), sizeof(_devices));

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
  DEBUG_PLUGIN("[%s] saving config\n", getName().c_str());
  File configFile = SPIFFS.open("/" + getName() + ".config", "w");
  if (!configFile) {
    DEBUG_PLUGIN("[%s] failed to open config file for writing\n", getName().c_str());
    return false;
  }
  configFile.write((uint8_t*)&_devices, sizeof(_devices));
  configFile.close();
  return true;
}

/**
 * Loop (idle -> uploading)
 */
void SimplePlugin::loop() {
  if (_status == PLUGIN_IDLE && elapsed(60*1000)) {
    _status = PLUGIN_UPLOADING;
    if (WiFi.status() == WL_CONNECTED) {
      upload();
    }
    _status = PLUGIN_IDLE;
  }
}
