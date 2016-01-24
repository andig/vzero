#ifndef PLUGIN_H
#define PLUGIN_H

#include <MD5Builder.h>
#include <ArduinoJson.h>

// plugin states
#define PLUGIN_IDLE 0

class Plugin {
public:
  static byte count();
  static Plugin* get(byte idx);

  Plugin();
  virtual ~Plugin();
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
  virtual void loop();

protected:
  long _timestamp;
  byte _status;

  boolean elapsed(long duration);

private:
  static byte instances;
  static Plugin* plugins[];
};

#endif
