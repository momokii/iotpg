/* Temp/humidity display: DHT11 on GPIO21 (proven pin), SSD1306 on I2C (SDA=22, SCL=23).
 *
 * Reads the sensor every 2 s (the DHT11 needs >=1 s between reads), shows
 * temperature and humidity on the OLED, and logs the same line to USB.
 * Using a DHT22 instead? Change DHT_TYPE below to DHT_TYPE_AM2301 —
 * wiring is identical.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "dht.h"
#include "oled.h"

static const char *TAG = "env_display";

#define DHT_PIN   GPIO_NUM_21
#define DHT_TYPE  DHT_TYPE_DHT11

void app_main(void)
{
    gpio_reset_pin(DHT_PIN);
    gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT_OD);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    oled_init();
    oled_text1206(0, 0, "DHT11 TEMP/HUM");
    oled_update();

    while (1) {
        float humidity = 0, temperature = 0;
        if (dht_read_float_data(DHT_TYPE, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.1f C  Hum: %.1f %%", temperature, humidity);
            oled_show_env(temperature, humidity);
        } else {
            ESP_LOGW(TAG, "DHT read failed — check wiring (DATA->21, VCC->3V3, GND->GND)");
            oled_show_error();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
