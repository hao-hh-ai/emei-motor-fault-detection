#pragma once

#include <stdint.h>

typedef struct {
    float rms;            /* m/s², AC 耦合 */
    float peak;           /* m/s², AC 耦合 */
    float crest_factor;   /* 无量纲 */
    float kurtosis;       /* 无量纲 */
    float skewness;       /* 无量纲 */
    float clearance;      /* 无量纲 */
    float shape_factor;   /* 无量纲 */
    float impulse_factor; /* 无量纲 */
} vibration_features_t;

/*
 * 从三轴 raw 值 (float 化, 各 256 点) 提取 8 维时域特征
 * 内部对合成幅值做 AC 耦合消除重力直流分量
 */
void feat_extract(const float *xs, const float *ys, const float *zs,
                  int n, vibration_features_t *out);
