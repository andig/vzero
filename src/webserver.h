/**
 * Web server
 */

#include "config.h"

#define SERVER "websrv"


// timestamp to trigger restart on
extern uint32_t g_restartTime;
// timestamp of last client access
extern uint32_t g_lastAccessTime;

/**
 * Start web server
 */
void webserver_start();
