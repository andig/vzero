#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "../config.h"
#include "OneWirePlugin.h"


#define TEMPERATURE_PRECISION 9

// plugin states
#define PLUGIN_REQUESTING 1
#define PLUGIN_READING 2

#define SLEEP_PERIOD 10 * 1000
#define REQUEST_WAIT_DURATION 1 * 1000

#define OPTIMISTIC_YIELD_TIME 16000

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

OneWirePlugin::OneWirePlugin(byte pin) : devices(), devs(0), ow(pin), sensors(&ow) {
  // allow reuse (if server supports it)
  http.setReuse(true);

  File configFile = SPIFFS.open(F("/1wire.config"), "r");
  if (configFile.size() == sizeof(devices)) {
    DEBUG_ONEWIRE("[1wire] reading config file\n");
    configFile.read(&devices[0].addr[0], sizeof(devices));
  
    // find first empty device slot
    DeviceAddress addr = {};
    char addr_c[20];
    addrToStr(addr_c, addr);
    devs = getSensorByAddr(addr_c) + 1;
  }
  configFile.close();

  int8_t devsConfigured = devs;

  // locate devices on the bus
  DEBUG_ONEWIRE("[1wire] looking for 1-Wire devices...\n");
  sensors.begin();
  sensors.setWaitForConversion(false);
  setupSensors();

  // found new devices?
  if (devsConfigured != devs) {
    saveConfig();
  }

  // report parasite power
  DEBUG_ONEWIRE("[1wire] parasite power: %s\n", (sensors.isParasitePowerMode()) ? "on" : "off");
}

String OneWirePlugin::getName() {
  return "1wire";
}

int8_t OneWirePlugin::getSensors() {
  return devs;
}

int8_t OneWirePlugin::getSensorByAddr(const char* addr_c) {
  DeviceAddress addr;
  strToAddr(addr_c, addr);

  for (int8_t i=0; i<devs; i++) {
    if (addrCompare(addr, devices[i].addr)) {
      return i;
    }
  }
  return -1;
}

bool OneWirePlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  addrToStr(&addr_c[0], devices[sensor].addr);
  return true;
}

bool OneWirePlugin::getUuid(char* uuid_c, int8_t sensor) {
  if (sensor >= devs)
    return false;
  strcpy(uuid_c, devices[sensor].uuid);
  return true;
}

bool OneWirePlugin::setUuid(const char* uuid_c, int8_t sensor) {
  if (sensor >= devs || strlen(uuid_c) != 36)
    return false;
  if (strlen(devices[sensor].uuid) > 0 && strlen(uuid_c) > 0) // erase before update
    return false;
  strcpy(devices[sensor].uuid, uuid_c);
  saveConfig();
  return true;
}

float OneWirePlugin::getValue(int8_t sensor) {
  if (sensor >= devs)
    return NAN;
  return devices[sensor].val;
}

void OneWirePlugin::getPluginJson(JsonObject* json) {
  JsonObject& config = json->createNestedObject("settings");
  config["interval"] = 30;

  JsonArray& sensorlist = json->createNestedArray("sensors");
  char addr_c[20];
  for (int8_t i=0; i<devs; i++) {
    addrToStr(addr_c, devices[i].addr);
    optimistic_yield(OPTIMISTIC_YIELD_TIME);

    JsonObject& data = sensorlist.createNestedObject();
    getSensorJson(&data, i);
  }
}

void OneWirePlugin::getSensorJson(JsonObject* json, int8_t sensor) {
  if (sensor >= devs)
    return;
    
  char addr_c[20];
  addrToStr(addr_c, devices[sensor].addr);
  (*json)["addr"] = String(addr_c);
  (*json)["uuid"] = String(devices[sensor].uuid);
  (*json)["value"] = devices[sensor].val;
}

/**
 * Loop (idle -> requesting -> reading)
 */
void OneWirePlugin::loop() {
  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD)) {
    _status = PLUGIN_REQUESTING;

    sensors.requestTemperatures();
  }
  else if (_status == PLUGIN_REQUESTING && elapsed(REQUEST_WAIT_DURATION)) {
    _status = PLUGIN_READING;

    readTemperatures();

    if (WiFi.status() == WL_CONNECTED) {
      upload();
    }
    _status = PLUGIN_IDLE;
  }
}

void OneWirePlugin::setupSensors() {
  DEBUG_ONEWIRE("[1wire] found %d devices\n", sensors.getDeviceCount());

  DeviceAddress addr;
  for (int8_t i=0; i<sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(addr, i)) {
      char addr_c[20];
      addrToStr(&addr_c[0], addr);
      DEBUG_ONEWIRE("[1wire] device: %s ", addr_c);

      int8_t sensorIndex = getSensorIndex(addr);
      if (sensorIndex >= 0) {
        DEBUG_ONEWIRE("(known)\n");
      }
      else {
        DEBUG_ONEWIRE("(new)\n");
        sensorIndex = addSensor(addr);
      }

      // set precision
      sensors.setResolution(addr, TEMPERATURE_PRECISION);
    }
  }

  for (int8_t i=0; i<devs; i++) {
    devices[i].val = NAN;
  }
}

/*
 * Private
 */
bool OneWirePlugin::saveConfig() {
  DEBUG_ONEWIRE("[1wire] saving config\n");
  File configFile = SPIFFS.open(F("/1wire.config"), "w");
  if (!configFile) {
    DEBUG_ONEWIRE("[1wire] failed to open config file for writing\n");
    return false;
  }
  
  configFile.write(&devices[0].addr[0], sizeof(devices));
  configFile.close();
  return true;
}

int8_t OneWirePlugin::getSensorIndex(const uint8_t* addr) {
  for (int8_t i=0; i<devs; ++i) {
    if (addrCompare(addr, devices[i].addr)) {
      return i;
    }
  }
  return(-1);
}

int8_t OneWirePlugin::addSensor(const uint8_t* addr) {
  if (devs >= MAX_SENSORS) {
    DEBUG_ONEWIRE("[1wire] too many devices\n");
    return -1;
  }
  for (uint8_t i=0; i<8; i++) {
    devices[devs].addr[i] = addr[i];
  }
  return(devs++);
}

void OneWirePlugin::readTemperatures() {
  for (int8_t i=0; i<devs; i++) {
    devices[i].val = sensors.getTempC(devices[i].addr);
    optimistic_yield(OPTIMISTIC_YIELD_TIME);

    if (devices[i].val == DEVICE_DISCONNECTED_C) {
      DEBUG_ONEWIRE("[1wire] device %s disconnected\n", devices[i].addr);
      continue;
    }
  }
}

void OneWirePlugin::upload() {
  for (int8_t i=0; i<devs; i++) {
    char uuid_c[40];
    getUuid(uuid_c, i);

    // uuid configured?
    if (strlen(uuid_c) > 0) {
      float val = getValue(i);

      char val_c[16];
      if (isnan(val))
        strcpy(val_c, "null");
      else
        dtostrf(val, -4, 2, val_c);

      String uri = g_middleware + "/data/" + String(uuid_c) + ".json?value=" + String(val_c);
      http.begin(uri);
      int httpCode = http.POST("");
      if (httpCode > 0) {
        http.getString();
        DEBUG_HEAP;
      }
      DEBUG_ONEWIRE("[1wire] upload %d %s\n", httpCode, uri.c_str());
      http.end();
    }
  }
}