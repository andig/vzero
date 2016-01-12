/**
 * Config file
 */

#include <WString.h>
#include <FS.h>
#include <ArduinoJson.h>

#include "config.h"

// default AP SSID
const char* ap_default_ssid = "VZERO";

// hostname prefix
String net_hostname = "vzero";

// global WiFi SSID
String g_ssid = "";

// global WiFi PSK
String g_pass = "";

// hostname prefix
String middleware = "http://demo.volkszaehler.org/middleware.php";


/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 */
bool loadConfig(String *ssid, String *pass)
{
  File configFile = SPIFFS.open("/config.json", "r");
  size_t size = configFile.size();

  // Allocate a buffer to store contents of the file
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());
  *ssid = json["ssid"].asString();
  *pass = json["password"].asString();

  Serial.println(F("-- Stored WiFi config --"));
  Serial.print("SSID:   ");
  Serial.println(*ssid);
  Serial.print("PSK:    ");
  Serial.println(*pass);
  
  return true;
}

/**
 * @brief Save WiFi SSID and PSK to configuration file.
 * @param ssid SSID as string pointer.
 * @param pass PSK as string pointer,
 * @return True or False.
 */
bool saveConfig(String *ssid, String *pass)
{
  // Open config file for writing
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing"));
    return false;
  }

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["ssid"] = *ssid;
  json["password"] = *pass;

  json.printTo(configFile);
  configFile.close();
  
  return true;
}

