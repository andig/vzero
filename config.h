/**
 * Config file
 */

#include <WString.h>
#include <MD5Builder.h>

/*
 * Defines
 */
#define DEBUG
#define OTA_SERVER
//#define SPIFFS_EDITOR

// enable deep sleep
// #define DEEP_SLEEP
// wakeup 5 seconds earlier
#define SLEEP_SAFETY_MARGIN 1 * 1000
// minimum deep sleep duration (must be bigger than SLEEP_SAFETY_MARGIN)
#define MIN_SLEEP_DURATION_MS 20 * 1000
// duration after boot during which no deep sleep can happen
#define STARTUP_ONLINE_DURATION_MS 120 * 1000

// included plugins
#define PLUGIN_ONEWIRE
#define PLUGIN_ANALOG
#define PLUGIN_WIFI

// plugin settings
#define ONEWIRE_PIN 14

#define BUILD "0.2a"   // version

#ifdef DEBUG
extern uint16_t g_minFreeHeap;
#define DEBUG_HEAP if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); Serial.printf("[core] heap: %d\n", g_minFreeHeap); }
#define DEBUG_CORE(...) Serial.printf( __VA_ARGS__ )
#else
#define DEBUG_HEAP if (ESP.getFreeHeap() < g_minFreeHeap) g_minFreeHeap = ESP.getFreeHeap()
#define DEBUG_CORE(...)
#endif


// default WiFi connection information.
extern const char* ap_default_ssid; // default SSID
extern const char* ap_default_psk;  // default PSK

// global vars
extern rst_info* g_resetInfo;
extern String net_hostname;

// global settings
extern String g_ssid;
extern String g_pass;
extern String g_middleware;


void validateFlash();

MD5Builder getHashBuilder();
String getHash();

bool loadConfig();
bool saveConfig();

