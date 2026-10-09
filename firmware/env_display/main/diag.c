#include <string.h>
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "diag.h"

static const char *TAG = "diag";
static uint32_t s_boots = 0;

void diag_init(void)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("diag", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    uint32_t boots = 0;
    err = nvs_get_u32(handle, "boots", &boots);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS read failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    boots++;
    err = nvs_set_u32(handle, "boots", boots);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS write failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS commit failed: %s", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    s_boots = boots;
    nvs_close(handle);
    ESP_LOGI(TAG, "boot #%lu", (unsigned long)boots);
}

void diag_get(diag_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));
    out->uptime_seconds = (uint64_t)esp_timer_get_time() / 1000000;
    out->free_heap_bytes = esp_get_free_heap_size();
    out->minimum_free_heap_bytes = esp_get_minimum_free_heap_size();
    out->reboot_count = s_boots;
    wifi_get_status(&out->wifi);
}
