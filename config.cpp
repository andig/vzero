/**
 * Config file
 */

#include <ESP8266WiFi.h>
#include <MD5Builder.h>
#include <WString.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "config.h"

// default AP SSID
const char* ap_default_ssid = "VZERO";

// global vars
String net_hostname = "vzero";
rst_info* g_resetInfo;
uint32_t g_lastAccess;

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
 * Hash builder initialized with unique module identifiers
 */
MD5Builder getHashBuilder()
{
  uint8_t mac[6];

  MD5Builder md5;
  md5.begin();

  long chipId = ESP.getChipId();
  md5.add((uint8_t*)&chipId, 4);

  uint32_t flashId = ESP.getFlashChipId();
  md5.add((uint8_t*)&flashId, 2);

  WiFi.macAddress(&mac[0]);
  md5.add(&mac[0], 6);

  return md5;
}

/**
 * Unique module identifier
 */
String getHash()
{
  MD5Builder md5 = getHashBuilder();
  md5.calculate();
  return md5.toString();
}

/**
 * Load config
 */
bool loadConfig()
{
  File configFile = SPIFFS.open(F("/config.json"), "r");
  if (!configFile)
    return false;
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  String arg;
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  arg = json["ssid"].asString();
  if (arg) g_ssid = arg;
  arg = json["password"].asString();
  if (arg) g_pass = arg;
  arg = json["middleware"].asString();
  if (arg) g_middleware = arg;

  DEBUG_CORE("[core] config ssid:   %s\n", g_ssid.c_str());
  // DEBUG_CORE("[core] config psk:    %s\n", g_pass.c_str());
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

