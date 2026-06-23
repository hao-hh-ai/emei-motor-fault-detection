#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FEAT_WINDOW_SIZE  256
#define FEAT_PERSIST_N    3   /* 连续 N 窗一致才切换等级 */

/* ── 8 维时域特征 ─────────────────────────────────── */

typedef struct {
    float rms;             /* 均方根 (m/s²) — 振动能量 */
    float peak;            /* 峰值 (m/s²) — 最大冲击 */
    float crest_factor;    /* 峰值因子 (Peak/RMS) — 早期冲击预警 */
    float kurtosis;        /* 峭度 (无量纲) — 信号尖峰度, 正态=3 */
    float skewness;        /* 偏度 — 不对称性, 冲击方向 */
    float clearance;       /* 间隙因子 = Peak / (mean(√|x|))² */
    float shape_factor;    /* 波形因子 = RMS / mean(|x|) */
    float impulse_factor;  /* 脉冲因子 = Peak / mean(|x|) */
} vibration_features_t;

/* ── API ───────────────────────────────────────────── */

void feat_window_init(void);

bool feat_window_feed(int16_t x, int16_t y, int16_t z,
                      vibration_features_t *out);

int  feat_detect_fault(const vibration_features_t *f);
