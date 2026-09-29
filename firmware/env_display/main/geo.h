/* WiFi geolocation with place names and a persistent last fix.
 *
 * Pipeline, all best-effort with bounded timeouts: scan nearby APs ->
 * BeaconDB geolocate (no key, User-Agent identified) -> lat/lon/accuracy
 * -> Nominatim reverse-geocode (no key, 1 req/s policy) -> city/province.
 * The last good fix (coords + place + epoch) persists in NVS, so the
 * screens always show something meaningful: fresh fix, stale fix with age,
 * or never-had-a-fix. Display code reads geo_get() only.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define GEO_PLACE_LEN 48

typedef struct {
    bool has_fix;
    bool stale;
    double lat;
    double lon;
    float accuracy_m;
    char place[GEO_PLACE_LEN];
    time_t updated;
} geo_fix_t;

void geo_init(void);
void geo_start(void);
void geo_update(void);
void geo_get(geo_fix_t *out);
