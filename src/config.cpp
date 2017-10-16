/**
 * Config file
 */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif

#ifdef ESP32
#include <WiFi.h>
#include "SPIFFS.h"
#include <rom/rtc.h>
#endif

#include <MD5Builder.h>
#include <WString.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "config.h"

#ifdef PLUGIN_ONEWIRE
#include "plugins/OneWirePlugin.h"
#endif

#ifdef PLUGIN_DHT
#include "plugins/DHTPlugin.h"
#endif

#ifdef PLUGIN_ANALOG
#include "plugins/AnalogPlugin.h"
#endif

#ifdef PLUGIN_WIFI
#include "plugins/WifiPlugin.h"
#endif

// default AP SSID
const char* ap_default_ssid = "VZERO";

// global vars
#ifdef ESP8266
rst_info* g_resetInfo;
#endif
String net_hostname = "vzero";
uint32_t g_minFreeHeap = -1;

// global settings
String g_ssid = "";
String g_pass = "";
String g_middleware = "";


#ifdef DEBUG
void debug_plain(const char *msg) {
  ets_printf(msg);
}

/**
 * Verbose debug output
 */
void debug_message(const char *module, const char *format, ...) {
  #define BUFFER_SIZE 150
  char buf[BUFFER_SIZE];
  ets_printf("%08d [%-6s] ", millis(), module);
  // snprintf(buf, BUFFER_SIZE, "%6.3f [%-6s] ", millis()/1000.0, module);

  va_list args;
  va_start(args, format);
  vsnprintf(buf, BUFFER_SIZE, format, args);
  ets_printf(buf);
  va_end(args);

  if (ESP.getFreeHeap() < g_minFreeHeap) {
    g_minFreeHeap = ESP.getFreeHeap();
  }
}
#endif

long getChipId()
{
#ifdef ESP8266
  return ESP.getChipId();
#endif
#ifdef ESP32
  long chipId;
  ESP.getEfuseMac();
  return chipId;
#endif
}

/**
 * Hash builder initialized with unique module identifiers
 */
MD5Builder getHashBuilder()
{
  uint8_t mac[6];

  MD5Builder md5;
  md5.begin();

  uint64_t chipId = getChipId();
  md5.add((uint8_t*)&chipId, 4);

#ifdef ESP8266
  uint32_t flashId = ESP.getFlashChipId();
  md5.add((uint8_t*)&flashId, 2);
#endif
  
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
  DEBUG_MSG(CORE, "loading config\n");
  File configFile = SPIFFS.open(F("/config.json"), "r");
  if (!configFile) {
    DEBUG_MSG(CORE, "open config failed");
    return false;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  String arg;
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  arg = json["ssid"].as<char*>();
  if (arg) g_ssid = arg;
  arg = json["password"].as<char*>();
  if (arg) g_pass = arg;
  arg = json["middleware"].as<char*>();
  if (arg) g_middleware = arg;

  DEBUG_MSG(CORE, "ssid:       %s\n", g_ssid.c_str());
  // DEBUG_MSG(CORE, "config psk:    %s\n", g_pass.c_str());
  DEBUG_MSG(CORE, "middleware: %s\n", g_middleware.c_str());

  return true;
}

/**
 * Save config
 */
bool saveConfig()
{
  File configFile = SPIFFS.open(F("/config.json"), "w");
  if (!configFile) {
    DEBUG_MSG(CORE, "save config failed");
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

/**
 * Start enabled plugins
 */
void startPlugins()
{
  DEBUG_MSG(CORE, "starting plugins\n");
#ifdef PLUGIN_ONEWIRE
  new OneWirePlugin(ONEWIRE_PIN);
#endif
#ifdef PLUGIN_DHT
  new DHTPlugin(DHT_PIN, DHT_TYPE);
#endif
#ifdef PLUGIN_ANALOG
  new AnalogPlugin();
#endif
#ifdef PLUGIN_WIFI
  new WifiPlugin();
#endif
}

#ifdef ESP8266
int getResetReason(int core)
{
  return (int)g_resetInfo->reason;
}

const char* getResetReasonStr(int core)
{
  return ESP.getResetReason().c_str();
}
#endif

#ifdef ESP32
int getResetReason(int core)
{
  return (int)rtc_get_reset_reason(core);
}

const char* getResetReasonStr(int core)
{
  switch (rtc_get_reset_reason(core))
  {
    case 1  : return "Vbat power on reset";
    case 3  : return "Software reset digital core";
    case 4  : return "Legacy watch dog reset digital core";
    case 5  : return "Deep Sleep reset digital core";
    case 6  : return "Reset by SLC module, reset digital core";
    case 7  : return "Timer Group0 Watch dog reset digital core";
    case 8  : return "Timer Group1 Watch dog reset digital core";
    case 9  : return "RTC Watch dog Reset digital core";
    case 10 : return "Instrusion tested to reset CPU";
    case 11 : return "Time Group reset CPU";
    case 12 : return "Software reset CPU";
    case 13 : return "RTC Watch dog Reset CPU";
    case 14 : return "for APP CPU, reseted by PRO CPU";
    case 15 : return "Reset when the vdd voltage is not stable";
    case 16 : return "RTC Watch dog reset digital core and rtc module";
    default : return "";
  }
}
#endif
