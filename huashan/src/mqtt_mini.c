#define _POSIX_C_SOURCE 200809L
/*
 * 最小 MQTT v3.1.1 客户端实现
 *
 * MQTT 报文格式 (v3.1.1):
 *   固定头: [控制类型+标志(1)] [剩余长度(1~4)]
 *   CONNECT:     10 xx xx [协议名] [协议级] [连接标志] [保活] [ClientID]
 *   SUBSCRIBE:   82 xx xx [报文ID] [主题+QoS ...]
 *   PUBLISH:     30 xx xx [主题] [负载]        ← QoS 0, 无报文ID
 *   PINGREQ:     C0 00
 *   PINGRESP:    D0 00
 */

#include "mqtt_mini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

/* ── 小工具: 获取时间 ms ── */
static uint64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/* ── 编码剩余长度 → buf, 返回写入字节数 ── */
static int encode_remaining_length(uint8_t *buf, uint32_t len)
{
    int i = 0;
    do {
        uint8_t byte = len & 0x7F;
        len >>= 7;
        if (len > 0) byte |= 0x80;
        buf[i++] = byte;
    } while (len > 0 && i < 4);
    return i;
}

/* ── 发送原始数据 ── */
static int mqtt_send(mqtt_mini_t *m, const void *data, size_t len)
{
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(m->sock, (const char *)data + total,
                         len - total, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    m->last_tx_ms = now_ms();
    return 0;
}

/* ── 写 16 位大端 ── */
static void write_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

/* ── 写 UTF-8 字符串 (2 字节长度前缀) ── */
static int write_utf8(uint8_t *buf, const char *str)
{
    uint16_t len = (uint16_t)strlen(str);
    write_u16(buf, len);
    memcpy(buf + 2, str, len);
    return 2 + len;
}

/* ── 连接到 TCP socket ── */
static int tcp_connect(const char *addr, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);

    /* 先尝试 IP 直连 */
    if (inet_pton(AF_INET, addr, &sa.sin_addr) == 1) {
        if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
            perror("connect");
            close(sock); return -1;
        }
        return sock;
    }

    /* 走域名解析 */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(addr, port_str, &hints, &res) != 0) {
        perror("getaddrinfo");
        close(sock); return -1;
    }
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(res);
        close(sock); return -1;
    }
    freeaddrinfo(res);
    return sock;
}

/* ────────────────────────────────────────────────────
 * 公共 API
 * ────────────────────────────────────────────────── */

