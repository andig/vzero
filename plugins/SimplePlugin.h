#ifndef SIMPLE_PLUGIN_H
#define SIMPLE_PLUGIN_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Plugin.h"


struct DeviceStructSimple {
  char uuid[UUID_LENGTH];
};

class SimplePlugin : public Plugin {
public:
  SimplePlugin();
  bool getUuid(char* uuid_c, int8_t sensor) override;
  bool setUuid(const char* uuid_c, int8_t sensor) override;
  void getSensorJson(JsonObject* json, int8_t sensor) override;
  bool saveConfig() override;

protected:
  DeviceStructSimple _devices;
};

#endif
