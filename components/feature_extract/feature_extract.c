#include "feature_extract.h"
#include <math.h>
#include <string.h>

/* LSB 转 m/s²: ±16g 全分辨率, 3.9mg/LSB */
#define LSB_TO_MS2  (0.0039f * 9.81f)

/* ── 滑动缓冲区 (三轴) ─────────────────────────────── */

static int16_t s_buf_x[FEAT_WINDOW_SIZE];
static int16_t s_buf_y[FEAT_WINDOW_SIZE];
static int16_t s_buf_z[FEAT_WINDOW_SIZE];
static int      s_count;

void feat_window_init(void)
{
    s_count = 0;
    memset(s_buf_x, 0, sizeof(s_buf_x));
    memset(s_buf_y, 0, sizeof(s_buf_y));
    memset(s_buf_z, 0, sizeof(s_buf_z));
}

/* ── 喂入数据, 窗满计算 ────────────────────────────── */

bool feat_window_feed(int16_t x, int16_t y, int16_t z,
                      vibration_features_t *out)
{
    s_buf_x[s_count] = x;
    s_buf_y[s_count] = y;
    s_buf_z[s_count] = z;
    s_count++;

    if (s_count < FEAT_WINDOW_SIZE) {
        return false;
    }

    /* ── 计算三轴合成幅值 ────────────────────────── */
    float mag[FEAT_WINDOW_SIZE];
    float sum     = 0;
    float sum_abs = 0;
    float sum_sq  = 0;

    for (int i = 0; i < FEAT_WINDOW_SIZE; i++) {
        float fx = (float)s_buf_x[i];
        float fy = (float)s_buf_y[i];
        float fz = (float)s_buf_z[i];
        float m  = sqrtf(fx * fx + fy * fy + fz * fz);
        mag[i]   = m;
        sum     += m;
        sum_abs += fabsf(m);
        sum_sq  += m * m;
    }

    float n_inv = 1.0f / FEAT_WINDOW_SIZE;
    float mean  = sum * n_inv;
    float rms   = sqrtf(sum_sq * n_inv);

    /* Peak */
    float peak = mag[0];
    for (int i = 1; i < FEAT_WINDOW_SIZE; i++) {
        if (mag[i] > peak) peak = mag[i];
    }

    /* 二阶矩 (标准差) */
    float var = 0;
    for (int i = 0; i < FEAT_WINDOW_SIZE; i++) {
        float d = mag[i] - mean;
        var += d * d;
    }
    float sigma = sqrtf(var * n_inv);

    /* 三阶矩 (偏度) 四阶矩 (峭度) */
    float m3 = 0, m4 = 0;
    for (int i = 0; i < FEAT_WINDOW_SIZE; i++) {
        float d = mag[i] - mean;
        float d2 = d * d;
        m3 += d2 * d;
        m4 += d2 * d2;
    }

    float skew = (sigma > 0.01f) ? (m3 * n_inv) / (sigma * sigma * sigma) : 0;
    float kurt = (sigma > 0.01f) ? (m4 * n_inv) / (sigma * sigma * sigma * sigma) : 1.0f;

    /* 间隙因子 */
    float sum_sqrt_abs = 0;
    for (int i = 0; i < FEAT_WINDOW_SIZE; i++) {
        sum_sqrt_abs += sqrtf(fabsf(mag[i]));
    }
    float clearance = (sum_sqrt_abs > 0) ? peak / ((sum_sqrt_abs * n_inv) * (sum_sqrt_abs * n_inv)) : 1.0f;

    /* 波形因子 / 脉冲因子 */
    float mean_abs = sum_abs * n_inv;
    float shape_factor   = (mean_abs > 0) ? rms / mean_abs : 1.0f;
    float impulse_factor = (mean_abs > 0) ? peak / mean_abs : 1.0f;

    /* 转为 m/s² */
    out->rms            = rms * LSB_TO_MS2;
    out->peak           = peak * LSB_TO_MS2;
    out->crest_factor   = (rms > 1.0f) ? peak / rms : 1.0f;
    out->kurtosis        = kurt;
    out->skewness        = skew;
    out->clearance       = clearance;
    out->shape_factor    = shape_factor;
    out->impulse_factor  = impulse_factor;

    /* 重置窗口 */
    s_count = 0;
    return true;
}

/* ── 加权评分故障判定 ──────────────────────────────── */

int feat_detect_fault(const vibration_features_t *f)
{
    /* g 换算 */
    float rms_g   = f->rms / 9.81f;
    float peak_g  = f->peak / 9.81f;
    float kurt    = f->kurtosis;
    float crest   = f->crest_factor;
    float clearance = f->clearance;
    float impulse  = f->impulse_factor;
    float shape    = f->shape_factor;

    /* ── 单项评分: 0=正常 1=注意 2=异常 3=严重 ── */
    #define SCORE(v, lo, hi, crit) \
        ((v) >= (crit) ? 3 : ((v) >= (hi) ? 2 : ((v) >= (lo) ? 1 : 0)))

    float score = 0;
    float weight_sum = 0;

    /* RMS (权重 2.0) */
    score += SCORE(rms_g, 0.3f, 0.6f, 1.5f) * 2.0f;
    weight_sum += 2.0f;

    /* Kurtosis (权重 2.0) */
    score += SCORE(kurt, 3.5f, 5.0f, 8.0f) * 2.0f;
    weight_sum += 2.0f;

    /* Peak (权重 1.5) */
    score += SCORE(peak_g, 0.8f, 2.0f, 5.0f) * 1.5f;
    weight_sum += 1.5f;

    /* Crest Factor (权重 1.5) */
    score += SCORE(crest, 4.0f, 7.0f, 12.0f) * 1.5f;
    weight_sum += 1.5f;

    /* Clearance Factor (权重 1.5) */
    score += SCORE(clearance, 5.0f, 10.0f, 20.0f) * 1.5f;
    weight_sum += 1.5f;

    /* Impulse Factor (权重 1.0) */
    score += SCORE(impulse, 3.0f, 6.0f, 10.0f) * 1.0f;
    weight_sum += 1.0f;

    /* Shape Factor (权重 0.5) */
    score += SCORE(shape, 1.5f, 2.0f, 3.0f) * 0.5f;
    weight_sum += 0.5f;

    /* 归一化到 0~3 */
    float avg_score = (weight_sum > 0) ? score / weight_sum : 0;

    /* ── 持续性滤波 ── */
    static int prev_levels[FEAT_PERSIST_N] = {0};
    static int persist_idx = 0;

    /* 当前窗的粗等级 */
    int raw_level;
    if (avg_score >= 2.5f)       raw_level = 3;
    else if (avg_score >= 1.8f)  raw_level = 2;
    else if (avg_score >= 0.8f)  raw_level = 1;
    else                         raw_level = 0;

    /* 推入历史窗口 */
    prev_levels[persist_idx] = raw_level;
    persist_idx = (persist_idx + 1) % FEAT_PERSIST_N;

    /* 判定: 最近 N 窗全部一致才切换 */
    for (int i = 1; i < FEAT_PERSIST_N; i++) {
        if (prev_levels[i] != raw_level) {
            return prev_levels[(persist_idx - 1 + FEAT_PERSIST_N) % FEAT_PERSIST_N];
        }
    }
    return raw_level;
}
