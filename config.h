/**
 * Config file
 */

#include <WString.h>
#include <MD5Builder.h>

/*
 * Configuration
 */
#define DEBUG
#define OTA_SERVER
// #define PLUGIN_ONEWIRE
#define PLUGIN_DHT
#define PLUGIN_ANALOG
#define PLUGIN_WIFI
#define SPIFFS_EDITOR

// enable deep sleep
// #define DEEP_SLEEP
// wakeup 5 seconds earlier
#define SLEEP_SAFETY_MARGIN 1 * 1000
// minimum deep sleep duration (must be bigger than SLEEP_SAFETY_MARGIN)
#define MIN_SLEEP_DURATION_MS 20 * 1000
// duration after boot during which no deep sleep can happen
#define STARTUP_ONLINE_DURATION_MS 120 * 1000
// client disconnect timeout
#define WIFI_CLIENT_TIMEOUT 120 * 1000

// plugin settings
#define ONEWIRE_PIN 14
#define DHT_PIN 14
#define DHT_TYPE DHT11

// other defines
#define BUILD "0.3.0"   // version
#define WIFI_CONNECT_TIMEOUT 10000
#define OPTIMISTIC_YIELD_TIME 10000


#ifdef DEBUG
extern uint32_t g_minFreeHeap;
#define DEBUG_HEAP() if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); Serial.printf("[core] heap/min: %d\n", g_minFreeHeap); }
#define DEBUG_CORE(...) Serial.printf( __VA_ARGS__ )
#else
#define DEBUG_HEAP() if (ESP.getFreeHeap() < g_minFreeHeap) g_minFreeHeap = ESP.getFreeHeap()
#define DEBUG_CORE(...)
#endif


// default WiFi connection information.
extern const char* ap_default_ssid; // default SSID
extern const char* ap_default_psk;  // default PSK

// global vars
extern String net_hostname;
extern rst_info* g_resetInfo;

// global settings
extern String g_ssid;
extern String g_pass;
extern String g_middleware;


#ifdef DEBUG
void validateFlash();
#endif

MD5Builder getHashBuilder();
String getHash();

bool loadConfig();
bool saveConfig();

