# AI 项目开发规范 v2.0

> **使用说明：** 每次启动 AI 辅助开发项目前，请先阅读本规范。确保 AI 和开发者都遵循相同的开发标准，避免项目失控。

### 开新项目时（三件套模板）

将以下文件复制到新项目根目录即可：

| 文件 | 操作 |
|------|------|
| `.claude/rules.md` | 直接复制，不用改（硬规则通用） |
| `CLAUDE.md` | 复制后，把占位内容改成新项目的具体信息（技术栈、入口文件、编译命令...） |
| `docs/CLAUDE-ops.md` | 复制后，填命令速查表（编译/烧录/检查命令） |
| `docs/development-standards.md` | 直接复制，不用改（本文件，原名 `AI项目开发规范.md`） |
| `docs/bugs/` 目录 | 直接复制（含 `.template.md` 和 `INDEX.md`） |
| `docs/features/` 目录 | 直接复制（含 `.template.md` 和 `INDEX.md`） |
| `docs/debug-gdb-tutorial.md` | 直接复制，不用改（GDB 调试通用教程） |
| `firmware/` 目录 | 复制后，改 `openocd.cfg` 芯片型号 + `gdb_debug.bat` 固件名和 OpenOCD 路径 |

一句话：**五件套+firmware目录复制走 → openocd.cfg改芯片+gdb_debug.bat改固件名+路径 → CLAUDE.md填项目特有信息 → CLAUDE-ops.md填命令 → codegraph init → 完成**

---

## 给 AI 的加载指南

> ⚠️ **本文档不会自动全部加载，请按需查阅。**

本规范采用 **三层分层设计**，不同层级有不同加载策略：

```
层级一：硬规则 (.claude/rules.md)               — 始终生效，~300 Token
层级二：项目地图 (CLAUDE.md)                    — 每次会话加载，~400 Token（仅项目事实）
层级二之补充：操作速查 (docs/CLAUDE-ops.md)      — 按需 Read，~350 Token
层级三：完整规范 (本文档)                        — 仅按需 Read，0 Token 静默
```

### 你的使用方式

1. **硬规则**（`.claude/rules.md`）已由系统加载，你必须始终遵守。
2. **项目地图**（`CLAUDE.md`）也已加载，里面写了项目概览（技术栈/目录/入口/约束）。**规范索引和任务流程在 `docs/CLAUDE-ops.md`**，需要时 `Read` 即可。
3. **完整规范**（本文档）在磁盘上但不会自动进入上下文。遇到以下场景时，请用 `Read` 工具读取本文档对应章节，不要凭记忆猜测：

| 你要做的事 | 读取章节 |
|-----------|---------|
| 写测试用例 | `Read` → 第八章 测试规范 |
| 处理密码/Token/用户输入 | `Read` → 第五章补充 安全红线 |
| Git 提交代码 | `Read` → 第六章 Git 提交规范 |
| 排查 API 调用费过高 | `Read` → 第十一章 Token 节省策略 |
| 多 Agent 同时改代码 | `Read` → 第七章 AI Agent 协作规范 |
| 开始改代码前 | `Read` → 第三章 任务执行规范 + 影响范围模板 |
| 不确定该不该重构 | `Read` → 第四章 文件修改规范 |
| **修复Bug后记录** | `Read` → 第十二章 Bug记录规范 |
| **搭建CI/CD质量门禁** | `Read` → 第十三章 自动化质量门禁标准要求 |
| **迁移工具链 / 重新生成CubeMX代码** | `Read` → 第三章 "工具链迁移专项检查清单" |
| **迁移后烧录验证失败** | `Read` → 第三章 "迁移后故障排查速查表" |
| **GDB 调试 STM32** | `Read` → `docs/debug-gdb-tutorial.md`（完整 GDB 调试教程） |

### 为什么这样设计？

