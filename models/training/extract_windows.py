#!/usr/bin/env python3
"""
CWRU 数据集 — 窗口化数据集构建器
=================================
降采样 12kHz→50Hz → AC耦合 → 256点窗口 → 标签 (0/1/2/3)

用法:
    python extract_windows.py [--data_dir ../cwru_data/] [--window 256] [--stride None]

输出: windowed_dataset.npz  (X: N×256×1, y: N, file_ids: N)
"""

import os
import glob
import argparse
import numpy as np
from scipy import io as sio
from scipy import signal as scisig
from collections import Counter

# 复用 extract_features.py 的标签映射
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'feature_analysis'))
from extract_features import LABEL_MAP, find_mat_variable, label_for_filename


def detect_sampling_rate(signal_length, filename):
    """检测采样率: 12kHz 文件 ~120K 点, 48kHz 文件 ~480K 点"""
    if signal_length > 400_000:
        return 48000
    return 12000


def downsample(signal, orig_fs, target_fs=50):
    """将信号降采样到目标采样率"""
    if orig_fs == target_fs:
        return signal
    target_len = int(len(signal) * target_fs / orig_fs)
    return scisig.resample(signal, target_len)


def ac_couple(signal):
    """减去均值, 去除 DC/重力偏移"""
    return signal - np.mean(signal)


def extract_windows(signal, window_size=256, stride=None):
    """
    从信号中提取固定长度窗口。
    stride=None → 非重叠 (stride = window_size)
    stride=N   → 重叠 N 点, 增加样本量
    """
    if stride is None:
        stride = window_size

    windows = []
    for start in range(0, len(signal) - window_size + 1, stride):
        win = signal[start:start + window_size]
        windows.append(win)
    return np.array(windows, dtype=np.float32)


def build_dataset(data_dir, window_size=256, stride=None):
    """
    主流程: 遍历 .mat 文件, 降采样→AC耦合→窗口→标签。
    返回 X, y, file_ids
    """
    mat_files = sorted(glob.glob(os.path.join(data_dir, "*.mat")))
    if not mat_files:
        print(f"[ERROR] 在 {data_dir} 找不到 .mat 文件!")
        return None, None, None

    if stride is None:
        stride = window_size // 8  # 默认 1/8 窗口重叠, 增加样本量

    print(f"找到 {len(mat_files)} 个 .mat 文件")
    print(f"窗口大小: {window_size}, 步长: {stride}\n")

    X_list, y_list, fid_list = [], [], []

    for fid, fpath in enumerate(mat_files):
        fname = os.path.basename(fpath)
        fault_type, severity, desc = label_for_filename(fpath)

        try:
            mat = sio.loadmat(fpath)
            var = find_mat_variable(mat)
            if var is None:
                print(f"  [SKIP] {fname}: 无振动变量")
                continue
            signal = mat[var].flatten().astype(np.float64)
        except Exception as e:
            print(f"  [SKIP] {fname}: {e}")
            continue

        # 降采样
        orig_fs = detect_sampling_rate(len(signal), fname)
        ds = downsample(signal, orig_fs, target_fs=50)

        # AC 耦合
        ds_ac = ac_couple(ds)

        # 窗口化
        wins = extract_windows(ds_ac, window_size, stride)
        if len(wins) == 0:
            print(f"  [SKIP] {fname}: 信号太短 ({len(ds_ac)} 点)")
            continue

        # 标签
        labels = np.full(len(wins), severity, dtype=np.int8)
        file_ids = np.full(len(wins), fid, dtype=np.int16)

        X_list.append(wins)
        y_list.append(labels)
        fid_list.append(file_ids)

        print(f"  {fname:25s} | {orig_fs//1000}kHz → 50Hz | "
              f"{len(wins):5d} 窗口 | Lv.{severity} {fault_type}")

    if not X_list:
        print("\n[ERROR] 没有生成任何窗口!")
        return None, None, None

    X = np.concatenate(X_list, axis=0)
    y = np.concatenate(y_list, axis=0)
    fids = np.concatenate(fid_list, axis=0)

    # 统计
    unique, counts = np.unique(y, return_counts=True)
    print(f"\n{'='*50}")
    print(f"数据集统计: {len(X)} 窗口 × {window_size} 点")
    for lv, cnt in zip(unique, counts):
        label_names = {0: '正常', 1: '警告(7mil)', 2: '危险(14mil)', 3: '严重(21mil)'}
        print(f"  Lv.{lv} {label_names.get(lv, '?')}: {cnt:6d} ({cnt/len(X)*100:.1f}%)")

    return X, y, fids


def main():
    parser = argparse.ArgumentParser(description="CWRU 窗口化数据集构建")
    parser.add_argument("--data_dir", default="../cwru_data/",
                        help="CWRU .mat 数据目录")
    parser.add_argument("--window", type=int, default=256,
                        help="窗口大小 (点数, 默认 256)")
    parser.add_argument("--stride", type=int, default=None,
                        help="窗口步长 (None=不重叠, 64=重叠)")
    parser.add_argument("--output", default="windowed_dataset.npz",
                        help="输出文件名")
    args = parser.parse_args()

    X, y, fids = build_dataset(args.data_dir, args.window, args.stride)
    if X is None:
        return

    # 保存
    out_path = os.path.join(os.path.dirname(__file__), args.output)
    np.savez_compressed(out_path, X=X, y=y, file_ids=fids)
    print(f"\n保存: {out_path}")
    print(f"文件大小: {os.path.getsize(out_path) / 1024:.0f} KB")


if __name__ == "__main__":
    main()
