#include "S0Plugin.h"

#ifdef ESP32
#include <SPIFFS.h>
#endif


#define SLEEP_PERIOD 10 * 1000

/*
class InterruptHandler {
private:
  int _pin;

public:
  S0Plugin* _plugin;
  InterruptHandler(S0Plugin* plugin, int pin, int mode) : _plugin(plugin), _pin(pin) {
    attachInterrupt(pin, _s_interruptHandler, mode);
  }

  static void _s_interruptHandler() {
    // reinterpret_cast<InterruptHandler*>(arg)->_plugin->handleInterrupt(1);
  }
};
*/

#define PREFIX "gpio"

/*
 * Static
 */

S0Plugin* S0Plugin::_instance;

/*
 * Virtual
 */

S0Plugin::S0Plugin(int8_t pin) : Plugin(1, 1), _pin(pin) {
  loadConfig();

  _instance = this;
  for (int i=0; i<sizeof(_power)/sizeof(_power[0]); i++)
    _power[i] = NAN;

  pinMode(pin, INPUT);

  switch (pin) {
    case 12:
      attachInterrupt(pin, _s_interrupt12, FALLING);
      break;
    case 14:
      attachInterrupt(pin, _s_interrupt14, FALLING);
      break;
    default:
      DEBUG_MSG("s0", "panic: unsupported pin %d\n", pin);
      PANIC();
  }
}

String S0Plugin::getName() {
  return "s0";
}

int8_t S0Plugin::getSensorByAddr(const char* addr_c) {
  String pin = PREFIX + String(_pin, 10);
  if (strcmp(addr_c, pin.c_str()) == 0)
    return 0;
  return -1;
}

bool S0Plugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  String pin = PREFIX + String(_pin, 10);
  strcpy(addr_c, pin.c_str());
  return true;
}

float S0Plugin::getValue(int8_t sensor) {
  uint16_t val = _power[sensor];
  return val;
}

/**
 * Loop (idle -> uploading)
 */
void S0Plugin::loop() {
  Plugin::loop();

  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD)) {
    _status = PLUGIN_UPLOADING;
  }
  if (_status == PLUGIN_UPLOADING) {
    if (isUploadSafe()) {
      upload();
      _status = PLUGIN_IDLE;
    }
  }
}

void S0Plugin::handleInterrupt(int8_t pin) {
  uint32_t ts = millis();
  _eventCnt[pin]++;
  if (_eventTs[pin] > 0) {
    _power[pin] = 1.0e6 / (ts - _eventTs[pin]);
    String pwr = String(_power[pin], 2);
    DEBUG_MSG("s0", "pwr %sW %d\n", pwr.c_str(), _eventCnt[pin]);
  }
  _eventTs[pin] = ts;
}

void S0Plugin::_s_interrupt12() {
  _instance->handleInterrupt(12);
}

void S0Plugin::_s_interrupt14() {
  _instance->handleInterrupt(14);
}