- 完整规范约 5000 字（~8000 Token），全部加载会吃掉 1/4 上下文窗口。
- 分层后：硬规则 (~300) + 项目地图 (~400) = ~700 Token 固定开销，操作速查和完整规范按需加载。
- 对比旧版 (~1100 Token 固定加载) 再省 36%。
- 需要时再 `Read` 对应章节，2 秒内读完，token 按需消费。

---

## 一、项目目录规范

AI 最怕项目结构混乱。推荐以下标准结构：

```
project/
├── .claude/
│   └── rules.md             # 层级一：硬规则（始终加载，~300 Token）
│
├── CLAUDE.md                # 层级二：项目地图（始终加载，~400 Token，仅项目事实）
│
├── docs/
│   ├── CLAUDE-ops.md             # 层级二之补充：操作速查（按需 Read，~350 Token）
│   ├── development-standards.md  # 层级三：完整规范（按需 Read，0 Token 静默）
│   ├── bugs/                        # Bug记录（一Bug一文件，含.template.md + INDEX.md）
│   ├── features/                 # 功能规格（一功能一文件，含.template.md + INDEX.md）
│   ├── requirements/             # 需求文档
│   ├── architecture/             # 架构文档
│   ├── api/                      # 接口文档
│   └── decisions/                # 架构决策记录 (ADR)
│
├── src/                      # 源代码
│
├── tests/                    # 测试代码
│
├── scripts/                  # 工具脚本
│
├── README.md
└── CHANGELOG.md
```

**分层原则：**

| 层级 | 文件 | Token | 加载时机 |
|------|------|-------|---------|
| 硬规则 | `.claude/rules.md` | ~300 | 每次会话自动 |
| 项目地图 | `CLAUDE.md` | ~400 | 每次会话自动 |
| 操作速查 | `docs/CLAUDE-ops.md` | ~350 | 按需 Read |
| 完整规范 | `docs/development-standards.md` | ~8000 | 仅在需要时 `Read` |

**核心原则：** 
- 硬规则写"绝对不能做什么"（铁律）
- CLAUDE.md 写"这个项目是什么 + 去哪查细则"（索引）
- 完整规范写"具体怎么做"（手册）

---

## 二、AI 开发核心原则

将以下原则写入 Claude Code Rules：

```
# Core Principles

Correctness > Speed          # 正确性 > 速度
Readability > Cleverness     # 可读性 > 巧妙
Maintainability > Optimization # 可维护性 > 优化
Small Changes > Large Refactors # 小改动 > 大重构
Explicit > Implicit          # 显式 > 隐式
```

**要求 AI：**
- 不要猜测需求
- 不要创造不存在的接口
- 不要修改无关代码
- 不要重构用户未要求的内容
- 发现问题先分析再修改

---

## 三、任务执行规范

很多 AI 项目最后失控，是因为用户让 AI 修一个 Bug，AI 顺手重构 20 个文件，项目直接炸了。

**每次任务必须遵循以下流程：**

```
Step 1: 分析问题
   ↓
Step 2: 制定计划
   ↓
Step 3: 输出影响范围 + 验收标准清单（必须列出"怎样算完成"，用户确认后方可实施）
   ↓
Step 4: 实施修改
   ↓
Step 5: 验证结果
   ↓
Step 6: 输出总结
   ↓
Step 7: 
   - 若为Bug修复 → 创建 docs/bugs/BUG-YYYYMMDD-序号.md 并更新 INDEX.md
   - 若为新增功能 → 创建 docs/features/FEAT-YYYYMMDD-序号-简短描述.md 并更新 INDEX.md
```

**关键约束：** 未经用户确认，AI 不得自行扩大修改范围。

**为什么？** AI 对"大型修改"的理解和人类不同。一个字段类型的改动在 AI 看来"很小"，但在运行时可能导致整个数据层崩溃。输出影响范围让人类做最终判断。

**影响范围模板（每次计划中必须包含）：**

