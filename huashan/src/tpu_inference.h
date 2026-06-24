/*
 * TPU 推理模块 — 华山派 CV1812H
 * v2: 8 特征输入 (替代 256 点波形)
 */

#ifndef TPU_INFERENCE_H
#define TPU_INFERENCE_H

#ifdef __cplusplus
extern "C" {
#endif

int  tpu_init(const char *model_path);
int  tpu_infer(const float *features_8);  /* 8 维特征 → 故障等级 0-3 */
void tpu_deinit(void);
int  tpu_is_ready(void);

#ifdef __cplusplus
}
#endif
#endif
