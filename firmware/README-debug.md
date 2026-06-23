# ESP32-C3 调试环境使用说明

## 前置条件
- ESP32-C3 已通过 USB 连接到 PC（内置 USB JTAG，无需外接调试器）
- OpenOCD 已安装（`tools/openocd-esp32/`）
- riscv32-esp-elf-gdb 可用（PlatformIO 或 ESP-IDF 工具链）

## 启动调试
1. 双击 `firmware/gdb_debug.bat`
2. OpenOCD 后台启动 → GDB 自动连接 → 停在 `app_main`

## 常用 GDB 命令
| 命令 | 作用 |
|------|------|
| `c` | 继续运行 |
| `n` | 单步跳过 |
| `s` | 单步进入 |
| `p 变量名` | 打印变量 |
| `b 文件名:行号` | 设置断点 |
| `bt` | 查看调用栈 |
| `info locals` | 查看局部变量 |
| `info registers` | 查看寄存器 |
| `delete 断点编号` | 删除断点 |
| `monitor reset halt` | 复位并暂停 |

## 手动启动
- 终端1：`openocd -f firmware/openocd.cfg`
- 终端2：`riscv32-esp-elf-gdb build/emei_fault_detection.elf`
- GDB：`target extended-remote :3333` → `monitor reset halt` → `thb app_main` → `c`
