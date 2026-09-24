/* Button-controlled lights: each press jumps to a random combination.
 *
 * Uses the onboard BOOT button (GPIO0 -> GND when pressed): zero external
 * wiring, so wiring faults are impossible. Internal pull-up is enabled;
 * pressed reads as LOW. Debounce: 50 ms stable-low required, press
 * registered once per physical click via release-wait.
 * NOTE: GPIO0 is a strapping pin — holding BOOT during reset/power-up
 * enters download mode instead of booting the app. Press it only while
 * the app is already running; if the board ever seems stuck, tap EN/RST
 * without touching BOOT.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "button_lights";

#define LED_RED     GPIO_NUM_21
#define LED_YELLOW  GPIO_NUM_22
#define LED_GREEN   GPIO_NUM_23
#define BTN_GPIO    GPIO_NUM_0

#define DEBOUNCE_MS 50
#define POLL_MS     10

typedef struct { int r, y, g; const char *name; } combo_t;

static const combo_t COMBOS[] = {
    { 1, 0, 0, "RED" },
    { 0, 1, 0, "YELLOW" },
    { 0, 0, 1, "GREEN" },
    { 1, 1, 0, "RED+YELLOW" },
    { 1, 1, 1, "ALL ON" },
};
#define COMBO_COUNT (sizeof(COMBOS) / sizeof(COMBOS[0]))

static void apply_combo(int idx)
{
    gpio_set_level(LED_RED, COMBOS[idx].r);
    gpio_set_level(LED_YELLOW, COMBOS[idx].y);
    gpio_set_level(LED_GREEN, COMBOS[idx].g);
    ESP_LOGI(TAG, "Combo %d: %s", idx, COMBOS[idx].name);
}

void app_main(void)
{
    const gpio_num_t leds[] = { LED_RED, LED_YELLOW, LED_GREEN };
    for (size_t i = 0; i < sizeof(leds) / sizeof(leds[0]); i++) {
        gpio_reset_pin(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(leds[i], 0);
    }

    gpio_reset_pin(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN_GPIO, GPIO_PULLUP_ONLY);

    int current = 0;
    apply_combo(current);
    ESP_LOGI(TAG, "Press the BOOT button to change combo");

    int low_streak = 0;
    int ticks = 0;
    while (1) {
        int level = gpio_get_level(BTN_GPIO);
        if ((++ticks % (1000 / POLL_MS)) == 0) {
            ESP_LOGI(TAG, "DBG button level=%d combo=%d (%s)",
                     level, current, COMBOS[current].name);
        }
        if (level == 0) {
            low_streak++;
        } else {
            low_streak = 0;
        }
        if (low_streak >= DEBOUNCE_MS / POLL_MS) {
            int next;
            do {
                next = esp_random() % COMBO_COUNT;
            } while (next == current);
            current = next;
            apply_combo(current);
            while (gpio_get_level(BTN_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(POLL_MS));
            }
            low_streak = 0;
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
