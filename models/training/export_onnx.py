#!/usr/bin/env python3
"""
模型导出 — PyTorch → ONNX
===========================
将训练好的 1D-CNN 导出为 ONNX 格式 (opset 13),
供后续 TPU-MLIR 工具链转换为 cvimodel。

用法: py -3.13 export_onnx.py [--model ../artifacts/cnn_model.pth]
"""

import os
import sys
import argparse
import time
import numpy as np

import torch
import onnx
import onnxruntime as ort

sys.path.insert(0, os.path.dirname(__file__))
from train_1dcnn import FaultClassifier1D


def export_to_onnx(model_path, output_path, opset=13):
    """
    将 PyTorch 模型转换为 ONNX 格式。

    参数:
        model_path:  输入 .pth 模型路径 (state_dict)
        output_path: 输出 .onnx 路径
        opset:       ONNX opset 版本 (13 兼容 TPU-MLIR)
    """
    device = torch.device('cpu')

    print(f"加载模型: {model_path}")
    model = FaultClassifier1D(input_channels=1, num_classes=4)
    model.load_state_dict(torch.load(model_path, map_location=device, weights_only=True))
    model.eval()
    print(f"参数量: {sum(p.numel() for p in model.parameters()):,}")

    # 创建 dummy 输入
    dummy_input = torch.randn(1, 1, 256)  # (batch, channels, seq_len)

    print(f"转换为 ONNX (opset {opset})...")
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=opset,
        input_names=['vibration_input'],
        output_names=['fault_level'],
        dynamic_axes={
            'vibration_input': {0: 'batch'},
            'fault_level': {0: 'batch'},
        },
    )

    # 验证
    print("验证 ONNX 模型...")
    onnx_model = onnx.load(output_path)
    onnx.checker.check_model(onnx_model)
    print("  ✓ onnx.checker 通过")

    print(f"\nONNX 模型信息:")
    for inp in onnx_model.graph.input:
        shape = [d.dim_value for d in inp.type.tensor_type.shape.dim]
        print(f"  输入: {inp.name}  shape={shape}")
    for out in onnx_model.graph.output:
        shape = [d.dim_value for d in out.type.tensor_type.shape.dim]
        print(f"  输出: {out.name}  shape={shape}")

    # 推理等价性
    print("\n推理等价性测试...")
    with torch.no_grad():
        torch_out = model(dummy_input).numpy()

    session = ort.InferenceSession(output_path)
    onnx_out = session.run(None, {'vibration_input': dummy_input.numpy()})[0]

    max_diff = np.max(np.abs(torch_out - onnx_out))
    print(f"  最大差异: {max_diff:.2e}")
    if max_diff < 1e-5:
        print("  ✓ PyTorch ↔ ONNX 推理一致")
    else:
        print("  ⚠ 差异较大, 请检查!")

    # 性能
    times = []
    for _ in range(100):
        t0 = time.perf_counter()
        session.run(None, {'vibration_input': dummy_input.numpy()})
        times.append(time.perf_counter() - t0)
    avg_time = np.mean(times) * 1000
    print(f"  推理延迟: {avg_time:.3f} ms/样本 (ONNX Runtime CPU)")

    print(f"\n导出完成: {output_path}")
    print(f"文件大小: {os.path.getsize(output_path) / 1024:.1f} KB")


def main():
    parser = argparse.ArgumentParser(description="PyTorch → ONNX 导出")
    parser.add_argument("--model", default="../artifacts/cnn_model.pth")
    parser.add_argument("--output", default="../artifacts/cnn_model.onnx")
    parser.add_argument("--opset", type=int, default=13)
    args = parser.parse_args()

    model_path = os.path.join(os.path.dirname(__file__), args.model)
    output_path = os.path.join(os.path.dirname(__file__), args.output)

    if not os.path.exists(model_path):
        print(f"[ERROR] 模型文件不存在: {model_path}")
        print("请先运行 train_1dcnn.py")
        return

    export_to_onnx(model_path, output_path, args.opset)


if __name__ == '__main__':
    main()
