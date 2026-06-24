/*
 * TPU 推理模块实现 — CVI NN Runtime API 封装
 * ============================================
 * 编译: 需要 cviruntime 开发库 (libcviruntime.so + cviruntime.h)
 *   make CROSS_COMPILE=...         (含 TPU 支持)
 *   make CROSS_COMPILE=... NOTPU=1 (不含 TPU, 纯规则系统)
 * 运行: 华山派 CV1812H 上 /mnt/data/ 目录下
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tpu_inference.h"

/* ── CVI NN Runtime API (来自 Sophon cviruntime) ── */

#ifdef USE_TPU
#include <cviruntime.h>
#endif

/* ── 模型规格 ── */

#define WINDOW_SIZE   256
#define NUM_CLASSES   4

/* ── 全局状态 ── */

#ifdef USE_TPU
static CVI_MODEL_HANDLE g_model = NULL;
static CVI_TENSOR      *g_inputs     = NULL;
static CVI_TENSOR      *g_outputs    = NULL;
static int32_t          g_input_num  = 0;
static int32_t          g_output_num = 0;
#endif
static int              g_ready = 0;


/* ── 每样本归一化 (与 PyTorch train_1dcnn.py 一致) ── */

static void per_sample_normalize(const float *src, float *dst, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += src[i];
    float mean = (float)(sum / n);

    double var = 0.0;
    for (int i = 0; i < n; i++) {
        float d = src[i] - mean;
        var += d * d;
    }
    float std = (float)sqrt(var / n);

    if (std < 1e-8f) std = 1e-8f;
    for (int i = 0; i < n; i++)
        dst[i] = (src[i] - mean) / std;
}


/* ── 公开 API ── */

#ifdef USE_TPU

int tpu_init(const char *model_path)
{
    if (g_ready) return 0;

    printf("[TPU] 加载模型: %s\n", model_path);

    CVI_RC ret = CVI_NN_LoadModel(model_path, &g_model);
    if (ret != CVI_RC_SUCCESS) {
        printf("[TPU] 加载失败 (ret=%d), 回退到规则系统\n", ret);
        return -1;
    }

    CVI_NN_GetInputOutputTensors(g_model, &g_inputs, &g_input_num,
                                  &g_outputs, &g_output_num);
    if (g_input_num < 1 || g_output_num < 1) {
        printf("[TPU] 张量获取失败 (in=%d out=%d)\n", g_input_num, g_output_num);
        CVI_NN_CleanupModel(g_model);
        g_model = NULL;
        return -1;
    }

    printf("[TPU] 输入: count=%d 输出: count=%d\n",
           (int)g_inputs[0].count, (int)g_outputs[0].count);

    g_ready = 1;
    printf("[TPU] 模型就绪\n");
    return 0;
}


int tpu_infer(const float *window)
{
    if (!g_ready) return -1;

    float normalized[WINDOW_SIZE];
    per_sample_normalize(window, normalized, WINDOW_SIZE);

    memcpy((float *)g_inputs[0].data, normalized,
           WINDOW_SIZE * sizeof(float));

    CVI_RC ret = CVI_NN_RunNeuralNetwork(g_model, g_inputs, g_outputs);
    if (ret != CVI_RC_SUCCESS) {
        printf("[TPU] 推理失败 (ret=%d)\n", ret);
        return -1;
    }

    const float *logits = (const float *)g_outputs[0].data;
    int best = 0;
    float best_val = logits[0];
    for (int i = 1; i < NUM_CLASSES; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best = i;
        }
    }

    return best;
}


void tpu_deinit(void)
{
    if (!g_ready) return;
    CVI_NN_CleanupModel(g_model);
    g_model = NULL;
    g_inputs = NULL;
    g_outputs = NULL;
    g_ready = 0;
    printf("[TPU] 资源已释放\n");
}


int tpu_is_ready(void)
{
    return g_ready;
}

#else /* !USE_TPU — 桩实现, 始终失败 → 自动回退规则系统 */

int tpu_init(const char *model_path)
{
    (void)model_path;
    printf("[TPU] 未编译 TPU 支持 (NOTPU=1), 使用规则系统\n");
    return -1;
}

int tpu_infer(const float *window)
{
    (void)window;
    return -1;
}

void tpu_deinit(void) {}

int tpu_is_ready(void)
{
    return 0;
}

#endif /* USE_TPU */
