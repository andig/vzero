#ifndef PLUGIN_H
#define PLUGIN_H

#include <ArduinoJson.h>

// plugin states
#define PLUGIN_IDLE 0

class Plugin {
public:
  static byte count();
  static Plugin* get(byte idx);
  
  Plugin();
  ~Plugin();
  virtual void loop();
  virtual void getData(JsonObject* json);

protected:
  long _timestamp;
  byte _status;

  boolean elapsed(long duration);

private:
  static byte instances;
  static Plugin* plugins[];
};

#endif
