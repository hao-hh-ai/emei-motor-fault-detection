/*
 * 微型 MLP 分类器 — 纯 C 实现 (316 params)
 * ===========================================
 * 输入: 8 维特征 (已用 StandardScaler 归一化)
 * 输出: 故障等级 0-3
 * 无外部依赖, 无 TPU 需要
 */

#ifndef MLP_CLASSIFIER_H
#define MLP_CLASSIFIER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MLP 推理: 8 特征 → 故障等级 0-3
 * @param features  8 维特征数组 (RMS/Peak/CF/Kurt/Skew/Clear/Shape/Impulse)
 * @return 故障等级 0-3
 */
int mlp_predict(const float *features);

#ifdef __cplusplus
}
#endif
#endif
