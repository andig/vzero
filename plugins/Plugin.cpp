#include <Arduino.h>
#include <MD5Builder.h>
#include "Plugin.h"


#define MAX_PLUGINS 3

int8_t Plugin::instances = 0;
Plugin* Plugin::plugins[MAX_PLUGINS] = {};


/*
 * Static
 */

int8_t Plugin::count() {
  return Plugin::instances;
}

Plugin* Plugin::get(int8_t idx) {
  if (idx < Plugin::instances) {
    return Plugin::plugins[idx];
  }
  return NULL;
}


/*
 * Virtual
 */

Plugin::Plugin() : _devs(0), _status(PLUGIN_IDLE), _timestamp(0), _duration(0) {
  Plugin::plugins[Plugin::instances++] = this;
}

Plugin::~Plugin() {
}

String Plugin::getName() {
  return "generic";
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
  return false;
}

bool Plugin::setUuid(const char* uuid_c, int8_t sensor) {
  return false;
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
  JsonArray& sensorlist = json->createNestedArray(F("sensors"));
  for (int8_t i=0; i<getSensors(); i++) {
    JsonObject& data = sensorlist.createNestedObject();
    getSensorJson(&data, i);
  }
}

void Plugin::getSensorJson(JsonObject* json, int8_t sensor) {
  char buf[UUID_LENGTH];
  if (getAddr(buf, sensor))
    (*json)[F("addr")] = String(buf);
  if (getUuid(buf, sensor))
    (*json)[F("uuid")] = String(buf);
  (*json)[F("hash")] = getHash(sensor);
}

bool Plugin::saveConfig() {
  return false;
}

void Plugin::loop() {
}

void Plugin::upload() {
  if (g_middleware == "")
    return;

  char uuid_c[UUID_LENGTH];
  for (int8_t i=0; i<_devs; i++) {
    // uuid configured?
    getUuid(uuid_c, i);
    if (strlen(uuid_c) > 0) {
      float val = getValue(i);

      if (isnan(val))
        break;

      char val_c[16];
      dtostrf(val, -4, 2, val_c);

      String uri = g_middleware + F("/data/") + String(uuid_c) + F(".json?value=") + String(val_c);
      http.begin(uri);
      int httpCode = http.POST("");
      if (httpCode > 0) {
        http.getString();
        DEBUG_HEAP();
      }
      DEBUG_PLUGIN("[%s] upload %d %s\n", getName().c_str(), httpCode, uri.c_str());
      http.end();
    }
  }
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
