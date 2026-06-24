#!/usr/bin/env python3
"""8 特征 MLP 分类器 (替代 1D-CNN, 独立于采样率/振幅)"""
import os, pickle, argparse, numpy as np
import torch, torch.nn as nn, torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.utils.class_weight import compute_class_weight
from sklearn.metrics import classification_report

FEATURE_NAMES = ['rms','peak','crest_factor','kurtosis','skewness','clearance','shape_factor','impulse_factor']

class FaultMLP(nn.Module):
    def __init__(self, in_dim=8, num_classes=4):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(in_dim, 32), nn.ReLU(), nn.Dropout(0.1),
            nn.Linear(32, 16), nn.ReLU(),
            nn.Linear(16, num_classes),
        )
    def forward(self, x): return self.net(x)

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--epochs', type=int, default=80)
    p.add_argument('--batch', type=int, default=16)
    p.add_argument('--outdir', default='../artifacts/')
    args = p.parse_args()

    outdir = os.path.join(os.path.dirname(__file__), args.outdir)
    os.makedirs(outdir, exist_ok=True)

    data = np.load('features_dataset.npz')
    X, y = data['X'].astype(np.float32), data['y'].astype(np.int64)

    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.15, stratify=y, random_state=42)
    X_tr, X_va, y_tr, y_va = train_test_split(X_tr, y_tr, test_size=0.15/0.85, stratify=y_tr, random_state=42)
    print(f'train={len(X_tr)} val={len(X_va)} test={len(X_te)}')

    scaler = StandardScaler().fit(X_tr)
    X_tr, X_va, X_te = scaler.transform(X_tr), scaler.transform(X_va), scaler.transform(X_te)
    with open(os.path.join(outdir, 'feat_scaler.pkl'), 'wb') as f: pickle.dump(scaler, f)
    means, scales = scaler.mean_.astype(np.float32), scaler.scale_.astype(np.float32)
    np.savez(os.path.join(outdir, 'feat_scaler.npz'), mean=means, scale=scales)
    print(f'Feature means: {dict(zip(FEATURE_NAMES, means.round(4)))}')
    print(f'Feature scales: {dict(zip(FEATURE_NAMES, scales.round(4)))}')

    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    cw = compute_class_weight('balanced', classes=np.unique(y_tr), y=y_tr)
    cw_t = torch.tensor(cw, dtype=torch.float32).to(device)

    tr_ld = DataLoader(TensorDataset(torch.tensor(X_tr), torch.tensor(y_tr)), args.batch, True)
    va_ld = DataLoader(TensorDataset(torch.tensor(X_va), torch.tensor(y_va)), args.batch)

    model = FaultMLP().to(device)
    print(f'Params: {sum(p.numel() for p in model.parameters()):,}')
    crit = nn.CrossEntropyLoss(weight=cw_t)
    opt = optim.Adam(model.parameters(), lr=0.001)
    sch = optim.lr_scheduler.ReduceLROnPlateau(opt, patience=15, factor=0.5)

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
    print(classification_report(y_te, te_pred, target_names=['OK','WARN','DANGER','CRIT']))

    torch.save(model.state_dict(), os.path.join(outdir, 'mlp_model.pth'))
    torch.save(model, os.path.join(outdir, 'mlp_model_full.pth'))
    print(f'Saved: mlp_model.pth ({sum(p.numel() for p in model.parameters()):,} params)')

if __name__ == '__main__': __import__('sys').exit(main())
