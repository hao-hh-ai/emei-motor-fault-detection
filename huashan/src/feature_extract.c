#include "feature_extract.h"
#include "threshold_config.h"
#include <math.h>
#include <stdlib.h>

void feat_extract(const float *xs, const float *ys, const float *zs,
                  int n, vibration_features_t *out)
{
    /* ── 合成幅值 + 分配临时缓冲 ── */
    float * restrict mag = (float *)malloc((size_t)n * sizeof(float));
    if (!mag) goto fail;

    for (int i = 0; i < n; i++) {
        float x = xs[i], y = ys[i], z = zs[i];
        mag[i] = sqrtf(x * x + y * y + z * z);
    }

    /* ── AC 耦合: 减去均值 ── */
    float sum = 0.0f;
    for (int i = 0; i < n; i++) sum += mag[i];
    float mean = sum / (float)n;

    for (int i = 0; i < n; i++)
        mag[i] -= mean;    /* mag 现在存放 AC 信号 */

    /* ── RMS, Peak, MeanAbs ── */
    float sum_sq = 0.0f;
    float peak   = 0.0f;
    float sum_abs = 0.0f;
    float sum_sqrt_abs = 0.0f;

    for (int i = 0; i < n; i++) {
        float a = mag[i];
        float abs_a = fabsf(a);
        sum_sq   += a * a;
        sum_abs  += abs_a;
        sum_sqrt_abs += sqrtf(abs_a);
        if (abs_a > peak) peak = abs_a;
    }

    float rms      = sqrtf(sum_sq / (float)n);
    float mean_abs = sum_abs / (float)n;

    /* ── 方差 + 高阶矩 ── */
    float var = sum_sq / (float)n;
    float sigma = sqrtf(var);

    float m3 = 0.0f, m4 = 0.0f;
    if (sigma > 0.01f) {
        for (int i = 0; i < n; i++) {
            float a = mag[i];
            float a3 = a * a * a;
            float a4 = a3 * a;
            m3 += a3;
            m4 += a4;
        }
        m3 /= (float)n;
        m4 /= (float)n;
    }

    float skew = (sigma > 0.01f) ? (m3 / (sigma * sigma * sigma)) : 0.0f;
    float kurt = (sigma > 0.01f) ? (m4 / (var * var)) : 1.0f;

    /* ── 无量纲因子 ── */
    float clearance = (sum_sqrt_abs > 0.0f)
        ? (peak / ((sum_sqrt_abs / (float)n) * (sum_sqrt_abs / (float)n)))
        : 1.0f;
    float shape   = (mean_abs > 0.0f) ? (rms / mean_abs) : 1.0f;
    float impulse = (mean_abs > 0.0f) ? (peak / mean_abs) : 1.0f;
    float crest   = (rms > 0.1f) ? (peak / rms) : 1.0f;

    /* ── 输出, RMS/Peak 转为 m/s² ── */
    out->rms            = rms  * LSB_TO_MS2;
    out->peak           = peak * LSB_TO_MS2;
    out->crest_factor   = crest;
    out->kurtosis       = kurt;
    out->skewness       = skew;
    out->clearance      = clearance;
    out->shape_factor   = shape;
    out->impulse_factor = impulse;

    free(mag);
    return;

fail:
    /* OOM — 填零 */
    out->rms = out->peak = out->crest_factor = 0.0f;
    out->kurtosis = 1.0f;
    out->skewness = out->clearance = 0.0f;
    out->shape_factor = out->impulse_factor = 1.0f;
}
