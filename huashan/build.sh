#!/bin/bash
# 华山派桥接程序 — 一键交叉编译
# 在虚拟机中设置 CROSS_COMPILE 路径后运行
#
# 用法:
#   export CROSS_COMPILE=/opt/riscv/bin/riscv64-unknown-linux-musl-
#   ./build.sh
#
# 或直接传参:
#   ./build.sh /opt/riscv/bin/riscv64-unknown-linux-musl-

set -e

CROSS="${1:-$CROSS_COMPILE}"
if [ -z "$CROSS" ]; then
    echo "用法: $0 <交叉编译器前缀>"
    echo "示例: $0 /opt/riscv/bin/riscv64-unknown-linux-musl-"
    echo "或设置环境变量: export CROSS_COMPILE=..."
    exit 1
fi

echo "=== 编译华山派 MQTT 桥接程序 ==="
echo "编译器: ${CROSS}gcc"
echo ""

make clean 2>/dev/null || true
make CROSS_COMPILE="$CROSS" -j4

echo ""
echo "=== 编译完成 ==="
file huashan_bridge
echo ""
echo "上传到华山派:"
echo "  scp huashan_bridge root@192.168.150.2:/mnt/data/"
echo ""
echo "在华山派上运行:"
echo "  ssh root@192.168.150.2"
echo "  cd /mnt/data && chmod +x huashan_bridge && ./huashan_bridge 10.38.177.114 4883"
