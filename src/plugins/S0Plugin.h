#ifndef S0_PLUGIN_H
#define S0_PLUGIN_H

#include "Plugin.h"


class S0Plugin : public Plugin {
public:
  S0Plugin(int8_t pin);
  String getName() override;
  int8_t getSensorByAddr(const char* addr_c) override;
  bool getAddr(char* addr_c, int8_t sensor) override;
  float getValue(int8_t sensor) override;
  void loop() override;
  void handleInterrupt(int8_t pin);
  static void _s_interrupt12();
  static void _s_interrupt14();
private:
  static S0Plugin* _instance;
  int8_t _pin = -1;
  uint32_t _eventTs[16] = {0};
  uint16_t _eventCnt[16] = {0};
  float _power[16] = {NAN};
};

#endif
