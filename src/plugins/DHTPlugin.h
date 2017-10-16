#ifndef DHT_PLUGIN_H
#define DHT_PLUGIN_H

#include <DHT.h>
#include "Plugin.h"


class DHTPlugin : public Plugin {
public:
  DHTPlugin(uint8_t pin, uint8_t type);
  String getName() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void loop() override;

protected:
  DHT _dht;
};

#endif
