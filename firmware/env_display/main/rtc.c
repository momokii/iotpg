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

static const char *TAG = "rtc";
static const int CAND_PINS[] = { 4, 5, 12, 13, 14, 21, 22, 23 };
static const uint8_t RTC_ADDR = 0x68;

static i2c_master_dev_handle_t s_dev;
static bool s_chip = false;

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
    size_t npins = sizeof(CAND_PINS) / sizeof(CAND_PINS[0]);
    for (size_t i = 0; i < npins && !s_chip; i++) {
        for (size_t j = 0; j < npins && !s_chip; j++) {
            if (i == j) {
                continue;
            }
            i2c_master_bus_config_t bus_cfg = {
                .i2c_port = I2C_NUM_0,
                .sda_io_num = CAND_PINS[i],
                .scl_io_num = CAND_PINS[j],
                .clk_source = I2C_CLK_SRC_DEFAULT,
                .glitch_ignore_cnt = 7,
                .flags.enable_internal_pullup = true,
            };
            i2c_master_bus_handle_t bus = NULL;
            if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
                continue;
            }
            if (i2c_master_probe(bus, RTC_ADDR, 20) != ESP_OK) {
                i2c_del_master_bus(bus);
                continue;
            }
            i2c_device_config_t dev_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = RTC_ADDR,
                .scl_speed_hz = 100000,
            };
            if (i2c_master_bus_add_device(bus, &dev_cfg, &s_dev) != ESP_OK) {
                i2c_del_master_bus(bus);
                continue;
            }
            ESP_LOGI(TAG, "DS3231 found at 0x68 on SDA=%d SCL=%d",
                     CAND_PINS[i], CAND_PINS[j]);
            s_chip = true;
        }
    }
    if (!s_chip) {
        ESP_LOGI(TAG, "no DS3231 on any probed pair — internal time (drifts, resets on power loss)");
    }
    seed_from_build();
    if (s_chip) {
        struct tm t = { 0 };
        if (!chip_read(&t)) {
            chip_write_build();
        }
    }
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
    return s_chip ? "RTC" : "INT";
}
