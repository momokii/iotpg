/* 1602 LCD via PCF8574 backpack. Nibbles go out D4-D7 with an EN strobe;
 * init follows the HD44780 4-bit power-on sequence. Backlight kept on.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "lcd1602.h"

#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

static const char *TAG = "lcd1602";
static i2c_master_dev_handle_t s_dev;

static esp_err_t write_nibble(uint8_t nib, bool rs)
{
    uint8_t base = (uint8_t)(((nib & 0x0F) << 4) | LCD_BL | (rs ? LCD_RS : 0));
    uint8_t seq[2] = { (uint8_t)(base | LCD_EN), base };
    esp_err_t err = i2c_master_transmit(s_dev, &seq[0], 1, -1);
    if (err != ESP_OK) {
        return err;
    }
    esp_rom_delay_us(1);
    return i2c_master_transmit(s_dev, &seq[1], 1, -1);
}

static esp_err_t write_byte(uint8_t v, bool rs)
{
    esp_err_t err = write_nibble(v >> 4, rs);
    if (err != ESP_OK) {
        return err;
    }
    esp_rom_delay_us(50);
    return write_nibble(v & 0x0F, rs);
}

static esp_err_t cmd(uint8_t c)
{
    return write_byte(c, false);
}

static esp_err_t goto_xy(uint8_t col, uint8_t row)
{
    static const uint8_t off[] = { 0x00, 0x40 };
    return cmd((uint8_t)(0x80 | (off[row & 1] + col)));
}

static esp_err_t print(const char *s)
{
    while (*s) {
        esp_err_t err = write_byte((uint8_t)*s++, true);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

void lcd1602_init(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev));
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_ERROR_CHECK(write_nibble(0x03, false));
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_ERROR_CHECK(write_nibble(0x03, false));
    esp_rom_delay_us(150);
    ESP_ERROR_CHECK(write_nibble(0x03, false));
    ESP_ERROR_CHECK(write_nibble(0x02, false));
    ESP_ERROR_CHECK(cmd(0x28));
    ESP_ERROR_CHECK(cmd(0x0C));
    ESP_ERROR_CHECK(cmd(0x06));
    ESP_ERROR_CHECK(cmd(0x01));
    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_LOGI(TAG, "1602 ready at 0x%02X", addr);
}

void lcd1602_show(float temperature, float humidity)
{
    char line[17];
    snprintf(line, sizeof(line), "Temp: %.1fC", temperature);
    ESP_ERROR_CHECK(goto_xy(0, 0));
    ESP_ERROR_CHECK(print(line));
    snprintf(line, sizeof(line), "Hum: %.1f%%", humidity);
    ESP_ERROR_CHECK(goto_xy(0, 1));
    ESP_ERROR_CHECK(print(line));
}

void lcd1602_error(void)
{
    ESP_ERROR_CHECK(goto_xy(0, 0));
    ESP_ERROR_CHECK(print("DHT ERR         "));
}
