/* Display layer: SSD1306 OLED and/or 1602 LCD with PCF8574 backpack.
 *
 * Auto-discovers every display: probes ordered SDA/SCL pairs among the
 * candidate pins for known addresses (SSD1306 at 0x3C/0x3D, PCF8574 at
 * 0x27/0x3F) and initializes each kind found, so text is mirrored to all
 * attached screens. Shield labels proved untrustworthy, so the firmware
 * finds displays wherever they are instead of believing any fixed mapping.
 * DHT + display sharing one pin (seen: GPIO21) works but is fragile —
 * prefer one signal per pin.
 */
#include <string.h>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "oled.h"
#include "oled_font.h"
#include "font57.h"
#include "lcd1602.h"
#include "rtc.h"

#define OLED_SDA_PIN 22
#define OLED_SCL_PIN 23
#define OLED_ADDR    0x3C

static const int CAND_PINS[] = { 4, 5, 12, 13, 14, 21, 22, 23 };
static const uint8_t SSD_ADDRS[] = { 0x3C, 0x3D };
static const uint8_t LCD_ADDRS[] = { 0x27, 0x3F };
static const uint8_t RTC_ADDR = 0x68;

static const char *TAG = "oled";
static i2c_master_dev_handle_t s_dev;
static bool s_ok = false;
static bool s_lcd_ok = false;
static uint8_t s_fb[8][128];

/* Probe one SDA/SCL pair for any known display address; returns it or 0.
 * Each candidate address is probed twice — a single ACK on a shared or
 * flaky bus is not trusted. */
static uint8_t probe_pair(i2c_master_bus_handle_t bus)
{
    for (size_t k = 0; k < sizeof(SSD_ADDRS); k++) {
        if (i2c_master_probe(bus, SSD_ADDRS[k], 20) == ESP_OK &&
            i2c_master_probe(bus, SSD_ADDRS[k], 20) == ESP_OK) {
            return SSD_ADDRS[k];
        }
    }
    for (size_t k = 0; k < sizeof(LCD_ADDRS); k++) {
        if (i2c_master_probe(bus, LCD_ADDRS[k], 20) == ESP_OK &&
            i2c_master_probe(bus, LCD_ADDRS[k], 20) == ESP_OK) {
            return LCD_ADDRS[k];
        }
    }
    return 0;
}

static i2c_master_bus_handle_t open_bus(int sda, int scl)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        return NULL;
    }
    return bus;
}

static esp_err_t write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), -1);
}