```
📋 改动影响分析
├── 修改文件：xxx.c (函数 yyy)、aaa.h (结构体 bbb)
├── 新增文件：无
├── 接口变化：无 / xxx() 参数从 int 改为 uint32_t
├── 数据库/存储：无 / 配置区格式不变
├── 向后兼容：✅ 完全兼容 / ⚠️ 旧帧仍可解析 / ❌ 需要重新格式化
└── 风险点：低 / 中（xxx 需要验证）/ 高（影响 zzz）
```

### 工具链迁移 / 代码生成任务专项检查清单

> 执行 CubeMX 重新生成、工具链切换（Keil↔GCC）、构建系统迁移（Makefile/CMake）前，必须完成以下配置源对比。硬规则见 `rules.md` 第 1.1 条。

**Step 1：读取 `.ioc` 配置清单**
用文本编辑器打开 `.ioc` 文件，搜索 `USART`、`I2C`、`SPI`、`TIM`、`RTC`、`ADC`、`DMA` 等外设配置条目，记录所有已配置外设。

**Step 2：扫描实际代码调用**
在 `Core/Src/` 下搜索 `MX_*_Init()`，记录所有实际调用的外设初始化函数。

**Step 3：差异分析**
对比两份清单，标记差异项：
- `.ioc` 有但代码没调用 → 可能是遗留配置，确认后可以清理。
- **代码调用了但 `.ioc` 没有 → 手动添加的外设，迁移后必须手动恢复代码。**（这是最常见的问题）

**Step 4：在影响范围报告中体现**
差异项单独列为"迁移后需手动恢复"条目。在阶段 3（实现）时必须逐一检查这些外设的初始化代码是否完整。

> **典型案例：** OV-Watch 项目中 USART2 是手动添加到代码中的，未在 `.ioc` 中配置。CubeMX 重新生成代码后，USART2 的 `MX_USART2_UART_Init()` 调用丢失，导致 ESP32 通信串口静默失效，直到人工排查才发现。

### 迁移后验证清单

迁移完成后，逐项检查：
- [ ] 所有手动添加的外设初始化代码已恢复（对照差异清单）
- [ ] 编译通过，0 错误 0 警告
- [ ] 固件烧录后所有外设功能正常（串口通信、传感器读数、显示输出、电机动作）
- [ ] 中断向量表正确（`startup_*.s` 与芯片型号匹配）

### 迁移后故障排查速查表

> 当迁移后烧录验证发现功能异常时，按以下优先级排查。硬规则见 `rules.md` 第 1.2 条。
> **核心原则：唯一变量是工具链 → 先查工具链差异，再查代码。**

| 故障现象 | 优先排查方向 | 常见根因 |
|----------|-------------|----------|
| 日志中浮点数显示为空/零 | `printf` 浮点支持 | `-specs=nano.specs` 裁剪了 `_printf_float`，需链 `-u _printf_float` |
| 串口帧解析失败（数据全零） | `sscanf` 浮点支持 | `-specs=nano.specs` 裁剪了 `_scanf_float`，需链 `-u _scanf_float` |
| 动态分配内存失败 | 堆大小/malloc行为 | GCC linker script 中 `_heap_size` 默认值可能远小于 Keil 配置 |
| 硬故障（HardFault） | 中断向量表/FPU | `startup_*.s` 的 vector table 顺序与 ARMCC 不同，或 `-mfloat-abi=softfp` vs `hard` 不匹配 |
| 某个外设不工作 | 外设初始化代码 | CubeMX 重新生成时丢失（`rules.md` 1.1 条覆盖），恢复 `MX_*_Init()` 调用 |
| 编译通过但行为诡异 | 预定义宏差异 | `#ifdef __CC_ARM` vs `#ifdef __GNUC__` 导致不同代码路径被编译 |

---

## 四、文件修改规范

Claude Code 非常适合遵守以下规范：

**修改代码前必须：**
1. 阅读完整文件
2. 阅读相关依赖
3. 分析调用链
4. 再进行修改

**禁止：**
- 只看局部代码直接修改
- 改 A 炸 B 影响 C

