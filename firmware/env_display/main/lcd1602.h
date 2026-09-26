/* 1602 character LCD over PCF8574 I2C backpack (standard wiring:
 * P0=RS, P1=RW, P2=EN, P3=backlight, P4-D4..P7=D7). 4-bit mode.
 */
#pragma once

#include "driver/i2c_master.h"

void lcd1602_init(i2c_master_bus_handle_t bus, uint8_t addr);
bool lcd1602_ok(void);
void lcd1602_show(float temperature, float humidity);
void lcd1602_error(void);
void lcd1602_clock(const char *time, const char *date);
