#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t mqtt_client_init(void);

esp_err_t mqtt_client_publish(float accel_x, float accel_y, float accel_z,
                               float temp, float humi);

bool mqtt_client_is_connected(void);
