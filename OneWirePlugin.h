#ifndef ONEWIRE_PLUGIN_H
#define ONEWIRE_PLUGIN_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Plugin.h"


#define MAX_SENSORS 10
#define UUID_LENGTH 40

struct DeviceStruct {
  DeviceAddress addr;
  char uuid[UUID_LENGTH];
};


class OneWirePlugin : public Plugin {
public:
  static bool addrCompare(const uint8_t* a, const uint8_t* b);
  static void addrToStr(char* ptr, const uint8_t* addr);
  static void strToAddr(const char* ptr, uint8_t* addr);

  OneWirePlugin(byte pin);
  void loop();
  void getData(JsonObject* json);

private:
  OneWire ow;
  DallasTemperature sensors;
  
  DeviceStruct devices[MAX_SENSORS];
  int8_t devs;

  int8_t getSensorIndex(const uint8_t* addr);
  void setupSensors();
  void readTemperatures();
};

#endif
