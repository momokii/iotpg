/* DS3231 (0x68) with fallback to build-stamp time. See rtc.h.
 *
 * DS3231 registers are BCD: seconds, minutes, hours (forced 24 h),
 * day-of-week, date, month, year-2000. Valid = year within 2024-2099.
 */
#include <string.h>
#include <sys/time.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "rtc.h"

static i2c_master_dev_handle_t s_dev;
static bool s_chip = false;
static bool s_ntp = false;
static const char *TAG = "rtc";

static uint8_t bcd2bin(uint8_t b)
{
    return (uint8_t)(((b >> 4) * 10) + (b & 0x0F));
}

static uint8_t bin2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static int month_num(const char *m)
{
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (int i = 0; i < 12; i++) {
        if (memcmp(m, names[i], 3) == 0) {
            return i;
        }
    }
    return 0;
}

/* Seed the internal clock from the firmware build timestamp (__DATE__ like
 * "Sep 26 2026", __TIME__ like "11:44:10"). Accuracy: whenever it was
 * flashed; drift after that is the internal oscillator's problem. */
static void seed_from_build(void)
{
    char mon[4] = { 0 };
    int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
    struct tm t = { 0 };
    t.tm_mon = month_num(mon);
    t.tm_mday = day;
    t.tm_year = year - 1900;
    t.tm_hour = hh;
    t.tm_min = mm;
    t.tm_sec = ss;
    struct timeval tv = { .tv_sec = mktime(&t), .tv_usec = 0 };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "internal time seeded from build stamp");
}

static bool chip_read(struct tm *out)
{
    uint8_t reg = 0x00;
    uint8_t raw[7] = { 0 };
    if (i2c_master_transmit_receive(s_dev, &reg, 1, raw, sizeof(raw), -1) != ESP_OK) {
        return false;
    }
    out->tm_sec = bcd2bin((uint8_t)(raw[0] & 0x7F));
    out->tm_min = bcd2bin((uint8_t)(raw[1] & 0x7F));
    out->tm_hour = bcd2bin((uint8_t)(raw[2] & 0x3F));
    out->tm_wday = bcd2bin((uint8_t)(raw[3] & 0x07));
    out->tm_mday = bcd2bin((uint8_t)(raw[4] & 0x3F));
    out->tm_mon = bcd2bin((uint8_t)(raw[5] & 0x1F)) - 1;
    out->tm_year = bcd2bin(raw[6]) + 100;
    return out->tm_year >= 124 && out->tm_year < 200;
}

static void chip_write_build(void)
{
    char mon[4] = { 0 };
    int day = 0, year = 0, hh = 0, mm = 0, ss = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
    uint8_t buf[8] = {
        0x00,
        bin2bcd((uint8_t)(ss % 60)),
        bin2bcd((uint8_t)(mm % 60)),
        bin2bcd((uint8_t)(hh % 24)),
        0x01,
        bin2bcd((uint8_t)day),
        bin2bcd((uint8_t)(month_num(mon) + 1)),
        bin2bcd((uint8_t)((year - 2000) % 100)),
    };
    if (i2c_master_transmit(s_dev, buf, sizeof(buf), -1) == ESP_OK) {
        ESP_LOGI(TAG, "DS3231 had no valid time — wrote build stamp to chip");
    }
}

void envclock_init(void)
{
    /* No hardware of our own: the display layer (oled.c) scans the bus
     * pairs once and hands us a bus via envclock_attach() when it finds
     * a DS3231. Until then (or forever, without the chip) time runs on
     * the internal clock seeded below. */
    seed_from_build();
}

void envclock_attach(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x68,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &s_dev) != ESP_OK) {
        return;
    }
    struct tm t = { 0 };
    if (!chip_read(&t)) {
        chip_write_build();
        if (!chip_read(&t)) {
            return;
        }
    }
    s_chip = true;
    ESP_LOGI(TAG, "DS3231 active");
}

bool envclock_now(struct tm *out)
{
    if (s_chip) {
        struct tm t = { 0 };
        if (chip_read(&t)) {
            *out = t;
            return true;
        }
    }
    time_t now = time(NULL);
    localtime_r(&now, out);
    return true;
}

const char *envclock_source(void)
{
    if (s_chip) {
        return "RTC";
    }
    return s_ntp ? "NTP" : "INT";
}

void envclock_note_ntp_sync(void)
{
    s_ntp = true;
}
