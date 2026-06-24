#!/usr/bin/env python3
"""
1D-CNN 故障分类模型训练 (PyTorch)
==================================
从 windowed_dataset.npz 加载数据, 训练 4 分类 (0/1/2/3) 模型。

架构: 4 层 Conv1D → GlobalAvgPool → Dense → Softmax (~25K params)
用法: py -3.13 train_1dcnn.py [--epochs 100] [--batch 32]
"""

import os
import argparse
import pickle
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split
from sklearn.utils.class_weight import compute_class_weight
from sklearn.preprocessing import StandardScaler

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset


# ── 1D-CNN 模型架构 ─────────────────────────────────

class FaultClassifier1D(nn.Module):
    """轻量 1D-CNN (~25K params), 适配 0.5 TOPS TPU"""

    def __init__(self, input_channels=1, num_classes=4):
        super().__init__()

        self.conv1 = nn.Sequential(
            nn.Conv1d(input_channels, 16, kernel_size=7, padding='same'),
            nn.BatchNorm1d(16),
            nn.ReLU(),
            nn.MaxPool1d(2),                    # → (128, 16)
        )
        self.conv2 = nn.Sequential(
            nn.Conv1d(16, 32, kernel_size=5, padding='same'),
            nn.BatchNorm1d(32),
            nn.ReLU(),
            nn.MaxPool1d(2),                    # → (64, 32)
        )
        self.conv3 = nn.Sequential(
            nn.Conv1d(32, 64, kernel_size=3, padding='same'),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.MaxPool1d(2),                    # → (32, 64)
        )
        self.conv4 = nn.Sequential(
            nn.Conv1d(64, 64, kernel_size=3, padding='same'),
            nn.BatchNorm1d(64),
            nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),            # → (64, 1)
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64, 64),
            nn.ReLU(),
            nn.Dropout(0.3),
            nn.Linear(64, num_classes),
        )

    def forward(self, x):
        x = self.conv1(x)
        x = self.conv2(x)
        x = self.conv3(x)
        x = self.conv4(x)
        x = self.classifier(x)
        return x


# ── 每样本归一化 ─────────────────────────────────────

def per_sample_normalize(X):
    """对每个窗口做零均值-单位方差归一化 (适配流式推理)"""
    # X: (N, 256) or (N, 1, 256)
    if X.ndim == 3:
        X = X[:, 0, :]
    X_norm = np.empty_like(X, dtype=np.float32)
    for i in range(len(X)):
        win = X[i]
        mu = np.mean(win)
        std = np.std(win)
        if std > 1e-8:
            X_norm[i] = (win - mu) / std
        else:
            X_norm[i] = win - mu
    return X_norm


def to_torch(X, y):
    """numpy → PyTorch DataLoader"""
    if X.ndim == 2:
        X = X[:, np.newaxis, :]  # (N, 256) → (N, 1, 256)
    X_t = torch.tensor(X, dtype=torch.float32)
    y_t = torch.tensor(y, dtype=torch.long)
    return X_t, y_t


# ── 训练函数 ────────────────────────────────────────

def train_epoch(model, loader, criterion, optimizer, device):
    model.train()
    total_loss, correct, total = 0, 0, 0
    for Xb, yb in loader:
        Xb, yb = Xb.to(device), yb.to(device)
        optimizer.zero_grad()
        out = model(Xb)
        loss = criterion(out, yb)
        loss.backward()
        optimizer.step()
        total_loss += loss.item() * len(yb)
        correct += (out.argmax(1) == yb).sum().item()
        total += len(yb)
    return total_loss / total, correct / total


def validate(model, loader, criterion, device):
    model.eval()
    total_loss, correct, total = 0, 0, 0
    with torch.no_grad():
        for Xb, yb in loader:
            Xb, yb = Xb.to(device), yb.to(device)
            out = model(Xb)
            loss = criterion(out, yb)
            total_loss += loss.item() * len(yb)
            correct += (out.argmax(1) == yb).sum().item()
            total += len(yb)
    return total_loss / total, correct / total


