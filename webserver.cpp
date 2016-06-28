/**
 * Web server
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <FileFallbackHandler.h>
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

String getIP()
{
  IPAddress ip = (WiFi.getMode() == WIFI_STA) ? WiFi.localIP() : WiFi.softAPIP();
  return ip.toString();
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
  if (request->hasParam("ssid") && request->hasParam("pass")) {
    ssid = request->getParam("ssid")->value();
    pass = request->getParam("pass")->value();
    if (ssid != "") {
      g_ssid = ssid;
      g_pass = pass;
      result = 200;
    }
  }
  if (request->hasParam("middleware")) {
    g_middleware = request->getParam("middleware")->value();
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
    json[F("wifimode")] = (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point";
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

void serveStaticDir(String path)
{
  DEBUG_SERVER("[webserver] static dir: %s\n", path.c_str());
  Dir dir = SPIFFS.openDir(path);
  while (dir.next()) {
    String file = dir.fileName();
    if (file.endsWith(".gz")) {
      file.remove(file.length()-3);
    }
    // DEBUG_SERVER("[webserver] static file: %s\n", file.c_str());
    g_server.serveStatic(file.c_str(), SPIFFS, file.c_str(), CACHE_HEADER);
  }
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

/**
 * Initialize web server and add requests
 */
void webserver_start()
{
  // not found
  g_server.onNotFound([](AsyncWebServerRequest *request) {
    DEBUG_SERVER("[webserver] file not found %s\n", request->url().c_str());
    request->send(404, F("text/plain"), F("File not found"));
  });

  // CDN
  g_server.addHandler(new FileFallbackHandler(SPIFFS, "/js/jquery-2.1.4.min.js", "/js/jquery-2.1.4.min.js", "http://code.jquery.com/jquery-2.1.4.min.js", CACHE_HEADER));
  g_server.addHandler(new FileFallbackHandler(SPIFFS, "/css/foundation.min.css", "/css/foundation.min.css", "http://cdnjs.cloudflare.com/ajax/libs/foundation/6.2.3/foundation.min.css", CACHE_HEADER));

  // static
  g_server.serveStatic("//", SPIFFS, "/index.html", CACHE_HEADER);
  g_server.serveStatic("/index.html", SPIFFS, "/index.html", CACHE_HEADER);
  serveStaticDir(F("/icons"));
  serveStaticDir(F("/img"));
  serveStaticDir(F("/js"));
  serveStaticDir(F("/css"));

  // GET
  g_server.on("/settings", HTTP_GET, handleSettings);
  g_server.on("/api/status", HTTP_GET, handleGetStatus);
  g_server.on("/api/plugins", HTTP_GET, handleGetPlugins);

  // POST
  g_server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
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