**修改原则：**
- 做最小可能的改动
- 不重构无关代码
- 保持向后兼容
- 永不删除现有功能

---

## 五、代码质量规范

**要求 AI 对每个函数做到：**

- 单一职责
- 明确输入（强类型定义）
- 明确输出（强类型定义）
- 错误处理
- 类型定义

**示例：**

```python
def create_user(
    email: str,
    password: str
) -> User:
    ...
```

**禁止：**

```python
def handle(data):
    ...
```

**代码质量检查清单：**
- [ ] 代码可读且命名良好
- [ ] 函数聚焦（< 50 行）
- [ ] 文件内聚（< 800 行）
- [ ] 无深层嵌套（> 4 层）
- [ ] 错误显式处理
- [ ] 无硬编码值（使用常量或配置）
- [ ] 不可变数据模式

---

## 五之补充：安全红线

> AI 生成的代码经常"能跑但不安全"。以下规则是硬性要求，违反即退回。

**代码安全审查清单：**

- [ ] **禁止硬编码密钥/Token**：所有凭证必须从环境变量或密钥管理服务读取。`const API_KEY = "sk-xxx"` 零容忍。
- [ ] **输入即敌人**：所有外部输入（用户表单、API 请求体、URL 参数、串口数据）必须校验和消毒。禁止直接拼接到 SQL/Shell 命令/格式化字符串中。
- [ ] **依赖漏洞检查**：新增任何第三方库前，必须先确认其无已知高危漏洞（CVE）。`pip install` / `npm install` 之前看一眼 advisory DB。
- [ ] **敏感日志脱敏**：打印日志时，必须对密码、Token、手机号、身份证号等敏感字段脱敏（`***`）。
- [ ] **缓冲区边界**：C/C++ 项目特别注意 `snprintf`/`strncpy` 大小参数不越界、数组下标不溢出、指针不悬空。

**AI 常见安全错误速查：**

| 错误模式 | 为什么危险 | 正确做法 |
|---------|-----------|---------|
| `sprintf(buf, user_input)` | 格式化字符串注入 | `snprintf(buf, sizeof(buf), "%s", user_input)` |
| `system("rm -rf " + path)` | 命令注入 | 用库函数操作文件系统 |
| `eval(user_data)` | 代码注入 | JSON.parse / 白名单校验 |
| 密码明文写日志 | 日志泄露 | `LOG("auth: user=%s, pass=***", user)` |
| 数组 `for(i=0;i<=N;i++)` | 越界写入 | `for(i=0;i<N;i++)` |

---

## 六、Git 提交规范

使用 Conventional Commits 格式：

```
feat:      新功能
fix:       Bug 修复
refactor:  重构
docs:      文档
test:      测试
chore:     杂务
perf:      性能优化
ci:        CI/CD
```

**示例：**

```
feat(auth): add jwt refresh token
fix(user): resolve email validation bug
refactor(db): simplify repository layer
```

---

## 七、AI Agent 协作规范

这是很多团队忽略的环节。

**建立 `/docs/architecture` 文档，写清楚：**

- 系统架构
- 数据库结构
- 接口规范
- 业务流程
- 模块职责

**然后告诉 AI：**

> 任何代码修改必须符合 architecture 文档。
> 如发现冲突，先报告，不要直接修改。

**多 Agent 权限模型：**

| 角色 | 权限 |
|------|------|
| Dev Agent（主开发） | 读写 — 唯一有写入权限的 Agent |
| Review Agent（审查） | 只读 — 生成审查意见，不得直接修改代码 |
| Doc Agent（文档） | 只读 — 生成文档建议，不得修改代码 |
| Test Agent（测试） | 只读 — 生成测试用例，不得修改业务代码 |

**冲突上报机制：**

- 若 AI 发现新增代码与 `/docs/architecture` 中的设计有冲突，**必须停止操作并上报**，列出冲突点和两种选择（改代码 vs 改架构），不得自作主张妥协修改。
- 若两个 Agent 对同一段代码给出了矛盾的修改建议，**以 Dev Agent 为准**，但 Review Agent 的建议必须人工过目后决定。

