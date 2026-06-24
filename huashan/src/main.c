/*
 * 华山派 TCP 数据处理服务 v2
 * ===========================
 * 监听 TCP 端口, 接收 PC bridge 转发的 ESP32 原始采样数据
 * → 滑窗 → 8 维特征 → 故障判定 → 打印结果
 * 未来: TPU AI 推理 + LVGL 显示
 *
 * 用法:
 *   ./huashan_bridge [port]
 *   默认端口: 9999
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "sliding_window.h"
#include "feature_extract.h"
#include "fault_detect.h"
#include "tpu_inference.h"

#define DEFAULT_PORT      9999
#define RX_BUF_SIZE       8192
#define MAX_SAMPLES       100
#define INFER_STRIDE      8     /* 每 8 个样本推理一次 (160ms @50Hz) */

static volatile int g_running = 1;

static void sig_handler(int sig) { (void)sig; g_running = 0; }

/* ── 轻量 JSON 解析 (同原 main.c) ── */

struct raw_sample { int16_t x, y, z; };

static int parse_telemetry_samples(const char *json,
                                   struct raw_sample *out, int max_out)
{
    const char *p = strstr(json, "\"samples\"");
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    p++;

    int n = 0;
    while (n < max_out && *p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == ']' || *p == '\0') break;

        if (*p == '{') {
            int x = 0, y = 0, z = 0, got = 0;
            const char *end = strchr(p, '}');
            if (!end) break;

            const char *kx = strstr(p, "\"x\"");
            if (kx && kx < end) { const char *v = strchr(kx, ':'); if (v && v < end) { x = atoi(v + 1); got++; } }
            const char *ky = strstr(p, "\"y\"");
            if (ky && ky < end) { const char *v = strchr(ky, ':'); if (v && v < end) { y = atoi(v + 1); got++; } }
            const char *kz = strstr(p, "\"z\"");
            if (kz && kz < end) { const char *v = strchr(kz, ':'); if (v && v < end) { z = atoi(v + 1); got++; } }

            if (got == 3) {
                out[n].x = (int16_t)x;
                out[n].y = (int16_t)y;
                out[n].z = (int16_t)z;
                n++;
            }
            p = end + 1;
        } else {
            p++;
        }
    }
    return n;
}

/* ── TCP 服务主逻辑 ── */

