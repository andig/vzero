/**
 * Web server
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "webserver.h"
#include "urlfunctions.h"
#include "plugins/Plugin.h"

// required for rst_info
extern "C" {
#include <user_interface.h>
}


#define CACHE_HEADER "max-age=86400"

// Webserver handle on port 80
ESP8266WebServer g_server(80);

unsigned long g_restartTime = 0;


class PluginRequestHandler : public RequestHandler {
public:
  PluginRequestHandler(const char* uri, Plugin* plugin, const int8_t sensor) : _uri(uri), _plugin(plugin), _sensor(sensor) {
  }

  bool canHandle(HTTPMethod requestMethod, String requestUri) override {
    if (requestMethod != HTTP_GET && requestMethod != HTTP_POST)
      return false;
    if (!requestUri.startsWith(_uri))
      return false;
    return true;
  }

  bool handle(ESP8266WebServer& server, HTTPMethod requestMethod, String requestUri) override {
    if (!canHandle(requestMethod, requestUri))
      return false;

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    int res = 400; // JSON error

    // GET
    if (requestMethod == HTTP_GET && server.args() == 0) {
      float val = _plugin->getValue(_sensor);
      if (val != NAN) {
        json["value"] = val;
        res = 200;
      }
    }
    // POST
    else if ((requestMethod == HTTP_POST && server.args() == 1 && server.argName(0) == "uuid")
      || (requestMethod == HTTP_GET && server.args() == 1 && server.argName(0) == "uuid")) {
      String uuid = server.arg(0);
      if (_plugin->setUuid(uuid.c_str(), _sensor)) {
        _plugin->getSensorJson(&json, _sensor);
        res = 200;
      }
    }
    
    char buffer[200];
    json.printTo(buffer, sizeof(buffer));
    server.send(res, "application/json", buffer);
    
    return true;
  }

protected:
  String _uri;
  Plugin* _plugin;
  int8_t _sensor;
};


String getIP()
{
  IPAddress ip = (WiFi.getMode() == WIFI_STA) ? WiFi.localIP() : WiFi.softAPIP();
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void requestRestart()
{
  g_restartTime = millis() + 100;
}

bool getUrlParam(String arg, String *value)
{
  *value = "";
  for (uint8_t i = 0; i < g_server.args(); i++) {
    if (g_server.argName(i) == arg) {
      *value = urldecode(g_server.arg(i));
      value->trim();
      return true;
    }
  }
  return false;
}

void jsonResponse(JsonVariant json, int res)
{
  char buffer[512];
  json.printTo(buffer, sizeof(buffer));
  g_server.send(res, "application/json", buffer);
}

/**
 * Handle http root request
 */
void handleRoot()
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", g_server.uri().c_str(), g_server.args());
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
    indexHTML.replace("[middleware]", g_middleware);
    indexHTML.replace("[wifimode]", (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point");
    indexHTML.replace("[ip]", getIP());
  }

  g_server.send(200, "text/html", indexHTML);
}

/**
 * Handle set request from http server.
 */
void handleSettings()
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", g_server.uri().c_str(), g_server.args());
  String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
  resp += net_hostname;
  resp += F(".local/\"></head><body>");
    
  // Check arguments
  if (g_server.args() != 3) {
    g_server.send(400, F("text/plain"), F("Bad request\n\n"));
    return;
  }

  String arg;
  String ssid = "";
  String pass = "";
  String middleware = "";
  int result = 400;

  // read ssid and psk
  for (uint8_t i = 0; i < g_server.args(); i++) {
    arg = urldecode(g_server.arg(i));
    arg.trim();
    
    if (g_server.argName(i) == "ssid") {
      ssid = arg;
    }
    else if (g_server.argName(i) == "pass") {
      pass = arg;
    }
    else if (g_server.argName(i) == "middleware") {
      middleware = arg;
    }
  }

  // check ssid and psk
  if (ssid != "") {
    // save ssid and psk to file
    if (saveConfig(&ssid, &pass, &middleware)) {
      // store SSID and PSK into global variables.
      g_ssid = ssid;
      g_pass = pass;
      g_middleware = middleware;

      resp += F("Success.");
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
    requestRestart(); 
  }
}

