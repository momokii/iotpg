/* Minimal SSD1306 128x64 driver on the new I2C master API.
 *
 * Framebuffer model: 8 pages x 128 columns, flushed per page with
 * page addressing. Text uses the vendored 12x6 font (oled_font.*),
 * each glyph occupying 2 pages; oled_text1206() draws at 7 px pitch.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

void oled_init(void);
void oled_clear(void);
void oled_text1206(uint8_t x, uint8_t page, const char *s);
void oled_update(void);
void oled_show_env(float temperature, float humidity);
void oled_show_error(void);
