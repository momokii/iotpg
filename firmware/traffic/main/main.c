/* Traffic light cycler: RED 5s -> YELLOW 2s -> GREEN 5s, repeating.
 *
 * Pins GPIO 21/22/23 match the user's traffic-light module wiring
 * (assumed order R/Y/G — confirmed visually, see DECISIONS_LOG).
 * All are non-strapping, non-UART GPIOs verified by pin_sweep,
 * so boot and serial are unaffected.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "traffic";

#define LED_RED     GPIO_NUM_21
#define LED_YELLOW  GPIO_NUM_22
#define LED_GREEN   GPIO_NUM_23

#define RED_TIME_MS     2000
#define YELLOW_TIME_MS  2000
#define GREEN_TIME_MS   2000

static void show(const char *name, gpio_num_t pin, uint32_t ms)
{
    ESP_LOGI(TAG, "%s ON (%lu ms)", name, (unsigned long)ms);
    gpio_set_level(LED_RED, pin == LED_RED);
    gpio_set_level(LED_YELLOW, pin == LED_YELLOW);
    gpio_set_level(LED_GREEN, pin == LED_GREEN);
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void app_main(void)
{
    const gpio_num_t pins[] = { LED_RED, LED_YELLOW, LED_GREEN };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }
    ESP_LOGI(TAG, "Traffic light running: R=%ds Y=%ds G=%ds",
             RED_TIME_MS / 1000, YELLOW_TIME_MS / 1000, GREEN_TIME_MS / 1000);

    while (1) {
        show("RED", LED_RED, RED_TIME_MS);
        show("YELLOW", LED_YELLOW, YELLOW_TIME_MS);
        show("GREEN", LED_GREEN, GREEN_TIME_MS);
    }
}
