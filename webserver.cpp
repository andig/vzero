/**
 * Web server
 */

#include <ESP8266WiFi.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "webserver.h"
#include "urlfunctions.h"
#include "plugins/Plugin.h"
#include "SPIFFSEditor.h"

// required for rst_info
extern "C" {
#include <user_interface.h>
}


#define CACHE_HEADER "max-age=86400"

AsyncWebServer g_server(80);

unsigned long g_restartTime = 0;


void jsonResponse(AsyncWebServerRequest *request, int res, JsonVariant json)
{
  char buffer[512];
  json.printTo(buffer, sizeof(buffer));
  
  request->send(res, "application/json", buffer);
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
    request->addInterestingHeader("ANY");
    return true;
  }
  
  void handleRequest(AsyncWebServerRequest *request) {
    StaticJsonBuffer<200> jsonBuffer;
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
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

void requestRestart()
{
  g_restartTime = millis() + 100;
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
  request->send(result, "text/html", resp);
}

/**
 * Status JSON api
 */
void handleGetStatus(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  rst_info* resetInfo = ESP.getResetInfoPtr();

  if (request->hasParam("initial")) {
    char buf[8];
    sprintf(buf, "%06x", ESP.getChipId());
    json["id"] = buf;
    json["build"] = BUILD;
    json["ssid"] = g_ssid;
    json["pass"] = g_pass;
    json["middleware"] = g_middleware;
    json["flash"] = ESP.getFlashChipRealSize();
    json["wifimode"] = (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point";
    json["ip"] = getIP();
  }

  json["uptime"] = millis();
  json["heap"] = ESP.getFreeHeap();
  json["minheap"] = g_minFreeHeap;
  json["resetcode"] = resetInfo->reason;
  // json["gpio"] = (uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16));
  
  jsonResponse(request, 200, json);
}

/**
 * Get plugin information
 */
void handleGetPlugins(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());
  
  StaticJsonBuffer<512> jsonBuffer;
  JsonArray& json = jsonBuffer.createArray();

  for (int8_t pluginIndex=0; pluginIndex<Plugin::count(); pluginIndex++) {
    JsonObject& obj = json.createNestedObject();
    Plugin* plugin = Plugin::get(pluginIndex);
    obj["name"] = plugin->getName();
    plugin->getPluginJson(&obj);
  }
  
  jsonResponse(request, 200, json);
}

void serveStaticDir(String path)
{
  Dir dir = SPIFFS.openDir(path);
  while (dir.next()) {
    DEBUG_SERVER("[webserver] static file: %s\n", dir.fileName().c_str());
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

    // register one handler per sensor
    String baseUri = "/api/" + plugin->getName() + "/";
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
  // SPIFFS editor for debugging
  g_server.addHandler(new SPIFFSEditor("", ""));

  // not found
  g_server.onNotFound([](AsyncWebServerRequest *request) {
    DEBUG_SERVER("[webserver] file not found %s\n", request->url().c_str());
    request->send(404, F("text/plain"), F("File not found"));
  });

  // static
  g_server.serveStatic("/", SPIFFS, "/index.html", CACHE_HEADER);
  g_server.serveStatic("/index.html", SPIFFS, "/index.html", CACHE_HEADER);
  g_server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico", CACHE_HEADER);
  serveStaticDir("/img");
  serveStaticDir("/js");
  serveStaticDir("/css");

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

  // start server
  g_server.begin();
}