int mqtt_mini_connect(mqtt_mini_t *m, const char *broker_addr,
                      int port, const char *client_id)
{
    memset(m, 0, sizeof(*m));
    m->sock = -1;
    m->keepalive_s = 60;
    m->pkt_id = 1;

    int sock = tcp_connect(broker_addr, port);
    if (sock < 0) return -1;
    m->sock = sock;

    /* ── 构建 CONNECT 报文 ── */
    /* 可变头: 协议名 "MQTT"(4) + 协议级(4) + 连接标志(1) + 保活(2) */
    uint8_t pkt[256];
    int pos = 2;  /* 跳过固定头 */

    pos += write_utf8(pkt + pos, "MQTT");         /* 协议名 */
    pkt[pos++] = 4;                                /* 协议级 v3.1.1 */
    pkt[pos++] = 0x02;                            /* 连接标志: 清理会话 */
    write_u16(pkt + pos, (uint16_t)m->keepalive_s);
    pos += 2;
    pos += write_utf8(pkt + pos, client_id);      /* ClientID */

    /* 固定头: CONNECT */
    pkt[0] = 0x10;  /* CONNECT */
    int rem_len = pos - 2;
    int hdr = encode_remaining_length(pkt + 1, (uint32_t)rem_len);

    /* 移动可变头紧挨固定头 (hdr通常为1, 预留了2字节) */
    if (hdr < 2)
        memmove(pkt + 1 + hdr, pkt + 3, (size_t)rem_len);
    pos = 1 + hdr + rem_len;

    if (mqtt_send(m, pkt, pos) < 0) {
        close(m->sock); m->sock = -1; return -1;
    }

    /* 等待 CONNACK (超时 5 秒) */
    {
        uint8_t buf[4];
        int total = 0;
        struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
        setsockopt(m->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        while (total < 4) {
            ssize_t n = recv(m->sock, buf + total, 4 - total, 0);
            if (n <= 0) {
                close(m->sock); m->sock = -1; return -1;
            }
            total += (int)n;
        }
        /* CONNACK: 固定头 0x20, 剩余长度 2, flags, rc */
        if (buf[0] != 0x20 || buf[2] != 0x00) {
            fprintf(stderr, "[MQTT] CONNACK rejected: type=%02x rc=%d\n",
                    buf[0], buf[3]);
            close(m->sock); m->sock = -1; return -1;
        }
    }
    return 0;
}

int mqtt_mini_subscribe(mqtt_mini_t *m, const char *topic)
{
    if (m->sock < 0) return -1;

    uint8_t pkt[256];
    int pos = 2;

    write_u16(pkt + pos, m->pkt_id++);  /* 报文 ID */
    pos += 2;
    pos += write_utf8(pkt + pos, topic);
    pkt[pos++] = 0x00;                   /* QoS 0 */

    pkt[0] = 0x82;  /* SUBSCRIBE */
    int rem_len = pos - 2;
    int hdr = encode_remaining_length(pkt + 1, (uint32_t)rem_len);
    if (hdr < 2)
        memmove(pkt + 1 + hdr, pkt + 3, (size_t)rem_len);
    pos = 1 + hdr + rem_len;

    return mqtt_send(m, pkt, pos);
}

int mqtt_mini_publish(mqtt_mini_t *m, const char *topic,
                      const char *payload)
{
    if (m->sock < 0) return -1;

    uint8_t pkt[MQTT_TX_BUF_SIZE];
    int pos = 2;

    pos += write_utf8(pkt + pos, topic);
    /* 负载 — QoS 0 直接复制 */
    size_t plen = strlen(payload);
    memcpy(pkt + pos, payload, plen);
    pos += (int)plen;

    pkt[0] = 0x30;  /* PUBLISH, QoS 0, 不保留 */
    int rem_len = pos - 2;
    int hdr = encode_remaining_length(pkt + 1, (uint32_t)rem_len);
    if (hdr < 2)
        memmove(pkt + 1 + hdr, pkt + 3, (size_t)rem_len);
    pos = 1 + hdr + rem_len;

    return mqtt_send(m, pkt, pos);
}

/* ── 解析收到的 PUBLISH 报文 ── */
static int parse_publish(mqtt_mini_t *m, const uint8_t *data, int len)
{
    /* data[0] 是固定头, 剩余长度从 data[1] 开始 (已解析) */
    /* data: [固定头] [剩余长度n字节] [Topic MSB LSB ...] [Payload] */
    int offset = 0;

    /* 跳过固定头 + 剩余长度 */
    offset++;  /* 固定头 */
    while ((data[offset] & 0x80) && offset < len) offset++;
    offset++;  /* 剩余长度最后 1 字节 */

    if (offset + 2 > len) return -1;

    uint16_t topic_len = ((uint16_t)data[offset] << 8) | data[offset + 1];
    offset += 2;

    if (offset + topic_len > len) return -1;

    int copy_len = topic_len < MQTT_MAX_TOPIC - 1
                   ? topic_len : MQTT_MAX_TOPIC - 1;
    memcpy(m->msg_topic, data + offset, (size_t)copy_len);
    m->msg_topic[copy_len] = '\0';
    offset += topic_len;

    m->msg_payload_len = len - offset;
    if (m->msg_payload_len > MQTT_RX_BUF_SIZE - 1)
        m->msg_payload_len = MQTT_RX_BUF_SIZE - 1;
    memcpy(m->msg_payload, data + offset, (size_t)m->msg_payload_len);
    m->msg_payload[m->msg_payload_len] = '\0';

    return 1;
}

int mqtt_mini_poll(mqtt_mini_t *m, int timeout_ms)
{
    if (m->sock < 0) return -1;

    /* ── 设置接收超时 ── */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(m->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* ── 尝试接收 ── */
    ssize_t n = recv(m->sock, m->rx_buf + m->rx_len,
                     sizeof(m->rx_buf) - m->rx_len - 1, 0);
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK
                       && errno != EINTR))
            return -1;

        /* ── 超时: 检查是否需要发送 PINGREQ ── */
        if (now_ms() - m->last_tx_ms > (uint64_t)(m->keepalive_s - 5) * 1000) {
            uint8_t ping[2] = { 0xC0, 0x00 };
            mqtt_send(m, ping, 2);
        }
        return 0;
    }

    m->rx_len += (int)n;
    m->rx_buf[m->rx_len] = '\0';

    /* ── 解析缓冲区中的报文 ── */
    int consumed = 0;
    int got_msg = 0;

    while (consumed < m->rx_len) {
        const uint8_t *p = (const uint8_t *)m->rx_buf + consumed;
        int remaining = m->rx_len - consumed;
        if (remaining < 2) break;

        uint8_t type = p[0] & 0xF0;

        /* 解码剩余长度 */
        uint32_t rem_len = 0;
        int rl_offset = 1;
        int mul = 1;
        while (rl_offset < remaining && rl_offset < 5) {
            uint8_t b = p[rl_offset];
            rem_len += (uint32_t)(b & 0x7F) * (uint32_t)mul;
            mul *= 128;
            rl_offset++;
            if (!(b & 0x80)) break;
        }
        if (rl_offset >= 5) return -1; /* 非法长度 */

        int pkt_total = rl_offset + (int)rem_len;
        if (pkt_total > remaining) break; /* 报文不完整 */

        switch (type) {
        case 0x30:  /* PUBLISH */
            got_msg = parse_publish(m, p + rl_offset,
                                    pkt_total - rl_offset);
            break;
        case 0x90:  /* SUBACK — 忽略 */
        case 0xD0:  /* PINGRESP — 忽略 */
        case 0x20:  /* CONNACK — 忽略 */
            break;
        default:
            break;
        }

        consumed += pkt_total;
        if (got_msg) break;  /* 一次返回一条消息 */
    }

    /* 移除已消耗的数据 */
    if (consumed > 0) {
        m->rx_len -= consumed;
        if (m->rx_len > 0)
            memmove(m->rx_buf, m->rx_buf + consumed, (size_t)m->rx_len);
    }

    return got_msg;
}

void mqtt_mini_disconnect(mqtt_mini_t *m)
{
    if (m->sock >= 0) {
        uint8_t dis[2] = { 0xE0, 0x00 };
        mqtt_send(m, dis, 2);
        close(m->sock);
        m->sock = -1;
    }
}
