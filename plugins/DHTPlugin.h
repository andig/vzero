#ifndef DHT_PLUGIN_H
#define DHT_PLUGIN_H

#include <DHT.h>
#include "SimplePlugin.h"


#ifdef DEBUG
#define DEBUG_DHT(...) ets_printf( __VA_ARGS__ )
#else
#define DEBUG_DHT(...)
#endif


class DHTPlugin : public SimplePlugin {
public:
  DHTPlugin(uint8_t pin, uint8_t type);
  String getName() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void loop() override;

private:
  DHT _dht;
  DeviceStructSimple _devices[2];
};

#endif