---

## 八、测试规范

**要求 AI：**

| 场景 | 要求 |
|------|------|
| 新增功能 | 必须新增测试 |
| Bug 修复 | 必须新增回归测试 |

**覆盖率目标：**

| 层级 | 目标覆盖率 |
|------|-----------|
| 核心逻辑 | 90%+ |
| 业务逻辑 | 80%+ |
| 整体项目 | 70%+ |

**TDD 工作流：**

```
1. 先写测试 (RED)
2. 运行测试 — 应该失败
3. 编写最小实现 (GREEN)
4. 运行测试 — 应该通过
5. 重构 (IMPROVE)
6. 验证覆盖率
```

---

## 九、最重要的规范

带团队做 AI 开发，最有效的一条是：

> **Never write code first.**
>
> **Understand first. → Plan second. → Implement third.**

翻译：不要先写代码。先理解，再规划，最后编码。

**这一条能减少至少 50% 的 AI 乱改代码问题。**

---

## 十、适合 Claude Code 的完整 Rules

将以下内容放入 `.claude/rules.md`：

```markdown
# AI Software Engineering Rules

## Core Principles

Correctness > Speed
Readability > Cleverness
Maintainability > Optimization

## Before Coding

1. Understand requirements completely
2. Read related files
3. Analyze dependencies
4. Create implementation plan

## Modification Rules

- Make smallest possible change
- Do not refactor unrelated code
- Preserve backward compatibility
- Never remove existing functionality

## Code Quality

- Strong typing
- Single responsibility
- Clear naming
- Error handling
- Unit tests required

## Git

Use Conventional Commits

## Architecture

Follow architecture documents strictly

If conflict exists:
  Report first
  Do not modify blindly

## Testing

New Feature → New Tests
Bug Fix → Regression Test

## Bug Recording

After every bug fix → Create docs/bugs/BUG-YYYYMMDD-NNN.md + update INDEX.md

## Output

Explain:
- What changed
- Why changed
- Risks
- Validation results
```

---

## 十一、Token 节省策略

> Token 省在两类：**API 输入缓存**（大段背景知识复用）和**工具调用**（搜索/读取次数）。前者省的是大头（API 调用费），后者省的是交互轮次。

### 0. API 提示词缓存策略 — 省钱见效最快的手段

**这是所有 Token 优化中收益最高的。** 大段 System Prompt / CLAUDE.md / 架构文档如果每次都重新上传，费用相当可观。

- **固定前缀原则**：将 System Prompt、角色设定、CLAUDE.md、架构文档放在 `messages` 数组的**最前面**。动态用户问题放在**最后面**。前缀不变 → 缓存命中 → 输入 token 成本降 90%。
- **消除动态干扰**：System Prompt 中**严禁**包含时间戳、随机数、UUID、`Date.now()` 等动态字符。一个会变的值就能破坏整段前缀匹配，导致缓存全部失效。
- **监控命中率**：每次请求检查返回体中的 `prompt_cache_hit_tokens` 与 `prompt_cache_miss_tokens`。若 `miss` 持续走高，立即检查前缀是否被动态内容污染。
- **长文档放 CLAUDE.md**：把架构文档、项目规范写进 `CLAUDE.md`（AI 开局自动加载到前缀），比每次对话中手动 `Read` 再贴进去能稳定命中缓存。

| 缓存状态 | 输入 token 价格 | 说明 |
|---------|--------------|------|
| 命中 (hit) | ~10% 原价 | 前缀与之前的请求相同 |
| 未命中 (miss) | 100% 原价 | 前缀变了，需要重新编码上传 |

### 1. CodeGraph — 项目启动第一步

**在项目根目录执行一次：**

```bash
codegraph init
```

**作用：** 扫描所有源码，建立符号索引数据库（`.codegraph/` 目录，类似 `.git/` 持久化）。

**为什么能省 token？**

