# 项目地图 — 峨眉派工业物联网电机故障检测系统

> 始终加载。只写"是什么"，不写"怎么做"。保持 < 500 Token。
> 操作速查 → `Read("docs/CLAUDE-ops.md")`
> 完整工作流 & 新项目初始化 → `Read("docs/111.md")`

---

## 技术栈

ESP32-C3 (RISC-V) / ESP-IDF v5.4.4 / FreeRTOS / ADXL345 I2C / DHT11 / MQTT (WiFi STA)
PC Bridge: Python 3.11 / paho-mqtt / numpy
华山派边缘端: CV1812H (RISC-V 64) / musl libc / C11 / Sophon TPU

---

## 目录速查

| 目录 | 职责 |
|------|------|
| `main/` | ESP32 主程序 |
| `components/adxl345/` | ADXL345 I2C 加速度计驱动 |
| `components/dht11/` | DHT11 温湿度传感器驱动 |
| `components/led_alarm/` | LED + 蜂鸣器 报警逻辑 |
| `components/mqtt_client/` | MQTT 通信 |
| `components/wifi_sta/` | WiFi STA |
| `components/feature_extract/` | 时域特征提取 (备用) |
| `huashan/src/` | 华山派 C 代码 (滑窗+特征+TCP服务) |
| `huashan/bridge/` | PC 桥接中枢 |
| `models/` | AI 模型训练 |
| `docs/` | 设计文档、规范 |
| `firmware/` | 调试模板 (OpenOCD + GDB) |
| `tools/` | 调试工具 (OpenOCD) |
| `.github/workflows/` | CI/CD 质量门禁 |
| `.claude/scripts/` | 静态检查脚本 |

---

## 入口 & 关键文件

| 文件 | 职责 |
|------|------|
| `main/main.c` | ESP32 固件入口 |
| `components/mqtt_client/mqtt_client.c` | MQTT 通信实现 |
| `components/adxl345/adxl345.c` | ADXL345 I2C 驱动 |
| `huashan/src/main.c` | 华山派 TCP 数据处理服务 |
| `huashan/bridge/bridge.py` | PC 桥接中枢 |
| `huashan/src/threshold_config.h` | 时域特征阈值表 |
| `sdkconfig` | ESP-IDF 项目配置 |
| `.github/workflows/quality-gate.yml` | CI 质量门禁 |
| `.claude/scripts/check-firmware.sh` | 静态回归检查脚本 |

---

## 约束 & 禁止事项

- 禁止修改硬件引脚定义（SDA→GPIO6, SCL→GPIO10, DHT11→GPIO18, LED→1/3/4, Buzzer→0）
- 禁止修改故障等级定义（0正常/1警告/2危险/3严重）
- MQTT 主题固定：上行 `motor/telemetry`，下行 `motor/command`
- 编译：ESP-IDF VS Code `Build → Flash`（UART, COM11），或 `idf.py build flash`
- **CI 红灯禁止合并**
- ESP32-C3 单核 160MHz，避免 busy-wait，使用 vTaskDelay 让出 CPU
- 华山派部署到 `/mnt/data/`，交叉编译器用虚拟机 musl 工具链

---

## 外部依赖

- MQTT Broker: Mosquitto 端口 4883, 匿名访问
- WiFi 热点: `www` / `88888888`
- 华山派: `192.168.150.2` root/cvitek
- 交叉编译 VM: `192.168.88.128` xiaohuihui
- 交叉编译器: `riscv64-unknown-linux-musl-gcc`

---

## 调试命令

- **一键调试**：双击 `firmware/gdb_debug.bat`（OpenOCD + GDB，停在 `app_main`）
- **手动**：`openocd -f firmware/openocd.cfg` → `riscv32-esp-elf-gdb build/emei_fault_detection.elf`
- **GDB 教程**：`Read("docs/debug-gdb-tutorial.md")`
