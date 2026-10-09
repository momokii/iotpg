/* Outdoor weather via Open-Meteo (keyless for non-commercial use).
 *
 * One HTTPS GET per update using the geo fix coordinates: current temp,
 * humidity, feels-like and WMO weather code, plus hourly rain probability
 * (max over the next 6 h). Result cached in NVS with timestamp; screens
 * show STALE/age when offline exactly like the geo page. WMO codes map
 * to short words sized for 16 columns (full descriptions in the docs).
 */
#pragma once

#include <stdbool.h>
#include <time.h>

#define WX_COND_LEN 12

typedef struct {
    bool has_data;
    bool stale;
    float temp;
    float humidity;
    float feels_like;
    int weather_code;
    int rain_pct;
    char cond[WX_COND_LEN];
    time_t updated;
} wx_t;

void wx_init(void);
void wx_update(double lat, double lon);
void wx_get(wx_t *out);
const char *wx_code_word(int code);