static esp_err_t init_sequence(void)
{
    const uint8_t seq[] = {
        0xAE,             /* display off */
        0x20, 0x02,       /* page addressing mode */
        0xB0,             /* page 0 */
        0xC8,             /* COM scan direction: remapped (flip vertically) */
        0x00,             /* low column address */
        0x10,             /* high column address */
        0x40,             /* start line 0 */
        0x81, 0x7F,       /* contrast */
        0xA1,             /* segment remap (flip horizontally) */
        0xA6,             /* normal display (not inverted) */
        0xA8, 0x3F,       /* multiplex ratio 1/64 */
        0xA4,             /* display follows RAM */
        0xD3, 0x00,       /* display offset 0 */
        0xD5, 0xF0,       /* clock divide / oscillator */
        0xD9, 0x22,       /* pre-charge period */
        0xDA, 0x12,       /* COM pins hardware config */
        0xDB, 0x20,       /* VCOMH deselect level */
        0x8D, 0x14,       /* charge pump on */
        0xAF,             /* display on */
    };
    for (size_t i = 0; i < sizeof(seq); i++) {
        esp_err_t err = write_cmd(seq[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static void init_ssd1306(int sda, int scl, uint8_t addr)
{
    i2c_master_bus_handle_t bus = open_bus(sda, scl);
    if (bus == NULL) {
        return;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &s_dev) != ESP_OK) {
        i2c_del_master_bus(bus);
        return;
    }
    if (init_sequence() != ESP_OK) {
        ESP_LOGW(TAG, "SSD1306 init failed at 0x%02X — OLED disabled", addr);
        return;
    }
    s_ok = true;
    oled_clear();
    ESP_LOGI(TAG, "SSD1306 ready at 0x%02X (SDA=%d SCL=%d)", addr, sda, scl);
}

static void init_lcd1602(int sda, int scl, uint8_t addr)
{
    i2c_master_bus_handle_t bus = open_bus(sda, scl);
    if (bus == NULL) {
        return;
    }
    lcd1602_init(bus, addr);
    s_lcd_ok = lcd1602_ok();
}

void oled_init(void)
{
    int ssd_sda = -1, ssd_scl = -1, lcd_sda = -1, lcd_scl = -1;
    int rtc_sda = -1, rtc_scl = -1;
    uint8_t ssd_addr = 0, lcd_addr = 0;
    bool rtc_found = false;

    size_t npins = sizeof(CAND_PINS) / sizeof(CAND_PINS[0]);
    for (size_t i = 0; i < npins && (ssd_addr == 0 || lcd_addr == 0 || !rtc_found); i++) {
        for (size_t j = 0; j < npins && (ssd_addr == 0 || lcd_addr == 0 || !rtc_found); j++) {
            if (i == j) {
                continue;
            }
            i2c_master_bus_handle_t bus = open_bus(CAND_PINS[i], CAND_PINS[j]);
            if (bus == NULL) {
                continue;
            }
            uint8_t found = probe_pair(bus);
            if (found != 0) {
                ESP_LOGI(TAG, "Display ACK at 0x%02X on SDA=%d SCL=%d",
                         found, CAND_PINS[i], CAND_PINS[j]);
            }
            for (size_t k = 0; k < sizeof(SSD_ADDRS); k++) {
                if (found == SSD_ADDRS[k] && ssd_addr == 0) {
                    ssd_addr = found;
                    ssd_sda = CAND_PINS[i];
                    ssd_scl = CAND_PINS[j];
                }
            }
            for (size_t k = 0; k < sizeof(LCD_ADDRS); k++) {
                if (found == LCD_ADDRS[k] && lcd_addr == 0) {
                    lcd_addr = found;
                    lcd_sda = CAND_PINS[i];
                    lcd_scl = CAND_PINS[j];
                }
            }
            if (!rtc_found && i2c_master_probe(bus, RTC_ADDR, 20) == ESP_OK &&
                i2c_master_probe(bus, RTC_ADDR, 20) == ESP_OK) {
                ESP_LOGI(TAG, "DS3231 ACK at 0x68 on SDA=%d SCL=%d", CAND_PINS[i], CAND_PINS[j]);
                rtc_found = true;
                rtc_sda = CAND_PINS[i];
                rtc_scl = CAND_PINS[j];
            }
            i2c_del_master_bus(bus);
        }
    }

    if (ssd_addr != 0) {
        init_ssd1306(ssd_sda, ssd_scl, ssd_addr);
    }
    if (lcd_addr != 0) {
        init_lcd1602(lcd_sda, lcd_scl, lcd_addr);
    }
    if (rtc_found) {
        i2c_master_bus_handle_t bus = open_bus(rtc_sda, rtc_scl);
        if (bus != NULL) {
            envclock_attach(bus);
        }
    } else {
        ESP_LOGI(TAG, "no DS3231 on any probed pair — internal time (drifts, resets on power loss)");
    }
    if (ssd_addr == 0 && lcd_addr == 0) {
        ESP_LOGW(TAG, "No display found on any probed SDA/SCL pair — sensor still logs to USB");
    }
}

void oled_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
    oled_update();
}

void oled_text1206(uint8_t x, uint8_t page, const char *s)
{
    if (!s_ok || page > 6) {
        return;
    }
    for (; *s && x + 6 < OLED_WIDTH; s++, x += 7) {
        unsigned char c = (unsigned char)*s;
        if (c < 32 || c > 126) {
            continue;
        }
        const uint8_t *g = c_chFont1206[c - 32];
        memcpy(&s_fb[page][x], g, 6);
        memcpy(&s_fb[page + 1][x], g + 6, 6);
    }
}

void oled_update(void)
{
    if (!s_ok) {
        return;
    }
    uint8_t chunk[1 + 128];
    chunk[0] = 0x40;
    for (uint8_t page = 0; page < 8 && s_ok; page++) {
        memcpy(&chunk[1], s_fb[page], 128);
        if (write_cmd(0xB0 | page) != ESP_OK || write_cmd(0x00) != ESP_OK ||
            write_cmd(0x10) != ESP_OK ||
            i2c_master_transmit(s_dev, chunk, sizeof(chunk), -1) != ESP_OK) {
            s_ok = false;
            ESP_LOGW(TAG, "OLED write failed mid-frame — OLED disabled");
            return;
        }
    }
}

/* Small text: 5x7 glyphs at 6 px pitch, one page per row. Unknown glyphs
 * render as blank so a missing table entry degrades visibly, not weirdly. */
static void oled_text57(uint8_t x, uint8_t page, const char *s)
{
    if (!s_ok || page > 7) {
        return;
    }
    for (; *s && x + 5 < OLED_WIDTH; s++, x += 6) {
        const uint8_t *g = NULL;
        for (size_t i = 0; i < FONT57_COUNT; i++) {
            if (FONT57[i].ch == *s) {
                g = FONT57[i].cols;
                break;
            }
        }
        if (g == NULL) {
            memset(&s_fb[page][x], 0, 5);
        } else {
            memcpy(&s_fb[page][x], g, 5);
        }
        s_fb[page][x + 5] = 0;
    }
}

void oled_show_env(float temperature, float humidity)
{
    char line[32];
    if (s_ok) {
        snprintf(line, sizeof(line), "Temp: %.1f C", temperature);
        oled_text1206(0, 2, "                ");
        oled_text1206(0, 2, line);
        snprintf(line, sizeof(line), "Hum: %.1f %%", humidity);
        oled_text1206(0, 4, "                ");
        oled_text1206(0, 4, line);
        oled_update();
    }
    if (s_lcd_ok) {
        lcd1602_show(temperature, humidity);
    }
}

void oled_show_error(void)
{
    if (s_ok) {
        oled_text1206(0, 2, "DHT ERR         ");
        oled_update();
    }
    if (s_lcd_ok) {
        lcd1602_error();
    }
}

void oled_show_all(float temperature, float humidity, const struct tm *t, bool env_ok)
{
    char hm[8], date[20], env[20];
    int HH = t->tm_hour, MM = t->tm_min;
    int DD = t->tm_mday, MO = t->tm_mon + 1, YY = (t->tm_year + 1900) % 100;
    if (HH < 0 || HH > 23) {
        HH = 0;
    }
    if (MM < 0 || MM > 59) {
        MM = 0;
    }
    if (DD < 1 || DD > 31) {
        DD = 1;
    }
    if (MO < 1 || MO > 12) {
        MO = 1;
    }
    if (YY < 0 || YY > 99) {
        YY = 0;
    }
    snprintf(hm, sizeof(hm), "%02d:%02d", HH, MM);
    snprintf(date, sizeof(date), "%02d-%02d-%02d %s", DD, MO, YY, envclock_source());
    if (env_ok) {
        snprintf(env, sizeof(env), "T:%.1fC H:%.0f%%", temperature, humidity);
    } else {
        snprintf(env, sizeof(env), "DHT ERR");
    }
    if (s_ok) {
        oled_text1206(0, 0, "                ");
        oled_text1206(0, 0, hm);
        oled_text57(0, 2, date);
        oled_text57(0, 3, env);
        for (uint8_t p = 4; p < 8; p++) {
            memset(&s_fb[p][0], 0, 128);
        }
        oled_update();
    }
    if (s_lcd_ok) {
        char line0[17], line1[17], d8[12];
        snprintf(d8, sizeof(d8), "%02d-%02d-%02d", DD, MO, YY);
        snprintf(line0, sizeof(line0), "%-16.16s", env);
        snprintf(line1, sizeof(line1), "%.5s %.8s", hm, d8);
        lcd1602_clock(line0, line1);
    }
}
