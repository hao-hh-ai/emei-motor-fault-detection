#include "mqtt_app.h"

#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "led_alarm.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "MQTT";

#define MQTT_TOPIC_TELEMETRY  "motor/telemetry"
#define MQTT_TOPIC_COMMAND    "motor/command"
#define DEVICE_ID             "motor_01"

static esp_mqtt_client_handle_t s_client;
static bool s_connected;

/* ── MQTT 事件处理 ──────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    esp_mqtt_event_handle_t evt = (esp_mqtt_event_handle_t)data;

    switch (evt->event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "已连接 Broker");
        esp_mqtt_client_subscribe(s_client, MQTT_TOPIC_COMMAND, 0);
        ESP_LOGI(TAG, "已订阅: %s", MQTT_TOPIC_COMMAND);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "断开, 自动重连中...");
        break;

    case MQTT_EVENT_DATA: {
        /* 仅处理 command 主题 */
        if (evt->topic_len != 13 ||
            memcmp(evt->topic, MQTT_TOPIC_COMMAND, 13) != 0) {
            break;
        }

        /* 简单字符串匹配 "fault_level":N, 容忍非标准 JSON */
        const char *key = "\"fault_level\"";
        const char *p = strstr(evt->data, key);
        if (!p) {
            /* 容错: 尝试不带引号的 fault_level */
            key = "fault_level";
            p = strstr(evt->data, key);
        }
        if (p) {
            p += strlen(key);
            /* 跳过 : 和空格 */
            while (*p == ':' || *p == ' ') p++;
            int lv = atoi(p);
            if (lv >= 0 && lv <= 3) {
                led_alarm_set_level((fault_level_t)lv);
                ESP_LOGI(TAG, "故障等级 → %d", lv);
            }
        }
        break;
    }

    default:
        break;
    }
}

/* ── API ────────────────────────────────────────────── */

esp_err_t mqtt_client_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "连接 %s ...", CONFIG_MQTT_BROKER_URI);
    return ESP_OK;
}

esp_err_t mqtt_client_publish(float accel_x, float accel_y, float accel_z,
                               float temp, float humi)
{
    /* 时间戳 */
    time_t now;
    time(&now);

    /* 构建 JSON */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
    cJSON_AddNumberToObject(root, "timestamp", (double)now);

    cJSON *accel = cJSON_CreateObject();
    cJSON_AddNumberToObject(accel, "x", accel_x);
    cJSON_AddNumberToObject(accel, "y", accel_y);
    cJSON_AddNumberToObject(accel, "z", accel_z);
    cJSON_AddItemToObject(root, "accel", accel);

    cJSON_AddNumberToObject(root, "temp", temp);
    cJSON_AddNumberToObject(root, "humi", humi);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        esp_mqtt_client_publish(s_client, MQTT_TOPIC_TELEMETRY,
                                json_str, 0, 0, 0);
        free(json_str);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

bool mqtt_client_is_connected(void)
{
    return s_connected;
}
