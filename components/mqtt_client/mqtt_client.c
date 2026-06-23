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
        if (evt->topic_len != 13 ||
            memcmp(evt->topic, MQTT_TOPIC_COMMAND, 13) != 0) {
            break;
        }
        const char *key = "\"fault_level\"";
        const char *p = strstr(evt->data, key);
        if (!p) {
            key = "fault_level";
            p = strstr(evt->data, key);
        }
        if (p) {
            p += strlen(key);
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

esp_err_t mqtt_client_publish_batch(const int16_t *buf_x,
                                     const int16_t *buf_y,
                                     const int16_t *buf_z,
                                     int count, int sample_rate_hz,
                                     float temp, float humi)
{
    if (!buf_x || !buf_y || !buf_z || count <= 0) return ESP_ERR_INVALID_ARG;

    time_t now;
    time(&now);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
    cJSON_AddNumberToObject(root, "timestamp", (double)now);
    cJSON_AddNumberToObject(root, "sample_rate", sample_rate_hz);

    /* 构建样本数组 */
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *pt = cJSON_CreateObject();
        cJSON_AddNumberToObject(pt, "x", buf_x[i]);
        cJSON_AddNumberToObject(pt, "y", buf_y[i]);
        cJSON_AddNumberToObject(pt, "z", buf_z[i]);
        cJSON_AddItemToArray(arr, pt);
    }
    cJSON_AddItemToObject(root, "samples", arr);

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
