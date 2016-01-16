#ifndef ANALOG_PLUGIN_H
#define ANALOG_PLUGIN_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Plugin.h"


class AnalogPlugin : public Plugin {
public:
  AnalogPlugin();
  String getName() override;
  int8_t getSensors() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  bool getUuid(char* uuid_c, int8_t sensor) override;
  bool setUuid(const char* uuid_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void getPluginJson(JsonObject* json) override;
  void getSensorJson(JsonObject* json, int8_t sensor) override;
  void loop() override;

private:
   int8_t devs;
};

#endif
