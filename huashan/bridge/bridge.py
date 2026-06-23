#!/usr/bin/env python3
"""
华山派 MQTT 桥接程序 v3 (PC 端全智能)
=======================================
接收 ESP32 批量原始采样 → 滑窗 → 特征提取 → 加权评分 → 发布故障等级

用法: python bridge.py [--broker localhost] [--port 1883]
"""

import json
import math
import argparse
import socket
import time
from collections import deque
from threshold_config import *

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("pip install paho-mqtt")
    exit(1)

# ── 常量 ────────────────────────────────────────────

WINDOW_SIZE  = 256
LSB_TO_MS2   = 0.0039 * 9.81   # int16 → m/s²
PERSIST_N    = 3

# ── 滑动窗口 ─────────────────═══════════════════════

class SlidingWindow:
    """三轴滑动窗口, 自动维护 256 点"""

    def __init__(self, size=WINDOW_SIZE):
        self.size = size
        self.buf_x = deque(maxlen=size)
        self.buf_y = deque(maxlen=size)
        self.buf_z = deque(maxlen=size)

    def feed_batch(self, samples):
        """samples: list of (x, y, z) int16 raw values"""
        for x, y, z in samples:
            self.buf_x.append(x)
            self.buf_y.append(y)
            self.buf_z.append(z)

    def is_ready(self):
        return len(self.buf_x) >= self.size

    def get_window(self):
        """返回最新 256 点 (三轴 int16 raw)"""
        n = len(self.buf_x)
        start = n - self.size
        xs = list(self.buf_x)[start:]
        ys = list(self.buf_y)[start:]
        zs = list(self.buf_z)[start:]
        return xs, ys, zs


# ── 特征提取 ─────────────────═══════════════════════

def extract_features(xs, ys, zs):
    """
    输入: 三轴 int16 raw 值 (各 256 点)
    输出: 8 维特征 dict
    """
    n = len(xs)

    # 合成幅值
    mag = []
    for x, y, z in zip(xs, ys, zs):
        m = math.sqrt(x*x + y*y + z*z)
        mag.append(m)

    # ── AC 耦合: 去掉重力直流分量 ──
    mean      = sum(mag) / n
    ac        = [m - mean for m in mag]         # 零均值振动信号
    ac_abs    = [abs(a) for a in ac]

    rms     = math.sqrt(sum(a*a for a in ac) / n)       # AC RMS
    peak    = max(ac_abs)                                 # AC 峰值
    mean_abs = sum(ac_abs) / n                            # 平均整流值

    # 方差
    var = sum(a*a for a in ac) / n
    sigma = math.sqrt(var)

    # 偏度 & 峭度
    if sigma > 0.01:
        m3 = sum(a*a*a for a in ac) / n
        m4 = sum(a*a*a*a for a in ac) / n
        skew = m3 / (sigma ** 3)
        kurt = m4 / (sigma ** 4)
    else:
        skew = 0.0
        kurt = 1.0

    # 间隙因子 (AC)
    sum_sqrt_ac = sum(math.sqrt(a) for a in ac_abs)
    clearance = peak / ((sum_sqrt_ac / n) ** 2) if sum_sqrt_ac > 0 else 1.0

    # 波形 / 脉冲因子 (AC)
    shape   = rms / mean_abs if mean_abs > 0 else 1.0
    impulse = peak / mean_abs if mean_abs > 0 else 1.0

    # Crest Factor (AC)
    crest = peak / rms if rms > 0.1 else 1.0

    return {
        "rms":            rms * LSB_TO_MS2,
        "peak":           peak * LSB_TO_MS2,
        "crest_factor":   crest,
        "kurtosis":       kurt,
        "skewness":       skew,
        "clearance":      clearance,
        "shape_factor":   shape,
        "impulse_factor": impulse,
    }


# ── 加权评分 ─────────────────═══════════════════════

_history = deque(maxlen=PERSIST_N)

def detect_fault(features):
    score = 0.0
    wsum  = 0.0

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
        v = features.get(key, 0)
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
    _history.append(raw)
    if len(_history) < PERSIST_N:
        return raw
    if all(h == raw for h in _history):
        return raw
    # 返回最近稳定值
    return _history[-2] if len(_history) >= 2 else 0


# ── MQTT 回调 ─────────────────══════════════════════

window = SlidingWindow()
huashan_sock = None      # 到华山派的 TCP 连接

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("[MQTT] 已连接, 订阅 motor/telemetry")
        client.subscribe("motor/telemetry")
    else:
        print(f"[MQTT] 连接失败 rc={rc}")


def on_message(client, userdata, msg):
    global huashan_sock

    # ── 原始 JSON 转发给华山派 ──
    if huashan_sock:
        try:
            huashan_sock.sendall(msg.payload + b'\n')
        except (BrokenPipeError, ConnectionResetError, OSError):
            print("[TCP] 华山派断开")
            huashan_sock = None

    try:
        payload = json.loads(msg.payload)
    except json.JSONDecodeError:
        return

    samples = payload.get("samples", [])
    if not samples:
        return

    # 喂入窗口
    raw = [(s["x"], s["y"], s["z"]) for s in samples]
    window.feed_batch(raw)

    if not window.is_ready():
        return

    # 窗口就绪 → 特征提取
    xs, ys, zs = window.get_window()
    feat = extract_features(xs, ys, zs)
    level = detect_fault(feat)

    icon = ["[OK]", "[WARN]", "[DANGER]", "[CRIT]"][level]
    print(f"{icon} Lv.{level} | "
          f"RMS={feat['rms']:.2f} Peak={feat['peak']:.2f} "
          f"CF={feat['crest_factor']:.1f} Kurt={feat['kurtosis']:.1f} "
          f"Clear={feat['clearance']:.1f} Imp={feat['impulse_factor']:.1f}")

    cmd = json.dumps({"fault_level": level})
    client.publish("motor/command", cmd)


# ── 主程序 ─────────────────════════════════════════

def main():
    global huashan_sock

    parser = argparse.ArgumentParser(description="华山派 MQTT 桥接 v3")
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=4883)
    parser.add_argument("--huashan", default="192.168.150.2:9999",
                        help="华山派地址 (host:port), 默认 192.168.150.2:9999")
    args = parser.parse_args()

    # 解析华山派地址
    hs_host, _, hs_port_str = args.huashan.partition(":")
    hs_port = int(hs_port_str) if hs_port_str else 9999

    print("=" * 50)
    print("华山派 MQTT 桥接 v3 (PC 端全智能)")
    print(f"MQTT Broker: {args.broker}:{args.port}")
    print(f"窗口: {WINDOW_SIZE} 点, 50Hz 下约 5.1s 填满")
    print("=" * 50)

    # ── 连接华山派 TCP ──
    try:
        huashan_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        huashan_sock.settimeout(5)
        huashan_sock.connect((hs_host, hs_port))
        huashan_sock.settimeout(None)
        print(f"[TCP] 已连接华山派 {hs_host}:{hs_port}")
    except OSError as e:
        print(f"[TCP] 华山派未就绪 ({e}), 继续运行(不转发)")
        huashan_sock = None

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1,
                         client_id="huashan_bridge")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(args.broker, args.port, 60)
    client.loop_forever()


if __name__ == "__main__":
    main()
