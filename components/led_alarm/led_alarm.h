#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

/* 报警外设引脚 */
#define LED_GREEN   GPIO_NUM_1
#define LED_YELLOW  GPIO_NUM_3
#define LED_RED     GPIO_NUM_4
#define BUZZER      GPIO_NUM_0

/* 故障等级 */
typedef enum {
    FAULT_NORMAL   = 0,  /* 绿灯亮 */
    FAULT_WARNING  = 1,  /* 黄灯亮 */
    FAULT_DANGER   = 2,  /* 红灯亮 + 蜂鸣器响 */
    FAULT_CRITICAL = 3,  /* 红灯亮 + 蜂鸣器响 */
} fault_level_t;

/* 初始化 LED 和蜂鸣器 */
esp_err_t led_alarm_init(void);

/* 设置故障等级 (直接控制 GPIO) */
void led_alarm_set_level(fault_level_t level);
