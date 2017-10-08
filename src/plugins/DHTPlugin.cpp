#include <FS.h>
#include "DHTPlugin.h"


#define SLEEP_PERIOD 10 * 1000
#define REQUEST_WAIT_DURATION 1 * 1000


/*
 * Virtual
 */

DHTPlugin::DHTPlugin(uint8_t pin, uint8_t type) : _dht(pin, type), Plugin(2, 2) {
  DEBUG_MSG("dht", "plugin started\n");
  loadConfig();
  _dht.begin();
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
    return NAN;
  return _devices[sensor].val;
}

/**
 * Loop (idle -> uploading)
 */
void DHTPlugin::loop() {
  Plugin::loop();

  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD - REQUEST_WAIT_DURATION)) {
    _status = PLUGIN_UPLOADING;
  }
  if (_status == PLUGIN_UPLOADING) {
    // force reading- valid for 2 seconds
    if (_dht.read(true)) {
      _devices[0].val = _dht.readTemperature();
      _devices[1].val = _dht.readHumidity();

      if (isUploadSafe()) {
        upload();
        _status = PLUGIN_IDLE;
      }
    }
    else {
      _devices[0].val = NAN;
      _devices[1].val = NAN;

      // retry failed read only after SLEEP_PERIOD
      _status = PLUGIN_IDLE;
      DEBUG_MSG("dht", "failed reading sensors\n");
    }
  }
}
