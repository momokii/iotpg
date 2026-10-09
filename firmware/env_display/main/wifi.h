/* WiFi station + NTP time sync. Best-effort: empty SSID or failed join
 * simply returns false and the app continues on RTC/internal time.
 *
 * Credential seam (read before changing): display and app code consume
 * connection state ONLY through wifi_status_t below — never Kconfig, never
 * esp_wifi calls. Today the SSID comes from static Kconfig; dynamic
 * provisioning (TASK-010) will fill the same struct from NVS/captive
 * portal with zero changes to consumers.
 */
#pragma once

#include <stdbool.h>

typedef struct {
    bool connected;
    char ssid[33];
    int rssi_dbm;
    const char *quality;
} wifi_status_t;

bool wifi_sntp_sync(void);
/* Returns true when CONFIG_WIFI_SSID is non-empty. */
bool wifi_is_configured(void);
bool time_is_ntp(void);
void wifi_get_status(wifi_status_t *out);
