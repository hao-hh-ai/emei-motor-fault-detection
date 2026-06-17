#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

/* ── ADXL345 I2C 引脚 ──────────────────────────────── */
#define ADXL345_I2C_SDA   GPIO_NUM_6
#define ADXL345_I2C_SCL   GPIO_NUM_10
#define ADXL345_I2C_ADDR  0x53       /* SDO 悬空/接地 = 0x53, 接 VDD = 0x1D */
#define ADXL345_I2C_FREQ  100000     /* 100kHz */

/* ── ADXL345 寄存器 ─────────────────────────────────── */
#define ADXL345_REG_DEVID        0x00
#define ADXL345_REG_POWER_CTL    0x2D
#define ADXL345_REG_DATA_FORMAT  0x31
#define ADXL345_REG_BW_RATE      0x2C
#define ADXL345_REG_DATAX0       0x32

/* ── 数据类型 ──────────────────────────────────────── */

typedef struct {
    float x;   /* m/s² */
    float y;
    float z;
} adxl345_data_t;

/* ── API ───────────────────────────────────────────── */

/**
 * @brief  初始化 ADXL345 I2C 驱动并配置传感器
 * @param  bus_handle  已初始化的 I2C 总线句柄
 * @param  dev_handle  输出，ADXL345 设备句柄
 * @return ESP_OK 成功
 */
esp_err_t adxl345_init(i2c_master_bus_handle_t bus_handle,
                       i2c_master_dev_handle_t *dev_handle);

/**
 * @brief  读取三轴加速度
 * @param  dev_handle  ADXL345 设备句柄
 * @param  data        输出加速度数据 (m/s²)
 * @return ESP_OK 成功
 */
esp_err_t adxl345_read(i2c_master_dev_handle_t dev_handle,
                       adxl345_data_t *data);
