#ifndef PLUGIN_H
#define PLUGIN_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "../config.h"


// plugin states
#define PLUGIN_IDLE 0
#define PLUGIN_UPLOADING 1

#define UUID_LENGTH 36


struct DeviceStruct {
  char uuid[UUID_LENGTH+1];
  float val;
};

class Plugin {
public:
  typedef std::function<void(Plugin*)> CallbackFunction;

  Plugin(int8_t maxDevices, int8_t actualDevices);
  virtual ~Plugin();
  static void each(CallbackFunction callback);
  virtual String getName();
  virtual int8_t getSensors();
  virtual int8_t getSensorByAddr(const char* addr_c);
  virtual bool getAddr(char* addr_c, int8_t sensor);
  virtual bool getUuid(char* uuid_c, int8_t sensor);
  virtual bool setUuid(const char* uuid_c, int8_t sensor);
  virtual String getHash(int8_t sensor);
  virtual float getValue(int8_t sensor);
  virtual void getPluginJson(JsonObject* json);
  virtual void getSensorJson(JsonObject* json, int8_t sensor);
  virtual bool loadConfig();
  virtual bool saveConfig();
  virtual void loop();
  virtual uint32_t getMaxSleepDuration();

protected:
  static HTTPClient http; // synchronous use only
  uint32_t _timestamp;
  uint32_t _duration;
  uint8_t _status;
  int8_t _devs;
  uint16_t _size;
  DeviceStruct* _devices;

  virtual void upload();
  virtual bool isUploadSafe();
  virtual bool elapsed(uint32_t duration);

private:
  static int8_t instances;
  static Plugin* plugins[];
};

#endif
