/* Button-controlled light shows: each BOOT press jumps to a random show.
 *
 * Uses the onboard BOOT button (GPIO0 -> GND when pressed): zero external
 * wiring, so wiring faults are impossible. Internal pull-up is enabled;
 * pressed reads as LOW with 50 ms debounce, one show-change per click.
 * NOTE: GPIO0 is a strapping pin — holding BOOT during reset/power-up
 * enters download mode instead of booting the app. Press it only while
 * the app is already running; if the board ever seems stuck, tap EN/RST
 * without touching BOOT.
 *
 * Shows (traffic module on GPIO 21/22/23, verified by pin_sweep):
 *   0 CHASE    - single lamp running R->Y->G, 400 ms each
 *   1 TRAFFIC  - classic sequence R 2 s -> Y 1 s -> G 2 s
 *   2 BLINK    - all lamps together, 500 ms on/off
 *   3 BUILD-UP - R, then R+Y, then all, hold, off
 *   4 PING-PONG- R->Y->G->Y->R bounce, 350 ms each
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

static void set_lamps(int r, int y, int g)
{
    gpio_set_level(LED_RED, r);
    gpio_set_level(LED_YELLOW, y);
    gpio_set_level(LED_GREEN, g);
}

/* Returns true if a debounced press completed (down 50 ms + released). */
static bool poll_press(void)
{
    static int low_streak = 0;
    if (gpio_get_level(BTN_GPIO) == 0) {
        if (++low_streak >= DEBOUNCE_MS / POLL_MS) {
            while (gpio_get_level(BTN_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(POLL_MS));
            }
            low_streak = 0;
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            return true;
        }
    } else {
        low_streak = 0;
    }
    return false;
}

/* Sleep that aborts early (true) when the button is pressed. */
static bool wait_ms(uint32_t ms)
{
    uint32_t waited = 0;
    while (waited < ms) {
        if (poll_press()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        waited += POLL_MS;
    }
    return false;
}

static bool show_chase(void)
{
    const int seq[][3] = { {1,0,0}, {0,1,0}, {0,0,1} };
    for (int i = 0; i < 3; i++) {
        set_lamps(seq[i][0], seq[i][1], seq[i][2]);
        if (wait_ms(400)) {
            return true;
        }
    }
    return false;
}

static bool show_traffic(void)
{
    set_lamps(1, 0, 0);
    if (wait_ms(2000)) {
        return true;
    }
    set_lamps(0, 1, 0);
    if (wait_ms(1000)) {
        return true;
    }
    set_lamps(0, 0, 1);
    if (wait_ms(2000)) {
        return true;
    }
    return false;
}

static bool show_blink(void)
{
    set_lamps(1, 1, 1);
    if (wait_ms(500)) {
        return true;
    }
    set_lamps(0, 0, 0);
    if (wait_ms(500)) {
        return true;
    }
    return false;
}

static bool show_buildup(void)
{
    const int seq[][3] = { {1,0,0}, {1,1,0}, {1,1,1}, {1,1,1}, {0,0,0} };
    const uint32_t hold[] = { 800, 800, 1000, 1000, 500 };
    for (int i = 0; i < 5; i++) {
        set_lamps(seq[i][0], seq[i][1], seq[i][2]);
        if (wait_ms(hold[i])) {
            return true;
        }
    }
    return false;
}

static bool show_pingpong(void)
{
    const int seq[][3] = { {1,0,0}, {0,1,0}, {0,0,1}, {0,1,0} };
    for (int i = 0; i < 4; i++) {
        set_lamps(seq[i][0], seq[i][1], seq[i][2]);
        if (wait_ms(350)) {
            return true;
        }
    }
    return false;
}

typedef bool (*show_fn_t)(void);

static const struct { show_fn_t fn; const char *name; } SHOWS[] = {
    { show_chase, "CHASE" },
    { show_traffic, "TRAFFIC" },
    { show_blink, "BLINK" },
    { show_buildup, "BUILD-UP" },
    { show_pingpong, "PING-PONG" },
};
#define SHOW_COUNT (sizeof(SHOWS) / sizeof(SHOWS[0]))

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
    ESP_LOGI(TAG, "Show %d: %s — press BOOT to change", current, SHOWS[current].name);

    while (1) {
        /* Run the current show until a press interrupts it. */
        while (!SHOWS[current].fn()) {
        }
        int next;
        do {
            next = esp_random() % SHOW_COUNT;
        } while (next == current);
        current = next;
        ESP_LOGI(TAG, "Show %d: %s — press BOOT to change", current, SHOWS[current].name);
    }
}
