idf_component_register(SRCS "mqtt_client.c"
                       INCLUDE_DIRS "."
                       REQUIRES mqtt json led_alarm esp_event)
