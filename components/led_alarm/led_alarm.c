#include "led_alarm.h"

esp_err_t led_alarm_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LED_GREEN) |
                        (1ULL << LED_YELLOW) |
                        (1ULL << LED_RED) |
                        (1ULL << BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    /* 初始全灭 */
    gpio_set_level(LED_GREEN,  0);
    gpio_set_level(LED_YELLOW, 0);
    gpio_set_level(LED_RED,    0);
    gpio_set_level(BUZZER,     0);

    return ESP_OK;
}

void led_alarm_set_level(fault_level_t level)
{
    /* 全灭 */
    gpio_set_level(LED_GREEN,  0);
    gpio_set_level(LED_YELLOW, 0);
    gpio_set_level(LED_RED,    0);
    gpio_set_level(BUZZER,     0);

    switch (level) {
    case FAULT_NORMAL:
        gpio_set_level(LED_GREEN, 1);
        break;
    case FAULT_WARNING:
        gpio_set_level(LED_YELLOW, 1);
        break;
    case FAULT_DANGER:
        gpio_set_level(LED_RED, 1);
        gpio_set_level(BUZZER, 1);
        break;
    case FAULT_CRITICAL:
        gpio_set_level(LED_RED, 1);
        gpio_set_level(BUZZER, 1);
        break;
    }
}
