/* Temp/humidity + clock station on one screen: DHT11 on GPIO21,
 * auto-discovered display (SSD1306 and/or 1602 LCD).
 *
 * Single unified view — time, date, zone, temperature, humidity together,
 * refreshed every second (sensor re-read every 2 s, its hardware limit).
 * BOOT toggles the timezone: WIB (Asia/Jakarta, default) vs UTC.
 * Time: DS3231 RTC if present, else NTP over Wi-Fi, else internal clock
 * from the build stamp (see rtc.h). DHT11 note: DHT_TYPE_DHT11 reads
 * integer units, hence humidity shown without decimals; DHT22 needs
 * DHT_TYPE_AM2301 with identical wiring.
 */
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"
#include "oled.h"
#include "rtc.h"
#include "wifi.h"

static const char *TAG = "env_display";

#define DHT_PIN   GPIO_NUM_21
#define DHT_TYPE  DHT_TYPE_DHT11
#define BTN_GPIO  GPIO_NUM_0

#define DEBOUNCE_MS 50
#define TICK_MS     100

void app_main(void)
{
    setenv("TZ", "WIB-7", 1);
    tzset();

    gpio_reset_pin(DHT_PIN);
    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_GPIO, GPIO_PULLUP_ONLY);

    oled_init();
    envclock_init();
    if (wifi_sntp_sync()) {
        envclock_note_ntp_sync();
    }
    oled_text1206(0, 0, "DHT11 TEMP/HUM");
    oled_update();

    float temperature = 0, humidity = 0;
    bool env_ok = false;
    int tick = 0, low_streak = 0, tz_utc = 0;
    ESP_LOGI(TAG, "unified screen: time + env, refreshed every second; BOOT toggles WIB/UTC");

    while (1) {
        if (gpio_get_level(BTN_GPIO) == 0) {
            if (++low_streak >= DEBOUNCE_MS / TICK_MS) {
                tz_utc ^= 1;
                ESP_LOGI(TAG, "timezone %s", tz_utc ? "UTC" : "WIB");
                while (gpio_get_level(BTN_GPIO) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(TICK_MS));
                }
                low_streak = 0;
            }
        } else {
            low_streak = 0;
        }
        tick++;
        if (tick % 20 == 0) {
            if (dht_read_float_data(DHT_TYPE, DHT_PIN, &humidity, &temperature) == ESP_OK) {
                ESP_LOGI(TAG, "Temp: %.1f C  Hum: %.1f %%", temperature, humidity);
                env_ok = true;
            } else {
                ESP_LOGW(TAG, "DHT read failed — check wiring (DATA->21, VCC->3V3, GND->GND)");
                env_ok = false;
            }
        }
        if (tick % 10 == 0) {
            struct tm wib = { 0 };
            struct tm show = { 0 };
            const char *zone = "WIB";
            if (envclock_now(&wib)) {
                if (tz_utc) {
                    time_t ep = mktime(&wib);
                    gmtime_r(&ep, &show);
                    zone = "UTC";
                } else {
                    show = wib;
                }
                oled_show_all(temperature, humidity, &show, env_ok, zone);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
