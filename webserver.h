/**
 * Web server
 */

#include <ESP8266WebServer.h>

// Webserver handle on port 80
extern ESP8266WebServer g_server;

// Restart will be triggert on this time
extern unsigned long g_restartTime;

/**
 * @brief Read WiFi connection information from file system.
 * @param ssid String pointer for storing SSID.
 * @param pass String pointer for storing PSK.
 * @return True or False.
 */
bool loadConfig(String *ssid, String *pass);

/**
 * @brief Handle http root request.
 */
void handleRoot();

/**
 * @brief Handle set request from http server.
 * URI: /set?ssid=[WiFi SSID],pass=[WiFi Pass]
 */
void handleSet();

/**
 * @brief Handle file not found from http server.
 */
void handleNotFound();

void webserver_start();