| 传统方式 | CodeGraph 方式 |
|---------|---------------|
| `grep -rn "函数名"` → 几十个文件 | `codegraph explore "函数名"` → 1 次调用拿到定义+调用链 |
| `read file.c` → 整个文件 | `codegraph node file.c` → 精确到符号级别 |
| 查调用链：多次 grep + read | 1 次 `codegraph explore` 返回完整调用路径 |
| 每次新会话都要重新搜索 | 索引持久化，新会话直接查 |

**典型 token 节省量：** 代码探索阶段可减少 60-80% 的 tool call 次数。

**索引后，告诉 AI 优先用它：**

```markdown
# 写入 CLAUDE.md
## CodeGraph
本项目已建立 CodeGraph 索引 (.codegraph/ 目录)。查代码时优先使用：
- `codegraph explore` — 查符号、调用链、多个文件之间的关系
- `codegraph node`   — 读单个符号或文件的精确内容
仅当 CodeGraph 覆盖不到时再用 grep/read。
```

### 2. CLAUDE.md — 让 AI 开局就有地图

在项目根目录创建 `CLAUDE.md`，写清楚：

```markdown
# 项目概览
- 项目名、用途、一句话描述
- 技术栈（语言/框架/数据库）
- 入口文件、核心模块路径

# 架构速查
- 目录结构（顶层即可）
- 关键模块职责（每个 1-2 句话）

# 约束规则
- 编译/运行命令
- 禁止修改的文件/目录
- 特殊约定（如"不要用 Python 3.12 新语法"）

# 外部依赖
- 引用的其他项目路径
- 外部 API 地址
```

**省在哪里？** 没有 CLAUDE.md，AI 每次新会话都要自己探索项目结构，浪费开头 5-10 轮对话。有了它，开局直接进入工作状态。

### 3. 交接文档 (HANDOFF.md)

记录项目当前状态，AI 读到后立刻知道：

- 哪些功能已完成
- 哪些功能有 bug、什么根因
- 烧录/部署步骤
- 引脚/接口速查表
- 历史踩坑记录

**省在哪里？** AI 不需要从日志和代码反推"现在项目是什么状态"。

### 4. Bug 记录 (docs/bugs/)

一个Bug一个文件，INDEX.md 只存汇总表。新增Bug时直接创建文件，无需读任何旧内容。

**省在哪里？** 不读旧内容就能追加，索引用一张表秒查。AI 不会把已知 bug 当成"新发现"重复分析。

### 5. 不要重复读同一个文件

- **同一个文件只读一次，记住内容，不要反复 Read**
- 编辑前确认修改点，读一次；编辑后相信 Edit 工具返回的结果，不要再 Read 验证
- 如果有 CodeGraph，查文件内容用 `codegraph node <file>` 而不是 Read（带行号输出更高效）

### 6. 并发 tool call

**以下场景可以并行调用（一次返回多个结果）：**

```python
# ✅ 好 — 3 个独立查询同时发出
Read("file_a.c")
Read("file_b.c")
Grep("symbol_name")

# ❌ 差 — 读一个等一个再读下一个
Read("file_a.c")
# 等待返回...
Read("file_b.c")
# 等待返回...
```

### 7. 不要过度解释

- **改完代码，1-2 句总结即可**，不要复述整个修改过程
- 用户说"做 X"，做完说"已完成 X"即可，不要展开论述
- 编译错误贴关键行，不要全文

### 8. Token 节省速查表

| 习惯 | 省 token 量 | 省钱？ |
|------|-----------|-------|
| **API 提示词缓存**（固定前缀，消除动态内容） | ⭐⭐⭐⭐⭐ 90% | ✅ 直接省 API 费 |
| `codegraph init` 后优先用它查代码 | ⭐⭐⭐⭐⭐ 60-80% | — |
| 项目有 CLAUDE.md | ⭐⭐⭐⭐ 开局省 5-10 轮 | ✅ 稳定命中缓存 |
| 有 HANDOFF.md + docs/bugs/ | ⭐⭐⭐ 省状态探测 | — |
| 并发 tool call | ⭐⭐⭐ 减少轮次 | — |
| 文件只读一次 | ⭐⭐ | — |
| 简短总结 | ⭐⭐ | — |

