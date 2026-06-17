================================================================================
CLAUDE.md - 峨眉派工业物联网电机故障检测系统
================================================================================

【项目概述】

这是一个工业物联网电机设备故障检测系统，通过振动分析实现实时故障诊断。

- 采集端：ESP32-C3 + ADXL345（SPI）+ DHT11，负责数据采集和声光报警
- 边缘端：CV1812H 华山派开发板 + TPU，负责 AI 推理和本地显示
- 通信：Wi-Fi + MQTT
- 显示：4寸HDMI屏，LVGL + 轻量级Web服务双模式


【硬件引脚定义（ESP32-C3）】

外设            接口类型    引脚
ADXL345         I2C         SDA→GPIO6, SCL→GPIO10 (addr=0x53, CS/SDO悬空)
DHT11           单总线      GPIO18
LED_Green       GPIO        GPIO1
LED_Yellow      GPIO        GPIO3
LED_Red         GPIO        GPIO4
蜂鸣器          GPIO        GPIO0


【故障等级定义（不可变更）】

等级  状态    LED显示     蜂鸣器
0     正常    绿灯常亮     静音
1     警告    黄灯常亮     静音
2     危险    红灯常亮     间歇鸣叫
3     严重    红灯闪烁     持续鸣叫


【通信协议】

ESP32-C3 → 华山派（上行）：
{
  "device_id": "motor_01",
  "timestamp": 1700000000,
  "accel": {"x": 1.23, "y": -0.45, "z": 9.81},
  "temp": 25.5,
  "humi": 60.0
}

华山派 → ESP32-C3（下行）：
{"fault_level": 0}


【编码规范】

ESP32-C3（ESP-IDF）：
- 遵循 ESP-IDF 编码风格指南
- 使用 esp_log 进行日志输出，不使用 printf
- 传感器驱动放在 components/ 目录下
- 主程序逻辑放在 main/ 目录

华山派（Linux C/C++）：
- 遵循 Linux 内核代码风格（K&R 变体）
- 使用 CMake 构建
- TPU 推理代码单独模块化

通用：
- 所有头文件使用 #pragma once 而非 include guards
- 函数命名：snake_case
- 宏定义：UPPER_SNAKE_CASE
- 每行不超过 120 字符


【项目结构】

equipmenttest_project/
├── esp32-c3/                  # ESP32-C3 采集端代码
│   ├── main/                  # 主程序
│   ├── components/            # 自定义组件
│   │   ├── adxl345/           # ADXL345 SPI驱动
│   │   ├── dht11/             # DHT11驱动
│   │   ├── led_alarm/         # LED+蜂鸣器报警逻辑
│   │   └── mqtt_client/       # MQTT通信
│   └── CMakeLists.txt
├── huashan/                   # 华山派边缘端代码
│   ├── src/                   # 源代码
│   │   ├── inference/         # TPU推理模块
│   │   ├── display/           # LVGL显示
│   │   └── webserver/         # 轻量级Web服务
│   ├── models/                # cvimodel模型文件
│   └── CMakeLists.txt
├── models/                    # AI模型训练（PC端）
│   ├── training/              # 训练脚本
│   ├── cwru_data/             # 西储大学数据集
│   └── converted/             # ONNX导出文件
├── docs/                      # 文档
└── CLAUDE.md                  # 本文件


【Git 分支策略】

- main：稳定版本，始终可编译通过
- feature/*：新功能开发，完成后合并到 main
- 不使用 dev 长期分支，保持简单


【提交信息格式】

<type>: <简短描述>

<详细说明（可选）>

type：feat / fix / docs / style / refactor / test / chore

示例：
feat: 完成ADXL345 SPI驱动

支持三轴加速度读取，采样率可配置为200Hz


【当前开发阶段（复选框）】

[ ] ESP32-C3 传感器驱动（ADXL345 + DHT11）
[ ] ESP32-C3 LED + 蜂鸣器报警逻辑
[ ] ESP32-C3 WiFi + MQTT 通信
[ ] 华山派 TPU 模型加载和推理
[ ] 华山派 LVGL 显示界面
[ ] 华山派 轻量级Web服务
[ ] 端到端联调

================================================================================
文档结束
================================================================================