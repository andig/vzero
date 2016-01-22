/**
 * Config file
 */

#include <WString.h>

/*
 * Defines
 */
#define DEBUG
#define OTA_SERVER

// included plugins
#define PLUGIN_ONEWIRE
#define PLUGIN_ANALOG
#define PLUGIN_WIFI

// plugin settings
#define ONEWIRE_PIN 14

#define BUILD "0.1"   // version

#ifdef DEBUG
extern uint16_t g_minFreeHeap;
#define DEBUG_HEAP if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); Serial.printf("[core] heap: %d\n", g_minFreeHeap); }
#define DEBUG_CORE(...) Serial.printf( __VA_ARGS__ )
#else
#define DEBUG_HEAP if (1==1) {}
#define DEBUG_CORE(...)
#endif


// default WiFi connection information.
extern const char* ap_default_ssid; // default SSID
extern const char* ap_default_psk;  // default PSK

extern String net_hostname;

// global settings
extern String g_ssid;
extern String g_pass;
extern String g_middleware;

void validateFlash();
bool loadConfig();
bool saveConfig();

