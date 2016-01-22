/**
 * Web server
 */

#include "config.h"

#ifdef DEBUG
#define DEBUG_SERVER(...) os_printf( __VA_ARGS__ ); if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); os_printf("[core] heap: %d\n", g_minFreeHeap); }
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

