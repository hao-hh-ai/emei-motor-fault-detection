#pragma once

#include "esp_err.h"

/**
 * @brief  WiFi STA 模式初始化 (自动重连)
 * @return ESP_OK 成功
 */
esp_err_t wifi_sta_init(void);