---

## 十二、Bug记录规范

> **核心原则：一Bug一文件，修完即建。INDEX.md 只存汇总表，永远精简。**

### 为什么不用单文件？
- 单文件追加记录前必须先读全文件 → 文件越大越浪费上下文
- 一个Bug一个 `.md` 文件，新增时直接 `Write`，不读任何旧内容
- 查历史时先看 `INDEX.md`（~200 Token 表格），找到目标再按需 `Read` 具体文件
- `INDEX.md` 每行只有7个字段，永远不会膨胀

### 目录结构
```
docs/bugs/
├── INDEX.md                          # 汇总表，始终精简
├── .template.md                      # 新建Bug文件时的模版
├── BUG-20260618-001-登录超时.md       # 单个Bug详情
└── BUG-20260619-001-内存泄漏.md       # 单个Bug详情
```

### 操作流程

**新增Bug记录（修完即做）：**
1. 复制 `.template.md`，改名为 `BUG-YYYYMMDD-序号-简短描述.md`
2. 填写模板中的字段
3. 在 `INDEX.md` 对应表格加一行摘要

**查历史Bug：**
1. 先读 `docs/bugs/INDEX.md` 查找目标
2. 再 `Read` 具体Bug文件获取详情

### 记录模板

```markdown
# [Bug标题]

- **Bug ID**：BUG-YYYYMMDD-序号
- **严重等级**：P0-阻断 / P1-严重 / P2-一般 / P3-轻微
- **发现日期**：YYYY-MM-DD
- **修复日期**：YYYY-MM-DD

## 现象描述
（Bug的具体表现是什么）

## 复现步骤
1. 步骤1
2. 步骤2
3. 观察到的错误行为

## 根因分析
（为什么会发生，一句话说清）

## 修复方案
（怎么修的，一句话说清）

## 影响文件
- 文件1
- 文件2

## 验证方式
（如何确认已修复）
```

### INDEX.md 格式

```markdown
# Bug 记录索引

## 已修复
| Bug ID | 标题 | 等级 | 发现 | 修复 | 文件 |
|--------|------|------|------|------|------|

## 已知未修复
| Bug ID | 标题 | 等级 | 发现 | 备注 |
|--------|------|------|------|------|

## 不予修复
| Bug ID | 标题 | 等级 | 原因 |
|--------|------|------|------|
```

### 为什么？
- 新增记录不读旧文件，0 token 追加成本
- 索引用一张表秒查，不会遗漏
- 积累项目知识库，新成员/AI 快速了解已知问题

---

## 十三、自动化质量门禁（CI/CD）标准要求

> 每个基于本模板创建的具体项目，必须在项目初期搭建自动化质量门禁。
> 本章基于嵌入式项目（STM32 + GCC + GitHub Actions）实战验证，可直接复用模式。

### 13.1 分工原则：模板定标准，项目落执行

| 内容 | 放在模板里（本仓库） | 放在具体项目里 |
|------|---------------------|---------------|
| CI 标准要求（必须覆盖哪些 AC） | ✅ 本章 | ❌ |
| "搭建 CI" 的 FEAT 任务模板（占位符） | ✅ `docs/features/.template.md` | ❌ |
| 具体的 CI 脚本（`.yml`） | ❌（每个项目技术栈不同） | ✅ 项目的 `.github/workflows/` |
| 具体的检查脚本（如 `check-firmware.sh`） | ❌（每个项目不同） | ✅ 项目的 `.claude/scripts/` |

### 13.2 强制覆盖的验收标准
以下 AC 项必须由 CI 流水线自动检查，不得依赖人工打勾：
- AC-03：编译/Lint 通过
- AC-04：无新增警告
- AC-06：无硬编码密钥（安全扫描）
- AC-09：新增测试自动运行
- AC-10：全部回归测试通过
- AC-11：测试覆盖率 ≥ 80%

