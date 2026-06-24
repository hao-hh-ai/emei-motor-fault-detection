/*
 * TPU 推理模块 — 华山派 CV1812H Sophon TPU
 * ==========================================
 * 封装 CVI NN Runtime API: 加载 cvimodel → 归一化输入 → 推理 → 故障等级
 *
 * 用法:
 *   tpu_init("/mnt/data/motor_fault_bf16.cvimodel");
 *   int level = tpu_infer(window_256);
 *   tpu_deinit();
 */

#ifndef TPU_INFERENCE_H
#define TPU_INFERENCE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化 TPU 并加载模型
 * @param model_path  cvimodel 文件路径
 * @return 0 成功, -1 失败 (可回退到规则系统)
 */
int tpu_init(const char *model_path);

/**
 * TPU 推理: 256 点归一化振动窗口 → 故障等级 0-3
 * @param window  256 点浮点数组 (合成幅值, 未归一化)
 * @return 故障等级 0=正常 1=警告 2=危险 3=严重, -1=推理失败
 */
int tpu_infer(const float *window);

/**
 * 释放 TPU 资源
 */
void tpu_deinit(void);

/**
 * 查询 TPU 是否可用
 * @return 1=已初始化, 0=未初始化/加载失败
 */
int tpu_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* TPU_INFERENCE_H */
