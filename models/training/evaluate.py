#!/usr/bin/env python3
"""
模型评估 — CNN vs 规则系统对比
===============================
在同一测试集上比较 1D-CNN 和当前基于阈值的规则系统。

用法: python evaluate.py [--data windowed_dataset.npz] [--model ../artifacts/cnn_model.h5]
"""

import os
import sys
import json
import argparse
import pickle
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from sklearn.metrics import (classification_report, confusion_matrix,
                              accuracy_score, f1_score, precision_score, recall_score)

# 导入规则系统所需
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'feature_analysis'))
from extract_features import compute_features, label_for_filename
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'huashan', 'bridge'))
from threshold_config import *

import torch
import torch.nn as nn
from train_1dcnn import FaultClassifier1D, per_sample_normalize


# ── 规则系统复现 (与 bridge.py detect_fault 一致) ──

PERSIST_N = 3

def rule_based_predict(windows, stride):
    """
    用规则系统对窗口序列做故障判定。
    返回: predicted_levels (与 windows 一一对应)
    """
    from collections import deque
    history = deque(maxlen=PERSIST_N)
    predictions = []

    for win in windows:
        # 提取 8 特征
        feats = compute_features(win, len(win))
        if not feats:
            predictions.append(0)
            continue
        f = feats[0]

        score = 0.0
        wsum = 0.0

        rules = {
            "rms":            (TH_RMS_WARN,   TH_RMS_DANGER,   TH_RMS_CRITICAL),
            "peak":           (TH_PEAK_WARN,  TH_PEAK_DANGER,  TH_PEAK_CRITICAL),
            "kurtosis":       (TH_KURT_WARN,  TH_KURT_DANGER,  TH_KURT_CRITICAL),
            "crest_factor":   (TH_CREST_WARN, TH_CREST_DANGER, TH_CREST_CRITICAL),
            "clearance":      (TH_CLEAR_WARN, TH_CLEAR_DANGER, TH_CLEAR_CRITICAL),
            "impulse_factor": (TH_IMP_WARN,   TH_IMP_DANGER,   TH_IMP_CRITICAL),
            "shape_factor":   (TH_SHAPE_WARN, TH_SHAPE_DANGER, TH_SHAPE_CRITICAL),
        }

        for key, (lo, hi, crit) in rules.items():
            v = f.get(key, 0)
            w = FEATURE_WEIGHTS.get(key, 1.0)
            if v >= crit:        s = 3.0
            elif v >= hi:        s = 2.0
            elif v >= lo:        s = 1.0
            else:                s = 0.0
            score += s * w
            wsum  += w

        avg = score / wsum if wsum > 0 else 0

        if   avg >= SCORE_CRITICAL: raw = 3
        elif avg >= SCORE_DANGER:   raw = 2
        elif avg >= SCORE_WARN:     raw = 1
        else:                       raw = 0

        # 持续性滤波
        history.append(raw)
        if len(history) >= PERSIST_N and all(h == raw for h in history):
            predictions.append(raw)
        else:
            predictions.append(history[-1] if len(history) > 0 else 0)

    return np.array(predictions, dtype=np.int8)


# ── 评估工具 ──

def compute_metrics(y_true, y_pred, label_names):
    """计算并返回各项指标"""
    return {
        'accuracy':  float(accuracy_score(y_true, y_pred)),
        'precision_macro': float(precision_score(y_true, y_pred, average='macro', zero_division=0)),
        'recall_macro':    float(recall_score(y_true, y_pred, average='macro', zero_division=0)),
        'f1_macro':        float(f1_score(y_true, y_pred, average='macro', zero_division=0)),
        'f1_per_class':    {str(k): float(v) for k, v in enumerate(
            f1_score(y_true, y_pred, average=None, zero_division=0))},
        'class_report': classification_report(y_true, y_pred,
                                               target_names=label_names,
                                               zero_division=0,
                                               output_dict=True),
    }


def plot_confusion(y_true, y_pred, label_names, title, path):
    """绘制并保存混淆矩阵"""
    cm = confusion_matrix(y_true, y_pred)
    fig, ax = plt.subplots(figsize=(6, 5))
    im = ax.imshow(cm, cmap='Blues')
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            ax.text(j, i, cm[i, j], ha='center', va='center',
                    fontsize=14, fontweight='bold',
                    color='white' if cm[i, j] > cm.max() / 2 else 'black')
    ax.set_xticks(range(len(label_names)))
    ax.set_yticks(range(len(label_names)))
    ax.set_xticklabels(label_names)
    ax.set_yticklabels(label_names)
    ax.set_xlabel('Predicted'); ax.set_ylabel('True')
    ax.set_title(title)
    plt.colorbar(im, ax=ax)
    fig.tight_layout()
    fig.savefig(path, dpi=100, bbox_inches='tight')
    plt.close(fig)


# ── 主程序 ──

