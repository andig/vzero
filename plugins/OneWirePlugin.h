#ifndef ONEWIRE_PLUGIN_H
#define ONEWIRE_PLUGIN_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Plugin.h"

#define DEBUG_ONEWIRE(...) Serial.printf( __VA_ARGS__ )


#define MAX_SENSORS 10
#define UUID_LENGTH 40

struct DeviceStruct {
  DeviceAddress addr;
  char uuid[UUID_LENGTH];
  float val;
};


class OneWirePlugin : public Plugin {
public:
  static bool addrCompare(const uint8_t* a, const uint8_t* b);
  static void addrToStr(char* ptr, const uint8_t* addr);
  static void strToAddr(const char* ptr, uint8_t* addr);

  OneWirePlugin(byte pin);
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
  OneWire ow;
  DallasTemperature sensors;
  
  DeviceStruct devices[MAX_SENSORS];
  int8_t devs;

  bool saveConfig();
  int8_t getSensorIndex(const uint8_t* addr);
  int8_t addSensor(const uint8_t* addr);
  void setupSensors();
  void readTemperatures();
};

#endif
