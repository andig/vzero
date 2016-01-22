/**
 * Config file
 */

#include <WString.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "config.h"

// default AP SSID
const char* ap_default_ssid = "VZERO";

// hostname prefix
String net_hostname = "vzero";

// global settings
String g_ssid = "";
String g_pass = "";
String g_middleware = "";

// global variables
uint16_t g_minFreeHeap = 65535;

/**
 * Validate physical flash settings vs IDE
 */
void validateFlash()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();

  DEBUG_CORE("[core] Flash size: %u\n", realSize);
  DEBUG_CORE("[core] Flash free: %u\n", ESP.getFreeSketchSpace());

  if (ideSize != realSize) {
    DEBUG_CORE("[core] Flash configuration wrong!\n");
  }
}

/**
 * Load config
 */
bool loadConfig()
{
  File configFile = SPIFFS.open(F("/config.json"), "r");
  if (!configFile)
    return false;
  char *buf = (char*)malloc(configFile.size()+1);
  if (!buf) 
    return false;
  configFile.read((uint8_t *)buf, configFile.size());
  buf[configFile.size()] = '\0';
/*
  Serial.printf("%d ", configFile.size());
  Serial.printf("%s\n", buf);
*/
  configFile.close();

  String arg;
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf);
  arg = json["ssid"].asString();
  if (arg) g_ssid = arg;
  arg = json["password"].asString();
  if (arg) g_pass = arg;
  arg = json["middleware"].asString();
  if (arg) g_middleware = arg;

  DEBUG_CORE("[core] config ssid:   %s\n", g_ssid.c_str());
  DEBUG_CORE("[core] config psk:    %s\n", g_pass.c_str());
  DEBUG_CORE("[core] middleware:    %s\n", g_middleware.c_str());
  
  return true;
}

/**
 * Save config
 */
bool saveConfig()
{
  File configFile = SPIFFS.open(F("/config.json"), "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing"));
    return false;
  }

  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["ssid"] = g_ssid;
  json["password"] = g_pass;
  json["middleware"] = g_middleware;

  json.printTo(configFile);
  configFile.close();

  return true;
}