/**
 * Status JSON api
 */
void handleGetStatus()
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", g_server.uri().c_str(), g_server.args());
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
  jsonResponse(json, 200);
}

/**
 * Wlan scan JSON api
 */
void handleGetWlan()
{
/*
  debug();
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

  jsonResponse(json, 200);
*/
}

/**
 * Get plugin information
 */
void handleGetPlugins()
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", g_server.uri().c_str(), g_server.args());
  StaticJsonBuffer<512> jsonBuffer;
  JsonArray& json = jsonBuffer.createArray();

  for (int8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    JsonObject& obj = json.createNestedObject();
    Plugin* plugin = Plugin::get(pluginIndex);
    obj["name"] = plugin->getName();
    plugin->getPluginJson(&obj);
  }
  
  jsonResponse(json, 200);
}

/**
 * Handle file not found from http server.
 */
void handleNotFound()
{
  DEBUG_SERVER("[webserver] file not found %s\n", g_server.uri().c_str());
  g_server.send(404, F("text/plain"), F("File not found"));
}

void serveStaticDir(String path)
{
  Dir dir = SPIFFS.openDir(path);
  while (dir.next()) {
/*
    File file = dir.openFile("r");
    DEBUG_SERVER("[webserver] registering static file: %s\n", file.name().c_str());
    g_server.serveStatic(file.name(), SPIFFS, file.name(), CACHE_HEADER);
    file.close();
*/
    DEBUG_SERVER("[webserver] registering static file: %s\n", dir.fileName().c_str());
    g_server.serveStatic(dir.fileName().c_str(), SPIFFS, dir.fileName().c_str(), CACHE_HEADER);
  }
}

/** 
 * Setup handlers for each plugin and sensor 
 * Structure is /api/<plugin>/<sensor>
 */
void registerPlugins()
{
  for (uint8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    Plugin* plugin = Plugin::get(pluginIndex);
    DEBUG_SERVER("[webserver] registering plugin: %s ", plugin->getName().c_str());
    
    String baseUri = "/api/";
    baseUri += plugin->getName();
    baseUri += "/";

    // register one handler per sensor
    for  (int8_t sensor=0; sensor<plugin->getSensors(); sensor++) {
      String uri = String(baseUri);
      char addr_c[20];
      plugin->getAddr(addr_c, sensor);
      uri += addr_c;
      DEBUG_SERVER("%s\n", uri.c_str());

      g_server.addHandler(new PluginRequestHandler(uri.c_str(), plugin, sensor));
    }
  }
}

/**
 * Initialize web server and add requests
 */
void webserver_start()
{
  g_server.on("/", handleRoot);
  g_server.on("/index.html", handleRoot);

  // static
  g_server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico", CACHE_HEADER);
  serveStaticDir("/img");
  serveStaticDir("/js");
  serveStaticDir("/css");

  // not found
  g_server.onNotFound(handleNotFound);

  // GET
  g_server.on("/settings", HTTP_GET, handleSettings);

  // POST
  g_server.on("/restart", HTTP_POST, []() {
    String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
    resp += net_hostname;
    resp += F(".local/\"></head><body>Restarting...<br/><img src=\"/img/loading.gif\"></body></html>");
    g_server.send(200, "text/html", resp);
    requestRestart();
  });

  // general api
  g_server.on("/api/status", HTTP_GET, handleGetStatus);
  g_server.on("/api/wlan", HTTP_GET, handleGetWlan);
  g_server.on("/api/plugins", HTTP_GET, handleGetPlugins);

  // sensor api
  registerPlugins();

  // start server
  g_server.begin();
}
