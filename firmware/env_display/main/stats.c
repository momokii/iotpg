/* Daily extremes with NVS persistence, trend, comfort, dew point. See stats.h.
 *
 * NVS layout (namespace "stats"): day as YYYYMMDD int; extremes as int
 * tenths with their epoch timestamps. A date change wipes extremes to the
 * current reading. NVS failures degrade to RAM-only rather than aborting.
 */
#include <math.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "stats.h"

static const char *TAG = "stats";

static int s_day = 0;
static float s_tmax = -1000, s_tmin = 1000, s_hmax = -1, s_hmin = 101;
static time_t s_tmax_t = 0, s_tmin_t = 0, s_hmax_t = 0, s_hmin_t = 0;
static float s_ref_temp = 0;
static time_t s_ref_at = 0;
static bool s_ref_set = false;
static bool s_have = false;

static int today_ymd(void)
{
    time_t now = time(NULL);
    struct tm t = { 0 };
    localtime_r(&now, &t);
    return (t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday;
}

static void nvs_load(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("stats", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    int32_t day = 0;
    if (nvs_get_i32(h, "day", &day) != ESP_OK || day != today_ymd()) {
        nvs_close(h);
        return;
    }
    int32_t v = 0;
    int64_t e = 0;
    if (nvs_get_i32(h, "tmax", &v) == ESP_OK) {
        s_tmax = v / 10.0f;
    }
    if (nvs_get_i32(h, "tmin", &v) == ESP_OK) {
        s_tmin = v / 10.0f;
    }
    if (nvs_get_i32(h, "hmax", &v) == ESP_OK) {
        s_hmax = v / 10.0f;
    }
    if (nvs_get_i32(h, "hmin", &v) == ESP_OK) {
        s_hmin = v / 10.0f;
    }
    if (nvs_get_i64(h, "tmax_t", &e) == ESP_OK) {
        s_tmax_t = (time_t)e;
    }
    if (nvs_get_i64(h, "tmin_t", &e) == ESP_OK) {
        s_tmin_t = (time_t)e;
    }
    if (nvs_get_i64(h, "hmax_t", &e) == ESP_OK) {
        s_hmax_t = (time_t)e;
    }
    if (nvs_get_i64(h, "hmin_t", &e) == ESP_OK) {
        s_hmin_t = (time_t)e;
    }
    s_day = day;
    nvs_close(h);
    ESP_LOGI(TAG, "restored today's extremes from NVS");
}

static void nvs_save(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("stats", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_i32(h, "day", (int32_t)s_day);
    nvs_set_i32(h, "tmax", (int32_t)(s_tmax * 10));
    nvs_set_i32(h, "tmin", (int32_t)(s_tmin * 10));
    nvs_set_i32(h, "hmax", (int32_t)(s_hmax * 10));
    nvs_set_i32(h, "hmin", (int32_t)(s_hmin * 10));
    nvs_set_i64(h, "tmax_t", (int64_t)s_tmax_t);
    nvs_set_i64(h, "tmin_t", (int64_t)s_tmin_t);
    nvs_set_i64(h, "hmax_t", (int64_t)s_hmax_t);
    nvs_set_i64(h, "hmin_t", (int64_t)s_hmin_t);
    nvs_commit(h);
    nvs_close(h);
}

void stats_init(void)
{
    s_day = today_ymd();
    nvs_load();
    if (s_day != today_ymd()) {
        s_day = today_ymd();
    }
}

void stats_record(float temperature, float humidity)
{
    time_t now = time(NULL);
    s_have = true;
    if (today_ymd() != s_day) {
        s_day = today_ymd();
        s_tmax = s_tmin = temperature;
        s_hmax = s_hmin = humidity;
        s_tmax_t = s_tmin_t = s_hmax_t = s_hmin_t = now;
        nvs_save();
        return;
    }
    bool changed = false;
    if (temperature > s_tmax) {
        s_tmax = temperature;
        s_tmax_t = now;
        changed = true;
    }
    if (temperature < s_tmin) {
        s_tmin = temperature;
        s_tmin_t = now;
        changed = true;
    }
    if (humidity > s_hmax) {
        s_hmax = humidity;
        s_hmax_t = now;
        changed = true;
    }
    if (humidity < s_hmin) {
        s_hmin = humidity;
        s_hmin_t = now;
        changed = true;
    }
    if (!s_ref_set || now - s_ref_at >= 600) {
        s_ref_temp = temperature;
        s_ref_at = now;
        s_ref_set = true;
    }
    if (changed) {
        nvs_save();
    }
}

bool stats_have(void)
{
    return s_have;
}

float stats_tmax(time_t *at)
{
    if (at) {
        *at = s_tmax_t;
    }
    return s_tmax;
}

float stats_tmin(time_t *at)
{
    if (at) {
        *at = s_tmin_t;
    }
    return s_tmin;
}

float stats_hmax(time_t *at)
{
    if (at) {
        *at = s_hmax_t;
    }
    return s_hmax;
}

float stats_hmin(time_t *at)
{
    if (at) {
        *at = s_hmin_t;
    }
    return s_hmin;
}

char stats_trend(float current)
{
    if (!s_ref_set) {
        return '-';
    }
    if (current - s_ref_temp > 0.5f) {
        return '^';
    }
    if (s_ref_temp - current > 0.5f) {
        return 'v';
    }
    return '-';
}

comfort_t stats_comfort(float temperature, float humidity)
{
    if (temperature > 30) {
        return COMFORT_HOT;
    }
    if (temperature < 18) {
        return COMFORT_COLD;
    }
    if (humidity >= 70) {
        return COMFORT_MUGGY;
    }
    if (humidity < 30) {
        return COMFORT_DRY;
    }
    return COMFORT_OK;
}

const char *stats_comfort_word(comfort_t c)
{
    switch (c) {
    case COMFORT_HOT:
        return "Hot";
    case COMFORT_COLD:
        return "Cold";
    case COMFORT_MUGGY:
        return "Muggy";
    case COMFORT_DRY:
        return "Dry";
    default:
        return "Comfort";
    }
}

float stats_dew_point(float temperature, float humidity)
{
    const float b = 17.625f, c = 243.04f;
    float rh = humidity < 1 ? 1 : (humidity > 100 ? 100 : humidity);
    float gamma = logf(rh / 100.0f) + (b * temperature) / (c + temperature);
    return (c * gamma) / (b - gamma);
}
