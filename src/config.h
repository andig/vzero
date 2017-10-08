/**
 * Config file
 */

#include <WString.h>
#include <MD5Builder.h>

#ifdef ESP8266
extern "C" {
  #include <user_interface.h>
  #include <umm_malloc/umm_malloc.h>
}
#endif

/*
 * Configuration
 */
#define DEBUG

#ifdef DEBUG
void debug_plain(const char *msg);
void debug_message(const char *module, const char *format, ...);

#define DEBUG_PLAIN(msg) debug_plain(msg)
#define DEBUG_MSG(module, format, ...) debug_message(module, format, ##__VA_ARGS__ )
#else
#define DEBUG_PLAIN(msg)
#define DEBUG_MSG(...) if (ESP.getFreeHeap() < g_minFreeHeap) { g_minFreeHeap = ESP.getFreeHeap(); }
#endif

#ifdef ESP8266
#define PANIC(...) panic()
#endif
#ifdef ESP32
#define PANIC(...) abort()
#endif

/*
 * Plugins
 */
#define OTA_SERVER
#define CAPTIVE_PORTAL
#define PLUGIN_ONEWIRE
#define PLUGIN_DHT
#define PLUGIN_ANALOG
#define PLUGIN_WIFI

// #define SPIFFS_EDITOR

// settings
#define ONEWIRE_PIN 14
#define DHT_PIN 14
#define DHT_TYPE DHT11

/*
 * Sleep mode
 */
// #define DEEP_SLEEP
// wakeup 5 seconds earlier
#define SLEEP_SAFETY_MARGIN 1 * 1000
// minimum deep sleep duration (must be bigger than SLEEP_SAFETY_MARGIN)
#define MIN_SLEEP_DURATION_MS 20 * 1000
// duration after boot during which no deep sleep can happen
#define STARTUP_ONLINE_DURATION_MS 120 * 1000
// client disconnect timeout
#define WIFI_CLIENT_TIMEOUT 120 * 1000

// memory management
#define HTTP_MIN_HEAP 4096

// other defines
#define CORE "core"		// module name
#define BUILD "0.4.0"   // version
#define WIFI_CONNECT_TIMEOUT 10000
#define OPTIMISTIC_YIELD_TIME 10000

// ESP32 specifics
#ifdef ESP32
#define REASON_DEEP_SLEEP_AWAKE 5
#endif

/*
 * Variables
 */

// default WiFi connection information.
extern const char* ap_default_ssid; // default SSID
extern const char* ap_default_psk;  // default PSK

// global vars
#ifdef ESP8266
extern rst_info* g_resetInfo;
#endif
extern String net_hostname;
extern uint32_t g_minFreeHeap;

// global settings
extern String g_ssid;
extern String g_pass;
extern String g_middleware;


/*
 * Functions
 */

#ifdef DEBUG
void validateFlash();
#endif

void startPlugins();

long getChipId();

MD5Builder getHashBuilder();
String getHash();

bool loadConfig();
bool saveConfig();

int getResetReason(int core);
const char* getResetReasonStr(int core);
