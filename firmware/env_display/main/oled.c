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
#include "geo.h"
#include "stats.h"
#include "stats.h"

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

void oled_show_all(float temperature, float humidity, const struct tm *t, bool env_ok, const char *zone)
{
    char hm[8], date[24], env[20];
    int HH = t->tm_hour, MM = t->tm_min;
    int DD = t->tm_mday, MO = t->tm_mon + 1;
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
    snprintf(hm, sizeof(hm), "%02d:%02d", HH, MM);
    snprintf(date, sizeof(date), "%02d-%02d %.3s %s", DD, MO, zone, envclock_source());
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
        snprintf(d8, sizeof(d8), "%02d-%02d", DD, MO);
        snprintf(line0, sizeof(line0), "%-16.16s", env);
        snprintf(line1, sizeof(line1), "%.5s%.3s %.5s", hm, zone, d8);
        lcd1602_clock(line0, line1);
    }
}

void oled_show_wifi(const char *ssid, bool connected, int rssi_dbm, const char *quality)
{
    char l0[24], l1[32];
    (void)rssi_dbm;
    if (connected) {
        snprintf(l0, sizeof(l0), "%.16s", ssid);
        snprintf(l1, sizeof(l1), "Signal: %s", quality);
    } else {
        snprintf(l0, sizeof(l0), "%.16s", ssid[0] ? ssid : "(no wifi set)");
        snprintf(l1, sizeof(l1), "Not connected");
    }
    if (s_ok) {
        oled_text1206(0, 0, "                ");
        oled_text1206(0, 0, "WIFI");
        oled_text57(0, 2, l0);
        oled_text57(0, 3, l1);
        for (uint8_t p = 4; p < 8; p++) {
            memset(&s_fb[p][0], 0, 128);
        }
        oled_update();
    }
    if (s_lcd_ok) {
        char line0[17], line1[17];
        snprintf(line0, sizeof(line0), "%-16.16s", l0);
        snprintf(line1, sizeof(line1), "%-16.16s", l1);
        lcd1602_clock(line0, line1);
    }
}

/* Marquee: long place names scroll in a width-sized window, bouncing
 * with a short dwell at each end so both edges stay readable. Each
 * screen width scrolls on its own state so the 16-char LCD and the
 * 21-char OLED stay in sync with themselves, not each other. */
typedef struct {
    int pos;
    int dir;
    int dwell;
} marquee_t;

static void marquee(char *dst, size_t dstsz, const char *text, size_t width, marquee_t *m)
{
    size_t len = strlen(text);
    if (len <= width) {
        snprintf(dst, dstsz, "%s", text);
        m->pos = 0;
        m->dir = 1;
        m->dwell = 0;
        return;
    }
    if (m->dwell > 0) {
        m->dwell--;
    } else {
        m->pos += m->dir;
        if (m->pos <= 0) {
            m->pos = 0;
            m->dir = 1;
            m->dwell = 2;
        } else if ((size_t)m->pos + width >= len) {
            m->pos = (int)(len - width);
            m->dir = -1;
            m->dwell = 2;
        }
    }
    size_t n = width < dstsz - 1 ? width : dstsz - 1;
    memcpy(dst, text + m->pos, n);
    dst[n] = '\0';
}

