#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

/* ── I2C 引脚 ──────────────────────────────────────── */
#define ADXL345_I2C_SDA   GPIO_NUM_6
#define ADXL345_I2C_SCL   GPIO_NUM_10
#define ADXL345_I2C_ADDR  0x53
#define ADXL345_I2C_FREQ  100000

/* ── 采样率选项 ─────────────────────────────────────── */
typedef enum {
    ADXL345_RATE_25_HZ   = 0x08,
    ADXL345_RATE_50_HZ   = 0x09,
    ADXL345_RATE_100_HZ  = 0x0A,
    ADXL345_RATE_200_HZ  = 0x0B,
    ADXL345_RATE_400_HZ  = 0x0C,
    ADXL345_RATE_800_HZ  = 0x0D,
} adxl345_rate_t;

/* ── 寄存器地址 ─────────────────────────────────────── */
#define ADXL345_REG_DEVID        0x00
#define ADXL345_REG_POWER_CTL    0x2D
#define ADXL345_REG_DATA_FORMAT  0x31
#define ADXL345_REG_BW_RATE      0x2C
#define ADXL345_REG_DATAX0       0x32

/* ── 原始 16 位数据类型 ─────────────────────────────── */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} adxl345_raw_t;

/* ── m/s² 浮点类型 ─────────────────────────────────── */
typedef struct {
    float x;
    float y;
    float z;
} adxl345_data_t;

/* ── API ───────────────────────────────────────────── */

/**
 * @brief 初始化 ADXL345，默认 ±16g 全分辨率
 */
esp_err_t adxl345_init(i2c_master_bus_handle_t bus_handle,
                       i2c_master_dev_handle_t *dev_handle);

/**
 * @brief 设置输出数据速率
 * @param dev_handle  ADXL345 设备句柄
 * @param rate       速率枚举值
 */
esp_err_t adxl345_set_rate(i2c_master_dev_handle_t dev_handle,
                           adxl345_rate_t rate);

/**
 * @brief 读取原始三轴加速度 (int16, 3.9mg/LSB)
 */
esp_err_t adxl345_read_raw(i2c_master_dev_handle_t dev_handle,
                           adxl345_raw_t *data);

/**
 * @brief 读取三轴加速度 (m/s²)
 */
esp_err_t adxl345_read(i2c_master_dev_handle_t dev_handle,
                       adxl345_data_t *data);
