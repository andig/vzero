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
  DEBUG_HEAP;
  json.printTo(buffer, sizeof(buffer));
  DEBUG_HEAP;
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
 * Handle http root request
 */
void handleRoot(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());
  String indexHTML = F("<!DOCTYPE html><html><head><title>File not found</title></head><body><h1>File not found.</h1></body></html>");
  DEBUG_HEAP;

  File indexFile = SPIFFS.open(F("/index.html"), "r");
  if (indexFile) {
    DEBUG_SERVER("[webserver] file read, size: %d\n", indexFile.size());
    delay(100);
    indexHTML = indexFile.readString();
/*
    char *buf = (char *)malloc(indexFile.size()+1);
    DEBUG_HEAP;delay(100);
    indexFile.read((uint8_t *)buf, indexFile.size());
    buf[indexFile.size()] = '\0';
    indexHTML = String(buf);
    DEBUG_HEAP;delay(100);
    free(buf);
    DEBUG_HEAP;delay(100);
*/
    DEBUG_SERVER("[webserver] file close\n");
    delay(100);
    indexFile.close();

    DEBUG_HEAP;
    DEBUG_SERVER("[webserver] content length: %d\n", indexHTML.length());
  }
/*
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
    DEBUG_HEAP;
  }
*/
  request->send(200, "text/html", indexHTML);
}

/**
 * Handle set request from http server.
 */
void handleSettings(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());
  DEBUG_HEAP;

  String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
  resp += net_hostname;
  resp += F(".local/\"></head><body>");
    
  // Check arguments
  if (request->args() != 3) {
    request->send(400, F("text/plain"), F("Bad request\n\n"));
    return;
  }

  String arg;
  String ssid = "";
  String pass = "";
  String middleware = "";
  int result = 400;

  // read ssid and psk
  if (request->params() == 3) {
    if (request->hasParam("ssid")) {
      ssid = request->getParam("ssid")->value();
    }
    if (request->hasParam("pass")) {
      pass = request->getParam("pass")->value();
    }
    if (request->hasParam("middleware")) {
      middleware = request->getParam("middleware")->value();
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
  request->send(result, "text/html", resp);

  if (result == 200) {
    requestRestart(); 
  }
}

/**
 * Status JSON api
 */
void handleGetStatus(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] uri: %s args: %d\n", request->url().c_str(), request->params());
  DEBUG_HEAP;

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["uptime"] = millis();
  json["wifimode"] = (WiFi.getMode() == WIFI_STA) ? "Connected" : "Access Point";
  json["ip"] = getIP();
  json["flash"] = ESP.getFlashChipRealSize();

  json["heap"] = ESP.getFreeHeap();
  json["minheap"] = g_minFreeHeap;

  // [/*0*/ "DEFAULT", /*1*/ "WDT", /*2*/ "EXCEPTION", /*3*/ "SOFT_WDT", /*4*/ "SOFT_RESTART", /*5*/ "DEEP_SLEEP_AWAKE", /*6*/ "EXT_SYS_RST"]
  rst_info* resetInfo = ESP.getResetInfoPtr();
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
  DEBUG_HEAP;

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

/**
 * Handle file not found from http server.
 */
void handleNotFound(AsyncWebServerRequest *request)
{
  DEBUG_SERVER("[webserver] file not found %s\n", request->url().c_str());
  DEBUG_HEAP;

  request->send(404, F("text/plain"), F("File not found"));
}

void serveStaticDir(String path)
{
  Dir dir = SPIFFS.openDir(path);
  while (dir.next()) {
/*
    File file = dir.openFile("r");
    DEBUG_SERVER("[webserver] registering static file: %s\n", file.name().c_str());
    DEBUG_HEAP;

    g_server.serveStatic(file.name(), SPIFFS, file.name(), CACHE_HEADER);
    file.close();
*/
    DEBUG_SERVER("[webserver] registering static file: %s\n", dir.fileName().c_str());
    DEBUG_HEAP;

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
      DEBUG_HEAP;

      g_server.addHandler(new PluginRequestHandler(uri.c_str(), plugin, sensor));
    }
  }
}

/**
 * Initialize web server and add requests
 */
void webserver_start()
{
  g_server.on("/", (ArRequestHandlerFunction) handleRoot);
  g_server.on("/index.html", (ArRequestHandlerFunction) handleRoot);

  // static
  g_server.serveStatic("/favicon.ico", SPIFFS, "/favicon.ico", CACHE_HEADER);
  serveStaticDir("/img");
  serveStaticDir("/js");
  serveStaticDir("/css");

  // not found
  g_server.onNotFound((ArRequestHandlerFunction) handleNotFound);
/*
  // GET
  g_server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleSettings(request);
  });
  
  // POST
  g_server.on("/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    String resp = F("<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"3; URL=http://");
    resp += net_hostname;
    resp += F(".local/\"></head><body>Restarting...<br/><img src=\"/img/loading.gif\"></body></html>");
    request->send(200, "text/html", resp);
    requestRestart();
  });

  // general api
  g_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleGetStatus(request);
  });
  g_server.on("/api/plugins", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleGetPlugins(request);
  });

  // sensor api
  registerPlugins();
*/
  // SPIFFS editor for debugging
  // g_server.addHandler(new SPIFFSEditor("", ""));

  // start server
  g_server.begin();
}
