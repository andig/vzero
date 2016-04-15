/**
 * VZero - Zero Config Volkszaehler Controller
 *
 * @author Andreas Goetz <cpuidle@gmx.de>
 */

#include <ESP8266WiFi.h>
#include <FS.h>

#include "config.h"
#include "webserver.h"
#include "plugins/Plugin.h"

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

#ifdef OTA_SERVER
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#endif

extern "C" {
  #include <user_interface.h>
  #include <umm_malloc/umm_malloc.h>
}

enum operation_t {
  OPERATION_NORMAL = 0, // deep sleep forbidden
  OPERATION_SLEEP =  1  // deep sleep allowed
};

/**
 * Get operation mode
 *
 * Return NORMAL if:
 *   - deep sleep disabled
 *   - not waking up from deep sleep
 *   - not running as access point
 */
operation_t getOperationMode()
{
#ifndef DEEP_SLEEP
  return OPERATION_NORMAL;
#endif
  if (g_resetInfo->reason != REASON_DEEP_SLEEP_AWAKE)
    return OPERATION_NORMAL;
  if (WiFi.getMode() & WIFI_STA == 0)
    return OPERATION_NORMAL;
  return OPERATION_SLEEP;
}

/**
 * (Re)connect WiFi - give ESP 10 seconds to connect to station
 */
wl_status_t wifiConnect() {
  DEBUG_CORE("[wifi] waiting for connection");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    DEBUG_CORE(".");
    delay(100);
  }
  DEBUG_CORE("\n");
  return WiFi.status();
}

/**
 * Get max deep sleep window in ms
 */
uint32_t getDeepSleepDurationMs()
{
#ifndef DEEP_SLEEP
  return 0;
#else
  // don't sleep if access point
  if (WiFi.getMode() & WIFI_STA == 0)
    return 0;
  // don't sleep during initial startup
  if (g_resetInfo->reason != 5 && millis() < STARTUP_ONLINE_DURATION_MS)
    return 0;
  // don't sleep if client connected
  if (millis() - g_lastAccessTime < WIFI_CLIENT_TIMEOUT)
    return 0;
    
  // check if deep sleep possible
  uint32_t maxSleep = -1; // max uint32_t
  Plugin::each([&maxSleep](Plugin* plugin) {
    uint32_t sleep = plugin->getMaxSleepDuration();
    // DEBUG_CORE("[core] sleep window is %u for %s\n", sleep, Plugin::get(pluginIndex)->getName().c_str());
    if (sleep < maxSleep)
      maxSleep = sleep;
  });
  
  // valid sleep window found?
  if (maxSleep == -1 || maxSleep < MIN_SLEEP_DURATION_MS)
    return 0;
    
  return maxSleep - SLEEP_SAFETY_MARGIN;
#endif
}

/**
 * Start ota server
 */
void start_ota() {
#ifdef OTA_SERVER
  if (MDNS.begin(net_hostname.c_str()))
    DEBUG_CORE("[core] mDNS responder started at %s.local\n", net_hostname.c_str());
  else
    DEBUG_CORE("[core] error setting up mDNS responder\n");

  // start OTA server
  DEBUG_CORE("[core] starting OTA server\n");
  ArduinoOTA.setHostname(net_hostname.c_str());
  ArduinoOTA.onStart([]() {
    DEBUG_CORE("[core] OTA start\n");
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_CORE("[core] OTA end\n");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_CORE("[core] OTA error [%u]\n", error);
  });
  ArduinoOTA.begin();
#endif
}

/**
 * Start enabled plugins
 */
void start_plugins()
{
  DEBUG_CORE("[core] starting plugins\n");
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

// use the internal hardware buffer
static void _u0_putc(char c) {
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
}

/**
 * Setup
 */
void setup()
{
  // hardware serial
  Serial.begin(115200);
  g_resetInfo = ESP.getResetInfoPtr();
  ets_install_putc1((void *) &_u0_putc);
  system_set_os_print(1);

  DEBUG_CORE("\n\n[core] Booting...\n");
  DEBUG_CORE("[core] Cause %d:    %s\n", g_resetInfo->reason, ESP.getResetReason().c_str());
  DEBUG_CORE("[core] Chip ID:    %05X\n", ESP.getChipId());

  // set hostname
  net_hostname += "-" + String(ESP.getChipId(), HEX);
  WiFi.hostname(net_hostname);
  DEBUG_CORE("[core] Hostname:   %s\n", net_hostname.c_str());

#ifdef DEBUG
  // check flash settings
  validateFlash();
#endif

  // initialize file system
  if (!SPIFFS.begin()) {
    DEBUG_CORE("[core] Failed to mount file system.\n");
    return;
  }

  // check WiFi connection
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // configuration changed - set new credentials
  if (loadConfig() && g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    DEBUG_CORE("[wifi] connecting to:  %s\n", WiFi.SSID().c_str());
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  }
  else {
    // reconnect to sdk-configured station
    DEBUG_CORE("[wifi] reconnecting to:  %s\n", WiFi.SSID().c_str());
    WiFi.begin();
  }

  // Check connection
  if (wifiConnect() == WL_CONNECTED) {
    DEBUG_CORE("[wifi] IP address: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

    if (getOperationMode() == OPERATION_NORMAL) {
      start_ota();
    }
  }
  else {
    // go into AP mode
    DEBUG_CORE("[wifi] could not connect to WiFi - going into AP mode\n");

    WiFi.mode(WIFI_AP); // WIFI_AP_STA
    delay(10);

    WiFi.softAP(ap_default_ssid);
    DEBUG_CORE("[wifi] IP address: %d.%d.%d.%d\n", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
  }

  // start plugins (before web server)
  start_plugins();

  // start web server if not in battery mode
  if (getOperationMode() == OPERATION_NORMAL) {
    webserver_start();
  }
}

long print = 0;
long heap = 0;

/**
 * Loop
 */
void loop()
{
#ifdef OTA_SERVER
  if (getOperationMode() == OPERATION_NORMAL) {
    ArduinoOTA.handle();
  }
#endif

  // call plugin's loop method
  Plugin::each([](Plugin* plugin) {
    plugin->loop();
    yield();
  });

  // check if deep sleep possible
  uint32_t sleep = getDeepSleepDurationMs();
  if (sleep > 0) {
    DEBUG_CORE("[core] going to deep sleep for %ums\n", sleep);
    ESP.deepSleep(sleep * 1000);
  }

  // trigger restart?
  if (g_restartTime > 0 && millis() >= g_restartTime) {
    DEBUG_CORE("[core] restarting...\n");
    g_restartTime = 0;
    ESP.restart();
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_CORE("[core] wifi connection lost\n");
    WiFi.reconnect();
    if (wifiConnect() != WL_CONNECTED) {
      DEBUG_CORE("[core] could not reconnect wifi - restarting\n");
      ESP.restart();
    }
  }

  if (millis() - print > 10000 || heap != ESP.getFreeHeap()) {
    heap = ESP.getFreeHeap();
    if (heap < g_minFreeHeap)
      g_minFreeHeap = heap;

    umm_info(NULL, 0);
    Serial.printf("[core] heap/cont/min: %d / %d / %d\n", heap, ummHeapInfo.maxFreeContiguousBlocks * 8, g_minFreeHeap);

    print = millis();
  }
  delay(1000);
}
