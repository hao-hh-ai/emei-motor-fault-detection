#!/usr/bin/env python3
"""
CWRU 轴承数据集 — 时域特征提取
=================================
从 .mat 文件读取振动信号，滑动窗口提取 RMS/Peak/Crest Factor/Kurtosis。

用法:
    python extract_features.py [--window 256] [--data_dir ../cwru_data/]

输出: extracted_features.csv (带故障标签)
"""

import os
import csv
import glob
import argparse
import numpy as np
from scipy import io as sio
from collections import defaultdict

# ── CWRU 数据集标签映射 ──────────────────────────────
# 文件名模式 → (故障类型, 故障等级, 转速)
LABEL_MAP = [
    # ── 正常 ──
    ("normal",   "normal",   0, "baseline"),
    ("97.mat",   "normal",   0, "1797rpm"),
    ("98.mat",   "normal",   0, "1772rpm"),
    ("99.mat",   "normal",   0, "1750rpm"),
    ("100.mat",  "normal",   0, "1730rpm"),
    # ── 内圈故障 (7mils / 14mils / 21mils) ──
    ("105.mat",  "inner_race",  1, "1797rpm_7mil"),
    ("106.mat",  "inner_race",  2, "1797rpm_14mil"),
    ("107.mat",  "inner_race",  3, "1797rpm_21mil"),
    ("169.mat",  "inner_race",  1, "1772rpm_7mil"),
    ("170.mat",  "inner_race",  2, "1772rpm_14mil"),
    ("171.mat",  "inner_race",  3, "1772rpm_21mil"),
    ("209.mat",  "inner_race",  1, "1750rpm_7mil"),
    ("210.mat",  "inner_race",  2, "1750rpm_14mil"),
    ("211.mat",  "inner_race",  3, "1750rpm_21mil"),
    # ── 外圈故障 ──
    ("130.mat",  "outer_race",  1, "1797rpm_7mil"),
    ("131.mat",  "outer_race",  2, "1797rpm_14mil"),
    ("132.mat",  "outer_race",  3, "1797rpm_21mil"),
    ("197.mat",  "outer_race",  1, "1772rpm_7mil"),
    ("198.mat",  "outer_race",  2, "1772rpm_14mil"),
    ("199.mat",  "outer_race",  3, "1772rpm_21mil"),
    ("234.mat",  "outer_race",  1, "1750rpm_7mil"),
    ("235.mat",  "outer_race",  2, "1750rpm_14mil"),
    ("236.mat",  "outer_race",  3, "1750rpm_21mil"),
    # ── 滚珠故障 ──
    ("118.mat",  "ball",        1, "1797rpm_7mil"),
    ("119.mat",  "ball",        2, "1797rpm_14mil"),
    ("120.mat",  "ball",        3, "1797rpm_21mil"),
    ("185.mat",  "ball",        1, "1772rpm_7mil"),
    ("186.mat",  "ball",        2, "1772rpm_14mil"),
    ("187.mat",  "ball",        3, "1772rpm_21mil"),
    ("222.mat",  "ball",        1, "1750rpm_7mil"),
    ("223.mat",  "ball",        2, "1750rpm_14mil"),
    ("224.mat",  "ball",        3, "1750rpm_21mil"),
]


def find_mat_variable(mat_data):
    """查找 .mat 文件中的振动数据变量名
    CWRU 文件中变量名格式: X???_DE_time (驱动端), X???_FE_time (风扇端)
    """
    candidates = []
    for key in mat_data.keys():
        if key.startswith('X') and ('DE_time' in key or 'FE_time' in key):
            candidates.append(key)
    # 优先取驱动端 (DE)
    for key in candidates:
        if 'DE' in key:
            return key
    if candidates:
        return candidates[0]
    return None