### 13.3 推荐门禁结构（5 道流水线，已验证可行）

以下结构在 OV-Watch（STM32F407 + FreeRTOS + GCC）项目中实战落地，可直接作为新项目的起点：

```
quality-gate.yml
  ├── Job 1: 静态检查 — 项目特定的回归脚本（check-firmware.sh）
  ├── Job 2: 密钥扫描 — 正则扫硬编码密钥/Token/密码
  ├── Job 3: 危险函数 — 扫 sprintf/strcpy/gets/scanf 等不安全调用
  ├── Job 4: 代码规范 — 文件行数 ≤ 800、函数行数 ≤ 50（可配）
  └── Job 5: 编译验证 — 安装工具链 → make → 输出固件体积
```

**为什么拆成 5 个独立 Job？**
- 并行执行，总耗时 = 最慢的那个 Job（编译），而非全部串行。
- 失败时精准定位：哪个 Job 红了就知道哪类问题，不用翻长日志。
- 静态检查不需要编译环境，可以秒级出结果。

### 13.4 嵌入式项目 CI 实战要点

**13.4.1 GCC 工具链缓存（省钱 + 提速）**

`gcc-arm-none-eabi` 包约 200MB，每次 CI 都 apt install 耗时 2-3 分钟。用 `actions/cache` 缓存：

```yaml
- name: Cache ARM GCC
  uses: actions/cache@v4
  with:
    path: /usr/share/gcc-arm-none-eabi
    key: arm-gcc-${{ runner.os }}
```

**13.4.2 扫描排除区（vendor code）**

第三方库（HAL/FreeRTOS/CMSIS）不应被项目规范扫描。所有扫描 Job 必须限定目录：

```bash
# ✅ 只扫项目代码
SCAN_DIRS="firmware/App firmware/Core firmware/Middleware firmware/Drivers/BSP"

# ❌ 不要扫 vendor（会有一堆无法修改的"违规"）
# Drivers/STM32F4xx_HAL_Driver Middlewares/Third_Party/FreeRTOS
```

**13.4.3 固件体积追踪**

编译 Job 最后输出 `arm-none-eabi-size`，每次 push 都能在 CI 日志里看到 Flash/RAM 用量变化，防止功能膨胀不知不觉撑爆芯片。

```yaml
- name: Show firmware size
  run: arm-none-eabi-size build/ov-watch.elf
```

### 13.5 CI 红灯 = 阻断合并（铁律）

> **CI 任何一道门禁未通过，代码不得合并到主分支。**

这条不是"建议"，是硬规则。写入 `CLAUDE.md` 约束章节：

```markdown
- CI 红灯禁止合并：`quality-gate.yml` 5 道门禁全绿才允许 merge
- 本地提交前必须运行静态检查脚本（如 `.claude/scripts/check-firmware.sh`）
```

### 13.6 实施指引（新项目初始化时执行）
1. 根据项目技术栈选择 CI 工具（GitHub Actions / GitLab CI / Jenkins）。
2. 从 `docs/features/.template.md` 复制创建 FEAT 任务，填入具体技术栈。
3. 在项目根目录创建 `.github/workflows/quality-gate.yml`，参考 13.3 的 5 道门禁结构。
4. 在 `.claude/scripts/` 下创建项目特定的静态检查脚本（回归项、引脚规则、硬件约束等）。
5. 配置本地 pre-commit 钩子（可选但推荐）。
6. 修改本项目的 `CLAUDE.md`：
   - 目录速查加 `.github/workflows/` 和 `.claude/scripts/`
   - 约束章节加"CI 红灯禁止合并"
   - 规范索引加 CI 相关读取命令

---

> **最后提醒：** 规范的价值在于执行。每次开发前花 2 分钟回顾这份文档，能减少 50% 以上的返工。
