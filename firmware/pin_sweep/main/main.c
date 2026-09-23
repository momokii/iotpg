/* Pin sweeper — finds which GPIO a breadboard lamp is wired to.
 *
 * Drives each candidate GPIO HIGH for 4 seconds in turn, forever, logging the
 * active pin. Watch the lamp: when it lights steadily, the pin in the latest
 * "GPIO x ON" monitor line is your lamp pin.
 *
 * Candidate set avoids strapping pins (0, 2, 12, 15), UART (1, 3) and
 * SPI-flash pins (6-11) so sweeping can never break boot or serial.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "pin_sweep";

static const gpio_num_t PINS[] = {
    GPIO_NUM_5,
    GPIO_NUM_4, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_16, GPIO_NUM_17,
    GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23,
    GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_32, GPIO_NUM_33,
};
#define PIN_COUNT (sizeof(PINS) / sizeof(PINS[0]))
#define HOLD_TIME pdMS_TO_TICKS(4000)

void app_main(void)
{
    for (size_t i = 0; i < PIN_COUNT; i++) {
        gpio_reset_pin(PINS[i]);
        gpio_set_direction(PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(PINS[i], 0);
    }

    ESP_LOGI(TAG, "Sweeping %d pins, 4s each. Watch the lamp!", (int)PIN_COUNT);

    while (1) {
        for (size_t i = 0; i < PIN_COUNT; i++) {
            ESP_LOGI(TAG, ">>> GPIO %d ON (4s) <<<", (int)PINS[i]);
            gpio_set_level(PINS[i], 1);
            vTaskDelay(HOLD_TIME);
            gpio_set_level(PINS[i], 0);
        }
    }
}
