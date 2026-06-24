/*
 * 微型 MLP 分类器 — 纯 C 实现 (316 params, ~1500 FLOPs)
 * ==========================================================
 * 架构: 8→16→ReLU→8→ReLU→4→argmax
 * 权重来自 deploy_mlp.pth (97.4% test accuracy)
 */

#include "mlp_classifier.h"
#include <math.h>

/* ── 权重 (训练时冻结) ── */

#define IN  8
#define H1 16
#define H2  8
#define OUT 4

#include "mlp_weights.h"  /* MLP_W0, MLP_B0, MLP_W1, MLP_B1, MLP_W2, MLP_B2 */

/* ── 矩阵乘法: y = x⋅W + b ── */

static void linear(const float *x, const float *w, const float *b,
                   int in_dim, int out_dim, float *y)
{
    for (int j = 0; j < out_dim; j++) {
        float s = b[j];
        for (int i = 0; i < in_dim; i++)
            s += x[i] * w[i * out_dim + j];
        y[j] = s;
    }
}

static void relu(float *x, int n) {
    for (int i = 0; i < n; i++)
        if (x[i] < 0.0f) x[i] = 0.0f;
}

/* ── 公开 API ── */

int mlp_predict(const float *features)
{
    float h1[H1], h2[H2], out[OUT];

    linear(features, MLP_W0, MLP_B0, IN,  H1, h1);
    relu(h1, H1);

    linear(h1,       MLP_W1, MLP_B1, H1,  H2, h2);
    relu(h2, H2);

    linear(h2,       MLP_W2, MLP_B2, H2,  OUT, out);

    /* argmax */
    int best = 0;
    for (int i = 1; i < OUT; i++)
        if (out[i] > out[best]) best = i;

    return best;
}
