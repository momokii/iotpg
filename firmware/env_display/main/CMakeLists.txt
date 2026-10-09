idf_component_register(SRCS "main.c" "oled.c" "oled_font.c" "lcd1602.c" "rtc.c" "wifi.c" "geo.c" "stats.c" "weather.c" "dashboard.c" "diag.c" INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_wifi esp_netif esp-tls esp_event esp_timer nvs_flash esp_http_client esp_http_server cjson
                       EMBED_TXTFILES "dashboard.html")