void oled_show_geo(const geo_fix_t *fix)
{
    /* Info line alternates every 4 s: human words first (Near 5m ago),
     * then what the accuracy figure means (within 25km) — so the km
     * number always arrives with its explanation attached. */
    static int gcalls = 0;
    static marquee_t m_big = { 0, 1, 0 };
    static marquee_t m_small = { 0, 1, 0 };
    char l0[24], l1[32], l2[32];
    if (!fix->has_fix) {
        snprintf(l0, sizeof(l0), "Finding you");
        snprintf(l1, sizeof(l1), "one moment");
        snprintf(l2, sizeof(l2), "                ");
        gcalls = 0;
    } else {
        long age = (long)(time(NULL) - fix->updated);
        char agestr[12];
        if (age < 0) {
            snprintf(agestr, sizeof(agestr), "--");
        } else if (age < 60) {
            snprintf(agestr, sizeof(agestr), "just now");
        } else if (age < 3600) {
            snprintf(agestr, sizeof(agestr), "%ldm ago", age / 60);
        } else {
            snprintf(agestr, sizeof(agestr), "%ldh ago", age / 3600);
        }
        const char *accword = "Rough";
        if (fix->accuracy_m < 100) {
            accword = "Here";
        } else if (fix->accuracy_m < 1000) {
            accword = "Near";
        } else if (fix->accuracy_m < 10000) {
            accword = "Around";
        }
        char place[GEO_PLACE_LEN + 5];
        snprintf(place, sizeof(place), "%s%s", fix->stale ? "OLD " : "", fix->place);
        char info[32];
        snprintf(info, sizeof(info), "%s %s", accword, agestr);
        char win_big[24], win_small[20];
        marquee(win_big, sizeof(win_big), place, 21, &m_big);
        marquee(win_small, sizeof(win_small), place, 16, &m_small);
        char within[20];
        if (fix->accuracy_m >= 1000) {
            snprintf(within, sizeof(within), "within %.0fkm", (double)fix->accuracy_m / 1000);
        } else {
            snprintf(within, sizeof(within), "within %dm", (int)fix->accuracy_m);
        }
        snprintf(l0, sizeof(l0), "%s", win_big);
        snprintf(l1, sizeof(l1), "%s", ((gcalls / 4) % 2 == 0) ? info : within);
        snprintf(l2, sizeof(l2), "%s", win_small);
        gcalls++;
    }
    if (s_ok) {
        oled_text1206(0, 0, "                ");
        oled_text1206(0, 0, "WHERE");
        oled_text57(0, 2, l0);
        oled_text57(0, 3, l1);
        for (uint8_t p = 4; p < 8; p++) {
            memset(&s_fb[p][0], 0, 128);
        }
        oled_update();
    }
    if (s_lcd_ok) {
        char line0[17], line1[17];
        if (!fix->has_fix) {
            snprintf(line0, sizeof(line0), "no fix yet      ");
            snprintf(line1, sizeof(line1), "wait for lookup ");
        } else {
            snprintf(line0, sizeof(line0), "%-16.16s", l2);
            snprintf(line1, sizeof(line1), "%-16.16s", l1);
        }
        lcd1602_clock(line0, line1);
    }
}

