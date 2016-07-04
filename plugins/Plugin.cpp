#include <Arduino.h>
#include <MD5Builder.h>
#include <FS.h>
#include "Plugin.h"


#define MAX_PLUGINS 5


/*
 * Static
 */

int8_t Plugin::instances = 0;
Plugin* Plugin::plugins[MAX_PLUGINS] = {};
HTTPClient Plugin::http;

void Plugin::each(CallbackFunction callback) {
  for (int8_t i=0; i<Plugin::instances; i++) {
    callback(Plugin::plugins[i]);
  }
}

/*
 * Virtual
 */

Plugin::Plugin(int8_t maxDevices = 0, int8_t actualDevices = 0) : _devs(actualDevices),
  _status(PLUGIN_IDLE), _timestamp(0), _duration(0)
{
  Plugin::plugins[Plugin::instances++] = this;
  Plugin::http.setReuse(true); // allow reuse (if server supports it)

  // buffer size
  _size = maxDevices * sizeof(DeviceStruct);

  if (maxDevices > 0) {
    // DEBUG_PLUGIN("[abstract]: alloc %d -> %d\n", maxDevices, _size);
    _devices = (DeviceStruct*)malloc(_size);
    if (_devices == NULL)
      panic();
  }
}

Plugin::~Plugin() {
}

String Plugin::getName() {
  return "abstract";
}

int8_t Plugin::getSensors() {
  return _devs;
}

int8_t Plugin::getSensorByAddr(const char* addr_c) {
  return -1;
}

bool Plugin::getAddr(char* addr_c, int8_t sensor) {
  return false;
}

bool Plugin::getUuid(char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  strcpy(uuid_c, _devices[sensor].uuid);
  return true;
}

bool Plugin::setUuid(const char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  if (strlen(_devices[sensor].uuid) + strlen(uuid_c) != UUID_LENGTH) // erase before update
    return false;
  strcpy(_devices[sensor].uuid, uuid_c);
  return saveConfig();
}

String Plugin::getHash(int8_t sensor) {
  char addr_c[32];
  if (getAddr(&addr_c[0], sensor)) {
    MD5Builder md5 = ::getHashBuilder();
    md5.add(getName());
    md5.add(addr_c);
    md5.calculate();
    return md5.toString();
  }
  return "";
}

float Plugin::getValue(int8_t sensor) {
  return NAN;
}

void Plugin::getPluginJson(JsonObject* json) {
  JsonArray& sensorlist = json->createNestedArray("sensors");
  for (int8_t i=0; i<getSensors(); i++) {
    JsonObject& data = sensorlist.createNestedObject();
    getSensorJson(&data, i);
  }
}

void Plugin::getSensorJson(JsonObject* json, int8_t sensor) {
  char buf[UUID_LENGTH+1];
  if (getAddr(buf, sensor))
    (*json)[F("addr")] = String(buf);
  if (getUuid(buf, sensor))
    (*json)[F("uuid")] = String(buf);

  float val = getValue(sensor);
  if (isnan(val))
    (*json)[F("value")] = NULL;
  else
    (*json)[F("value")] = val;

  (*json)[F("hash")] = getHash(sensor);
}

bool Plugin::loadConfig() {
  DEBUG_MSG(getName().c_str(), "load config %d\n", _size);
  File configFile = SPIFFS.open("/" + getName() + ".config", "r");
  if (configFile.size() == _size)
    configFile.read((uint8_t*)_devices, _size);
  for (int8_t sensor = 0; sensor<getSensors(); sensor++)
    if (strlen(_devices[sensor].uuid) != UUID_LENGTH)
      _devices[sensor].uuid[0] = '\0';
  configFile.close();
  return true;
}

bool Plugin::saveConfig() {
  DEBUG_MSG(getName().c_str(), "save config %d\n", _size);
  File configFile = SPIFFS.open("/" + getName() + ".config", "w");
  if (!configFile) {
    DEBUG_MSG(getName().c_str(), "failed to open config file for writing\n");
    return false;
  }
  configFile.write((uint8_t*)_devices, _size);
  configFile.close();
  return true;
}

void Plugin::loop() {
  // DEBUG_MSG(getName().c_str(), "loop %d\n", _status);
}

void Plugin::upload() {
  if (g_middleware == "")
    return;

  char uuid_c[UUID_LENGTH+1];
  char val_c[16];

  for (int8_t i=0; i<getSensors(); i++) {
    // uuid configured?
    getUuid(uuid_c, i);
    if (strlen(uuid_c) > 0) {
      float val = getValue(i);

      if (isnan(val))
        break;

      dtostrf(val, -4, 2, val_c);

      String uri = g_middleware + F("/data/") + String(uuid_c) + F(".json?value=") + String(val_c);
      http.begin(uri);
      int httpCode = http.POST("");
      if (httpCode > 0) {
        http.getString();
      }
      DEBUG_MSG(getName().c_str(), "POST %d %s\n", httpCode, uri.c_str());
      http.end();
    }
  }
}

bool Plugin::isUploadSafe() {
  // no upload in AP mode, no logging
  if ((WiFi.getMode() & WIFI_STA) == 0)
    return false;
  bool isSafe = WiFi.status() == WL_CONNECTED && ESP.getFreeHeap() >= HTTP_MIN_HEAP;
  if (!isSafe) {
    DEBUG_MSG(getName().c_str(), "cannot upload (wifi: %d mem:%d)\n", WiFi.status(), ESP.getFreeHeap());
  }
  return isSafe;
}

bool Plugin::elapsed(uint32_t duration) {
  if (_timestamp == 0 || millis() - _timestamp >= duration) {
    _timestamp = millis();
    return true;
  }
  _duration = duration;
  return false;
}

uint32_t Plugin::getMaxSleepDuration() {
  if (_timestamp == 0)
    return -1;
  uint32_t elapsed = millis() - _timestamp;
  if (elapsed < _duration)
    return _duration - elapsed;
  return 0;
}
