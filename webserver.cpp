/**
 * Web server
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

// required for rst_info
extern "C" {
#include <user_interface.h>
}

#include "config.h"
#include "webserver.h"
#include "urlfunctions.h"
#include "Plugin.h"


// Webserver handle on port 80
ESP8266WebServer g_server(80);

unsigned long g_restartTime = 0;

String getIP()
{
  IPAddress ip = (WiFi.getMode() == WIFI_STA) ? WiFi.localIP() : WiFi.softAPIP();
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void restart()
{
  g_restartTime = millis() + 100;
}

void g_debug()
{
  Serial.print("uri: ");
  Serial.print(g_server.uri());
  Serial.print(" ");
  Serial.println(g_server.args());
}

void jsonResponse(JsonVariant json)
{
  char buffer[512];
  json.printTo(buffer, sizeof(buffer));
  g_server.send(200, "application/json", buffer);
}

/**
 * Handle http root request
 */
void handleRoot()
{
  g_debug();
  String indexHTML = F("<!DOCTYPE html><html><head><title>File not found</title></head><body><h1>File not found.</h1></body></html>");

  File indexFile = SPIFFS.open(F("/index.html"), "r");
  if (indexFile) {
    indexHTML = indexFile.readString();
    indexFile.close();

    char buff[10];
    uint16_t s = millis() / 1000;
    uint16_t m = s / 60;
    uint8_t h = m / 60;
    snprintf(buff, 10, "%02d:%02d:%02d", h, m % 60, s % 60);

    // replace placeholder
    indexHTML.replace("[build]", BUILD);
    indexHTML.replace("[esp8266]", String(ESP.getChipId(), HEX));
    indexHTML.replace("[uptime]", buff);
    indexHTML.replace("[ssid]", g_ssid);
    indexHTML.replace("[pass]", g_pass);
    indexHTML.replace("[wifimode]", (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point");
    indexHTML.replace("[ip]", getIP());
  }

  g_server.send(200, "text/html", indexHTML);
}

/**
 * Handle set request from http server.
 */
void handleSetWlan()
{
  g_debug();
  String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
  resp += net_hostname;
  resp += F(".local/\"></head><body>");
    
  // Check arguments
  if (g_server.args() != 2) {
    g_server.send(400, "text/plain", F("Invalid number of arguments\n\n"));
    return;
  }

  String ssid = "";
  String pass = "";
  int result = 400;

  // read ssid and psk
  for (uint8_t i = 0; i < g_server.args(); i++) {
    if (g_server.argName(i) == "ssid") {
      ssid = urldecode(g_server.arg(i));
      ssid.trim();
    }
    else if (g_server.argName(i) == "pass") {
      pass = urldecode(g_server.arg(i));
      pass.trim();
    }
  }

  // check ssid and psk
  if (ssid != "") {
    // save ssid and psk to file
    if (saveConfig(&ssid, &pass)) {
      // store SSID and PSK into global variables.
      g_ssid = ssid;
      g_pass = pass;

      resp += F("Success");
      result = 200; // HTTP OK
    }
    else {
      resp += F("<h1>Failed to save config file.</h1>");
    }
  }
  else {
    resp += F("<h1>Wrong arguments.</h1>");
  }

  resp += F("</body></html>");
  g_server.send(result, "text/html", resp);

  if (result == 200) {
    restart(); 
  }
}

/**
 * Status JSON api
 */
void handleGetStatus()
{
  g_debug();
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["uptime"] = millis();
  json["heap"] = ESP.getFreeHeap();
  json["wifimode"] = (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point";
  json["ip"] = getIP();
  json["flash"] = ESP.getFlashChipRealSize();

  // [/*0*/ "DEFAULT", /*1*/ "WDT", /*2*/ "EXCEPTION", /*3*/ "SOFT_WDT", /*4*/ "SOFT_RESTART", /*5*/ "DEEP_SLEEP_AWAKE", /*6*/ "EXT_SYS_RST"]
  rst_info* resetInfo = ESP.getResetInfoPtr();
  json["resetcode"] = resetInfo->reason;
  
  // json["gpio"] = (uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16));
  jsonResponse(json);
}

/**
 * Wlan scan JSON api
 */
void handleGetWlan()
{
/*
  g_debug();
  StaticJsonBuffer<200> jsonBuffer;
  JsonArray& json = jsonBuffer.createArray();

  // Wlan scan is only possible if in AP mode with STA enabled but disconnected
  if (WiFi.getMode() == WIFI_AP_STA) {
    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println(F("Wlan scan done"));
    Serial.print(n);
    Serial.println(F(" networks found"));
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");

      JsonObject& data = json.createNestedObject();
      data["ssid"] = WiFi.SSID(i);
      data["rssi"] = WiFi.RSSI(i);
      data["encryption"] = WiFi.encryptionType(i);
    }
    Serial.println("");
  }

  jsonResponse(json);
*/
}

/**
 * Get plugin information
 */
void handleGetPlugins()
{
  g_debug();
  StaticJsonBuffer<200> jsonBuffer;
  JsonArray& json = jsonBuffer.createArray();

  for (uint8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    Serial.print(F("vzero::loop plugin #"));
    Serial.print(pluginIndex);
    Serial.println();

    JsonObject& plugin = json.createNestedObject();

    // call plugin's loop method

    Plugin::get(pluginIndex++)->getData(&plugin);
  }
  
  jsonResponse(json);
}

/**
 * Handle file not found from http server.
 */
void handleNotFound()
{
  Serial.print(F("File not found: "));
  Serial.println(g_server.uri());

  String response = F("File not found");
  g_server.send(404, "text/plain", response);
}

void serveStaticDir(String path)
{
  Serial.print(F("Static: "));
  Serial.println(path);

  Dir dir = SPIFFS.openDir(path);
  while (dir.next()) {
    File file = dir.openFile("r");
    Serial.print(F("Static: "));
    Serial.println(file.name());
    g_server.serveStatic(file.name(), SPIFFS, file.name());
    file.close();
/*
    Serial.print("Static: ");
    Serial.println(dir.fileName());
    g_server.serveStatic(dir.fileName(), SPIFFS, dir.fileName());
*/
  }
}

/**
 * Initialize web server and add requests
 */
void webserver_start()
{
  g_server.on("/", handleRoot);
  g_server.on("/index.html", handleRoot);

  // GET
  g_server.on("/set", HTTP_GET, handleSetWlan);

  // POST
  g_server.on("/restart", HTTP_POST, []() {
    String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
    resp += net_hostname;
    resp += F(".local/\"></head><body>Restarting...<br/><img src=\"/img/loading.gif\"></body></html>");
    g_server.send(200, "text/html", resp);
    restart();
  });

  // api functions
  g_server.on("/api/status", HTTP_GET, handleGetStatus);
  g_server.on("/api/wlan", HTTP_GET, handleGetWlan);
  g_server.on("/api/plugin", HTTP_GET, handleGetPlugins);

  // static
  g_server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico");
  serveStaticDir("/img");
  serveStaticDir("/js");
  serveStaticDir("/css");

  // not found
  g_server.onNotFound(handleNotFound);

  // Start server
  g_server.begin();
}
