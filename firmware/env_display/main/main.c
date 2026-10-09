/* Temp/humidity + clock station on one screen: DHT11 on GPIO21,
 * auto-discovered display (SSD1306 and/or 1602 LCD).
 *
 * Page 0 (main): time, date, zone, temperature, humidity together.
 * Page 1 (wifi): SSID, link status, RSSI + quality word.
 * Single-button UX (BOOT, GPIO0): short press toggles WIB/UTC, holding
 * ~1.5 s flips pages. Fire-on-threshold for long, fire-on-release for
 * short — the standard single-button pattern.
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
#include "geo.h"
#include "stats.h"
#include "weather.h"

static const char *TAG = "env_display";

#define DHT_PIN   GPIO_NUM_21
#define DHT_TYPE  DHT_TYPE_DHT11
#define BTN_GPIO  GPIO_NUM_0

#define LONG_TICKS    15
#define TICK_MS       100

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
    geo_init();
    stats_init();
    wx_init();
    geo_start();
    oled_text1206(0, 0, "DHT11 TEMP/HUM");
    oled_update();

    float temperature = 0, humidity = 0;
    bool env_ok = false;
    int tick = 0, held = 0, tz_utc = 0, page = 0;
    bool long_fired = false;
    ESP_LOGI(TAG, "pages: 0 main, 1 wifi, 2 where, 3 stats, 4 outside — short press toggles WIB/UTC, hold flips page");

    while (1) {
        if (gpio_get_level(BTN_GPIO) == 0) {
            held++;
            if (held >= LONG_TICKS && !long_fired) {
                long_fired = true;
                page = (page + 1) % 5;
                const char *pname = page == 0 ? "main" : (page == 1 ? "wifi" : (page == 2 ? "where" : (page == 3 ? "stats" : "outside")));
                ESP_LOGI(TAG, "page %d (%s)", page, pname);
            }
        } else {
            if (held > 0 && held < LONG_TICKS && !long_fired) {
                tz_utc ^= 1;
                ESP_LOGI(TAG, "timezone %s", tz_utc ? "UTC" : "WIB");
            }
            held = 0;
            long_fired = false;
        }
        tick++;
        if (tick % 20 == 0) {
            if (dht_read_float_data(DHT_TYPE, DHT_PIN, &humidity, &temperature) == ESP_OK) {
                ESP_LOGI(TAG, "Temp: %.1f C  Hum: %.1f %%", temperature, humidity);
                env_ok = true;
                stats_record(temperature, humidity);
            } else {
                ESP_LOGW(TAG, "DHT read failed — check wiring (DATA->21, VCC->3V3, GND->GND)");
                env_ok = false;
            }
        }
        if (tick % 10 == 0) {
            if (page == 0) {
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
            } else if (page == 1) {
                wifi_status_t ws = { 0 };
                wifi_get_status(&ws);
                ESP_LOGI(TAG, "WiFi %s ssid=%s rssi=%d %s",
                         ws.connected ? "UP" : "DOWN", ws.ssid, ws.rssi_dbm, ws.quality);
                oled_show_wifi(ws.ssid, ws.connected, ws.rssi_dbm, ws.quality);
            } else if (page == 2) {
                geo_fix_t fix = { 0 };
                geo_get(&fix);
                oled_show_geo(&fix);
            } else if (page == 3) {
                oled_show_stats(temperature, humidity);
            } else {
                wx_t w = { 0 };
                wx_get(&w);
                oled_show_wx(&w);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
