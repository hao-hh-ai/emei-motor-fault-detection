# 时域特征阈值表 (8 维特征)
# 基于 CWRU 轴承数据集统计分析
# 单位: m/s² (RMS/Peak), 无量纲 (其余)

# ── RMS (m/s², AC 耦合后值约为直流的 5%~50%) ──
TH_RMS_WARN     = 0.08 * 9.81   # 0.78 m/s² (轻微振动)
TH_RMS_DANGER   = 0.20 * 9.81   # 1.96 m/s² (明显振动)
TH_RMS_CRITICAL = 0.50 * 9.81   # 4.90 m/s² (剧烈振动)

# ── Peak (m/s²) ──
TH_PEAK_WARN    = 0.25 * 9.81   # 2.45 m/s²
TH_PEAK_DANGER  = 0.60 * 9.81   # 5.89 m/s²
TH_PEAK_CRITICAL = 1.50 * 9.81  # 14.7 m/s²

# ── Kurtosis ──
TH_KURT_WARN    = 3.5
TH_KURT_DANGER  = 5.0
TH_KURT_CRITICAL = 8.0

# ── Crest Factor ──
TH_CREST_WARN   = 4.0
TH_CREST_DANGER = 7.0
TH_CREST_CRITICAL = 12.0

# ── Clearance Factor ──
TH_CLEAR_WARN   = 5.0
TH_CLEAR_DANGER = 10.0
TH_CLEAR_CRITICAL = 20.0

# ── Impulse Factor ──
TH_IMP_WARN     = 3.0
TH_IMP_DANGER   = 6.0
TH_IMP_CRITICAL = 10.0

# ── Shape Factor ──
TH_SHAPE_WARN   = 1.5
TH_SHAPE_DANGER = 2.0
TH_SHAPE_CRITICAL = 3.0

# ── 特征权重 ──
FEATURE_WEIGHTS = {
    "rms":            2.0,
    "kurtosis":       2.0,
    "peak":           1.5,
    "crest_factor":   1.5,
    "clearance":      1.5,
    "impulse_factor": 1.0,
    "shape_factor":   0.5,
    "skewness":       0.5,
}

# ── 判定阈值 (加权平均分) ──
SCORE_WARN   = 0.8
SCORE_DANGER = 1.8
SCORE_CRITICAL = 2.5
