#include <Arduino.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>

#include "../config.h"
#include "Plugin.h"


#define MAX_PLUGINS 3

byte Plugin::instances = 0;
Plugin* Plugin::plugins[MAX_PLUGINS] = {};


/*
 * Static
 */

byte Plugin::count() {
  return Plugin::instances;
}

Plugin* Plugin::get(byte idx) {
  if (idx < Plugin::instances) {
    return Plugin::plugins[idx];  
  }
  return NULL;
}


/*
 * Virtual
 */

Plugin::Plugin() : _status(PLUGIN_IDLE), _timestamp(0) {
  Plugin::plugins[Plugin::instances++] = this;
}

Plugin::~Plugin() {
}

String Plugin::getName() {
  return "generic";
}

int8_t Plugin::getSensors() {
  return 0;
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
}

void Plugin::getSensorJson(JsonObject* json, int8_t sensor) {
}

void Plugin::loop() {
}

boolean Plugin::elapsed(long duration) {
  if (millis() - _timestamp > duration) {
    _timestamp = millis();
    return true;
  }
  return false;
}

