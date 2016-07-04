/**
 * Web server
 */

#include "config.h"

#define SERVER "websrv"

#ifdef BROWSER_EVENTS
void browser_event(const char *event, const char *format, ...);
#endif


// timestamp to trigger restart on
extern uint32_t g_restartTime;
// timestamp of last client access
extern uint32_t g_lastAccessTime;

/**
 * Start web server
 */
void webserver_start();
