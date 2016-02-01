#include "DHTPlugin.h"


#define SLEEP_PERIOD 60 * 1000
#define REQUEST_WAIT_DURATION 1 * 1000


/*
 * Virtual
 */

DHTPlugin::DHTPlugin(uint8_t pin, uint8_t type) : _devices(), _dht(pin, type) {
}

String DHTPlugin::getName() {
  return "dht";
}

int8_t DHTPlugin::getSensorByAddr(const char* addr_c) {
  if (strcmp(addr_c, "temp") == 0)
    return 0;
  else if (strcmp(addr_c, "humidity") == 0)
    return 1;
  return -1;
}

bool DHTPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  if (sensor == 0)
    strcpy(addr_c, "temp");
  else if (sensor == 1)
    strcpy(addr_c, "humidity");
  return true;
}

float DHTPlugin::getValue(int8_t sensor) {
  if (sensor >= _devs)
    return false;
  float f = NAN;
  if (sensor == 0)
    f = _dht.readTemperature();
  else if (sensor == 1)
    f = _dht.readHumidity();
  if (isnan(f))
    return false;
  return true;
}

/**
 * Loop (idle -> uploading)
 */
void DHTPlugin::loop() {
  // DEBUG_ONEWIRE("[1wire] status: %d\n", _status);
  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD - REQUEST_WAIT_DURATION)) {
    // DEBUG_ONEWIRE("[1wire] idle -> uploading\n");
    _status = PLUGIN_UPLOADING;

    // force reading- valid for 2 seconds
    _dht.readTemperature(false, true);

    if (WiFi.status() == WL_CONNECTED) {
      upload();
    }
    _status = PLUGIN_IDLE;
  }
}