#include "wifi_sta.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI";
static EventGroupHandle_t s_wifi_evt;

#define WIFI_CONNECTED BIT0

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "断开, 重连中...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_evt, WIFI_CONNECTED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "已连接: " IPSTR, IP2STR(&evt->ip_info.ip));
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED);
    }
}

esp_err_t wifi_sta_init(void)
{
    s_wifi_evt = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);

    wifi_config_t sta = { 0 };
    snprintf((char *)sta.sta.ssid, sizeof(sta.sta.ssid), "%s", CONFIG_WIFI_SSID);
    snprintf((char *)sta.sta.password, sizeof(sta.sta.password), "%s", CONFIG_WIFI_PASSWORD);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta);
    esp_wifi_start();

    ESP_LOGI(TAG, "连接 %s ...", CONFIG_WIFI_SSID);

    /* 等待首次连接 (最多 10 秒) */
    xEventGroupWaitBits(s_wifi_evt, WIFI_CONNECTED, pdFALSE, pdTRUE,
                        pdMS_TO_TICKS(10000));

    return ESP_OK;
}
