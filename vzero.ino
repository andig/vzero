/**
 * VZero - Zero Config Volkszaehler Controller
 *
 * @author Andreas Goetz <cpuidle@gmx.de>
 * Based on works by Pascal Gollor (http://www.pgollor.de/cms/)
 */

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
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

#ifdef OTA_SERVER
#include <ArduinoOTA.h>
#endif

#define DEBUG_CORE(...) Serial.printf( __VA_ARGS__ )

/**
 * Validate physical flash settings vs IDE
 */
void validateFlash()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();

  DEBUG_CORE("[core] Flash ID:   %08X\n", ESP.getFlashChipId());
  DEBUG_CORE("[core] Flash size: %u\n", realSize);
  DEBUG_CORE("[core] Flash free: %u\n", ESP.getFreeSketchSpace());

  if (ideSize != realSize) {
    DEBUG_CORE("[core] Flash configuration wrong!\n");
  }
}

/**
 * Setup
 */
void setup()
{
  Serial.begin(115200);
  DEBUG_CORE("\n\n[core] Booting...\n");
  DEBUG_CORE("[core] Chip ID:    %05X\n", ESP.getChipId());

  // set hostname
  net_hostname += "-";
  net_hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(net_hostname);

  // print hostname
  DEBUG_CORE("[core] Hostname:   %s\n\n", net_hostname.c_str());

  // check flash settings
  validateFlash();

  // initialize file system
  if (!SPIFFS.begin()) {
    DEBUG_CORE("[core] Failed to mount file system.\n");
    return;
  }

  // Check WiFi connection
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  DEBUG_CORE("[wifi] current ssid:   %s\n", WiFi.SSID().c_str());
  DEBUG_CORE("[wifi] current psk:    %s\n", WiFi.psk().c_str());
  
  // load wifi connection information
  if (!loadConfig(&g_ssid, &g_pass, &g_middleware)) {
    g_ssid = "";
    g_pass = "";
    DEBUG_CORE("[wifi] no wifi config found.\n");
  }

  // compare file config with sdk config
  if (g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    DEBUG_CORE("[wifi] wifi config changed.\n");

    // try to connect to WiFi station
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
  }
  else {
    // begin with sdk config
    WiFi.begin();
  }

  DEBUG_CORE("[wifi] waiting for connection");

  // give ESP 10 seconds to connect to station
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
      DEBUG_CORE("[core] Error setting up mDNS responder!\n");
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
  DEBUG_CORE("[core] Starting OTA server.\n");
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

  // start webserver after plugins
  webserver_start();
}

/**
 * Loop
 */
void loop()
{
  // call plugin's loop method
  for (uint8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    // handle Webserver requests
    g_server.handleClient();

#ifdef OTA_SERVER
    // handle OTA requests
    ArduinoOTA.handle();
#endif

    Plugin::get(pluginIndex)->loop();
  }

  // trigger restart?
  if (g_restartTime > 0 && millis() >= g_restartTime) {
    DEBUG_CORE("[core] Restarting...\n");
    g_restartTime = 0;
    ESP.restart();
  }
}
