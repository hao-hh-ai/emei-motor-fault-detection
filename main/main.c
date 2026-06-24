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

/* ── 采样配置 ──────────────────────────────────────── */
#define SAMPLE_RATE_HZ  50          /* 每秒采样数 */
#define BATCH_SIZE      50          /* 每批 50 点, ~1s 发一次 */

/* ── 辅助 ──────────────────────────────────────────── */

static bool wifi_is_connected(void)
{
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
}

/* ── 主程序 (单任务, 不崩) ─────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "=== 峨眉派 电机故障检测系统 v3 (轻量采集) ===");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* I2C + ADXL345 */
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

    /* 采样缓冲区 */
    int16_t buf_x[BATCH_SIZE];
    int16_t buf_y[BATCH_SIZE];
    int16_t buf_z[BATCH_SIZE];
    int buf_idx = 0;

    int tick = 0;
    int dht_tick = 0;
    dht11_data_t dht = {0};

    TickType_t t_start = xTaskGetTickCount();
    (void)t_start;

    ESP_LOGI(TAG, "采集启动: %dHz, 批量 %d 点", SAMPLE_RATE_HZ, BATCH_SIZE);

    while (1) {
        /* ── 读 ADXL345 ── */
        adxl345_raw_t raw;
        if (adxl345_read_raw(adxl_dev, &raw) == ESP_OK) {
            buf_x[buf_idx] = raw.x;
            buf_y[buf_idx] = raw.y;
            buf_z[buf_idx] = raw.z;
            buf_idx++;
        }

        /* ── 批量满, 上报 MQTT ── */
        if (buf_idx >= BATCH_SIZE) {
            buf_idx = 0;

            /* DHT11 每 2 秒 */
            if (++dht_tick >= 2) {
                dht_tick = 0;
                if (dht11_read(&dht) != ESP_OK) {
                    dht.temperature = 0;
                    dht.humidity = 0;
                }
            }

            mqtt_client_publish_batch(buf_x, buf_y, buf_z, BATCH_SIZE,
                                       SAMPLE_RATE_HZ,
                                       dht.temperature, dht.humidity);

            if (++tick % 10 == 0) {
                ESP_LOGI(TAG, "状态 | WiFi:%s  MQTT:%s  已发:%d批",
                         wifi_is_connected() ? "在线" : "离线",
                         mqtt_client_is_connected() ? "在线" : "离线",
                         tick);
            }
        }

        /* ── 20ms 间隔 → 50Hz ── */
        vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE_HZ));
    }
}
