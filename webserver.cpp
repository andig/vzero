/**
 * Web server
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>

#include "config.h"
#include "webserver.h"
#include "urlfunctions.h"
#include "plugins/Plugin.h"

#ifdef SPIFFS_EDITOR
#include "SPIFFSEditor.h"
#endif

// required for rst_info
extern "C" {
#include <user_interface.h>
}


#define CACHE_HEADER "max-age=86400"


uint32_t g_restartTime = 0;
uint32_t g_lastAccessTime = 0;

AsyncWebServer g_server(80);


void requestRestart()
{
  g_restartTime = millis() + 100;
}

void jsonResponse(AsyncWebServerRequest *request, int res, JsonVariant json)
{
  // touch
  g_lastAccessTime = millis();

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  json.printTo(*response);
  request->send(response);
}

String getIP()
{
  IPAddress ip = (WiFi.getMode() & WIFI_STA) ? WiFi.localIP() : WiFi.softAPIP();
  return ip.toString();
}

#ifdef CAPTIVE_PORTAL
class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {
  }

  bool canHandle(AsyncWebServerRequest *request){
    // redirect if not in wifi client mode (through filter)  
    // and request for different host (due to DNS * response)
    if (request->host() != WiFi.softAPIP().toString()) 
      return true;
    else
      return false;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    DEBUG_SERVER("[webserver] captive request to %s\n", request->url().c_str());
    String location = "http://" + WiFi.softAPIP().toString();
    if (request->host() == net_hostname + ".local")
      location += request->url();
    request->redirect(location);
  }
};
#endif

class PluginRequestHandler : public AsyncWebHandler {
public:
  PluginRequestHandler(const char* uri, Plugin* plugin, const int8_t sensor) : _uri(uri), _plugin(plugin), _sensor(sensor) {
  }

  bool canHandle(AsyncWebServerRequest *request){
    if(request->method() != HTTP_GET && request->method() != HTTP_POST)
      return false;
    if(!request->url().startsWith(_uri))
      return false;
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    int res = 400; // JSON error

    // GET
    if (request->method() == HTTP_GET && request->params() == 0) {
      float val = _plugin->getValue(_sensor);
      if (val != NAN) {
        json["value"] = val;
        res = 200;
      }
    }
    // POST
    else if ((request->method() == HTTP_POST || request->method() == HTTP_GET)
      && request->params() == 1 && request->hasParam("uuid")) {
      String uuid = request->getParam(0)->value();
      if (_plugin->setUuid(uuid.c_str(), _sensor)) {
        _plugin->getSensorJson(&json, _sensor);
        res = 200;
      }
    }

    jsonResponse(request, res, json);
  }
protected:
  String _uri;
  Plugin* _plugin;
  int8_t _sensor;
};

/**
 * Handle set request from http server.
 */
void handleNotFound(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] file not found %s\n", request->url().c_str());
  request->send(404, F("text/plain"), F("File not found"));
}

/**
 * Handle set request from http server.
 */
void handleSettings(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());

  String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"5; url=/\"></head><body>");
  String ssid = "";
  String pass = "";
  int result = 400;

  // read ssid and psk
  if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
    ssid = request->getParam("ssid", true)->value();
    pass = request->getParam("pass", true)->value();
    if (ssid != "") {
      g_ssid = ssid;
      g_pass = pass;
      result = 200;
    }
  }
  if (request->hasParam("middleware", true)) {
    g_middleware = request->getParam("middleware", true)->value();
    result = 200;
  }
  if (result == 400) {
    request->send(result, F("text/plain"), F("Bad request\n\n"));
    return;
  }

  if (saveConfig()) {
    resp += F("<h1>Settings saved.</h1>");
  }
  else {
    resp += F("<h1>Failed to save config file.</h1>");
    result = 400;
  }
  resp += F("</body></html>");
  if (result == 200) {
    requestRestart();
  }
  request->send(result, F("text/html"), resp);
}

/**
 * Status JSON api
 */
void handleGetStatus(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());

  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  if (request->hasParam("initial")) {
    char buf[8];
    sprintf(buf, "%06x", ESP.getChipId());
    json[F("serial")] = buf;
    json[F("build")] = BUILD;
    json[F("ssid")] = g_ssid;
    // json[F("pass")] = g_pass;
    json[F("middleware")] = g_middleware;
    json[F("flash")] = ESP.getFlashChipRealSize();
    json[F("wifimode")] = (WiFi.getMode() & WIFI_STA) ? "Connected" : "Access Point";
    json[F("ip")] = getIP();
  }

  json[F("uptime")] = millis();
  json[F("heap")] = ESP.getFreeHeap();
  json[F("minheap")] = g_minFreeHeap;
  json[F("resetcode")] = g_resetInfo->reason;
  // json[F("gpio")] = (uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16));

  jsonResponse(request, 200, json);
}

