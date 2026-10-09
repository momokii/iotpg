#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "dashboard.h"
#include "diag.h"
#include "geo.h"
#include "rtc.h"
#include "weather.h"
#include "wifi.h"
#include "esp_http_server.h"

static const char *TAG = "dashboard";

#define DASHBOARD_BUFFER_SIZE 4096
#define DASHBOARD_EVENT_GOT_IP BIT0
#define DASHBOARD_EVENT_DISCONNECTED BIT1

extern const char dashboard_html_start[] asm("_binary_dashboard_html_start");
extern const char dashboard_html_end[] asm("_binary_dashboard_html_end");

typedef struct {
    const char *marker;
    const char *value;
} replacement_t;

typedef struct {
    char temp[20];
    char hum[20];
    char time[16];
    char date[16];
    char source[24];
    char place[6 * GEO_PLACE_LEN + 1];
    char out_temp[20];
    char out_cond[WX_COND_LEN];
    char rain[12];
    char ssid[6 * sizeof(((wifi_status_t *)0)->ssid) + 1];
    char wifi_state[16];
    char rssi[32];
    char uptime[24];
    char heap[24];
    char boots[12];
} dashboard_values_t;

typedef struct {
    float temp;
    float humidity;
    bool valid;
    bool published;
} env_snapshot_t;

static TaskHandle_t s_lifecycle_task;
static SemaphoreHandle_t s_env_mutex;
static bool s_initialized;
static char s_response[DASHBOARD_BUFFER_SIZE];
static env_snapshot_t s_env;

static size_t html_escape(char *out, size_t out_size, const char *in)
{
    size_t used = 0;

    while (*in != '\0') {
        const char *escaped = NULL;
        char input = *in++;

        switch (input) {
        case '&': escaped = "&amp;"; break;
        case '<': escaped = "&lt;"; break;
        case '>': escaped = "&gt;"; break;
        case '"': escaped = "&quot;"; break;
        case '\'': escaped = "&#39;"; break;
        default:
            if (used + 1 >= out_size) {
                break;
            }
            out[used++] = input;
            continue;
        }

        size_t escaped_len = strlen(escaped);
        if (used + escaped_len >= out_size) {
            break;
        }
        memcpy(&out[used], escaped, escaped_len);
        used += escaped_len;
    }

    out[used] = '\0';
    return used;
}

static bool append_bytes(char **out, size_t *remaining, const char *value, size_t value_len)
{
    if (value_len >= *remaining) {
        return false;
    }
    memcpy(*out, value, value_len);
    *out += value_len;
    *remaining -= value_len;
    return true;
}

