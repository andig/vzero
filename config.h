/**
 * Config file
 */

#include <WString.h>

/*
 * Defines
 */
#define DEBUG_CORE(...) Serial.printf( __VA_ARGS__ )

#define OTA_SERVER
#define DEBUG

// global settings
#define MIDDLEWARE "http://demo.volkszaehler.org/middleware.php"

// included plugins
#define PLUGIN_ONEWIRE
#define PLUGIN_ANALOG
#define PLUGIN_WIFI

// plugin settings
#define ONEWIRE_PIN 14

#define BUILD "0.1"   // version


/**
 * @brief Default WiFi connection information.
 */
extern const char* ap_default_ssid; // Default SSID
extern const char* ap_default_psk;  // Default PSK

extern String net_hostname;

// global settings
extern String g_ssid;
extern String g_pass;
extern String g_middleware;

bool loadConfig(String *ssid, String *pass, String *middleware);
bool saveConfig(String *ssid, String *pass, String *middleware);
