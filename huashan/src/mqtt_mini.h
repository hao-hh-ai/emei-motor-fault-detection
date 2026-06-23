#pragma once
/*
 * 最小 MQTT v3.1.1 客户端 — 零外部依赖
 * 仅支持: CONNECT(匿名), SUBSCRIBE(QoS0), PUBLISH(QoS0), PINGREQ
 *
 * 用法:
 *   mqtt_mini_t mq;
 *   mqtt_mini_connect(&mq, "192.168.1.1", 4883, "huashan_01");
 *   mqtt_mini_subscribe(&mq, "motor/telemetry");
 *   mqtt_mini_publish(&mq, "motor/command", "{\"fault_level\":0}");
 *   while (1) {
 *       char *topic, *payload;
 *       if (mqtt_mini_poll(&mq, &topic, &payload, 500) > 0) {
 *           // 处理 topic/payload
 *       }
 *   }
 */

#include <stdint.h>
#include <stddef.h>

#define MQTT_RX_BUF_SIZE  4096
#define MQTT_TX_BUF_SIZE  2048
#define MQTT_MAX_TOPIC    128

typedef struct {
    int         sock;
    char        rx_buf[MQTT_RX_BUF_SIZE];
    int         rx_len;
    char        tx_buf[MQTT_TX_BUF_SIZE];

    /* 最近收到的消息 topic/payload — 指针到 rx_buf 内 */
    char        msg_topic[MQTT_MAX_TOPIC];
    char        msg_payload[MQTT_RX_BUF_SIZE];
    int         msg_payload_len;

    uint16_t    pkt_id;        /* 报文标识符 */
    int         keepalive_s;   /* 保活间隔秒 */
    uint64_t    last_tx_ms;    /* 上次发送时间戳 */
} mqtt_mini_t;

/*
 * 连接 MQTT Broker
 * broker_addr: IP 或主机名
 * port: 默认 1883
 * client_id: 客户端 ID
 * 返回 0 成功, -1 失败
 */
int mqtt_mini_connect(mqtt_mini_t *m, const char *broker_addr,
                      int port, const char *client_id);

/*
 * 订阅主题 (QoS 0)
 * 返回 0 成功, -1 失败
 */
int mqtt_mini_subscribe(mqtt_mini_t *m, const char *topic);

/*
 * 发布消息 (QoS 0, retain=0)
 * 返回 0 成功, -1 失败
 */
int mqtt_mini_publish(mqtt_mini_t *m, const char *topic,
                      const char *payload);

/*
 * 轮询接收, timeout_ms 超时
 * 收到 PUBLISH 消息时 topic/payload 填入 m->msg_topic / m->msg_payload
 * 返回 >0 有新消息, 0 超时无消息, -1 出错 (连接断开)
 *
 * 内部自动发送 PINGREQ 保持连接
 */
int mqtt_mini_poll(mqtt_mini_t *m, int timeout_ms);

/* 断开连接 */
void mqtt_mini_disconnect(mqtt_mini_t *m);
