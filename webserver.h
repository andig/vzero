/**
 * Web server
 */

#include "config.h"

#ifdef DEBUG
#define DEBUG_SERVER(...) ets_printf( __VA_ARGS__ ); if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); ets_printf("[core] heap min: %d\n", g_minFreeHeap); }
#else
#define DEBUG_SERVER(...) if (ESP.getFreeHeap() < g_minFreeHeap) g_minFreeHeap = ESP.getFreeHeap()
#endif


// timestamp to trigger restart on
extern uint32_t g_restartTime;
// timestamp of last client access
extern uint32_t g_lastAccessTime;

/**
 * Start web server
 */
void webserver_start();
