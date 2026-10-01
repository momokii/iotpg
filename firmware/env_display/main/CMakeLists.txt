idf_component_register(SRCS "main.c" "oled.c" "oled_font.c" "lcd1602.c" "rtc.c" "wifi.c" "geo.c" "stats.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp-tls esp_event nvs_flash esp_http_client cjson)
