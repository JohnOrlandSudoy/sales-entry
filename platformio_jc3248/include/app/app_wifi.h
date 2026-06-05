#ifndef APP_WIFI_H
#define APP_WIFI_H

#include "app_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Call once from setup() so Settings/WiFi UI never see "WiFi init failed". */
void app_wifi_boot_init(void);

/** Call when opening WiFi screen — retries init if needed. */
void app_wifi_prepare_ui(void);

/** Load saved credentials from app settings. */
void app_wifi_get_saved(char * ssid, size_t ssid_sz, char * pass, size_t pass_sz);

/** Persist SSID/password to settings.cfg (via app settings). */
bool app_wifi_save_credentials(const char * ssid, const char * pass);

/**
 * Scan for 2.4 GHz networks (ESP: real scan; PC: demo list).
 * @return number of networks written to @p out (0..max).
 */
int app_wifi_scan(app_wifi_network_t * out, int max);

/** Connect to network. PC: simulated success after save. */
bool app_wifi_connect(const char * ssid, const char * pass, char * err, size_t err_sz);

/** Clear saved SSID/password and disconnect current station if any. */
bool app_wifi_forget(char * err, size_t err_sz);

bool app_wifi_is_connected(void);

/** Resolve hostname via lwIP (esp-netif). Use IP for SMTP — avoids Arduino hostByName panic. */
bool app_wifi_resolve_ipv4(const char * host, char * ip_out, size_t ip_sz);

/** Short status for UI, e.g. "Connected: MyHotspot" or "Not connected". */
void app_wifi_status_line(char * buf, size_t buf_sz);

#ifdef __cplusplus
}
#endif

#endif
