#include "dht11.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT11";

/* 等待指定电平，超时返回 false */

static bool wait_level(int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(DHT11_PIN) != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return false;
        }
    }
    return true;
}

/* 初始化 */

esp_err_t dht11_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << DHT11_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level(DHT11_PIN, 1);
    ESP_LOGI(TAG, "Init OK, pin=GPIO%d", DHT11_PIN);
    return ESP_OK;
}

/* 读取 */

esp_err_t dht11_read(dht11_data_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    uint8_t buf[5] = {0};

    /* 关键时序段: 关中断避免 FreeRTOS 抢占 */
    portENTER_CRITICAL();

    /* 步骤 1: 主机发送起始信号 (拉低 >18ms) */
    gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_PIN, 0);
    esp_rom_delay_us(20000);  /* 20ms > 18ms */

    /* 释放总线, 等待从机响应 */
    gpio_set_level(DHT11_PIN, 1);
    esp_rom_delay_us(30);

    /* 步骤 2: 切回输入 */
    gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);

    /* DHT11 响应: 先拉低 ~80us 再拉高 ~80us */
    if (!wait_level(0, 100)) {
        portEXIT_CRITICAL();
        ESP_LOGW(TAG, "Response timeout (LOW)");
        return ESP_ERR_TIMEOUT;
    }
    if (!wait_level(1, 100)) {
        portEXIT_CRITICAL();
        ESP_LOGW(TAG, "Response timeout (HIGH)");
        return ESP_ERR_TIMEOUT;
    }

    /* 步骤 3: 读取 40 位数据 */
    for (int i = 0; i < 40; i++) {
        if (!wait_level(0, 80)) {
            portEXIT_CRITICAL();
            ESP_LOGW(TAG, "Data bit %d timeout (LOW)", i);
            return ESP_ERR_TIMEOUT;
        }
        if (!wait_level(1, 80)) {
            portEXIT_CRITICAL();
            ESP_LOGW(TAG, "Data bit %d timeout (HIGH)", i);
            return ESP_ERR_TIMEOUT;
        }
        /* 延迟 35us 后采样: 若仍为高 => bit=1 */
        esp_rom_delay_us(35);
        if (gpio_get_level(DHT11_PIN)) {
            buf[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    portEXIT_CRITICAL();

    /* 步骤 4: CRC 校验 */
    uint8_t sum = buf[0] + buf[1] + buf[2] + buf[3];
    if (sum != buf[4]) {
        ESP_LOGW(TAG, "CRC fail: %d+%d+%d+%d=%d != %d",
                 buf[0], buf[1], buf[2], buf[3], sum, buf[4]);
        return ESP_ERR_INVALID_CRC;
    }

    /* 步骤 5: 解析数据 */
    data->humidity    = (float)buf[0] + buf[1] * 0.1f;
    data->temperature = (float)buf[2] + buf[3] * 0.1f;

    return ESP_OK;
}
