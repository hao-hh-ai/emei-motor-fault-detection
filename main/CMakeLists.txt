idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES adxl345 dht11 led_alarm wifi_sta mqtt_client nvs_flash esp_wifi)
