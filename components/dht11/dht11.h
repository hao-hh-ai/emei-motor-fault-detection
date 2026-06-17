#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#define DHT11_PIN  GPIO_NUM_18

typedef struct {
    float temperature;  /* °C */
    float humidity;     /* %RH */
} dht11_data_t;

/**
 * @brief  初始化 DHT11 GPIO 为开漏输出
 * @return ESP_OK
 */
esp_err_t dht11_init(void);

/**
 * @brief  读取一次温湿度 (阻塞约 25ms)
 * @param  data  输出温湿度
 * @return ESP_OK / ESP_ERR_TIMEOUT / ESP_ERR_INVALID_CRC
 */
esp_err_t dht11_read(dht11_data_t *data);
