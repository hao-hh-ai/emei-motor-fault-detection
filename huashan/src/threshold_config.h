#pragma once

/* ── 时域特征阈值表 (8 维) ───────────────────────────────
 * 基于 CWRU 轴承数据集统计分析
 * 单位: m/s² (RMS/Peak), 无量纲 (其余)
 *
 * 源自 huashan/bridge/threshold_config.py, 保持同步
 * ─────────────────────────────────────────────────────── */

/* ── RMS (m/s², AC 耦合) ── */
#define TH_RMS_WARN      (0.08f * 9.81f)   /* 0.78 m/s² */
#define TH_RMS_DANGER    (0.20f * 9.81f)   /* 1.96 m/s² */
#define TH_RMS_CRITICAL  (0.50f * 9.81f)   /* 4.90 m/s² */

/* ── Peak (m/s²) ── */
#define TH_PEAK_WARN     (0.25f * 9.81f)   /* 2.45 m/s² */
#define TH_PEAK_DANGER   (0.60f * 9.81f)   /* 5.89 m/s² */
#define TH_PEAK_CRITICAL (1.50f * 9.81f)   /* 14.7 m/s² */

/* ── Kurtosis ── */
#define TH_KURT_WARN      3.5f
#define TH_KURT_DANGER    5.0f
#define TH_KURT_CRITICAL  8.0f

/* ── Crest Factor ── */
#define TH_CREST_WARN      4.0f
#define TH_CREST_DANGER    7.0f
#define TH_CREST_CRITICAL 12.0f

/* ── Clearance Factor ── */
#define TH_CLEAR_WARN      5.0f
#define TH_CLEAR_DANGER   10.0f
#define TH_CLEAR_CRITICAL 20.0f

/* ── Impulse Factor ── */
#define TH_IMP_WARN        3.0f
#define TH_IMP_DANGER      6.0f
#define TH_IMP_CRITICAL   10.0f

/* ── Shape Factor ── */
#define TH_SHAPE_WARN      1.5f
#define TH_SHAPE_DANGER    2.0f
#define TH_SHAPE_CRITICAL  3.0f

/* ── 特征权重 ── */
static const float feat_weight_rms            = 2.0f;
static const float feat_weight_kurtosis       = 2.0f;
static const float feat_weight_peak           = 1.5f;
static const float feat_weight_crest_factor   = 1.5f;
static const float feat_weight_clearance      = 1.5f;
static const float feat_weight_impulse_factor = 1.0f;
static const float feat_weight_shape_factor   = 0.5f;
static const float feat_weight_skewness       = 0.5f;

/* ── 判定阈值 (加权平均分) ── */
#define SCORE_WARN      0.8f
#define SCORE_DANGER    1.8f
#define SCORE_CRITICAL  2.5f

/* ── 窗体与持续性 ── */
#define WINDOW_SIZE      256
#define PERSIST_N          3
#define LSB_TO_MS2        (0.0039f * 9.81f)  /* int16 raw → m/s² */