def main():
    parser = argparse.ArgumentParser(description="CNN vs 规则 对比评估")
    parser.add_argument("--data", default="windowed_dataset.npz")
    parser.add_argument("--model", default="../artifacts/cnn_model.pth")
    parser.add_argument("--outdir", default="../artifacts/")
    args = parser.parse_args()

    data_path = os.path.join(os.path.dirname(__file__), args.data)
    model_path = os.path.join(os.path.dirname(__file__), args.model)
    outdir = os.path.join(os.path.dirname(__file__), args.outdir)
    os.makedirs(outdir, exist_ok=True)

    label_names = ['正常(0)', '警告(1)', '危险(2)', '严重(3)']

    # ── 加载数据 ──
    print(f"加载测试数据: {data_path}")
    data = np.load(data_path)
    X, y = data['X'], data['y']

    # 分层划分 (与 train_1dcnn.py 一致: 70/15/15)
    from sklearn.model_selection import train_test_split
    _, X_test, _, y_test = train_test_split(
        X.reshape(len(X), -1), y, test_size=0.15, stratify=y, random_state=42)
    X_test = X_test.reshape(-1, 256)

    print(f"  测试集: {len(X_test)} 窗口")
    for lv in range(4):
        cnt = np.sum(y_test == lv)
        print(f"    Lv.{lv}: {cnt}")

    # ── CNN 评估 ──
    print(f"\n{'='*50}")
    print("1D-CNN 模型评估 (PyTorch)")
    print(f"{'='*50}")

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    model = FaultClassifier1D().to(device)
    model.load_state_dict(torch.load(model_path, map_location=device, weights_only=True))
    model.eval()

    # 归一化
    X_norm = per_sample_normalize(X_test.reshape(len(X_test), -1))
    X_t = torch.tensor(X_norm[:, np.newaxis, :], dtype=torch.float32)

    # 批量推理
    cnn_preds = []
    with torch.no_grad():
        for i in range(0, len(X_t), 64):
            batch = X_t[i:i+64].to(device)
            out = model(batch)
            cnn_preds.append(out.argmax(1).cpu().numpy())
    cnn_preds = np.concatenate(cnn_preds).astype(np.int8)
    cnn_metrics = compute_metrics(y_test, cnn_preds, label_names)
    print(f"\nCNN Accuracy: {cnn_metrics['accuracy']:.4f}")
    print(f"CNN F1-macro: {cnn_metrics['f1_macro']:.4f}")
    print(f"CNN F1/class:  { {k: f'{v:.3f}' for k, v in cnn_metrics['f1_per_class'].items()} }")

    # ── 规则系统评估 ──
    print(f"\n{'='*50}")
    print("规则系统评估 (8 特征 + 加权评分 + 持续性滤波)")
    print(f"{'='*50}")

    # 规则系统需要原始信号 (非归一化)
    windows_raw = [X_test[i].flatten() for i in range(len(X_test))]
    rule_preds = rule_based_predict(windows_raw, stride=None)
    rule_metrics = compute_metrics(y_test, rule_preds, label_names)
    print(f"\nRule Accuracy: {rule_metrics['accuracy']:.4f}")
    print(f"Rule F1-macro: {rule_metrics['f1_macro']:.4f}")
    print(f"Rule F1/class: { {k: f'{v:.3f}' for k, v in rule_metrics['f1_per_class'].items()} }")

    # ── 对比 ──
    print(f"\n{'='*50}")
    print("对比汇总")
    print(f"{'='*50}")
    print(f"{'指标':<20} {'CNN':>10} {'Rule':>10} {'提升':>10}")
    print(f"{'-'*50}")
    for metric in ['accuracy', 'precision_macro', 'recall_macro', 'f1_macro']:
        c = cnn_metrics[metric]
        r = rule_metrics[metric]
        delta = c - r
        sign = '+' if delta > 0 else ''
        print(f"{metric:<20} {c:10.4f} {r:10.4f} {sign}{delta:9.4f}")

    # ── 混淆矩阵 ──
    plot_confusion(y_test, cnn_preds, label_names,
                   '1D-CNN Confusion Matrix',
                   os.path.join(outdir, 'confusion_cnn.png'))
    plot_confusion(y_test, rule_preds, label_names,
                   'Rule-Based Confusion Matrix',
                   os.path.join(outdir, 'confusion_rule.png'))
    print(f"\n混淆矩阵: confusion_cnn.png / confusion_rule.png")

    # ── 保存报告 ──
    report = {
        'cnn': cnn_metrics,
        'rule_based': rule_metrics,
        'comparison': {
            'accuracy_delta':  float(cnn_metrics['accuracy'] - rule_metrics['accuracy']),
            'f1_macro_delta':  float(cnn_metrics['f1_macro'] - rule_metrics['f1_macro']),
            'test_samples':    int(len(y_test)),
        },
        'label_distribution': {str(k): int(v) for k, v in zip(*np.unique(y_test, return_counts=True))},
    }
    report_path = os.path.join(outdir, 'evaluation_report.json')
    with open(report_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print(f"报告: {report_path}")


if __name__ == '__main__':
    main()
