#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t mqtt_client_init(void);

/**
 * @brief  批量上报原始采样数据 (JSON 数组)
 * @param buf_x/y/z  三轴 int16 原始值 (3.9mg/LSB)
 * @param count      样本数
 * @param temp/humi  温湿度
 */
esp_err_t mqtt_client_publish_batch(const int16_t *buf_x,
                                     const int16_t *buf_y,
                                     const int16_t *buf_z,
                                     int count, int sample_rate_hz,
                                     float temp, float humi);

bool mqtt_client_is_connected(void);
