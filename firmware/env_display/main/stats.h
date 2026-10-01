/* Daily min/max with timestamps, trend, comfort verdict, dew point.
 *
 * Min/max persist in NVS and reset on date rollover (local WIB date), so a
 * reboot never loses today's extremes. Trend compares against the reading
 * from 10 minutes ago. Comfort words follow ASHRAE-grounded bands
 * (30-60% RH comfortable, >=70% muggy/mold watch, <30% dry). Dew point
 * uses the Magnus-Tetens approximation (b=17.625, c=243.04 over water).
 */
#pragma once

#include <stdbool.h>
#include <time.h>

typedef enum {
    COMFORT_OK = 0,
    COMFORT_HOT,
    COMFORT_COLD,
    COMFORT_MUGGY,
    COMFORT_DRY,
} comfort_t;

void stats_init(void);
void stats_record(float temperature, float humidity);
bool stats_have(void);
float stats_tmax(time_t *at);
float stats_tmin(time_t *at);
float stats_hmax(time_t *at);
float stats_hmin(time_t *at);
char stats_trend(float current);
comfort_t stats_comfort(float temperature, float humidity);
const char *stats_comfort_word(comfort_t c);
float stats_dew_point(float temperature, float humidity);