int main(int argc, char *argv[])
{
    int port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    setbuf(stdout, NULL);

    /* 创建监听 socket */
    int lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(lsock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(lsock); return 1;
    }
    if (listen(lsock, 1) < 0) {
        perror("listen"); close(lsock); return 1;
    }

    printf("╔══════════════════════════════════════════╗\n");
    printf("║  华山派 TCP 数据处理 v2                  ║\n");
    printf("║  端口: %d  等待 PC bridge 连接...       ║\n", port);
    printf("╚══════════════════════════════════════════╝\n");

    /* ── 初始化 TPU: 加载 cvimodel, 失败则回退到规则系统 ── */
    int use_tpu = 0;
    if (tpu_init("/mnt/data/motor_fault_bf16.cvimodel") == 0) {
        use_tpu = 1;
    } else {
        printf("[系统] TPU 不可用, 使用规则系统\n");
    }

    while (g_running) {
        printf("[TCP] 等待连接...\n");

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(lsock, &rfds);
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        if (select(lsock + 1, &rfds, NULL, NULL, &tv) <= 0)
            continue;

        int csock = accept(lsock, NULL, NULL);
        if (csock < 0) { if (g_running) perror("accept"); continue; }

        printf("[TCP] 客户端已连接\n");

        /* 初始化处理引擎 */
        sliding_window_t sw;
        sw_init(&sw);
        int last_level = -1;
        char buf[RX_BUF_SIZE];
        int  buf_len = 0;

        while (g_running) {
            fd_set rfds2;
            FD_ZERO(&rfds2);
            FD_SET(csock, &rfds2);
            struct timeval tv2 = {.tv_sec = 1, .tv_usec = 0};

            if (select(csock + 1, &rfds2, NULL, NULL, &tv2) <= 0)
                continue;

            ssize_t n = recv(csock, buf + buf_len,
                             sizeof(buf) - buf_len - 1, 0);
            if (n <= 0) {
                printf("[TCP] 连接断开\n");
                break;
            }
            buf_len += (int)n;
            buf[buf_len] = '\0';

            /* 按换行分割处理多条 JSON */
            char *line_start = buf;
            while (1) {
                char *nl = strchr(line_start, '\n');
                if (!nl) break;

                *nl = '\0';
                struct raw_sample samples[MAX_SAMPLES];
                int ns = parse_telemetry_samples(line_start,
                                                  samples, MAX_SAMPLES);
                for (int i = 0; i < ns; i++) {
                    sw_feed(&sw, samples[i].x, samples[i].y, samples[i].z);

                    /* 每 INFER_STRIDE 个样本推理一次, 降低延迟 */
                    static int feed_count = 0;
                    feed_count++;
                    if (!sw_is_ready(&sw) || (feed_count % INFER_STRIDE != 0))
                        continue;

                    static float xs_buf[SW_WINDOW_SIZE];
                    static float ys_buf[SW_WINDOW_SIZE];
                    static float zs_buf[SW_WINDOW_SIZE];
                    sw_get_window(&sw, xs_buf, ys_buf, zs_buf);

                    /* ── 故障判定: TPU 优先, 规则系统兜底 ── */
                    int level;
                    float rms_val = 0.0f, peak_val = 0.0f,
                          cf_val = 0.0f, kurt_val = 0.0f;

                    if (use_tpu) {
                        /* 合成三轴幅值 → TPU 推理 */
                        float mag_window[SW_WINDOW_SIZE];
                        for (int i = 0; i < SW_WINDOW_SIZE; i++) {
                            float x = xs_buf[i], y = ys_buf[i], z = zs_buf[i];
                            mag_window[i] = sqrtf(x*x + y*y + z*z);
                        }
                        int raw_level = tpu_infer(mag_window);

                        /* 持续性滤波: 3 帧一致才确认 (防抖动) */
                        static int persist_buf[3] = {-1, -1, -1};
                        persist_buf[0] = persist_buf[1];
                        persist_buf[1] = persist_buf[2];
                        persist_buf[2] = raw_level;
                        if (persist_buf[0] == persist_buf[1] &&
                            persist_buf[1] == persist_buf[2]) {
                            level = raw_level;
                        } else {
                            level = -1;  /* 未确认, 不输出 */
                        }

                        /* 打印时仍提取特征用于监控 */
                        vibration_features_t feat;
                        feat_extract(xs_buf, ys_buf, zs_buf,
                                     SW_WINDOW_SIZE, &feat);
                        rms_val  = feat.rms;
                        peak_val = feat.peak;
                        cf_val   = feat.crest_factor;
                        kurt_val = feat.kurtosis;
                    } else {
                        /* 规则系统 */
                        vibration_features_t feat;
                        feat_extract(xs_buf, ys_buf, zs_buf,
                                     SW_WINDOW_SIZE, &feat);
                        level = fault_detect(&feat);
                        rms_val  = feat.rms;
                        peak_val = feat.peak;
                        cf_val   = feat.crest_factor;
                        kurt_val = feat.kurtosis;
                    }

                    if (level >= 0 && level != last_level) {
                        const char *icon[] = {"[OK]","[WARN]","[DANGER]","[CRIT]"};
                        printf("%s Lv.%d | RMS=%.2f Peak=%.2f "
                               "CF=%.1f Kurt=%.1f  [%s]\n",
                               icon[level], level,
                               rms_val, peak_val, cf_val, kurt_val,
                               use_tpu ? "TPU" : "rule");
                        last_level = level;
                    }
                }

                line_start = nl + 1;
            }

            /* 移除已处理的数据 */
            int consumed = (int)(line_start - buf);
            if (consumed > 0) {
                buf_len -= consumed;
                if (buf_len > 0)
                    memmove(buf, line_start, (size_t)buf_len);
            }
        }
        close(csock);
    }

    close(lsock);
    tpu_deinit();
    printf("[系统] 退出\n");
    return 0;
}
