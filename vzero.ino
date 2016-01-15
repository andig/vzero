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
#include "Plugin.h"

#ifdef PLUGIN_ONEWIRE
#include "OneWirePlugin.h"
#endif

#ifdef OTA_SERVER
#include <ArduinoOTA.h>
#endif


/**
 * Validate physical flash settings vs IDE
 */
void validateFlash()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();

  Serial.printf("Flash ID:   %08X\n", ESP.getFlashChipId());
  Serial.printf("Flash size: %u\n", realSize);
  Serial.printf("Flash free: %u\n", ESP.getFreeSketchSpace());

  if (ideSize != realSize) {
    Serial.println(F("Flash configuration wrong!\n"));
  }
  else {
    Serial.println(F("Flash configuration ok.\n"));
  }
}

/**
 * Setup
 */
void setup()
{
  Serial.begin(115200);
  Serial.println(F("\nBooting..."));
  Serial.printf("Chip ID:    %05X\n", ESP.getChipId());

  // set hostname
  net_hostname += "-";
  net_hostname += String(ESP.getChipId(), HEX);
  WiFi.hostname(net_hostname);

  // print hostname
  Serial.println("Hostname:   " + net_hostname + "\n");

  // check flash settings
  validateFlash();

  // initialize file system
  if (!SPIFFS.begin()) {
    Serial.println(F("Failed to mount file system."));
    return;
  }

  Serial.println(F("-- Current WiFi config --"));
  Serial.println("SSID:   " + WiFi.SSID());
  Serial.println("PSK:    " + WiFi.psk() + "\n");
  
  // load wifi connection information
  if (!loadConfig(&g_ssid, &g_pass, &g_middleware)) {
    g_ssid = "";
    g_pass = "";
    Serial.println(F("No WiFi connection information available."));
  }

  // Check WiFi connection
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(10);
  }

  // compare file config with sdk config
  if (g_ssid != "" && (String(WiFi.SSID()) != g_ssid || String(WiFi.psk()) != g_pass)) {
    Serial.println(F("WiFi config changed."));

    // try to connect to WiFi station
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());

    // uncomment this for debugging output
    WiFi.printDiag(Serial);
  }
  else {
    // begin with sdk config
    WiFi.begin();
  }

  Serial.print(F("Waiting for WiFi connection."));

  // give ESP 10 seconds to connect to station
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    Serial.write('.');
    delay(100);
  }
  Serial.println();

  // Check connection
  if (WiFi.status() == WL_CONNECTED) {
    // print IP address
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());

    // start MDNS
    if (!MDNS.begin(net_hostname.c_str())) {
      Serial.println(F("Error setting up MDNS responder!"));
    }
    else {
      Serial.print(F("mDNS responder started at "));
      Serial.print(net_hostname);
      Serial.println(F(".local"));
    }
  }
  else {
    Serial.println(F("Cannot connect to WiFi. Going into AP mode."));

    // go into AP mode
    WiFi.mode(WIFI_AP); // WIFI_AP_STA
    delay(10);

    WiFi.softAP(ap_default_ssid);

    Serial.print(F("IP address: "));
    Serial.println(WiFi.softAPIP());
  }

#ifdef OTA_SERVER
  // start OTA server
  Serial.println(F("Starting OTA server."));
  ArduinoOTA.setHostname(net_hostname.c_str());
  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA Start"));
    g_otaInProgress = 1;
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("OTA End"));
    g_otaInProgress = 0;
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    g_otaInProgress = 2;
    if (error == OTA_AUTH_ERROR) Serial.println(F("OTA Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("OTA Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("OTA Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("OTA Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("OTA End Failed"));
  });
  ArduinoOTA.begin();
#endif

  // start plugins
#ifdef PLUGIN_ONEWIRE
  new OneWirePlugin(ONEWIRE_PIN);
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
    Serial.println(F("Restarting..."));
    g_restartTime = 0;
    ESP.restart();
  }
}
