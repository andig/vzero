/**
 * Web server
 */

#include "config.h"

#ifdef DEBUG
#define DEBUG_SERVER(...) Serial.printf( __VA_ARGS__ )
#else
#define DEBUG_SERVER(...)
#endif


// Restart will be triggert on this time
extern unsigned long g_restartTime;


/**
 * Read WiFi connection information from file system.
 */
bool loadConfig(String *ssid, String *pass);

/**
 * Start web server
 */
void webserver_start();