def compute_features(signal, window_size=256):
    """
    对一段一维信号做滑窗特征提取 (8 维时域特征 + AC 耦合)。
    与 bridge.py 和 feature_extract.c 的公式完全一致。

    返回:
        features: list of dict, 每个元素包含一窗的 8 个特征值
    """
    n = len(signal)
    features = []

    for start in range(0, n - window_size + 1, window_size):
        win = signal[start:start + window_size].astype(np.float64)

        # ── AC 耦合: 去掉直流分量 ──
        mean = np.mean(win)
        ac = win - mean                    # 零均值振动信号
        ac_abs = np.abs(ac)

        rms       = np.sqrt(np.mean(ac ** 2))
        peak      = np.max(ac_abs)
        mean_abs  = np.mean(ac_abs)

        # 方差 & 偏度 & 峭度
        var = np.var(ac)
        sigma = np.sqrt(var)
        if sigma > 0.01:
            m3 = np.mean(ac ** 3)
            m4 = np.mean(ac ** 4)
            skew = m3 / (sigma ** 3)
            kurt = m4 / (sigma ** 4)
        else:
            skew = 0.0
            kurt = 1.0

        # 间隙因子
        sum_sqrt_ac = np.sum(np.sqrt(ac_abs))
        clearance = peak / ((sum_sqrt_ac / n) ** 2) if sum_sqrt_ac > 0 else 1.0

        # 波形因子 / 脉冲因子
        shape   = rms / mean_abs if mean_abs > 0 else 1.0
        impulse = peak / mean_abs if mean_abs > 0 else 1.0

        # 峰值因子
        crest = peak / rms if rms > 0.1 else 1.0

        features.append({
            'rms':             round(rms, 6),
            'peak':            round(peak, 6),
            'crest_factor':    round(crest, 4),
            'kurtosis':        round(kurt, 4),
            'skewness':        round(skew, 4),
            'clearance':       round(clearance, 4),
            'shape_factor':    round(shape, 4),
            'impulse_factor':  round(impulse, 4),
        })

    return features


def label_for_filename(fname):
    """根据文件名匹配标签"""
    base = os.path.basename(fname)
    for pattern, fault_type, severity, desc in LABEL_MAP:
        if pattern in base:
            return fault_type, severity, desc
    return "unknown", 0, base


def process_file(filepath, window_size):
    """处理单个 .mat 文件, 返回特征列表"""
    try:
        mat = sio.loadmat(filepath)
        var = find_mat_variable(mat)
        if var is None:
            print(f"  [SKIP] {filepath}: 找不到振动变量")
            return []
        signal = mat[var].flatten().astype(np.float64)
        feats = compute_features(signal, window_size)
        return feats
    except Exception as e:
        print(f"  [ERROR] {filepath}: {e}")
        return []


# ── 主程序 ────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="CWRU 特征提取")
    parser.add_argument("--data_dir", default="../cwru_data/",
                        help="CWRU .mat 数据目录")
    parser.add_argument("--window", type=int, default=256,
                        help="滑窗大小 (点数)")
    parser.add_argument("--output", default="extracted_features.csv",
                        help="输出 CSV 文件")
    args = parser.parse_args()

    mat_files = glob.glob(os.path.join(args.data_dir, "*.mat"))
    if not mat_files:
        print(f"[ERROR] 在 {args.data_dir} 找不到 .mat 文件!")
        print("请从 CWRU Bearing Data Center 下载数据集:")
        print("  https://engineering.case.edu/bearingdatacenter/download-data-file")
        print("下载后将 .mat 文件放入 models/cwru_data/ 目录")
        return

    print(f"找到 {len(mat_files)} 个 .mat 文件")
    print(f"窗口大小: {args.window} 点\n")

    rows = []
    total_feats = 0

    for fpath in sorted(mat_files):
        fname = os.path.basename(fpath)
        fault_type, severity, desc = label_for_filename(fpath)
        print(f"处理: {fname:30s} → {fault_type:15s} Lv.{severity} ({desc})")

        feats = process_file(fpath, args.window)
        for f in feats:
            row = {
                'file': fname,
                'fault_type': fault_type,
                'severity': severity,
                'description': desc,
                **f,
            }
            rows.append(row)

        print(f"  → {len(feats)} 窗特征")
        total_feats += len(feats)

    # 写 CSV
    fieldnames = ['file', 'fault_type', 'severity', 'description',
                  'rms', 'peak', 'crest_factor', 'kurtosis',
                  'skewness', 'clearance', 'shape_factor', 'impulse_factor']
    with open(args.output, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(f"\n{'='*50}")
    print(f"完成! 共 {total_feats} 条特征 → {args.output}")
    print(f"故障类型分布:")
    counts = defaultdict(int)
    for r in rows:
        counts[r['fault_type']] += 1
    for k, v in sorted(counts.items()):
        print(f"  {k:20s}: {v:6d}")


if __name__ == '__main__':
    main()
