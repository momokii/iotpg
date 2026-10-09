/* Outdoor weather: Open-Meteo current + hourly rain, NVS cached. See weather.h.
 *
 * Update runs in the geo task (12 KB stack, same cadence): needs a geo fix
 * for coordinates, skips cleanly without one. All network steps bounded;
 * any failure keeps the previous data and marks it stale.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "weather.h"

static const char *TAG = "wx";
static const char *UA = "iotpg-esp32/1.0 (learning project)";

#define HTTP_CAP 8192

static wx_t s_wx = { 0 };
static bool s_fresh = false;

typedef struct {
    char *data;
    size_t len;
} http_buf_t;

static esp_err_t http_sink(esp_http_client_event_t *ev)
{
    http_buf_t *b = ev->user_data;
    if (ev->event_id == HTTP_EVENT_ON_DATA && b->len < HTTP_CAP) {
        size_t take = ev->data_len;
        if (b->len + take > HTTP_CAP) {
            take = HTTP_CAP - b->len;
        }
        char *nb = realloc(b->data, b->len + take + 1);
        if (nb == NULL) {
            return ESP_FAIL;
        }
        b->data = nb;
        memcpy(b->data + b->len, ev->data, take);
        b->len += take;
        b->data[b->len] = '\0';
    }
    return ESP_OK;
}

static bool http_get(const char *url, http_buf_t *b)
{
    esp_http_client_config_t cc = {
        .url = url,
        .event_handler = http_sink,
        .user_data = b,
        .timeout_ms = 20000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cc);
    esp_http_client_set_header(c, "User-Agent", UA);
    esp_err_t perr = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    if (perr != ESP_OK || status != 200 || !b->data) {
        ESP_LOGW(TAG, "GET failed: perform=%s status=%d", esp_err_to_name(perr), status);
        return false;
    }
    return true;
}

const char *wx_code_word(int code)
{
    if (code == 0) {
        return "Clear";
    }
    if (code == 1) {
        return "Fair";
    }
    if (code == 2) {
        return "Cloudy";
    }
    if (code == 3) {
        return "Overcast";
    }
    if (code == 45 || code == 48) {
        return "Fog";
    }
    if (code >= 51 && code <= 57) {
        return "Drizzle";
    }
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) {
        return "Rain";
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return "Snow";
    }
    if (code >= 95) {
        return "Storm";
    }
    return "?";
}

static void nvs_load(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("wx", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    int32_t t = 0, hu = 0, fl = 0, code = 0, rain = 0;
    int64_t upd = 0;
    size_t n = sizeof(s_wx.cond);
    if (nvs_get_i64(h, "upd", &upd) == ESP_OK && upd > 0) {
        s_wx.has_data = true;
        s_wx.updated = (time_t)upd;
        if (nvs_get_i32(h, "t", &t) == ESP_OK) {
            s_wx.temp = t / 10.0f;
        }
        if (nvs_get_i32(h, "hu", &hu) == ESP_OK) {
            s_wx.humidity = hu / 10.0f;
        }
        if (nvs_get_i32(h, "fl", &fl) == ESP_OK) {
            s_wx.feels_like = fl / 10.0f;
        }
        if (nvs_get_i32(h, "code", &code) == ESP_OK) {
            s_wx.weather_code = (int)code;
            snprintf(s_wx.cond, sizeof(s_wx.cond), "%s", wx_code_word((int)code));
        }
        if (nvs_get_i32(h, "rain", &rain) == ESP_OK) {
            s_wx.rain_pct = (int)rain;
        }
        ESP_LOGI(TAG, "restored last outdoor data from NVS (age %lld s)", (long long)(time(NULL) - upd));
    }
    (void)n;
    nvs_close(h);
}

static void nvs_save(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("wx", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_i32(h, "t", (int32_t)(s_wx.temp * 10));
    nvs_set_i32(h, "hu", (int32_t)(s_wx.humidity * 10));
    nvs_set_i32(h, "fl", (int32_t)(s_wx.feels_like * 10));
    nvs_set_i32(h, "code", (int32_t)s_wx.weather_code);
    nvs_set_i32(h, "rain", (int32_t)s_wx.rain_pct);
    nvs_set_i64(h, "upd", (int64_t)s_wx.updated);
    nvs_commit(h);
    nvs_close(h);
}

void wx_init(void)
{
    nvs_load();
}

void wx_get(wx_t *out)
{
    *out = s_wx;
    out->stale = s_wx.has_data && (!s_fresh || time(NULL) - s_wx.updated > 3600);
}

static double json_num(cJSON *o, const char *key, double dflt)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsNumber(v) ? v->valuedouble : dflt;
}

static int rain_next_hours(cJSON *hourly, const char *now_iso)
{
    cJSON *times = cJSON_GetObjectItemCaseSensitive(hourly, "time");
    cJSON *probs = cJSON_GetObjectItemCaseSensitive(hourly, "precipitation_probability");
    if (!cJSON_IsArray(times) || !cJSON_IsArray(probs)) {
        return -1;
    }
    int n = cJSON_GetArraySize(times);
    int start = 0;
    if (now_iso) {
        char hour_prefix[14] = { 0 };
        snprintf(hour_prefix, sizeof(hour_prefix), "%.13s", now_iso);
        for (int i = 0; i < n; i++) {
            cJSON *t = cJSON_GetArrayItem(times, i);
            if (cJSON_IsString(t) && strncmp(t->valuestring, hour_prefix, 13) == 0) {
                start = i;
                break;
            }
        }
    }
    int best = 0;
    for (int i = start; i < n && i < start + 6; i++) {
        cJSON *p = cJSON_GetArrayItem(probs, i);
        if (cJSON_IsNumber(p) && (int)p->valuedouble > best) {
            best = (int)p->valuedouble;
        }
    }
    return best;
}

void wx_update(double lat, double lon)
{
    char url[220];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code"
             "&hourly=precipitation_probability&forecast_days=1",
             lat, lon);
    http_buf_t b = { 0 };
    if (!http_get(url, &b)) {
        s_fresh = false;
        return;
    }
    bool ok = false;
    cJSON *root = cJSON_Parse(b.data);
    cJSON *cur = root ? cJSON_GetObjectItemCaseSensitive(root, "current") : NULL;
    if (cur) {
        s_wx.temp = (float)json_num(cur, "temperature_2m", 0);
        s_wx.humidity = (float)json_num(cur, "relative_humidity_2m", 0);
        s_wx.feels_like = (float)json_num(cur, "apparent_temperature", 0);
        s_wx.weather_code = (int)json_num(cur, "weather_code", 0);
        snprintf(s_wx.cond, sizeof(s_wx.cond), "%s", wx_code_word(s_wx.weather_code));
        cJSON *ct = cJSON_GetObjectItemCaseSensitive(cur, "time");
        const char *now_iso = (cJSON_IsString(ct) ? ct->valuestring : NULL);
        cJSON *hourly = cJSON_GetObjectItemCaseSensitive(root, "hourly");
        int rain = hourly ? rain_next_hours(hourly, now_iso) : -1;
        s_wx.rain_pct = rain < 0 ? 0 : rain;
        s_wx.updated = time(NULL);
        s_wx.has_data = true;
        s_fresh = true;
        nvs_save();
        ok = true;
        ESP_LOGI(TAG, "outdoor: %.1fC %.0f%% feels %.1fC %s rain %d%%",
                 s_wx.temp, (double)s_wx.humidity, s_wx.feels_like, s_wx.cond, s_wx.rain_pct);
    } else {
        ESP_LOGW(TAG, "forecast response unusable");
    }
    cJSON_Delete(root);
    free(b.data);
    if (!ok) {
        s_fresh = false;
    }
}
