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

// use the internal hardware buffer
static void _u0_putc(char c){
  while(((U0S >> USTXC) & 0x7F) == 0x7F);
  U0F = c;
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
  DEBUG_CORE("[wifi] current psk:   %s\n", WiFi.psk().c_str());

  // load wifi connection information
  if (!loadConfig()) {
    DEBUG_CORE("[wifi] no wifi config found.\n");
  }

  // compare file config with sdk config
  if (g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    DEBUG_CORE("[wifi] wifi config changed\n");

    // try to connect to configured station
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  }
  else {
    // connect to sdk-configured station
    WiFi.begin();
  }

  // give ESP 10 seconds to connect to station
  DEBUG_CORE("[wifi] waiting for connection");
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    DEBUG_CORE(".");
    delay(100);
  }
  DEBUG_CORE("\n");

  // Check connection
  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_CORE("[wifi] IP address: %d.%d.%d.%d\n", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

    // start MDNS
    if (!MDNS.begin(net_hostname.c_str())) {
      DEBUG_CORE("[core] error setting up mDNS responder\n");
    }
    else {
      DEBUG_CORE("[core] mDNS responder started at %s.local\n", net_hostname.c_str());
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

  // start webserver after plugins
  system_set_os_print(1);
  system_show_malloc();
  webserver_start();
}

/**
 * Loop
 */
void loop()
{
#ifdef OTA_SERVER
  ArduinoOTA.handle();
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
  delay(100);

  // trigger restart?
  if (g_restartTime > 0 && millis() >= g_restartTime) {
    DEBUG_CORE("[core] Restarting...\n");
    g_restartTime = 0;
    ESP.restart();
  }
/*
  DEBUG_CORE("[core] going to light sleep\n");
  m = millis();
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  delay(20000);
  DEBUG_CORE("[core] slept for %dms\n", millis()-m);
*/
}
