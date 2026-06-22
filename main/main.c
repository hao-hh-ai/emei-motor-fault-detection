#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adxl345.h"
#include "dht11.h"
#include "led_alarm.h"
#include "wifi_sta.h"
#include "mqtt_app.h"

static const char *TAG = "MAIN";

static bool wifi_is_connected(void)
{
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== 峨眉派 电机故障检测系统 ===");

    /* NVS 初始化 */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* I2C + 传感器 */
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = ADXL345_I2C_SDA,
        .scl_io_num = ADXL345_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &i2c_bus));

    i2c_master_dev_handle_t adxl_dev;
    ret = adxl345_init(i2c_bus, &adxl_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADXL345 未检测到! 检查接线.");
    }

    dht11_init();
    led_alarm_init();

    /* WiFi + MQTT */
    ESP_LOGI(TAG, "WiFi 连接 %s ...", CONFIG_WIFI_SSID);
    wifi_sta_init();

    ESP_LOGI(TAG, "MQTT 连接 %s ...", CONFIG_MQTT_BROKER_URI);
    mqtt_client_init();

    /* 主循环 */
    int tick = 0;
    while (1) {
        ++tick;

        /* 每 10 秒输出状态总览 */
        if (tick % 10 == 1 || tick == 1) {
            ESP_LOGI(TAG, "状态 | WiFi:%s  MQTT:%s  Tick:%d",
                     wifi_is_connected() ? "在线" : "离线",
                     mqtt_client_is_connected() ? "在线" : "离线",
                     tick);
        }

        /* 读 ADXL345 */
        adxl345_data_t accel;
        esp_err_t acc_ret = adxl345_read(adxl_dev, &accel);
        if (acc_ret == ESP_OK) {
            ESP_LOGI(TAG, "ACCEL: X=%.3f Y=%.3f Z=%.3f", accel.x, accel.y, accel.z);
        }

        /* 读 DHT11 */
        dht11_data_t dht;
        esp_err_t dht_ret = dht11_read(&dht);
        if (dht_ret == ESP_OK) {
            ESP_LOGI(TAG, "DHT11: T=%.1f C  H=%.1f %%", dht.temperature, dht.humidity);
        }

        /* MQTT 上报 (不管连没连都尝试发) */
        mqtt_client_publish(accel.x, accel.y, accel.z,
                            dht.temperature, dht.humidity);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
