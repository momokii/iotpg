/* WiFi geolocation pipeline with NVS-cached last fix. See geo.h.
 *
 * BeaconDB geolocate (MLS-compatible, keyless) turns scanned APs into
 * lat/lon; Nominatim reverse-geocode turns coordinates into a city +
 * province (keyless, User-Agent identified, well under its 1 req/s
 * policy at our cadence). Every network step is bounded; any failure
 * keeps the previous fix and marks it stale instead of blanking screens.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "geo.h"
#include "wifi.h"
#include "weather.h"

static const char *TAG = "geo";
static const char *UA = "iotpg-esp32/1.0 (learning project)";

#define SCAN_MAX_APS 12
#define HTTP_CAP     4096

static geo_fix_t s_fix = { 0 };
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

static void nvs_load(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("geo", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    int32_t lat = 0, lon = 0, acc = 0;
    int64_t upd = 0;
    size_t n = sizeof(s_fix.place);
    if (nvs_get_i32(h, "lat", &lat) == ESP_OK && nvs_get_i32(h, "lon", &lon) == ESP_OK &&
        nvs_get_i64(h, "upd", &upd) == ESP_OK && upd > 0) {
        s_fix.has_fix = true;
        s_fix.lat = lat / 1e6;
        s_fix.lon = lon / 1e6;
        if (nvs_get_i32(h, "acc", &acc) == ESP_OK) {
            s_fix.accuracy_m = acc / 10.0f;
        }
        if (nvs_get_str(h, "place", s_fix.place, &n) != ESP_OK) {
            s_fix.place[0] = '\0';
        }
        s_fix.updated = (time_t)upd;
        ESP_LOGI(TAG, "restored last fix from NVS (age %lld s)", (long long)(time(NULL) - upd));
    }
    nvs_close(h);
}

static void nvs_save(void)
{
    nvs_handle_t h = 0;
    if (nvs_open("geo", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_i32(h, "lat", (int32_t)(s_fix.lat * 1e6));
    nvs_set_i32(h, "lon", (int32_t)(s_fix.lon * 1e6));
    nvs_set_i32(h, "acc", (int32_t)(s_fix.accuracy_m * 10));
    nvs_set_str(h, "place", s_fix.place);
    nvs_set_i64(h, "upd", (int64_t)s_fix.updated);
    nvs_commit(h);
    nvs_close(h);
}

void geo_init(void)
{
    nvs_load();
}

/* Geo work (blocking scan, TLS handshake, JSON parse) needs far more stack
 * than app_main owns, so it runs in a dedicated task. First fix attempt
 * shortly after boot (past WiFi join), then every 15 minutes. */
static void geo_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(15000));
    for (;;) {
        geo_update();
        geo_fix_t fix = { 0 };
        geo_get(&fix);
        if (fix.has_fix) {
            wx_update(fix.lat, fix.lon);
        }
        vTaskDelay(pdMS_TO_TICKS(15 * 60 * 1000));
    }
}

void geo_start(void)
{
    xTaskCreate(geo_task, "geo", 12288, NULL, 5, NULL);
}

void geo_get(geo_fix_t *out)
{
    *out = s_fix;
    out->stale = s_fix.has_fix && (!s_fresh || time(NULL) - s_fix.updated > 3600);
}

static int build_ap_body(char *out, size_t cap)
{
    uint16_t total = 0;
    if (esp_wifi_scan_get_ap_num(&total) != ESP_OK || total == 0) {
        return -1;
    }
    if (total > SCAN_MAX_APS) {
        total = SCAN_MAX_APS;
    }
    wifi_ap_record_t *list = malloc(total * sizeof(*list));
    if (list == NULL) {
        return -1;
    }
    uint16_t n = total;
    int w = 0;
    if (esp_wifi_scan_get_ap_records(&n, list) == ESP_OK && n > 0) {
        w = snprintf(out, cap, "{\"wifiAccessPoints\":[");
        for (uint16_t i = 0; i < n && w > 0 && (size_t)w < cap; i++) {
            w += snprintf(out + w, cap - w,
                          "%s{\"macAddress\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"signalStrength\":%d}",
                          i ? "," : "", list[i].bssid[0], list[i].bssid[1], list[i].bssid[2],
                          list[i].bssid[3], list[i].bssid[4], list[i].bssid[5], list[i].rssi);
        }
        if (w > 0 && (size_t)w < cap) {
            w += snprintf(out + w, cap - w, "]}");
        }
    }
    free(list);
    return (w > 0 && (size_t)w < cap) ? 0 : -1;
}

