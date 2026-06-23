#include "adxl345.h"
#include "esp_log.h"

static const char *TAG = "ADXL345";

/* ── 内部辅助 ──────────────────────────────────────── */

static esp_err_t adxl345_write_reg(i2c_master_dev_handle_t dev,
                                   uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev, buf, sizeof(buf), 10);
}

static esp_err_t adxl345_read_multi(i2c_master_dev_handle_t dev,
                                    uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    return i2c_master_transmit_receive(dev, &start_reg, 1, buf, len, 10);
}

/* ── 初始化 ────────────────────────────────────────── */

esp_err_t adxl345_init(i2c_master_bus_handle_t bus_handle,
                       i2c_master_dev_handle_t *dev_handle)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADXL345_I2C_ADDR,
        .scl_speed_hz = ADXL345_I2C_FREQ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 验证 DEVID */
    uint8_t devid = 0;
    ret = adxl345_read_multi(*dev_handle, ADXL345_REG_DEVID, &devid, 1);
    if (ret != ESP_OK || devid != 0xE5) {
        ESP_LOGE(TAG, "DEVID mismatch: 0x%02X (expected 0xE5)", devid);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "DEVID OK (0xE5)");

    /* 测量模式 + ±16g 全分辨率 */
    adxl345_write_reg(*dev_handle, ADXL345_REG_POWER_CTL, 0x08);
    adxl345_write_reg(*dev_handle, ADXL345_REG_DATA_FORMAT, 0x0B);

    /* 默认 400Hz */
    adxl345_set_rate(*dev_handle, ADXL345_RATE_400_HZ);

    ESP_LOGI(TAG, "Init OK, addr=0x%02X, range=±16g @400Hz",
             ADXL345_I2C_ADDR);
    return ESP_OK;
}

/* ── 速率设置 ──────────────────────────────────────── */

esp_err_t adxl345_set_rate(i2c_master_dev_handle_t dev_handle,
                           adxl345_rate_t rate)
{
    return adxl345_write_reg(dev_handle, ADXL345_REG_BW_RATE, (uint8_t)rate);
}

/* ── 原始读取 ──────────────────────────────────────── */

esp_err_t adxl345_read_raw(i2c_master_dev_handle_t dev_handle,
                           adxl345_raw_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    uint8_t buf[6];
    esp_err_t ret = adxl345_read_multi(dev_handle, ADXL345_REG_DATAX0, buf, 6);
    if (ret != ESP_OK) return ret;

    data->x = (int16_t)(buf[1] << 8 | buf[0]);
    data->y = (int16_t)(buf[3] << 8 | buf[2]);
    data->z = (int16_t)(buf[5] << 8 | buf[4]);

    return ESP_OK;
}

/* ── 浮点读取 ──────────────────────────────────────── */

esp_err_t adxl345_read(i2c_master_dev_handle_t dev_handle,
                       adxl345_data_t *data)
{
    adxl345_raw_t raw;
    esp_err_t ret = adxl345_read_raw(dev_handle, &raw);
    if (ret != ESP_OK) return ret;

    /* 3.9mg/LSB → m/s² */
    data->x = raw.x * 0.0039f * 9.81f;
    data->y = raw.y * 0.0039f * 9.81f;
    data->z = raw.z * 0.0039f * 9.81f;

    return ESP_OK;
}
