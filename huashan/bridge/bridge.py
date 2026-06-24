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
import threading
import random
from collections import deque
from queue import Queue
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
_tcp_queue = Queue(maxsize=500)   # TCP 发送队列 (非阻塞)
_hs_addr = None                    # 华山派地址 (host, port)


def tcp_sender():
    """后台线程: 从队列取数据发送到华山派, 自动重连, 绝不阻塞 MQTT 线程"""
    sock = None
    while True:
        data = _tcp_queue.get()    # 阻塞等待数据
        if data is None:           # 关闭信号
            if sock:
                sock.close()
            break

        # 确保连接
        if sock is None:
            try:
                sock = socket.create_connection(_hs_addr, timeout=3)
                sock.settimeout(2)  # send 最多阻塞 2 秒
                print(f"[TCP] 已连接华山派 {_hs_addr[0]}:{_hs_addr[1]}")
            except OSError:
                sock = None
                continue           # 丢弃本条, 下一条重试连接

        # 发送
        try:
            sock.sendall(data)
        except OSError:
            sock.close()
            sock = None
            # 放回队列头部, 重连后重发 (最多积压 1 条)
            if _tcp_queue.qsize() < _tcp_queue.maxsize - 1:
                _tcp_queue.put(data)

def on_connect(client, userdata, flags, reason_code, properties):
    if not reason_code.is_failure:
        print("[MQTT] 已连接, 订阅 motor/telemetry")
        client.subscribe("motor/telemetry")
    else:
        print(f"[MQTT] 连接失败 reason={reason_code}")

def on_disconnect(client, userdata, flags, reason_code, properties):
    if reason_code != 0:
        print(f"[MQTT] 断开 reason={reason_code}, 自动重连中...")


def on_message(client, userdata, msg):
    # ── 原始 JSON 转发给华山派 (非阻塞, 交后台线程发送) ──
    if _hs_addr:
        try:
            _tcp_queue.put_nowait(msg.payload + b'\n')
        except Exception:
            pass  # 队列满则丢弃, 绝不阻塞 MQTT 线程

    try:
        payload = json.loads(msg.payload)
    except json.JSONDecodeError:
        return

    samples = payload.get("samples", [])
    if not samples:
        return

    # 逐点喂入窗口, 每 INFER_STRIDE 个样本推理一次 (降低延迟)
    INFER_STRIDE = 8
    raw = [(s["x"], s["y"], s["z"]) for s in samples]
    for x, y, z in raw:
        window.buf_x.append(x)
        window.buf_y.append(y)
        window.buf_z.append(z)

        # 窗口未满 或 不在推理步长上 → 跳过
        if not window.is_ready():
            continue
        if (len(window.buf_x) % INFER_STRIDE) != 0:
            continue

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
    global _hs_addr

    parser = argparse.ArgumentParser(description="华山派 MQTT 桥接 v3")
    parser.add_argument("--broker", default="localhost")
    parser.add_argument("--port", type=int, default=4883)
    parser.add_argument("--huashan", default="192.168.150.2:9999",
                        help="华山派地址 (host:port), 默认 192.168.150.2:9999")
    args = parser.parse_args()

    # 解析华山派地址
    hs_host, _, hs_port_str = args.huashan.partition(":")
    hs_port = int(hs_port_str) if hs_port_str else 9999
    _hs_addr = (hs_host, hs_port)

    print("=" * 50)
    print("华山派 MQTT 桥接 v4 (PC 端全智能)")
    print(f"MQTT Broker: {args.broker}:{args.port}")
    print(f"TCP 转发: {hs_host}:{hs_port}")
    print(f"窗口: {WINDOW_SIZE} 点, 50Hz 下约 5.1s 填满")
    print("=" * 50)

    # ── 启动 TCP 发送线程 (daemon: 主线程退出时自动结束) ──
    sender = threading.Thread(target=tcp_sender, daemon=True, name="tcp-sender")
    sender.start()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f"huashan_bridge_{random.randint(1000,9999)}")
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.connect(args.broker, args.port, 60)
    client.loop_start()  # 后台线程跑 MQTT 网络 I/O, 不阻塞

    try:
        while True:
            time.sleep(1)  # 主线程保持存活
    except KeyboardInterrupt:
        print("\n[系统] 退出")
        _tcp_queue.put(None)  # 通知 TCP 线程关闭


if __name__ == "__main__":
    main()