void oled_show_stats(float temperature, float humidity)
{
    /* Two views alternate every 4 s so 16 columns stay readable:
     * view A = today's temp range plus trend, view B = humidity range
     * plus dew point. Words, not codes: Today / Feels / Dew point. */
    /* Three views rotate every 4 s so 16 columns stay readable: view A =
     * today's temp range plus trend, view B = humidity range plus dew
     * point, view C = which place these readings belong to, with its
     * freshness — so stats never silently borrow a stale location.
     * Words, not codes. */
    static int calls = 0;
    char l0[24], l1[32], l2[32];
    if (!stats_have()) {
        snprintf(l0, sizeof(l0), "warming up");
        snprintf(l1, sizeof(l1), "wait for reads");
        snprintf(l2, sizeof(l2), "                ");
    } else {
        time_t tmax_t = 0, tmin_t = 0;
        float tmax = stats_tmax(&tmax_t), tmin = stats_tmin(&tmin_t);
        float hmax = stats_hmax(NULL), hmin = stats_hmin(NULL);
        float dew = stats_dew_point(temperature, humidity);
        const char *word = stats_comfort_word(stats_comfort(temperature, humidity));
        char tr = stats_trend(temperature);
        struct tm a = { 0 }, b = { 0 };
        localtime_r(&tmax_t, &a);
        localtime_r(&tmin_t, &b);
        if ((calls / 4) % 3 == 0) {
            snprintf(l0, sizeof(l0), "Today %.0f-%.0fC %c", tmin, tmax, tr);
            snprintf(l1, sizeof(l1), "Feels %s", word);
        } else if ((calls / 4) % 3 == 1) {
            snprintf(l0, sizeof(l0), "Hum %.0f-%.0f%%", hmin, hmax);
            snprintf(l1, sizeof(l1), "Dew point %.0fC", (double)dew);
        } else {
            geo_fix_t fix = { 0 };
            geo_get(&fix);
            if (!fix.has_fix) {
                snprintf(l0, sizeof(l0), "@?");
                snprintf(l1, sizeof(l1), "unplaced yet");
            } else {
                char city[20] = { 0 };
                size_t ci = 0;
                while (fix.place[ci] && fix.place[ci] != ',' && ci < sizeof(city) - 1) {
                    city[ci] = fix.place[ci];
                    ci++;
                }
                city[ci] = '\0';
                long age = (long)(time(NULL) - fix.updated);
                char agestr[12];
                if (age < 0) {
                    snprintf(agestr, sizeof(agestr), "--");
                } else if (age < 60) {
                    snprintf(agestr, sizeof(agestr), "just now");
                } else if (age < 3600) {
                    snprintf(agestr, sizeof(agestr), "%ldm ago", age / 60);
                } else {
                    snprintf(agestr, sizeof(agestr), "%ldh ago", age / 3600);
                }
                snprintf(l0, sizeof(l0), "@%s", city[0] ? city : "?");
                snprintf(l1, sizeof(l1), "%s%s", fix.stale ? "OLD " : "", agestr);
            }
        }
        snprintf(l2, sizeof(l2), "High %02d:%02d Low %02d:%02d",
                 a.tm_hour, a.tm_min, b.tm_hour, b.tm_min);
    }
    calls++;
    if (s_ok) {
        oled_text1206(0, 0, "                ");
        oled_text1206(0, 0, "TODAY");
        oled_text57(0, 2, l0);
        oled_text57(0, 3, l1);
        oled_text57(0, 4, l2);
        for (uint8_t p = 5; p < 8; p++) {
            memset(&s_fb[p][0], 0, 128);
        }
        oled_update();
    }
    if (s_lcd_ok) {
        char line0[17], line1[17];
        if (!stats_have()) {
            snprintf(line0, sizeof(line0), "warming up      ");
            snprintf(line1, sizeof(line1), "wait for reads  ");
        } else {
            snprintf(line0, sizeof(line0), "%-16.16s", l0);
            snprintf(line1, sizeof(line1), "%-16.16s", l1);
        }
        lcd1602_clock(line0, line1);
    }
}

void oled_show_wx(const wx_t *wx)
{
    char l0[24], l1[32], l2[32];
    if (!wx->has_data) {
        snprintf(l0, sizeof(l0), "no sky yet");
        snprintf(l1, sizeof(l1), "wait for fetch");
        snprintf(l2, sizeof(l2), "                ");
    } else {
        snprintf(l0, sizeof(l0), "Out %.0fC %s", (double)wx->temp, wx->cond);
        snprintf(l1, sizeof(l1), "Rain %d%%", wx->rain_pct);
        snprintf(l2, sizeof(l2), "Feels %.0fC%s", (double)wx->feels_like,
                 wx->stale ? " OLD" : "");
    }
    if (s_ok) {
        oled_text1206(0, 0, "                ");
        oled_text1206(0, 0, "OUTSIDE");
        oled_text57(0, 2, l0);
        oled_text57(0, 3, l1);
        oled_text57(0, 4, l2);
        for (uint8_t p = 5; p < 8; p++) {
            memset(&s_fb[p][0], 0, 128);
        }
        oled_update();
    }
    if (s_lcd_ok) {
        char line0[17], line1[17];
        if (!wx->has_data) {
            snprintf(line0, sizeof(line0), "no sky yet      ");
            snprintf(line1, sizeof(line1), "wait for fetch  ");
        } else {
            snprintf(line0, sizeof(line0), "%-16.16s", l0);
            snprintf(line1, sizeof(line1), "%-16.16s", l1);
        }
        lcd1602_clock(line0, line1);
    }
}
