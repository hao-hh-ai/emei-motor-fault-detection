#!/usr/bin/env python3
"""用部署实测数据训练 MLP (规则系统做 Teacher)
================================================
数据来源: bridge.py 运行时自动保存的 training_data.csv
用法: py -3.13 train_deploy_mlp.py [--csv path/to/training_data.csv]
"""

import os, pickle, argparse, numpy as np
import torch, torch.nn as nn, torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import classification_report, accuracy_score

FEATURE_NAMES = ['rms','peak','crest_factor','kurtosis',
                 'skewness','clearance','shape_factor','impulse_factor']

class TinyMLP(nn.Module):
    """微型 MLP: 8→16→8→4, <500 params"""
    def __init__(self):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(8, 16), nn.ReLU(),
            nn.Linear(16, 8), nn.ReLU(),
            nn.Linear(8, 4),
        )
    def forward(self, x): return self.net(x)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--csv', default='../../huashan/bridge/training_data.csv')
    p.add_argument('--epochs', type=int, default=100)
    p.add_argument('--batch', type=int, default=16)
    p.add_argument('--outdir', default='../artifacts/')
    args = p.parse_args()

    outdir = os.path.join(os.path.dirname(__file__), args.outdir)
    os.makedirs(outdir, exist_ok=True)

    # Load data
    csv_path = os.path.join(os.path.dirname(__file__), args.csv)
    if not os.path.exists(csv_path):
        print(f"[ERROR] {csv_path} 不存在!")
        print("请先运行 bridge.py 收集训练数据: py -u huashan/bridge/bridge.py")
        return

    import pandas as pd
    df = pd.read_csv(csv_path)
    print(f"加载: {len(df)} 条样本")
    for lv in range(4):
        cnt = (df['fault_level'] == lv).sum()
        print(f"  Lv.{lv}: {cnt}")

    X = df[FEATURE_NAMES].values.astype(np.float32)
    y = df['fault_level'].values.astype(np.int64)

    # Drop duplicates to avoid overfitting on identical windows
    X, idx = np.unique(X, axis=0, return_index=True)
    y = y[idx]
    print(f"去重后: {len(X)} 条")

    if len(X) < 100:
        print(f"[WARN] 数据太少 ({len(X)}条), 建议继续采集")
        return

    # Split
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.15, stratify=y, random_state=42)
    X_tr, X_va, y_tr, y_va = train_test_split(X_tr, y_tr, test_size=0.15/0.85, stratify=y_tr, random_state=42)
    print(f'train={len(X_tr)} val={len(X_va)} test={len(X_te)}')

    # Scale
    scaler = StandardScaler().fit(X_tr)
    X_tr, X_va, X_te = scaler.transform(X_tr), scaler.transform(X_va), scaler.transform(X_te)
    with open(os.path.join(outdir, 'deploy_scaler.pkl'), 'wb') as f:
        pickle.dump(scaler, f)
    np.savez(os.path.join(outdir, 'deploy_scaler.npz'),
             mean=scaler.mean_.astype(np.float32), scale=scaler.scale_.astype(np.float32))

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

    # Handle class imbalance
    from sklearn.utils.class_weight import compute_class_weight
    cw = compute_class_weight('balanced', classes=np.unique(y_tr), y=y_tr)

    tr_ld = DataLoader(TensorDataset(torch.tensor(X_tr), torch.tensor(y_tr)), args.batch, True)
    va_ld = DataLoader(TensorDataset(torch.tensor(X_va), torch.tensor(y_va)), args.batch)

    model = TinyMLP().to(device)
    print(f'Params: {sum(p.numel() for p in model.parameters()):,}')

    crit = nn.CrossEntropyLoss(weight=torch.tensor(cw, dtype=torch.float32).to(device))
    opt = optim.Adam(model.parameters(), lr=0.001)
    sch = optim.lr_scheduler.ReduceLROnPlateau(opt, patience=20, factor=0.5, min_lr=1e-5)

    best_va, best_st = 0, None
    for ep in range(args.epochs):
        model.train()
        for xb, yb in tr_ld:
            opt.zero_grad(); crit(model(xb.to(device)), yb.to(device)).backward(); opt.step()
        model.eval()
        with torch.no_grad():
            va_acc = sum((model(xb.to(device)).argmax(1) == yb.to(device)).sum().item() for xb, yb in va_ld) / len(y_va)
        sch.step(1 - va_acc)
        if va_acc > best_va: best_va, best_st = va_acc, {k: v.cpu().clone() for k, v in model.state_dict().items()}
        if (ep+1) % 20 == 0: print(f'  epoch {ep+1}: va_acc={va_acc:.4f}')

    model.load_state_dict(best_st)
    with torch.no_grad():
        te_out = model(torch.tensor(X_te).to(device))
        te_acc = (te_out.argmax(1).cpu() == torch.tensor(y_te)).float().mean().item()
    te_pred = te_out.argmax(1).cpu().numpy()
    print(f'\ntest_acc={te_acc:.4f}')
    print(classification_report(y_te, te_pred, target_names=['OK','WARN','DANGER','CRIT'], zero_division=0))

    torch.save(model.state_dict(), os.path.join(outdir, 'deploy_mlp.pth'))
    torch.save(model, os.path.join(outdir, 'deploy_mlp_full.pth'))
    print(f'Saved: deploy_mlp.pth ({sum(p.numel() for p in model.parameters()):,} params)')
    print(f'Scaler: deploy_scaler.pkl')

if __name__ == '__main__': __import__('sys').exit(main())