# ── 主程序 ──────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="1D-CNN 故障分类训练 (PyTorch)")
    parser.add_argument("--data", default="windowed_dataset.npz")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--batch", type=int, default=32)
    parser.add_argument("--lr", type=float, default=0.001)
    parser.add_argument("--outdir", default="../artifacts/")
    args = parser.parse_args()

    data_path = os.path.join(os.path.dirname(__file__), args.data)
    outdir = os.path.join(os.path.dirname(__file__), args.outdir)
    os.makedirs(outdir, exist_ok=True)

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"设备: {device}")

    # ── 加载数据 ──
    print(f"加载: {data_path}")
    data = np.load(data_path)
    X, y = data['X'], data['y']
    print(f"  X: {X.shape}, y: {y.shape}")
    for lv in range(4):
        print(f"  Lv.{lv}: {np.sum(y == lv)}")

    # ── 划分数据集 (70/15/15 分层) ──
    X_temp, X_test, y_temp, y_test = train_test_split(
        X, y, test_size=0.15, stratify=y, random_state=42)
    X_train, X_val, y_train, y_val = train_test_split(
        X_temp, y_temp, test_size=0.15/0.85, stratify=y_temp, random_state=42)
    print(f"划分: train={len(X_train)}, val={len(X_val)}, test={len(X_test)}")

    # ── 归一化 ──
    print("归一化 (per-sample)...")
    X_train = per_sample_normalize(X_train)
    X_val   = per_sample_normalize(X_val)
    X_test  = per_sample_normalize(X_test)

    # ── 类别权重 ──
    cls_weights = compute_class_weight('balanced', classes=np.unique(y_train), y=y_train)
    cls_weight_tensor = torch.tensor(cls_weights, dtype=torch.float32).to(device)
    print(f"类别权重: {[round(w, 2) for w in cls_weights]}")

    # ── DataLoader ──
    X_train_t, y_train_t = to_torch(X_train, y_train)
    X_val_t, y_val_t = to_torch(X_val, y_val)
    X_test_t, y_test_t = to_torch(X_test, y_test)

    train_loader = DataLoader(TensorDataset(X_train_t, y_train_t), batch_size=args.batch, shuffle=True)
    val_loader   = DataLoader(TensorDataset(X_val_t, y_val_t), batch_size=args.batch, shuffle=False)
    test_loader  = DataLoader(TensorDataset(X_test_t, y_test_t), batch_size=args.batch, shuffle=False)

    # ── 模型 & 优化器 ──
    model = FaultClassifier1D().to(device)
    print(f"参数量: {sum(p.numel() for p in model.parameters()):,}")

    criterion = nn.CrossEntropyLoss(weight=cls_weight_tensor)
    optimizer = optim.Adam(model.parameters(), lr=args.lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(
        optimizer, mode='min', factor=0.5, patience=10, min_lr=1e-6)

    # ── 训练 ──
    print(f"\n开始训练 ({args.epochs} epochs, batch={args.batch})...")
    history = {'train_loss': [], 'train_acc': [], 'val_loss': [], 'val_acc': []}
    best_val_acc = 0
    best_state = None
    patience_counter = 0

    for epoch in range(args.epochs):
        train_loss, train_acc = train_epoch(model, train_loader, criterion, optimizer, device)
        val_loss, val_acc = validate(model, val_loader, criterion, device)

        history['train_loss'].append(train_loss)
        history['train_acc'].append(train_acc)
        history['val_loss'].append(val_loss)
        history['val_acc'].append(val_acc)

        scheduler.step(val_loss)

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}
            patience_counter = 0
        else:
            patience_counter += 1

        if (epoch + 1) % 10 == 0 or epoch == 0:
            print(f"Epoch {epoch+1:3d} | train_loss={train_loss:.4f} train_acc={train_acc:.4f} "
                  f"| val_loss={val_loss:.4f} val_acc={val_acc:.4f}")

        if patience_counter >= 20:
            print(f"\nEarlyStopping at epoch {epoch+1}")
            break

    # 恢复最佳权重
    if best_state:
        model.load_state_dict(best_state)
        print(f"最佳 val_acc: {best_val_acc:.4f}")

    # ── 测试 ──
    test_loss, test_acc = validate(model, test_loader, criterion, device)
    print(f"\n测试集: accuracy={test_acc:.4f}, loss={test_loss:.4f}")

    # ── 保存 ──
    torch.save(model.state_dict(), os.path.join(outdir, 'cnn_model.pth'))
    torch.save(model, os.path.join(outdir, 'cnn_model_full.pth'))

    # 保存为标准 Scaler (供参考)
    scaler = StandardScaler().fit(X_train.reshape(len(X_train), -1))
    with open(os.path.join(outdir, 'scaler.pkl'), 'wb') as f:
        pickle.dump(scaler, f)

    print(f"\n产物: {outdir}")
    print(f"  cnn_model.pth / cnn_model_full.pth")
    print(f"  scaler.pkl")

    # ── 训练曲线 ──
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))
    ax1.plot(history['train_acc'], label='train')
    ax1.plot(history['val_acc'], label='val')
    ax1.set_title('Accuracy'); ax1.legend(); ax1.grid(True)
    ax2.plot(history['train_loss'], label='train')
    ax2.plot(history['val_loss'], label='val')
    ax2.set_title('Loss'); ax2.legend(); ax2.grid(True)
    fig.savefig(os.path.join(outdir, 'training_history.png'), dpi=100)
    print("  training_history.png")


if __name__ == '__main__':
    main()