static bool geolocate(double *lat, double *lon, float *acc)
{
    wifi_scan_config_t cfg = { .ssid = NULL, .bssid = NULL, .channel = 0,
                               .show_hidden = true, .scan_time.active.min = 100,
                               .scan_time.active.max = 300 };
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) {
        ESP_LOGW(TAG, "AP scan failed");
        return false;
    }
    char *body = malloc(2048);
    if (body == NULL) {
        return false;
    }
    bool ok = false;
    if (build_ap_body(body, 2048) == 0) {
        http_buf_t b = { 0 };
        esp_http_client_config_t cc = {
            .url = "https://api.beacondb.net/v1/geolocate",
            .method = HTTP_METHOD_POST,
            .event_handler = http_sink,
            .user_data = &b,
            .timeout_ms = 15000,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_http_client_handle_t c = esp_http_client_init(&cc);
        esp_http_client_set_header(c, "User-Agent", UA);
        esp_http_client_set_header(c, "Content-Type", "application/json");
        esp_http_client_set_post_field(c, body, strlen(body));
        esp_err_t perr = esp_http_client_perform(c);
        int status = esp_http_client_get_status_code(c);
        if (perr == ESP_OK && status == 200 && b.data) {
            cJSON *root = cJSON_Parse(b.data);
            cJSON *loc = root ? cJSON_GetObjectItemCaseSensitive(root, "location") : NULL;
            cJSON *la = loc ? cJSON_GetObjectItemCaseSensitive(loc, "lat") : NULL;
            cJSON *lo = loc ? (cJSON_GetObjectItemCaseSensitive(loc, "lng") ?
                               cJSON_GetObjectItemCaseSensitive(loc, "lng") :
                               cJSON_GetObjectItemCaseSensitive(loc, "lon")) : NULL;
            cJSON *ac = root ? cJSON_GetObjectItemCaseSensitive(root, "accuracy") : NULL;
            if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
                *lat = la->valuedouble;
                *lon = lo->valuedouble;
                *acc = (cJSON_IsNumber(ac) ? (float)ac->valuedouble : 0);
                ok = true;
            } else {
                ESP_LOGW(TAG, "geolocate response unusable (no coverage here?)");
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "geolocate request failed: perform=%s status=%d%s",
                     esp_err_to_name(perr), status, b.data ? "" : " (empty body)");
        }
        free(b.data);
        esp_http_client_cleanup(c);
    }
    free(body);
    return ok;
}

static void pick_addr_field(cJSON *addr, char *out, size_t cap)
{
    static const char *city_keys[] = { "city", "town", "village", "municipality", "county", NULL };
    static const char *prov_keys[] = { "state", "province", "region", NULL };
    char city[32] = { 0 }, prov[32] = { 0 };
    for (int i = 0; city_keys[i] && !city[0]; i++) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(addr, city_keys[i]);
        if (cJSON_IsString(v)) {
            snprintf(city, sizeof(city), "%.31s", v->valuestring);
        }
    }
    for (int i = 0; prov_keys[i] && !prov[0]; i++) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(addr, prov_keys[i]);
        if (cJSON_IsString(v)) {
            snprintf(prov, sizeof(prov), "%.31s", v->valuestring);
        }
    }
    if (city[0] && prov[0]) {
        snprintf(out, cap, "%s, %s", city, prov);
    } else if (city[0]) {
        snprintf(out, cap, "%s", city);
    } else if (prov[0]) {
        snprintf(out, cap, "%s", prov);
    }
}

static bool reverse_geocode(double lat, double lon)
{
    char url[160];
    snprintf(url, sizeof(url),
             "https://nominatim.openstreetmap.org/reverse?format=json&lat=%.6f&lon=%.6f&zoom=10&addressdetails=1",
             lat, lon);
    http_buf_t b = { 0 };
    esp_http_client_config_t cc = {
        .url = url,
        .event_handler = http_sink,
        .user_data = &b,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cc);
    esp_http_client_set_header(c, "User-Agent", UA);
    bool ok = false;
    esp_err_t perr = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    if (perr == ESP_OK && status == 200 && b.data) {
        cJSON *root = cJSON_Parse(b.data);
        cJSON *addr = root ? cJSON_GetObjectItemCaseSensitive(root, "address") : NULL;
        if (addr) {
            char place[GEO_PLACE_LEN] = { 0 };
            pick_addr_field(addr, place, sizeof(place));
            if (place[0]) {
                snprintf(s_fix.place, sizeof(s_fix.place), "%s", place);
                ok = true;
            }
        }
        if (!ok) {
            ESP_LOGW(TAG, "reverse-geocode yielded no place name");
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGW(TAG, "reverse-geocode failed: perform=%s status=%d",
                 esp_err_to_name(perr), status);
    }
    free(b.data);
    esp_http_client_cleanup(c);
    return ok;
}

void geo_update(void)
{
    wifi_status_t ws = { 0 };
    wifi_get_status(&ws);
    if (!ws.connected) {
        ESP_LOGI(TAG, "offline — keeping last fix (stale)");
        s_fresh = false;
        return;
    }
    double lat = 0, lon = 0;
    float acc = 0;
    if (!geolocate(&lat, &lon, &acc)) {
        s_fresh = false;
        return;
    }
    s_fix.has_fix = true;
    s_fix.lat = lat;
    s_fix.lon = lon;
    s_fix.accuracy_m = acc;
    s_fix.updated = time(NULL);
    reverse_geocode(lat, lon);
    s_fresh = true;
    nvs_save();
    ESP_LOGI(TAG, "fix: %.5f,%.5f +-%.0fm %s", lat, lon, (double)acc, s_fix.place);
}
