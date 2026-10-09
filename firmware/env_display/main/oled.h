/* Minimal SSD1306 128x64 driver on the new I2C master API.
 *
 * Framebuffer model: 8 pages x 128 columns, flushed per page with
 * page addressing. Text uses the vendored 12x6 font (oled_font.*),
 * each glyph occupying 2 pages; oled_text1206() draws at 7 px pitch.
 */
#pragma once

#include <stdint.h>
#include <time.h>
#include "esp_err.h"
#include "geo.h"
#include "weather.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

void oled_init(void);
void oled_clear(void);
void oled_text1206(uint8_t x, uint8_t page, const char *s);
void oled_update(void);
void oled_show_env(float temperature, float humidity);
void oled_show_error(void);
void oled_show_all(float temperature, float humidity, const struct tm *t, bool env_ok, const char *zone);
void oled_show_wifi(const char *ssid, bool connected, int rssi_dbm, const char *quality);
void oled_show_geo(const geo_fix_t *fix);
void oled_show_stats(float temperature, float humidity);
void oled_show_wx(const wx_t *wx);
