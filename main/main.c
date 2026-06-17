#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adxl345.h"
#include "dht11.h"
#include "led_alarm.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sensor Test Start ===");

    /* I2C 总线初始化 */
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

    /* ADXL345 初始化 */
    i2c_master_dev_handle_t adxl_dev;
    esp_err_t ret = adxl345_init(i2c_bus, &adxl_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADXL345 not found! Check wiring.");
    }

    /* DHT11 初始化 */
    dht11_init();

    /* LED 初始化 */
    led_alarm_init();

    /* 主循环: 每秒读一次传感器 */
    int count = 0;
    while (1) {
        ESP_LOGI(TAG, "--- Tick %d ---", ++count);

        /* 读 ADXL345 */
        adxl345_data_t accel;
        if (adxl345_read(adxl_dev, &accel) == ESP_OK) {
            ESP_LOGI(TAG, "ACCEL: X=%.3f  Y=%.3f  Z=%.3f m/s^2",
                     accel.x, accel.y, accel.z);
        }

        /* 读 DHT11 */
        dht11_data_t dht;
        if (dht11_read(&dht) == ESP_OK) {
            ESP_LOGI(TAG, "DHT11: T=%.1f C  H=%.1f %%",
                     dht.temperature, dht.humidity);
        }

        /* 轮流切换 LED 演示 */
        fault_level_t demo = (fault_level_t)(count % 4);
        led_alarm_set_level(demo);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
