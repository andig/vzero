#ifndef WIFI_PLUGIN_H
#define WIFI_PLUGIN_H

#include "Plugin.h"


class WifiPlugin : public Plugin {
public:
  WifiPlugin();
  String getName() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void loop() override;
};

#endif
