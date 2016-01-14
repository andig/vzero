#include <Arduino.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "OneWirePlugin.h"


#define TEMPERATURE_PRECISION 9

// plugin states
#define PLUGIN_REQUESTING 1
#define PLUGIN_READING 2

#define SLEEP_PERIOD 10 * 1000
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
  for (int8_t i = 0; i < sizeof(DeviceAddress); ++i) {
    if (a[i] != b[i]) {
      return(false);
    }
  }
  return(true);
}

void OneWirePlugin::addrToStr(char* ptr, const uint8_t* addr) {
  for (uint8_t b=0; b<8; b++) {
    sprintf(ptr, "%02X", addr[b]);
    ptr += 2;
    if (b == 0) {
      *ptr++ = '-'; // hyphen after first hex number
    }
  }
  *ptr = '\0';
}

void OneWirePlugin::strToAddr(const char* ptr, uint8_t* addr) {
  uint8_t b = 0;
  while (b < sizeof(DeviceAddress)) {
    addr[b++] = (uint8_t)hex2int(ptr, 2);
    ptr += 2;
    if (b == 1 && *ptr == '-') {
      ptr++; // hyphen after first hex number
    }
  }
}


/*
 * Virtual
 */

OneWirePlugin::OneWirePlugin(byte pin) : devices(), devs(0), ow(pin), sensors(&ow) {
  Serial.println(F("Looking for 1-Wire devices..."));
  // locate devices on the bus
  sensors.begin();
  setupSensors();
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
    yield();

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

void OneWirePlugin::loop() {
  // plugin states: idle -> requesting -> reading
  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD)) {
    Serial.println(F("1wire: request temp"));
    _status = PLUGIN_REQUESTING;

    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
  }
  else if (_status == PLUGIN_REQUESTING && elapsed(REQUEST_WAIT_DURATION)) {
    Serial.println(F("1wire: read temp"));
    _status = PLUGIN_READING;

    readTemperatures();
    _status = PLUGIN_IDLE;
  }
}

void OneWirePlugin::setupSensors() {
  Serial.println(F("OneWirePlugin::setupSensors"));

  devs = sensors.getDeviceCount();
  Serial.print(F("Found "));
  Serial.print(devs);
  Serial.println(F(" devices."));

  // report parasite power requirements
  Serial.print(F("Parasite power is: "));
  Serial.println((sensors.isParasitePowerMode()) ? F("on") : F("off"));

  for (int i=0; i<sensors.getDeviceCount(); i++) {
    if (sensors.getAddress(devices[i].addr, i)) {
      char addr_c[20];
      addrToStr(&addr_c[0], devices[i].addr);
      Serial.print(F("Found device: "));
      Serial.println(addr_c);

      // set precision
      sensors.setResolution(devices[i].addr, TEMPERATURE_PRECISION);
    }
  }
}

int8_t OneWirePlugin::getSensorIndex(const uint8_t* addr) {
  for (int8_t i=0; i<devs; ++i) {
    if (addrCompare(addr, devices[i].addr)) {
      return i;
    }
  }
  return(-1);
}

void OneWirePlugin::readTemperatures() {
  for (int i=0; i<devs; i++) {
    devices[i].val = sensors.getTempC(devices[i].addr);
    yield();

    if (devices[i].val == DEVICE_DISCONNECTED_C) {
      Serial.println(F("Device disconnected"));
      continue;
    }

    Serial.print(F("Temp: "));
    Serial.println(devices[i].val);
  }
}
