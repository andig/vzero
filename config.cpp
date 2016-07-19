/**
 * Config file
 */

#include <ESP8266WiFi.h>
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
rst_info* g_resetInfo;
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
  arg = json["ssid"].asString();
  if (arg) g_ssid = arg;
  arg = json["password"].asString();
  if (arg) g_pass = arg;
  arg = json["middleware"].asString();
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
void start_plugins()
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
