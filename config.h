/**
 * Config file
 */

#include <WString.h>

/**
 * Defines
 */
#define OTA_SERVER
#define DEBUG

// included plugins
#define PLUGIN_ONEWIRE

// plugin settings
#define ONEWIRE_PIN 2

#define BUILD "0.1"   // version


/**
 * @brief Default WiFi connection information.
 */
extern const char* ap_default_ssid; // Default SSID
extern const char* ap_default_psk;  // Default PSK

extern String net_hostname;

// global WiFi SSID
extern String g_ssid;

// global WiFi PSK
extern String g_pass;

bool loadConfig(String *ssid, String *pass);
bool saveConfig(String *ssid, String *pass);
