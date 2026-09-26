idf_component_register(SRCS "main.c" "oled.c" "oled_font.c" "lcd1602.c" "rtc.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c)
