#include <Arduino.h>
#include <ArduinoJson.h>

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

void Plugin::loop() {
}

void Plugin::getData(JsonObject* json) {
}

boolean Plugin::elapsed(long duration) {
  if (millis() - _timestamp > duration) {
    _timestamp = millis();
    return true;
  }
  return false;
}

