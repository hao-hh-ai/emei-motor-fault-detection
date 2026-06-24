#!/usr/bin/env python3
"""
阈值统计分析
=============
读取 extract_features.py 输出的 extracted_features.csv，
统计各故障类型的特征值分布，生成阈值表。

用法:
    python threshold_analysis.py [--input extracted_features.csv]
"""

import csv
import argparse
import sys
import numpy as np

# ── 阈值定义 ──────────────────────────────────────────
# 基于统计学 (百分位数) 自动生成
# 可手动微调


def stats_for_group(values):
    """返回一组值的统计摘要"""
    arr = np.array(values)
    return {
        'mean':   np.mean(arr),
        'std':    np.std(arr),
        'min':    np.min(arr),
        'max':    np.max(arr),
        'p25':    np.percentile(arr, 25),
        'p50':    np.percentile(arr, 50),
        'p75':    np.percentile(arr, 75),
        'p90':    np.percentile(arr, 90),
        'p95':    np.percentile(arr, 95),
        'p99':    np.percentile(arr, 99),
        'count':  len(arr),
    }


def build_threshold_table(rows):
    """分析特征分布，输出建议阈值"""

    # 按故障类型分组
    groups = {}
    for r in rows:
        ft = r['fault_type']
        if ft not in groups:
            groups[ft] = []
        groups[ft].append(r)

    print("=" * 60)
    print("CWRU 特征值统计分析")
    print("=" * 60)

    for ftype in ["normal", "inner_race", "outer_race", "ball"]:
        if ftype not in groups:
            continue
        group = groups[ftype]
        n = len(group)
        print(f"\n{'─' * 40}")
        print(f"故障类型: {ftype}  (样本数: {n})")
        print(f"{'─' * 40}")

        for feat_name in ['rms', 'peak', 'crest_factor', 'kurtosis']:
            vals = [float(r[feat_name]) for r in group]
            s = stats_for_group(vals)

            print(f"\n  [{feat_name}]")
            print(f"    mean={s['mean']:.4f}  std={s['std']:.4f}")
            print(f"    min={s['min']:.4f}   max={s['max']:.4f}")
            print(f"    P50={s['p50']:.4f}   P95={s['p95']:.4f}   P99={s['p99']:.4f}")

    # ── 自动生成阈值建议 ──
    normal = groups.get("normal", [])
    if not normal:
        print("\n[WARN] 没有正常数据，无法生成阈值!")
        return

    # 取正常数据的 P95 作为警告阈值起点
    print(f"\n{'=' * 60}")
    print("建议阈值表 (基于正常数据 P95 + 安全余量)")
    print("=" * 60)

    ref = {}
    for feat_name in ['rms', 'peak', 'crest_factor', 'kurtosis']:
        vals = [float(r[feat_name]) for r in normal]
        s = stats_for_group(vals)
        ref[feat_name] = s

    rms_normal = ref['rms']['p95']
    rms_warn   = round(rms_normal * 4.0, 2)     # 4x 正常 P95
    rms_danger = round(rms_normal * 8.0, 2)     # 8x 正常 P95
    rms_crit   = round(rms_normal * 16.0, 2)    # 16x 正常 P95

    kurt_normal = ref['kurtosis']['p95']
    kurt_warn   = round(max(kurt_normal + 1.0, 3.5), 1)
    kurt_danger = round(max(kurt_normal + 2.0, 5.0), 1)

    crest_warn  = round(ref['crest_factor']['p95'] * 2.0, 1)

    print(f"""
建议阈值 (可根据实际数据分布调整):
----------------------------------------------------------
  FAULT_RMS_WARN       = {rms_warn:.2f}f   // RMS > {rms_warn:.2f} g → 警告
  FAULT_RMS_DANGER     = {rms_danger:.2f}f  // RMS > {rms_danger:.2f} g → 危险
  FAULT_RMS_CRITICAL   = {rms_crit:.2f}f  // RMS > {rms_crit:.2f} g → 严重
  FAULT_KURT_WARN      = {kurt_warn:.1f}f    // Kurtosis > {kurt_warn:.1f} → 异常
  FAULT_KURT_DANGER    = {kurt_danger:.1f}f   // Kurtosis > {kurt_danger:.1f} → 严重
  FAULT_CREST_WARN     = {crest_warn:.1f}f   // Crest Factor > {crest_warn:.1f} → 早期冲击
""")

    # ── 生成 threshold_config.py ──
    out_path = "../../huashan/bridge/threshold_config.py"
    with open(out_path, 'w') as f:
        f.write(f'''# 时域特征阈值表
# 自动生成于 threshold_analysis.py
# 基于 CWRU 轴承数据集统计分析

# ── RMS 阈值 (单位: m/s²) ──
TH_RMS_WARN     = {rms_warn:.2f} * 9.81   # 转为 m/s²
TH_RMS_DANGER   = {rms_danger:.2f} * 9.81
TH_RMS_CRITICAL = {rms_crit:.2f} * 9.81

# ── Kurtosis 阈值 ──
TH_KURT_WARN    = {kurt_warn:.1f}
TH_KURT_DANGER  = {kurt_danger:.1f}

# ── Crest Factor 阈值 ──
TH_CREST_WARN   = {crest_warn:.1f}
''')
    print(f"阈值配置已生成 → {out_path}")

    # ── 生成 C 头文件阈值 ──
    out_h = "../../components/feature_extract/threshold_config.h"
    with open(out_h, 'w') as f:
        f.write(f'''#pragma once

/* 时域特征故障阈值表 (基于 CWRU 数据集分析) */

/* RMS 阈值 (单位: g, 即 9.81 m/s² 的倍数) */
#define FAULT_RMS_WARN      {rms_warn:.2f}f
#define FAULT_RMS_DANGER    {rms_danger:.2f}f
#define FAULT_RMS_CRITICAL  {rms_crit:.2f}f

/* Kurtosis 阈值 (纯量纲) */
#define FAULT_KURT_WARN     {kurt_warn:.1f}f
#define FAULT_KURT_DANGER   {kurt_danger:.1f}f

/* Crest Factor 阈值 */
#define FAULT_CREST_WARN    {crest_warn:.1f}f
''')
    print(f"C 阈值头文件已生成 → {out_h}")


def main():
    parser = argparse.ArgumentParser(description="阈值统计分析")
    parser.add_argument("--input", default="extracted_features.csv",
                        help="特征 CSV 文件")
    args = parser.parse_args()

    rows = []
    try:
        with open(args.input, 'r') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
    except FileNotFoundError:
        print(f"[ERROR] 找不到 {args.input}")
        print("请先运行: python extract_features.py")
        sys.exit(1)

    print(f"加载 {len(rows)} 条特征记录\n")
    build_threshold_table(rows)


if __name__ == '__main__':
    main()
