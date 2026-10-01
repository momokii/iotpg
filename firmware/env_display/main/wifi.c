/* WiFi station join + one-shot NTP sync with hourly re-sync.
 *
 * Credentials come from Kconfig (menuconfig -> gitignored sdkconfig).
 * Empty SSID skips everything. Join waits up to 15 s, NTP sync up to
 * 10 s; either timeout returns false and time falls back gracefully.
 * On success the system clock is disciplined by SNTP from then on.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "wifi.h"

static const char *TAG = "wifi";

#define GOT_IP_BIT BIT0
#define JOIN_TIMEOUT_MS 15000
#define NTP_TIMEOUT_MS  10000

static EventGroupHandle_t s_events;
static bool s_ntp = false;
static bool s_connected = false;
static bool s_ever_dropped = false;
static char s_ssid[sizeof(((wifi_config_t *)0)->sta.ssid)] = { 0 };

static void on_sntp(struct timeval *tv)
{
    (void)tv;
    s_ntp = true;
    ESP_LOGI(TAG, "NTP sync acquired");
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        s_ever_dropped = true;
        ESP_LOGI(TAG, "disconnected, retrying...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        if (s_ever_dropped) {
            ESP_LOGI(TAG, "reconnected — re-syncing clock now");
            esp_sntp_restart();
        }
        xEventGroupSetBits(s_events, GOT_IP_BIT);
    }
}

bool time_is_ntp(void)
{
    return s_ntp;
}

bool wifi_sntp_sync(void)
{
    if (CONFIG_WIFI_SSID[0] == '\0') {
        ESP_LOGI(TAG, "no SSID configured — skipping WiFi, time from RTC/internal");
        return false;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wc = { 0 };
    strncpy((char *)wc.sta.ssid, CONFIG_WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wc.sta.password) - 1);
    strncpy(s_ssid, CONFIG_WIFI_SSID, sizeof(s_ssid) - 1);
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_LOGI(TAG, "joining '%s'...", CONFIG_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());

    if (xEventGroupWaitBits(s_events, GOT_IP_BIT, pdFALSE, pdTRUE,
                            pdMS_TO_TICKS(JOIN_TIMEOUT_MS)) == 0) {
        ESP_LOGW(TAG, "join timed out — continuing without network time");
        return false;
    }
    ESP_LOGI(TAG, "joined, starting SNTP (pool.ntp.org, re-sync hourly)");

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    sntp_cfg.sync_cb = on_sntp;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_cfg));
    ESP_ERROR_CHECK(esp_netif_sntp_start());
    esp_sntp_set_sync_interval(3600000);

    int waited = 0;
    while (!s_ntp && waited < NTP_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(200));
        waited += 200;
    }
    if (!s_ntp) {
        ESP_LOGW(TAG, "NTP sync timed out — continuing on RTC/internal");
        return false;
    }
    return true;
}

static const char *rssi_quality(int dbm)
{
    if (dbm >= -50) {
        return "Strong";
    }
    if (dbm >= -60) {
        return "Good";
    }
    if (dbm >= -70) {
        return "OK";
    }
    return "Weak";
}

void wifi_get_status(wifi_status_t *out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->ssid, s_ssid, sizeof(out->ssid) - 1);
    out->connected = s_connected;
    if (!s_connected) {
        out->rssi_dbm = 0;
        out->quality = "--";
        return;
    }
    int rssi = 0;
    if (esp_wifi_sta_get_rssi(&rssi) != ESP_OK) {
        out->connected = false;
        out->quality = "--";
        return;
    }
    out->rssi_dbm = rssi;
    out->quality = rssi_quality(rssi);
}
