#!/bin/bash
# 本地质量门禁 — 提交前运行
# 用法: bash .claude/scripts/check-firmware.sh
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "========================================"
echo "  项目质量门禁 (5 道)"
echo "========================================"
PASS=0
FAIL=0

# ── 门禁1: 静态检查 ──
echo ""
echo "[1/5] 静态检查 — 引脚/MQTT/故障等级一致性"
# 检查关键定义是否存在
check_define() {
    if grep -q "$2" "$1" 2>/dev/null; then
        echo "  OK: $1 → $2"
        PASS=$((PASS + 1))
    else
        echo "  WARN: $1 中未找到 $2"
        FAIL=$((FAIL + 1))
    fi
}
check_define "main/main.c"   "ADXL345"
check_define "main/main.c"   "DHT11"
check_define "main/main.c"   "mqtt_client"
check_define "components/led_alarm/led_alarm.c" "fault_level"
check_define "components/mqtt_client/mqtt_client.c" "motor/telemetry"
check_define "components/mqtt_client/mqtt_client.c" "motor/command"
check_define "components/adxl345/adxl345.h" "ADXL345"

# ── 门禁2: 密钥扫描 ──
echo ""
echo "[2/5] 密钥扫描 — 硬编码凭据"
SECRETS=0
for pat in \
    'password\s*=\s*"[^"]\{3,\}"' \
    'passwd\s*=\s*"[^"]\{3,\}"' \
    'secret\s*=\s*"[^"]\{3,\}"' \
    'api_key\s*=\s*"[^"]\{3,\}"' \
    'token\s*=\s*"[^"]\{3,\}"'; do
    FOUND=$(grep -rn "$pat" main/ components/ huashan/src/ huashan/bridge/ 2>/dev/null || true)
    if [ -n "$FOUND" ]; then
        echo "  FAIL: $FOUND"
        SECRETS=$((SECRETS + 1))
    fi
done
if [ "$SECRETS" -eq 0 ]; then
    echo "  PASS: 无硬编码密钥"
    PASS=$((PASS + 1))
else
    FAIL=$((FAIL + 1))
fi

# ── 门禁3: 危险函数 ──
echo ""
echo "[3/5] 危险函数扫描"
DANG=0
for pat in 'sprintf\s*\(' 'strcpy\s*\(' 'gets\s*\(' 'scanf\s*\('; do
    FOUND=$(grep -rn "$pat" main/ components/ huashan/src/ --include="*.c" --include="*.h" 2>/dev/null || true)
    if [ -n "$FOUND" ]; then
        echo "  WARN: $FOUND"
        DANG=$((DANG + 1))
    fi
done
if [ "$DANG" -eq 0 ]; then
    echo "  PASS: 无危险函数调用"
    PASS=$((PASS + 1))
else
    echo "  INFO: 发现 $DANG 处不安全调用（建议替换为安全版本）"
    PASS=$((PASS + 1))
fi

# ── 门禁4: 文件行数 ──
echo ""
echo "[4/5] 文件行数检查 (≤800行)"
MAX_LINES=800
BIG=0
for f in $(find main/ components/ huashan/src/ -name "*.c" -o -name "*.h" 2>/dev/null); do
    LINES=$(wc -l < "$f")
    if [ "$LINES" -gt "$MAX_LINES" ]; then
        echo "  WARN: $f → $LINES 行"
        BIG=$((BIG + 1))
    fi
done
if [ "$BIG" -eq 0 ]; then
    echo "  PASS"
    PASS=$((PASS + 1))
else
    FAIL=$((FAIL + 1))
fi

# ── 门禁5: 编译 ──
echo ""
echo "[5/5] ESP-IDF 编译"
if command -v idf.py &> /dev/null; then
    if idf.py build 2>&1 | tail -5; then
        echo "  PASS: 编译通过"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: 编译失败"
        FAIL=$((FAIL + 1))
    fi
else
    echo "  SKIP: idf.py 不可用（非 ESP-IDF 环境）"
fi

# ── 总结 ──
echo ""
echo "========================================"
echo "  结果: $PASS 通过, $FAIL 失败"
echo "========================================"
if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
