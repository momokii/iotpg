/* WiFi station + NTP time sync. Best-effort: empty SSID or failed join
 * simply returns false and the app continues on RTC/internal time.
 */
#pragma once

#include <stdbool.h>

bool wifi_sntp_sync(void);
bool time_is_ntp(void);
