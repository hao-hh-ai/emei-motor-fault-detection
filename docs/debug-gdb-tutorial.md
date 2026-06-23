# STM32 GDB 调试完整教程（OV-Watch / pointer-desk 实战版）

> 本教程基于你已搭建的 **OpenOCD + GDB** 环境，适用于任何 STM32 项目。写进模板，新项目开箱即用。


## 一、为什么需要这套调试工具？

| 调试方式 | 优点 | 缺点 |
|----------|------|------|
| `printf` 串口打印 | 简单 | 需要改代码、重新烧录、看不见运行中状态 |
| 逻辑分析仪 | 精准 | 贵、复杂、不适合调试代码逻辑 |
| **GDB + OpenOCD** | 不修改代码、可设断点、查看变量、单步执行 | 需要学习几个命令 |

**一句话：GDB 让你"看到"芯片内部正在发生什么。**


## 二、原理（一句话版）

```
GDB (调试命令) → TCP/IP (端口3333) → OpenOCD (翻译官) → ST-Link (硬件) → STM32芯片
```

- **终端1**：OpenOCD（翻译官）持续运行，监听端口 3333
- **终端2**：GDB（指挥官）连接端口 3333，发送调试命令


## 三、前置条件（一次性准备）

### 3.1 工具已安装

| 工具 | 路径 | 验证命令 |
|------|------|----------|
| OpenOCD | `C:\OpenOCD\xpack-openocd-0.12.0-7\bin\` | `openocd --version` |
| arm-none-eabi-gdb | `C:\ST\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\` | `arm-none-eabi-gdb --version` |

### 3.2 项目已有配置文件

在 `firmware/` 目录下必须有这两个文件：

**`openocd.cfg`**（OpenOCD 配置文件）：
```tcl
source [find interface/stlink.cfg]
transport select swd
source [find target/stm32f4x.cfg]
adapter speed 4000
```

**`.gdbinit`**（GDB 自动初始化脚本，可选）：
```gdb
target remote :3333
monitor reset halt
load
break main
echo === GDB ready. Type 'continue' to run. ===\n
```


## 四、标准调试流程（每次调试照做）

### 第 1 步：编译固件（带调试信息）

在 `firmware/` 目录下执行：
```bash
mingw32-make -j6
```

确保 `Makefile` 中编译选项包含 `-g`（你的已经包含）。

---

### 第 2 步：启动 OpenOCD（终端 1）

打开第一个终端（CMD 或 PowerShell），进入项目目录：
```bash
cd C:\Projects\pointer-desk\firmware
openocd -f openocd.cfg
```

**预期输出**：
```
Info : ST-LINK V2J37S7 (firmware version)
Info : SWD frequency 4000 kHz
Info : stm32f4x.cpu: hardware has 6 breakpoints, 4 watchpoints
Info : Listening on port 3333 for gdb connections
```

**看到 `Listening on port 3333 for gdb connections` 表示成功。**  
**⚠️ 保持这个终端窗口开着，不要关闭。**

---

### 第 3 步：启动 GDB（终端 2）

打开第二个终端，进入项目目录：
```bash
cd C:\Projects\pointer-desk\firmware
arm-none-eabi-gdb build/ov-watch.elf
```

此时会进入 `(gdb)` 提示符。

---

### 第 4 步：连接并烧录（在 GDB 提示符下依次输入）

```gdb
target remote :3333
```

连接成功会显示：
```
Remote debugging using :3333
0x08006d10 in prvIdleTask (...)
```

然后：
```gdb
monitor reset halt
```

复位芯片并暂停，输出：
```
[stm32f4x.cpu] halted due to debug-request
```

然后烧录固件：
```gdb
load
```

输出示例：
```
Loading section .isr_vector, size 0x188 lma 0x8000000
Loading section .text, size 0x13728 lma 0x8000190
Start address 0x0800db00, load size 87612
```

---

### 第 5 步：设置断点并运行

在 `main` 函数入口设断点：
```gdb
break main
```

开始运行：
```gdb
continue
```

程序会运行到 `main` 函数入口并暂停：
```
Breakpoint 1, main () at Core/Src/main.c:169
169     {
```

**现在你已经在调试模式了！**


## 五、调试中的常用命令

### 5.1 运行与控制

| 命令 | 简写 | 作用 |
|------|------|------|
| `continue` | `c` | 继续运行到下一个断点 |
| `next` | `n` | 单步执行（不进入函数） |
| `step` | `s` | 单步执行（进入函数） |
| `finish` | `fin` | 执行完当前函数并返回 |

### 5.2 查看状态

| 命令 | 简写 | 作用 |
|------|------|------|
| `print 变量名` | `p 变量名` | 打印变量值 |
| `print 变量名=新值` | `p 变量名=新值` | 修改变量值（调试神器！） |
| `backtrace` | `bt` | 查看函数调用栈 |
| `info locals` | `i lo` | 查看当前函数所有局部变量 |
| `info registers` | `i r` | 查看所有寄存器状态 |

### 5.3 断点管理

| 命令 | 简写 | 作用 |
|------|------|------|
| `break 函数名` | `b 函数名` | 在函数入口设断点 |
| `break 文件名:行号` | `b 文件:行号` | 在指定行设断点 |
| `break 行号 if 条件` | `b 行号 if 条件` | 条件断点（如 `b 100 if i==5`） |
| `info breakpoints` | `i b` | 查看所有断点 |
| `delete 编号` | `d 编号` | 删除指定编号的断点 |

### 5.4 其他实用命令

| 命令 | 作用 |
|------|------|
| `watch 变量名` | 监视变量，被修改时自动暂停 |
| `list` | 显示当前执行的源码 |
| `whatis 变量名` | 显示变量类型 |
| `set 变量=值` | 运行时修改变量值（注：`print 变量=值` 也能做到） |


## 六、实战示例：调试天气数据问题

假设你想验证 `g_weather` 数据是否被正确解析：

```gdb
# 1. 在解析函数设断点
b weather_bridge.c:138

# 2. 继续运行
c

# 3. 当断点触发时，打印结构体
p g_weather

# 输出示例：
# $1 = {temperature = 30, humidity = 31 '\037', description = "Clear", valid = true}

# 4. 查看具体字段
p g_weather.temperature
# $2 = 30

# 5. 继续运行
c
```


## 七、一键调试脚本（省去手动输入）

创建 `firmware/gdb_debug.bat`：

```batch
@echo off
setlocal
echo ========================================
echo GDB Debug with OpenOCD (STM32)
echo ========================================
echo.

:: 如果 OpenOCD 未加入系统 PATH，取消下面注释并修改路径
:: set OPENOCD_HOME=C:\OpenOCD\xpack-openocd-0.12.0-7
:: set PATH=%OPENOCD_HOME%\bin;%PATH%

echo [1/3] Starting OpenOCD (port 3333)...
start "OpenOCD" openocd.exe -f openocd.cfg

echo [2/3] Waiting for OpenOCD to start...
timeout /t 3 /nobreak >nul

echo [3/3] Starting GDB...
arm-none-eabi-gdb build/ov-watch.elf -ex "target remote :3333" -ex "monitor reset halt" -ex "load" -ex "break main" -ex "echo === GDB ready. Type 'continue' to run. ===\n"

pause
```

双击即可启动调试。


## 八、如何退出调试

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | 在 GDB 窗口输入 `q` | 退出 GDB |
| 2 | 切换到 OpenOCD 窗口，按 `Ctrl+C` | 停止 OpenOCD |
| 3 | 关闭两个终端窗口 | 可选 |

如果 GDB 卡住，按 `Ctrl+C` 强制中断，再输入 `q`。


## 九、常见问题排查

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| `openocd: command not found` | OpenOCD 未加入 PATH | 使用完整路径或加入环境变量 |
| `Error: open failed` | ST-Link 未连接或驱动问题 | 检查 USB，设备管理器查看 ST-Link |
| `Error: target not detected` | 板子未供电或 SWD 接线松动 | 检查 VCC/GND/SWDIO/SWCLK |
| GDB 连接超时 | OpenOCD 未运行或端口被占用 | 确认 OpenOCD 窗口显示 `Listening on port 3333` |
| 断点不生效 | 编译优化级别太高（如 `-Os`） | 使用 `-Og` 编译（你的 Makefile 已用 `-Og`） |


## 十、把本教程放进模板

将本教程保存为 `al开发项目规范/docs/debug-gdb-tutorial.md`，并在 `CLAUDE.md` 中添加引用：

```markdown
## 调试参考

- **GDB 调试完整教程**：`Read("docs/debug-gdb-tutorial.md")`
- **一键调试脚本**：`firmware/gdb_debug.bat`
```

新项目复制模板后，调试环境开箱即用。
