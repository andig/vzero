 /**
 * VZero - Zero Config Volkszaehler Controller
 *
 * @author Andreas Goetz <cpuidle@gmx.de>
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#include "config.h"
#include "webserver.h"
#include "plugins/Plugin.h"

#ifdef PLUGIN_ONEWIRE
#include "plugins/OneWirePlugin.h"
#endif

#ifdef PLUGIN_ANALOG
#include "plugins/AnalogPlugin.h"
#endif

#ifdef PLUGIN_WIFI
#include "plugins/WifiPlugin.h"
#endif

#ifdef OTA_SERVER
#include <ArduinoOTA.h>
#endif

extern "C" {
  #include "user_interface.h"
}
//extern "C" void system_set_os_print(uint8 onoff);
//extern "C" void ets_install_putc1(void* routine);

/**
 * Use the internal hardware buffer
 */
static void _u0_putc(char c){
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
}

enum operation_t {
  OPERATION_NORMAL = 0x0, // deep sleep forbidden
  OPERATION_SLEEP =  0x1  // deep sleep allowed
};

/**
 * Get operation mode
 *
 * Return NORMAL if:
 *   - deep sleep disabled
 *   - cold start
 *   - no WiFi connection
 */
operation_t getOperationMode()
{
#ifndef DEEP_SLEEP
  return OPERATION_NORMAL;
#endif
  if (g_resetInfo->reason != REASON_DEEP_SLEEP_WAKE)
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
  // don't sleep if client connected
  if (millis() - g_lastAccess < WIFI_CLIENT_TIMEOUT)
    return 0;
  // check if deep sleep possible
  uint32_t maxSleep = -1; // max uint32_t
  for (uint8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    uint32_t sleep = Plugin::get(pluginIndex)->getMaxSleepDuration();
    // DEBUG_CORE("[core] sleep window is %u for %s\n", sleep, Plugin::get(pluginIndex)->getName().c_str());
    if (sleep < maxSleep)
      maxSleep = sleep;
  }
  // valid sleep window found?
  if (maxSleep == -1 || maxSleep < MIN_SLEEP_DURATION_MS)
    return 0;
  // not during initial startup?
  if (g_resetInfo->reason != 5 && millis() < STARTUP_ONLINE_DURATION_MS)
    return 0;
  return maxSleep - SLEEP_SAFETY_MARGIN;
#endif
}

/**
 * Setup
 */
void setup()
{
  Serial.begin(115200);
  // hardware serial
  ets_install_putc1((void *) &_u0_putc);
  system_set_os_print(0);
  g_resetInfo = ESP.getResetInfoPtr();

  DEBUG_CORE("\n\n[core] Booting...\n");
  DEBUG_CORE("[core] Cause %d:    %s\n", g_resetInfo->reason, ESP.getResetReason().c_str());
  DEBUG_CORE("[core] Chip ID:    %05X\n", ESP.getChipId());

  // set hostname
  net_hostname += "-" + String(ESP.getChipId(), HEX);
  WiFi.hostname(net_hostname);

  // print hostname
  DEBUG_CORE("[core] Hostname:   %s\n", net_hostname.c_str());

  // check flash settings
  validateFlash();

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

  DEBUG_CORE("[wifi] current ssid:  %s\n", WiFi.SSID().c_str());
  // DEBUG_CORE("[wifi] current psk:   %s\n", WiFi.psk().c_str());

  // try connect to configured station
  if (loadConfig() && g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  }
  else {
    // connect to sdk-configured station
    DEBUG_CORE("[wifi] no wifi config found\n");
    WiFi.begin();
  }

  // Check connection
  if (wifiConnect() == WL_CONNECTED) {
    DEBUG_CORE("[wifi] IP address: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

    // start MDNS
    if (getOperationMode() == OPERATION_NORMAL) {
      if (MDNS.begin(net_hostname.c_str()))
        DEBUG_CORE("[core] mDNS responder started at %s.local\n", net_hostname.c_str());
      else
        DEBUG_CORE("[core] error setting up mDNS responder\n");
    }
  }
  else {
    // go into AP mode
    DEBUG_CORE("[wifi] could connect to WiFi- going into AP mode\n");

    WiFi.mode(WIFI_AP); // WIFI_AP_STA
    delay(10);

    WiFi.softAP(ap_default_ssid);
    DEBUG_CORE("[wifi] IP address: %d.%d.%d.%d\n", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
  }

  // start webserver after plugins
  system_set_os_print(1);
  system_show_malloc();

  // start plugins
  DEBUG_CORE("[core] starting plugins\n");
#ifdef PLUGIN_ONEWIRE
  new OneWirePlugin(ONEWIRE_PIN);
#endif
#ifdef PLUGIN_ANALOG
  new AnalogPlugin();
#endif
#ifdef PLUGIN_WIFI
  new WifiPlugin();
#endif

  // start OTA and web server if not in battery mode
  if (getOperationMode() == OPERATION_NORMAL) {
#ifdef OTA_SERVER
    // start OTA server
    DEBUG_CORE("[core] starting OTA server\n");
    ArduinoOTA.setHostname(net_hostname.c_str());
    ArduinoOTA.onStart([]() {
      DEBUG_CORE("[core] OTA Start\n");
    });
    ArduinoOTA.onEnd([]() {
      DEBUG_CORE("[core] OTA End\n");
    });
    ArduinoOTA.onError([](ota_error_t error) {
      DEBUG_CORE("[core] OTA Error [%u]\n", error);
    });
    ArduinoOTA.begin();
#endif

    webserver_start();
  }
}

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
  for (uint8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    Plugin::get(pluginIndex)->loop();
  }

  // check we deep sleep possible
  uint32_t sleep = getDeepSleepDurationMs();
  if (sleep > 0) {
    DEBUG_CORE("[core] going to deep sleep for %ums\n", sleep);
    ESP.deepSleep(sleep * 1000);
  }

  // trigger restart?
  if (g_restartTime > 0 && millis() >= g_restartTime) {
    DEBUG_CORE("[core] Restarting...\n");
    g_restartTime = 0;
    ESP.restart();
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_CORE("[core] wifi connection lost\n");
    if (wifiConnect() != WL_CONNECTED) {
      DEBUG_CORE("[core] could not reconnect wifi - restarting\n");
      ESP.reset();
    }
  }

  delay(100);
/*
  DEBUG_CORE("[core] going to light sleep\n");
  m = millis();
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  delay(20000);
  DEBUG_CORE("[core] slept for %dms\n", millis()-m);
*/
}
