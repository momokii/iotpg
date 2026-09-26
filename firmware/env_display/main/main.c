/* Temp/humidity + clock station: DHT11 on GPIO21, auto-discovered display.
 *
 * BOOT button toggles two screens: env (Temp/Hum) and clock (HH:MM:SS +
 * date). Time comes from a DS3231 RTC if one answers on the bus, else the
 * internal clock seeded from the firmware build stamp (drifts, resets on
 * power loss — see rtc.h). DHT11 note: DHT_TYPE_DHT11 reads integer units;
 * DHT22 needs DHT_TYPE_AM2301 with identical wiring.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"
#include "oled.h"
#include "rtc.h"

static const char *TAG = "env_display";

#define DHT_PIN   GPIO_NUM_21
#define DHT_TYPE  DHT_TYPE_DHT11
#define BTN_GPIO  GPIO_NUM_0

#define DEBOUNCE_MS 50
#define TICK_MS     100

void app_main(void)
{
    gpio_reset_pin(DHT_PIN);
    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    gpio_reset_pin(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_GPIO, GPIO_PULLUP_ONLY);

    oled_init();
    envclock_init();
    oled_text1206(0, 0, "DHT11 TEMP/HUM");
    oled_update();

    int screen = 0; /* 0 = env, 1 = clock — BOOT toggles */
    int low_streak = 0, tick = 0;
    ESP_LOGI(TAG, "BOOT toggles env/clock screens");

    while (1) {
        if (gpio_get_level(BTN_GPIO) == 0) {
            if (++low_streak >= DEBOUNCE_MS / TICK_MS) {
                screen ^= 1;
                ESP_LOGI(TAG, "screen %d (%s)", screen, screen ? "clock" : "env");
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
            float humidity = 0, temperature = 0;
            if (dht_read_float_data(DHT_TYPE, DHT_PIN, &humidity, &temperature) == ESP_OK) {
                ESP_LOGI(TAG, "Temp: %.1f C  Hum: %.1f %%", temperature, humidity);
                if (screen == 0) {
                    oled_show_env(temperature, humidity);
                }
            } else {
                ESP_LOGW(TAG, "DHT read failed — check wiring (DATA->21, VCC->3V3, GND->GND)");
                if (screen == 0) {
                    oled_show_error();
                }
            }
        }
        if (screen == 1 && tick % 10 == 0) {
            struct tm now = { 0 };
            if (envclock_now(&now)) {
                ESP_LOGI(TAG, "Clock: %02d:%02d:%02d (%s)",
                         now.tm_hour, now.tm_min, now.tm_sec, envclock_source());
                oled_show_clock(&now);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}
