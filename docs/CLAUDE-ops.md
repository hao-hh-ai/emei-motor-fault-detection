# 操作速查手册 — 峨眉派电机故障检测系统

> 按需 Read，不自动加载。查完即走，省 Token。

---

## 规范索引

| 场景 | 读取命令 |
|------|---------|
| 写测试用例 | `Read("docs/development-standards.md", offset=测试规范章节)` |
| 处理密码/Token/敏感数据 | `Read("docs/development-standards.md", offset=安全红线章节)` |
| Git 提交 | `Read("docs/development-standards.md", offset=Git提交规范章节)` |
| 开始改代码 | `Read("docs/development-standards.md", offset=任务执行规范章节)` |
| 不确定改动范围 | `Read("docs/development-standards.md", offset=文件修改规范章节)` |
| **修复Bug后记录** | `Read("docs/development-standards.md", offset=Bug记录规范章节)` |
| **搭建CI/CD质量门禁** | `Read("docs/development-standards.md", offset=自动化质量门禁章节)` |
| CI 红灯排查 | `Read("docs/development-standards.md", offset=自动化质量门禁章节)` |
| **GDB 调试 / 断点 / 查看变量** | `Read("docs/debug-gdb-tutorial.md")` |

---

## 任务执行铁律

1. **查规格** → `docs/features/` 下找对应 `FEAT-*.md`。
2. **验收驱动** → 编码后必须逐条打勾 AC。
3. **遇阻即停** → 标准不满足或计划遇阻，立即提问。
4. **地基禁碰** → 涉及 `rules.md` 第 8 条的改动，先停摆填评估报告。
5. **角色声明** → 每阶段开头声明角色 (Dev/Test/Review)。

---

## 命令速查

| 操作 | 命令 |
|------|------|
| **ESP32 编译** | VS Code: `Build` (Ctrl+E B) 或 `idf.py build` |
| **ESP32 烧录** | VS Code: `Flash` (UART, COM11) 或 `idf.py -p COM11 flash` |
| **ESP32 监视** | VS Code: `Monitor` 或 `idf.py -p COM11 monitor` |
| **ESP32 编译+烧录+监视** | `idf.py -p COM11 build flash monitor` |
| **ESP32 清理** | `idf.py fullclean` |
| **ESP32 menuconfig** | `idf.py menuconfig` |
| **ESP32 调试 (一键)** | 双击 `firmware/gdb_debug.bat` |
| **ESP32 调试 (手动)** | `openocd -f firmware/openocd.cfg` → `riscv32-esp-elf-gdb build/emei_fault_detection.elf` |
| **华山派 C 编译** | 虚拟机: `cd ~/桌面/huashan && CROSS_COMPILE=riscv64-unknown-linux-musl- make` |
| **华山派 部署** | `scp huashan_bridge root@192.168.150.2:/mnt/data/` |
| **华山派 运行** | `ssh root@192.168.150.2 "./mnt/data/huashan_bridge 9999"` |
| **PC Bridge 启动** | `py huashan/bridge/bridge.py --huashan 192.168.150.2:9999` |
| **MQTT Broker 启动** | `D:\mosquitto\mosquitto.exe -v -c mosquitto.conf` |
| **MQTT 测试发布** | `D:\mosquitto\mosquitto_pub.exe -h localhost -p 4883 -t motor/command -m "{\"fault_level\":2}"` |
