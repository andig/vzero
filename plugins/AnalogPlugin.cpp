#include "AnalogPlugin.h"


/*
 * Virtual
 */

AnalogPlugin::AnalogPlugin() {
}

String AnalogPlugin::getName() {
  return "analog";
}

int8_t AnalogPlugin::getSensorByAddr(const char* addr_c) {
  if (strcmp(addr_c, "a0") == 0)
    return 0;
  return -1;
}

bool AnalogPlugin::getAddr(char* addr_c, int8_t sensor) {
  if (sensor >= _devs)
    return false;
  strcpy(addr_c, "a0");
  return true;
}

float AnalogPlugin::getValue(int8_t sensor) {
  return analogRead(A0) / 1023.0;
}

/**
 * Loop
 */
void AnalogPlugin::loop() {
}