/**
 * Get plugin information
 */
void handleGetPlugins(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());

  DynamicJsonBuffer jsonBuffer;
  JsonArray& json = jsonBuffer.createArray();

  Plugin::each([&json](Plugin* plugin) {
    JsonObject& obj = json.createNestedObject();
    obj[F("name")] = plugin->getName();
    plugin->getPluginJson(&obj);
  });

  jsonResponse(request, 200, json);
}

/**
 * Setup handlers for each plugin and sensor
 * Structure is /api/<plugin>/<sensor>
 */
void registerPlugins()
{
  Plugin::each([](Plugin* plugin) {
    DEBUG_SERVER("[webserver] plugin: %s\n", plugin->getName().c_str());

    // register one handler per sensor
    String baseUri = "/api/" + plugin->getName() + "/";
    for (int8_t sensor=0; sensor<plugin->getSensors(); sensor++) {
      String uri = String(baseUri);
      char addr_c[20];
      plugin->getAddr(addr_c, sensor);
      uri += addr_c;
      DEBUG_SERVER("[webserver] sensor: %s\n", uri.c_str());

      g_server.addHandler(new PluginRequestHandler(uri.c_str(), plugin, sensor));
    }
  });
}

void handleWifiScan(AsyncWebServerRequest *request)
{
  String json = "[";
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED){
    WiFi.scanNetworks(true);
  } 
  else if (n > 0) { // scan finished
    for (int i = 0; i < n; ++i) {
      if (i) json += ",";
      json += "{";
      json += "\"rssi\":" + String(WiFi.RSSI(i));
      json += ",\"ssid\":\"" + WiFi.SSID(i) + "\"";
      // json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
      // json += ",\"channel\":"+String(WiFi.channel(i));
      json += ",\"secure\":" + String(WiFi.encryptionType(i));
      json += ",\"hidden\":" + String(WiFi.isHidden(i) ? "true" : "false");
      json += "}";
    }
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
  }
  json += "]";

  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
 * Initialize web server and add requests
 */
void webserver_start()
{
  // not found
  g_server.onNotFound(handleNotFound);

#ifdef CAPTIVE_PORTAL
  // handle captive requests
  g_server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
#endif

  // static
  g_server.serveStatic("//", SPIFFS, "/index.html", CACHE_HEADER);
  g_server.serveStatic("/index.html", SPIFFS, "/index.html", CACHE_HEADER);

  // CDN
  g_server.on("/js/jquery-2.1.4.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://code.jquery.com/jquery-2.1.4.min.js");
  }).setFilter(ON_STA_FILTER);
  g_server.on("/css/foundation.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://cdnjs.cloudflare.com/ajax/libs/foundation/6.2.3/foundation.min.css");
  }).setFilter(ON_STA_FILTER);

  // static, after CDN exceptions
  g_server.serveStatic("/icons", SPIFFS, "/icons", CACHE_HEADER);
  g_server.serveStatic("/img", SPIFFS, "/img", CACHE_HEADER);
  g_server.serveStatic("/js", SPIFFS, "/js", CACHE_HEADER);
  g_server.serveStatic("/css", SPIFFS, "/css", CACHE_HEADER);

  // GET
  g_server.on("/api/status", HTTP_GET, handleGetStatus);
  g_server.on("/api/plugins", HTTP_GET, handleGetPlugins);
  g_server.on("/scan", HTTP_GET, handleWifiScan);

  // POST
  g_server.on("/settings", HTTP_POST, handleSettings);
  g_server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    // AsyncWebServerResponse *response = request->beginResponse(302);
    // response->addHeader("Location", net_hostname + ".local");
    // request->send(response);

    request->send(200, F("text/html"), F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"5; url=/\"></head><body>Restarting...<br/><img src=\"/img/loading.gif\"></body></html>"));
    requestRestart();
  });

  // sensor api
  registerPlugins();

#ifdef SPIFFS_EDITOR
  g_server.addHandler(new SPIFFSEditor());
#endif

  // start server
  g_server.begin();
}
