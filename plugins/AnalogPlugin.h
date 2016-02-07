#ifndef ANALOG_PLUGIN_H
#define ANALOG_PLUGIN_H

#include "Plugin.h"


class AnalogPlugin : public Plugin {
public:
  AnalogPlugin();
  String getName() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void loop() override;
};

#endif
