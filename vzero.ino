/**
 * VZero - Zero Config Volkszaehler Controller
 *
 * @author Andreas Goetz <cpuidle@gmx.de>
 */

#include <Arduino.h>
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

#ifdef CAPTIVE_PORTAL
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif

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
  if ((WiFi.getMode() & WIFI_STA) == 0)
    return OPERATION_NORMAL;
  return OPERATION_SLEEP;
}

/**
 * (Re)connect WiFi - give ESP 10 seconds to connect to station
 */
wl_status_t wifiConnect() {
  DEBUG_MSG("wifi", "waiting for connection");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    DEBUG_PLAIN(".");
    delay(100);
  }
  DEBUG_PLAIN("\n");
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
  if ((WiFi.getMode() & WIFI_STA) == 0)
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
    // DEBUG_MSG(CORE, "sleep window is %u for %s\n", sleep, Plugin::get(pluginIndex)->getName().c_str());
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
  if (MDNS.begin(net_hostname.c_str())) {
    DEBUG_MSG(CORE, "mDNS responder started at %s.local\n", net_hostname.c_str());
  }
  else {
    DEBUG_MSG(CORE, "error setting up mDNS responder\n");
  }

  // start OTA server
  DEBUG_MSG(CORE, "starting OTA server\n");
  ArduinoOTA.setHostname(net_hostname.c_str());
  ArduinoOTA.onStart([]() {
    DEBUG_MSG(CORE, "OTA start\n");
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_MSG(CORE, "OTA end\n");

    // save config after OTA Update
    if (SPIFFS.begin()) {
      File configFile = SPIFFS.open(F("/config.json"), "r");
      if (!configFile) {
        DEBUG_MSG(CORE, "config wiped by OTA - saving\n");
        saveConfig();
      }
      else {
        configFile.close();
      }
      SPIFFS.end();
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_MSG(CORE, "OTA error [%u]\n", error);
  });
  ArduinoOTA.begin();
#endif
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
  ets_install_putc1((void *) &_u0_putc);
  system_set_os_print(1);

  g_resetInfo = ESP.getResetInfoPtr();
  DEBUG_PLAIN("\n");
  DEBUG_MSG(CORE, "Booting...\n");
  DEBUG_MSG(CORE, "Cause %d:    %s\n", g_resetInfo->reason, ESP.getResetReason().c_str());
  DEBUG_MSG(CORE, "Chip ID:    %05X\n", ESP.getChipId());

  // set hostname
  net_hostname += "-" + String(ESP.getChipId(), HEX);
  WiFi.hostname(net_hostname);
  DEBUG_MSG(CORE, "Hostname:   %s\n", net_hostname.c_str());

  // initialize file system
  if (!SPIFFS.begin()) {
    DEBUG_MSG(CORE, "failed mounting file system\n");
    return;
  }

  // check WiFi connection
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // configuration changed - set new credentials
  if (loadConfig() && g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    DEBUG_MSG("wifi", "connect:    %s\n", WiFi.SSID().c_str());
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  }
  else {
    // reconnect to sdk-configured station
    DEBUG_MSG("wifi", "reconnect:  %s\n", WiFi.SSID().c_str());
    WiFi.begin();
  }

  // Check connection
  if (wifiConnect() == WL_CONNECTED) {
    DEBUG_MSG("wifi", "IP address: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  }
  else {
    // go into AP mode
    DEBUG_MSG("wifi", "could not connect to WiFi - going into AP mode\n");

    WiFi.mode(WIFI_AP); // WIFI_AP_STA
    delay(10);

    WiFi.softAP(ap_default_ssid);
    DEBUG_MSG("wifi", "IP address: %d.%d.%d.%d\n", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);

#ifdef CAPTIVE_PORTAL
    // start DNS server for any domain
    DEBUG_MSG("wifi", "starting captive DNS server\n");
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
#endif
  }

  // start OTA - both in AP as in STA mode
  if (getOperationMode() == OPERATION_NORMAL) {
    start_ota();
  }

  // start plugins (before web server)
  start_plugins();

  // start web server if not in battery mode
  if (getOperationMode() == OPERATION_NORMAL) {
    webserver_start();
  }
}

long _minFreeHeap = 0;
long _freeHeap = 0;

/**
 * Loop
 */
void loop()
{
#ifdef CAPTIVE_PORTAL
  dnsServer.processNextRequest();
#endif

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
    DEBUG_MSG(CORE, "going to deep sleep for %ums\n", sleep);
    ESP.deepSleep(sleep * 1000);
  }

  // trigger restart?
  if (g_restartTime > 0 && millis() >= g_restartTime) {
    DEBUG_MSG(CORE, "restarting...\n");
    g_restartTime = 0;
    ESP.restart();
  }

  // check WLAN if not AP
  if ((WiFi.getMode() & WIFI_AP) == 0) {
    if (WiFi.status() != WL_CONNECTED) {
      DEBUG_MSG(CORE, "wifi connection lost\n");
      WiFi.reconnect();
      if (wifiConnect() != WL_CONNECTED) {
        DEBUG_MSG(CORE, "could not reconnect wifi - restarting\n");
        ESP.restart();
      }
    }
  }

  if (g_minFreeHeap != _minFreeHeap || ESP.getFreeHeap() != _freeHeap) {
    _freeHeap = ESP.getFreeHeap();
    if (_freeHeap < g_minFreeHeap)
      g_minFreeHeap = _freeHeap;
    _minFreeHeap = g_minFreeHeap;

    umm_info(NULL, 0);
    DEBUG_MSG(CORE, "heap min: %d (%d blk, %d tot)\n", g_minFreeHeap, ummHeapInfo.maxFreeContiguousBlocks * 8, _freeHeap);

    // print = millis();
  }
  delay(1000);
}
