/*
 * TPU 推理模块 v3 — 8 特征 Conv1D 模型
 * ======================================
 * 模型: deploy_conv.onnx → deploy_conv.cvimodel
 * 输入: 8 维特征 reshape 为 (1,8,1)
 * 输出: (1,4,1) → argmax → 故障等级 0-3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "tpu_inference.h"

#ifdef USE_TPU
#include <cviruntime.h>
#endif

#define NUM_FEATURES 8
#define NUM_CLASSES  4

/* StandardScaler 参数 (训练时拟合, 与 deploy_scaler.npz 一致) */
static const float FEAT_MEAN[8] = {
    2.618515f, 13.537484f, 5.409517f, 14.595733f,
    1.492129f, 14.915948f, 1.602330f, 9.757107f,
};
static const float FEAT_SCALE[8] = {
    2.936853f, 14.211527f, 2.379896f, 22.117490f,
    1.925765f, 20.042430f, 0.583052f, 8.990097f,
};

#ifdef USE_TPU
static CVI_MODEL_HANDLE g_model = NULL;
static CVI_TENSOR      *g_inputs     = NULL;
static CVI_TENSOR      *g_outputs    = NULL;
static int32_t          g_input_num  = 0;
static int32_t          g_output_num = 0;
#endif
static int              g_ready = 0;


#ifdef USE_TPU

int tpu_init(const char *model_path)
{
    if (g_ready) return 0;
    printf("[TPU] 加载: %s\n", model_path);

    CVI_RC ret = CVI_NN_RegisterModel(model_path, &g_model);
    if (ret != 0) { printf("[TPU] 失败 (ret=%d)\n", ret); return -1; }

    CVI_NN_GetInputOutputTensors(g_model, &g_inputs, &g_input_num,
                                  &g_outputs, &g_output_num);
    if (g_input_num < 1 || g_output_num < 1) {
        CVI_NN_CleanupModel(g_model); g_model = NULL; return -1;
    }
    g_ready = 1;
    printf("[TPU] 就绪 (Conv1D MLP, 325 params)\n");
    return 0;
}

int tpu_infer(const float *features)
{
    if (!g_ready) return -1;

    /* 归一化: (x - mean) / scale, 然后 reshape 成 (1,8,1) */
    float *input = (float *)CVI_NN_TensorPtr(&g_inputs[0]);
    for (int i = 0; i < NUM_FEATURES; i++)
        input[i] = (features[i] - FEAT_MEAN[i]) / FEAT_SCALE[i];

    /* 推理 */
    CVI_RC ret = CVI_NN_Forward(g_model, g_inputs, g_input_num,
                                 g_outputs, g_output_num);
    if (ret != 0) return -1;

    /* 输出: (1,4,1) → argmax over first 4 values */
    const float *logits = (const float *)CVI_NN_TensorPtr(&g_outputs[0]);
    int best = 0;
    for (int i = 1; i < NUM_CLASSES; i++)
        if (logits[i] > logits[best]) best = i;
    return best;
}

void tpu_deinit(void)
{
    if (!g_ready) return;
    CVI_NN_CleanupModel(g_model);
    g_model = g_inputs = g_outputs = NULL;
    g_ready = 0;
}

int tpu_is_ready(void) { return g_ready; }

#else
int  tpu_init(const char *p)   { (void)p; return -1; }
int  tpu_infer(const float *f) { (void)f; return -1; }
void tpu_deinit(void)          {}
int  tpu_is_ready(void)        { return 0; }
#endif
