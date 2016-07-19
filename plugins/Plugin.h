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
#define JSON_NULL static_cast<const char*>(NULL)

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

  /**
   * Get plugin name
   */
  virtual String getName();

  /**
   * Get number of sensors for plugin
   */
  virtual int8_t getSensors();

  /**
   * Get sensor index by sensor name (e.g. /analog/<a0>)
   * Reversed by getAddr
   */
  virtual int8_t getSensorByAddr(const char* addr_c);

  /**
   * Get senor name by sensor index
   * Reversed by getSensorByAddr
   */
  virtual bool getAddr(char* addr_c, int8_t sensor);

  /**
   * Get middleware entity UUID for sensor
   */
  virtual bool getUuid(char* uuid_c, int8_t sensor);

  /**
   * Set middleware entity UUID for sensor
   */
  virtual bool setUuid(const char* uuid_c, int8_t sensor);

  /**
   * Get sensor hash value
   * Used to uniquely identify a sensor entity at the middleware even if
   * UUID has been erased from config
   */
  virtual String getHash(int8_t sensor);

  /**
   * Get sensor value. Returns NAN is sensor not connected.
   */
  virtual float getValue(int8_t sensor);

  /**
   * Get plugin json inluding all sensors
   */
  virtual void getPluginJson(JsonObject* json);

  /**
   * Get senor json
   */
  virtual void getSensorJson(JsonObject* json, int8_t sensor);

  /**
   * Load plugin configuration
   */
  virtual bool loadConfig();

  /**
   * Save plugin configuration
   */
  virtual bool saveConfig();

  /**
   * Plugin loop function called from main loop()
   */
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