static bool copy_env_snapshot(env_snapshot_t *snapshot)
{
    if (s_env_mutex == NULL || xSemaphoreTake(s_env_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }
    *snapshot = s_env;
    xSemaphoreGive(s_env_mutex);
    return snapshot->published && snapshot->valid;
}

static void collect_values(dashboard_values_t *values)
{
    wifi_status_t wifi = { 0 };
    geo_fix_t geo = { 0 };
    wx_t weather = { 0 };
    diag_snapshot_t diag = { 0 };
    env_snapshot_t env = { 0 };
    struct tm now = { 0 };

    wifi_get_status(&wifi);
    geo_get(&geo);
    wx_get(&weather);
    diag_get(&diag);

    if (copy_env_snapshot(&env)) {
        snprintf(values->temp, sizeof(values->temp), "%.1f C", env.temp);
        snprintf(values->hum, sizeof(values->hum), "%.0f %%", env.humidity);
    } else {
        snprintf(values->temp, sizeof(values->temp), "DHT ERR");
        snprintf(values->hum, sizeof(values->hum), "DHT ERR");
    }

    if (envclock_now(&now)) {
        strftime(values->time, sizeof(values->time), "%H:%M:%S", &now);
        strftime(values->date, sizeof(values->date), "%Y-%m-%d", &now);
    } else {
        snprintf(values->time, sizeof(values->time), "--");
        snprintf(values->date, sizeof(values->date), "--");
    }
    snprintf(values->source, sizeof(values->source), "%s", envclock_source());

    html_escape(values->place, sizeof(values->place), geo.has_fix ? geo.place : "--");
    if (weather.has_data) {
        snprintf(values->out_temp, sizeof(values->out_temp), "%.1f C", weather.temp);
        snprintf(values->out_cond, sizeof(values->out_cond), "%s", weather.cond);
        snprintf(values->rain, sizeof(values->rain), "%d%%", weather.rain_pct);
    } else {
        snprintf(values->out_temp, sizeof(values->out_temp), "--");
        snprintf(values->out_cond, sizeof(values->out_cond), "--");
        snprintf(values->rain, sizeof(values->rain), "--");
    }

    html_escape(values->ssid, sizeof(values->ssid), wifi.ssid[0] != '\0' ? wifi.ssid : "--");
    snprintf(values->wifi_state, sizeof(values->wifi_state), "%s",
             wifi.connected ? "Connected" : "Disconnected");
    if (wifi.connected) {
        snprintf(values->rssi, sizeof(values->rssi), "%d dBm (%s)", wifi.rssi_dbm, wifi.quality);
    } else {
        snprintf(values->rssi, sizeof(values->rssi), "--");
    }

    uint64_t uptime_s = (uint64_t)esp_timer_get_time() / 1000000ULL;
    snprintf(values->uptime, sizeof(values->uptime), "%" PRIu64 " s", uptime_s);
    snprintf(values->heap, sizeof(values->heap), "%" PRIu32 " B", esp_get_free_heap_size());
    if (diag.reboot_count != 0) {
        snprintf(values->boots, sizeof(values->boots), "%" PRIu32, diag.reboot_count);
    } else {
        snprintf(values->boots, sizeof(values->boots), "--");
    }
}

static bool render_page(const dashboard_values_t *values, size_t *page_len)
{
    const replacement_t replacements[] = {
        { "{{TEMP}}", values->temp },
        { "{{HUM}}", values->hum },
        { "{{TIME}}", values->time },
        { "{{DATE}}", values->date },
        { "{{SRC}}", values->source },
        { "{{PLACE}}", values->place },
        { "{{OUT_TEMP}}", values->out_temp },
        { "{{OUT_COND}}", values->out_cond },
        { "{{RAIN}}", values->rain },
        { "{{SSID}}", values->ssid },
        { "{{WIFI_STATE}}", values->wifi_state },
        { "{{RSSI}}", values->rssi },
        { "{{UPTIME}}", values->uptime },
        { "{{HEAP}}", values->heap },
        { "{{BOOTS}}", values->boots },
    };
    size_t found[sizeof(replacements) / sizeof(replacements[0])] = { 0 };
    const char *input = dashboard_html_start;
    const char *input_end = dashboard_html_end;
    char *output = s_response;
    size_t remaining = sizeof(s_response);

    while (input < input_end) {
        size_t replacement_index = sizeof(replacements) / sizeof(replacements[0]);

        for (size_t index = 0; index < sizeof(replacements) / sizeof(replacements[0]); ++index) {
            size_t marker_len = strlen(replacements[index].marker);
            if ((size_t)(input_end - input) >= marker_len &&
                memcmp(input, replacements[index].marker, marker_len) == 0) {
                replacement_index = index;
                break;
            }
        }

        if (replacement_index == sizeof(replacements) / sizeof(replacements[0])) {
            if (!append_bytes(&output, &remaining, input, 1)) {
                return false;
            }
            ++input;
            continue;
        }

        const replacement_t *replacement = &replacements[replacement_index];
        if (!append_bytes(&output, &remaining, replacement->value, strlen(replacement->value))) {
            return false;
        }
        input += strlen(replacement->marker);
        ++found[replacement_index];
    }

    for (size_t index = 0; index < sizeof(found) / sizeof(found[0]); ++index) {
        if (found[index] != 1) {
            return false;
        }
    }
    *output = '\0';
    *page_len = (size_t)(output - s_response);
    return true;
}

static esp_err_t dashboard_get_handler(httpd_req_t *request)
{
    dashboard_values_t values = { 0 };
    size_t page_len = 0;

    collect_values(&values);
    if (!render_page(&values, &page_len)) {
        ESP_LOGE(TAG, "template rendering failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_send(request, s_response, page_len);
}

static const httpd_uri_t s_dashboard_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = dashboard_get_handler,
    .user_ctx = NULL,
};

static httpd_handle_t start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    config.max_open_sockets = 3;
    config.lru_purge_enable = true;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGW(TAG, "server start failed");
        return NULL;
    }
    if (httpd_register_uri_handler(server, &s_dashboard_uri) != ESP_OK) {
        ESP_LOGW(TAG, "URI registration failed");
        httpd_stop(server);
        return NULL;
    }
    ESP_LOGI(TAG, "server started");
    return server;
}

static void lifecycle_task(void *arg)
{
    httpd_handle_t server = NULL;

    (void)arg;
    while (true) {
        uint32_t events = 0;
        xTaskNotifyWait(0, UINT32_MAX, &events, portMAX_DELAY);

        if ((events & DASHBOARD_EVENT_DISCONNECTED) != 0 && server != NULL) {
            ESP_LOGI(TAG, "WiFi disconnected; stopping server");
            httpd_stop(server);
            server = NULL;
        }
        if ((events & DASHBOARD_EVENT_GOT_IP) != 0 && server == NULL) {
            server = start_server();
        }
    }
}

static void on_network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    uint32_t event = 0;

    (void)arg;
    (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        event = DASHBOARD_EVENT_GOT_IP;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        event = DASHBOARD_EVENT_DISCONNECTED;
    }
    if (event != 0 && s_lifecycle_task != NULL) {
        xTaskNotify(s_lifecycle_task, event, eSetBits);
    }
}

void dashboard_init(void)
{
    wifi_status_t wifi = { 0 };

    if (s_initialized) {
        return;
    }
    if (!wifi_is_configured()) {
        ESP_LOGI(TAG, "WiFi is not configured; dashboard disabled");
        return;
    }
    s_env_mutex = xSemaphoreCreateMutex();
    if (s_env_mutex == NULL) {
        ESP_LOGW(TAG, "environment snapshot mutex unavailable");
    }
    if (xTaskCreate(lifecycle_task, "dashboard", 4096, NULL, 5, &s_lifecycle_task) != pdPASS) {
        ESP_LOGW(TAG, "lifecycle task creation failed");
        return;
    }
    if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            on_network_event, NULL, NULL) != ESP_OK ||
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                            on_network_event, NULL, NULL) != ESP_OK) {
        ESP_LOGW(TAG, "network event registration failed");
        return;
    }
    s_initialized = true;

    wifi_get_status(&wifi);
    if (wifi.connected) {
        xTaskNotify(s_lifecycle_task, DASHBOARD_EVENT_GOT_IP, eSetBits);
    }
}

void dashboard_publish_env(float temp, float humidity, bool valid)
{
    if (s_env_mutex == NULL || xSemaphoreTake(s_env_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }
    s_env.temp = temp;
    s_env.humidity = humidity;
    s_env.valid = valid;
    s_env.published = true;
    xSemaphoreGive(s_env_mutex);
}
