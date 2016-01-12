#include <Arduino.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "OneWirePlugin.h"


#define TEMPERATURE_PRECISION 9

// plugin states
#define PLUGIN_REQUESTING 1
#define PLUGIN_READING 2

#define SLEEP_PERIOD 5 * 1000
#define REQUEST_WAIT_DURATION 1 * 1000


/*
 * Static
 */

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
  }
}

void OneWirePlugin::strToAddr(const char* ptr, uint8_t* addr) {
  int i;
  uint8_t b = 0;
  while (b < sizeof(DeviceAddress) && sscanf(ptr, "%2x", &i) == 1) {
    addr[b++] = i;
    ptr += 2;
  }
}


/*
 * Virtual
 */

OneWirePlugin::OneWirePlugin(byte pin) : ow(pin), sensors(&ow) {
  Serial.println(F("Looking for 1-Wire devices..."));
  // locate devices on the bus
  sensors.begin();
  setupSensors();
}

void OneWirePlugin::loop() {
  Serial.println("OneWirePlugin::loop()");

  // plugin states:
  //    sleeping -> requesting -> reading
  if (_status == PLUGIN_IDLE && elapsed(SLEEP_PERIOD)) {
    Serial.println(F("Requesting temparatures"));
    _status = PLUGIN_REQUESTING;

    sensors.setWaitForConversion(false);
    sensors.requestTemperatures();
  }
  else if (_status == PLUGIN_REQUESTING && elapsed(REQUEST_WAIT_DURATION)) {
    Serial.println(F("Reading temparatures"));
    _status = PLUGIN_READING;

    readTemperatures();
    _status = PLUGIN_IDLE;
  }
}

void OneWirePlugin::getData(JsonObject* json) {
  JsonObject& config = json->createNestedObject("settings");
  config["middleware"] = "http://demo.volkszaehler.org/middleware.php";
  config["interval"] = 30;

  JsonArray& sensorlist = json->createNestedArray("sensors");

  char addr_c[16];
  for (int i=0; i<devs; i++) {
    addrToStr(addr_c, devices[i].addr);
    yield();

    JsonObject& data = sensorlist.createNestedObject();
    data["addr"] = String(addr_c);
    data["uuid"] = String(devices[i].uuid);
  }
}

void OneWirePlugin::setupSensors() {
  Serial.println(F("OneWirePlugin::setupSensors"));

  devs = sensors.getDeviceCount();
  Serial.print(F("Found "));
  Serial.print(devs, DEC);
  Serial.println(F(" devices."));

  // report parasite power requirements
  Serial.print(F("Parasite power is: "));
  Serial.println((sensors.isParasitePowerMode()) ? F("on") : F("off"));

  char addr_c[16];
  for (int i=0; i<sensors.getDeviceCount(); i++) {
    sensors.getAddress(devices[i].addr, i);
    yield();

    addrToStr(&addr_c[0], devices[i].addr);
    Serial.print(F("Found device: "));
    Serial.println(addr_c);

    // set precision
    sensors.setResolution(devices[i].addr, TEMPERATURE_PRECISION);
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
  Serial.println("OneWirePlugin::readTemperatures()");

  for (int i=0; i<devs; i++) {
    float tempC = sensors.getTempC(devices[i].addr);
    yield();

    if (tempC == DEVICE_DISCONNECTED_C) {
      Serial.println(F("Device disconnected"));
      continue;
    }

    Serial.print(F("Temp: "));
    Serial.println(tempC);
  }
}
