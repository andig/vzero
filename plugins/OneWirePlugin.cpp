#include <FS.h>
#include "OneWirePlugin.h"


#define TEMPERATURE_PRECISION 9

// plugin states
#define PLUGIN_REQUESTING PLUGIN_UPLOADING + 1

#define SLEEP_PERIOD 60 * 1000
#define REQUEST_WAIT_DURATION 1 * 1000


/*
 * Static
 */

unsigned long hex2int(const char *a, unsigned int len)
{
  unsigned long val = 0;
  for (int i=0; i<len; i++) {
    if (a[i] <= 57)
      val += (a[i]-48)*(1<<(4*(len-1-i)));
    else
      val += (a[i]-55)*(1<<(4*(len-1-i)));
  }
  return val;
}

bool OneWirePlugin::addrCompare(const uint8_t* a, const uint8_t* b) {
  for (uint8_t i=0; i<7; i++) {
    if (a[i] != b[i]) {
      return(false);
    }
  }
  return(true);
}

void OneWirePlugin::addrToStr(char* ptr, const uint8_t* addr) {
  for (uint8_t b=0; b<7; b++) {
    sprintf(ptr, "%02X", addr[b]);
    ptr += 2;
    if (b == 0) {
      *ptr++ = '-'; // hyphen after first hex number
    }
  }
  *ptr = '\0';
}

void OneWirePlugin::strToAddr(const char* ptr, uint8_t* addr) {
  for (uint8_t b=0; b<7; b++) {
    addr[b] = (uint8_t)hex2int(ptr, 2);
    ptr += 2;
    if (b == 0 && *ptr == '-') {
      ptr++; // hyphen after first hex number
    }
  }
}


/*
 * Virtual
 */

OneWirePlugin::OneWirePlugin(byte pin) : _devices(), ow(pin), sensors(&ow), Plugin(0, 0) {
  loadConfig();

  // locate _devices on the bus
  DEBUG_MSG("1wire", "looking for 1-Wire devices...\n");
  sensors.begin();
  sensors.setWaitForConversion(false);
  setupSensors();

  // report parasite power
  DEBUG_MSG("1wire", "parasite power: %s\n", (sensors.isParasitePowerMode()) ? "on" : "off");
}

String OneWirePlugin::getName() {
  return "1wire";
}

int8_t OneWirePlugin::getSensorByAddr(const char* addr_c) {
  DeviceAddress addr;
  strToAddr(addr_c, addr);

  for (int8_t i=0; i<_devs; i++) {
    if (addrCompare(addr, _devices[i].addr)) {
      return i;
    }
  }
  return -1;
}

bool OneWirePlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  addrToStr((char*)addr_c, _devices[sensor].addr);
  return true;
}

bool OneWirePlugin::getUuid(char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  strcpy(uuid_c, _devices[sensor].uuid);
  return true;
}

bool OneWirePlugin::setUuid(const char* uuid_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  if (strlen(_devices[sensor].uuid) + strlen(uuid_c) != 36) // erase before update
    return false;
  strcpy(_devices[sensor].uuid, uuid_c);
  return saveConfig();
}

float OneWirePlugin::getValue(int8_t sensor) {
  if (sensor >= _devs)
    return NAN;
  return _devices[sensor].val;
}

void OneWirePlugin::getPluginJson(JsonObject* json) {
  JsonObject& config = json->createNestedObject("settings");
  config[F("interval")] = 30;
  Plugin::getPluginJson(json);
}

bool OneWirePlugin::loadConfig() {
  File configFile = SPIFFS.open(F("/1wire.config"), "r");
  if (configFile.size() == sizeof(_devices)) {
    DEBUG_MSG("1wire", "reading config file\n");
    configFile.read((uint8_t*)_devices, sizeof(_devices));

    // find first empty device slot
    DeviceAddress addr = {};
    char addr_c[20];
    addrToStr(addr_c, addr);
    _devs = getSensorByAddr(addr_c) + 1;
  }
  configFile.close();
  return true;
}

bool OneWirePlugin::saveConfig() {
  DEBUG_MSG("1wire", "saving config\n");
  File configFile = SPIFFS.open(F("/1wire.config"), "w");
  if (!configFile) {
    DEBUG_MSG("1wire", "failed to open config file for writing\n");
    return false;
  }

  configFile.write((uint8_t*)_devices, sizeof(_devices));
  configFile.close();
  return true;
}

/**
 * Loop (idle -> requesting -> reading)
 */
void OneWirePlugin::loop() {
  Plugin::loop();

  // exit if no sensors found
  if (!_devs)
    return;

  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD - REQUEST_WAIT_DURATION)) {
    DEBUG_MSG("1wire", "requesting temp\n");
    _status = PLUGIN_REQUESTING;
    sensors.requestTemperatures();
  }
  else if (_status == PLUGIN_REQUESTING && elapsed(REQUEST_WAIT_DURATION)) {
    DEBUG_MSG("1wire", "reading temp\n");
    _status = PLUGIN_UPLOADING;
    readTemperatures();
  }
  if (_status == PLUGIN_UPLOADING) {
    if (isUploadSafe()) {
      upload();
      _status = PLUGIN_IDLE;
    }
  }
}

void OneWirePlugin::setupSensors() {
  DEBUG_MSG("1wire", "found %d devices\n", sensors.getDeviceCount());

  DeviceAddress addr;
  for (int8_t i=0; i<sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(addr, i)) {
      char addr_c[20];
      addrToStr((char*)addr_c, addr);
      DEBUG_MSG("1wire", "device: %s ", addr_c);

      int8_t sensorIndex = getSensorIndex(addr);
      if (sensorIndex >= 0) {
        DEBUG_MSG("1wire", "(known)\n");
      }
      else {
        sensorIndex = addSensor(addr);
        DEBUG_MSG("1wire", "(new at %d)\n", sensorIndex);
      }

      // set precision
      sensors.setResolution(addr, TEMPERATURE_PRECISION);
    }
  }

  for (int8_t i=0; i<_devs; i++) {
    _devices[i].val = NAN;
  }
}

/*
 * Private
 */

int8_t OneWirePlugin::getSensorIndex(const uint8_t* addr) {
  for (int8_t i=0; i<_devs; ++i) {
    if (addrCompare(addr, _devices[i].addr)) {
      return i;
    }
  }
  return(-1);
}

int8_t OneWirePlugin::addSensor(const uint8_t* addr) {
  if (_devs >= MAX_SENSORS) {
    DEBUG_MSG("1wire", "too many devices\n");
    return -1;
  }
  for (uint8_t i=0; i<8; i++) {
    _devices[_devs].addr[i] = addr[i];
  }
  return(_devs++);
}

void OneWirePlugin::readTemperatures() {
  for (int8_t i=0; i<_devs; i++) {
    _devices[i].val = sensors.getTempC(_devices[i].addr);
    optimistic_yield(OPTIMISTIC_YIELD_TIME);

    if (_devices[i].val == DEVICE_DISCONNECTED_C) {
      DEBUG_MSG("1wire", "device %s disconnected\n", _devices[i].addr);
      continue;
    }
  }
}
