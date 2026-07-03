# 数据链路层滑动窗口协议开发记录 LOG03

## 0. LOG03 生成背景

本文件生成于 `DEVELOPMENT_LOG00_RECORD.md`、`DEVELOPMENT_LOG01_RECORD.md`、`DEVELOPMENT_LOG02_RECORD.md` 和 `HIGH_BER_OPTIMIZATION_NOTES.md` 之后，用于后续另起 ChatGPT/Codex 窗口继续开发、验收准备、报告整理和代码维护。

本文件不是实验报告正文。旧记录只作为历史上下文；若旧记录与当前源码、Git 状态或测试证据冲突，以本 LOG03 的当前仓库实证为准。生成和二次核查过程中未执行 `git add`、`git commit`、`git reset`、`git clean`，未删除日志目录，未继续改协议逻辑。

## 1. 项目路径与当前仓库状态

| 项目 | 当前记录 |
|---|---|
| 当前工作目录 | `C:\Users\28641\Desktop\Experiment-1` |
| 主工程目录 | `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)` |
| Windows 源码路径 | `Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-Windows-VS2019` |
| Linux 源码路径 | `Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux` |
| 工程记录目录 | `Lab1-2024(Win+Linux)/DEV_LOG` |
| 旧记录文件 | `DEV_LOG/DEVELOPMENT_LOG00_RECORD.md`、`DEV_LOG/DEVELOPMENT_LOG01_RECORD.md`、`DEV_LOG/DEVELOPMENT_LOG02_RECORD.md`、`DEV_LOG/HIGH_BER_OPTIMIZATION_NOTES.md` |
| 最近提交 | `eb3ec94 Update generated outputs`; `82bacc6 Initial commit` |
| 当前工作区 | 非干净；存在 staged、unstaged、untracked 的源码、脚本、日志、summary、文档和归档文件 |

### 1.1 Git 状态摘要

| 类别 | 关键文件/目录 | 当前状态 |
|---|---|---|
| 协议源码修改 | `Lab1-Windows-VS2019/datalink.c`; `Lab1-linux/datalink.c` | `MM`；当前未暂存 diff 是本轮 repeat NAK 变更；两份文件 SHA256 相同 |
| 头文件/注释修改 | `Lab1-Windows-VS2019/datalink.h`; `Lab1-linux/datalink.h`; `Lab1-linux/lprintf.h` | Git 状态中存在 staged 修改；当前未暂存 diff 未显示新增差异 |
| Linux 兼容/框架文件 | `protocol.c/h`; `lprintf.c/h`; `Makefile` | 多数属于前序轮次的已暂存或测试复制产物，需要提交前筛选 |
| 测试脚本 | `run_windows_tests.ps1`; `run_quick_test.ps1`; `validate-final.sh`; `verify-current.sh`; `verify-nak-repeat-matrix.sh`; `run-performance-tests.ps1` 等 | 多个脚本新增或修改；`verify-nak-repeat-matrix.sh` 是历史调参脚本，不建议直接复用 |
| 测试结果 | `performance-tests-*`; `minimal-matrix-*`; `param-sweep-*`; `final-validation-*`; `nak-repeat-*`; `linux-run-verify-*` | 大量日志和 summary；关键有效证据见第 9 节 |
| 文档记录 | `Lab1-2024(Win+Linux)/DEV_LOG/` | 整个 `DEV_LOG/` 当前为 untracked；根目录旧 LOG 在 Git 状态中显示删除/迁移 |
| 归档/产物 | `Lab1-2024(Win+Linux)_normal.rar`; 旧 `Lab1-2024(Win+Linux).rar`; `*.o`; Linux `datalink`; 大日志目录 | 不建议全部提交，需人工确认 |

未暂存 `git diff --stat` 主要显示根目录旧 LOG 删除、旧 `.rar` 删除，以及两份 `datalink.c` 约 41 行级别改动。完整 staged 状态很大，包含大量日志/脚本/产物。

## 2. 与 LOG00 / LOG01 / LOG02 / HIGH_BER 的关系

| 来源记录 | 当时结论 | 当前是否仍成立 | LOG03 修正或新增证据 |
|---|---|---|---|
| LOG00 | 初始接手，梳理 SR 协议、双平台工程和同步要求 | 部分成立 | 当前仓库路径已迁移到 `Experiment-1`；旧记录实际集中在 `DEV_LOG/` |
| LOG01 | 代码、注释、脚本和测试流程有一轮整理；强调 Windows/Linux 同步 | 基本成立 | 当前两份 `datalink.c` 哈希完全一致，但工作区新增大量测试产物，提交前需筛选 |
| LOG02 | 稳定参数为 `DATA_TIMER=2300`；Windows/Linux `datalink.c` 当时一致；遗留 Windows 复测和 Linux 长测 | 部分成立 | `DATA_TIMER=2300` 仍成立；但新增 `NAK_REPEAT_INTERVAL=800` 和 repeat NAK 逻辑，LOG02 的“无 repeat NAK”状态已过时 |
| HIGH_BER_OPTIMIZATION_NOTES | 高误码瓶颈来自 NAK 丢失后依赖 DATA timeout；建议研究限速重复 NAK，不改变窗口 | 成立且已落地一版 | 当前实现为干净 `NAK_REPEAT_INTERVAL=800` 版本，只改两份 `datalink.c` 的协议逻辑；Linux 300s/1200s 证据支持保留 |
| LOG02 遗留风险 | Windows 重编译/复测、Linux 五组长测、报告填写、日志清理 | 仍成立 | Windows exe 早于当前源码；Linux 只有当前源码高误码 1200s 和矩阵，尚未补齐当前源码五组 20 分钟 |

## 3. 当前实现状态总览

| 功能 | 当前状态 | 相关文件/函数 | LOG03 备注 |
|---|---|---|---|
| Selective Repeat | 已实现 | `between`; `ack_expected`; `next_frame_to_send`; `nbuffered` | 当前不是 GBN |
| Go-Back-N 独立实现 | 未发现 | `datalink.c` | 只有 SR 风格窗口、乱序缓存和单帧重传 |
| 全双工通信 | 已实现 | `main`; `NETWORK_LAYER_READY`; `FRAME_RECEIVED` | 多组 A/B flood 测试可佐证 |
| CRC32 | 已实现 | `put_frame`; `recv_frame`; `crc32` | 仍使用 CRC32 |
| 手工帧序列化 | 已实现 | `send_data_frame`; `put_frame` | 不是直接发送结构体 |
| Piggyback ACK | 已实现 | `send_data_frame` | DATA 帧携带 ACK 字段 |
| ACK timer | 已实现 | `ACK_TIMEOUT`; `start_ack_timer` | 本轮还用于 repeat NAK 节流 |
| NAK | 已实现并增强 | `send_nak`; `FRAME_NAK` | 新增限速重复 NAK |
| 乱序缓存 | 已实现 | `in_buf`; `arrived`; `too_far` | SR 接收窗口核心 |
| 序号回绕 | 已实现 | `between` | `MAX_SEQ=15`，`NR_BUFS=8` |
| DATA timer | 已实现 | `DATA_TIMEOUT`; `start_timer` | 当前 `DATA_TIMER=2300` |
| 单帧重传 | 已实现 | `DATA_TIMEOUT`; 收到 `FRAME_NAK` | 不回退整个窗口 |
| 网络层 enable/disable | 已实现 | `update_network_layer` | 根据发送窗口是否满控制网络层 |
| 物理层发送队列节流 | 已实现 | `wait_for_physical_layer`; `PHL_QUEUE_LIMIT` | 当前值 `FRAME_DATA_LEN * 2` |
| Linux 编译/运行 | 当前源码有证据 | Linux 测试目录 | repeat800 冒烟、300s 矩阵、1200s 高误码复测 |
| Windows 编译/运行 | 当前源码未复测 | Windows exe/summary | `datalink.exe` 时间早于当前 `datalink.c`，现有五组是旧 exe 证据 |
| 自动化测试脚本 | 多个存在 | `.ps1`; `.sh` | 部分是历史调参脚本，提交前筛选 |
| 当前最终参数 | 2300 + repeat800 | 两份 `datalink.c` | 推荐候选，但 Windows 需重编译复测 |

## 4. 当前最终参数与协议配置

两份 `datalink.c` 当前 SHA256 完全一致：

`627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43`

```c
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define DATA_TIMER 2300
#define ACK_TIMER 300
#define NAK_REPEAT_INTERVAL 800
#define FRAME_HEAD_LEN 3
#define FRAME_CRC_LEN 4
#define FRAME_DATA_LEN (FRAME_HEAD_LEN + PKT_LEN)
#define FRAME_MAX_LEN (FRAME_DATA_LEN + FRAME_CRC_LEN)
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

| 参数 | 当前值 | 作用 | 是否继续调整 |
|---|---:|---|---|
| `MAX_SEQ` | 15 | 逻辑序号 0..15 | 暂不建议 |
| `NR_BUFS` | 8 | SR 窗口大小 | 暂不建议 |
| `DATA_TIMER` | 2300 | DATA 重传超时 | LOG02 稳定值，保留 |
| `ACK_TIMER` | 300 | 延迟 ACK 基准 | 可保留 |
| `NAK_REPEAT_INTERVAL` | 800 | NAK 丢失后 repeat NAK 间隔 | 本轮新增，当前推荐 |
| `FRAME_HEAD_LEN` | 3 | kind/seq/ack 头部 | 不建议 |
| `FRAME_CRC_LEN` | 4 | CRC32 | 不建议 |
| `FRAME_DATA_LEN` | `FRAME_HEAD_LEN + PKT_LEN` | DATA 不含 CRC 长度 | 不建议 |
| `FRAME_MAX_LEN` | `FRAME_DATA_LEN + FRAME_CRC_LEN` | 最大帧长 | 不建议 |
| `PHL_QUEUE_LIMIT` | `FRAME_DATA_LEN * 2` | 物理层队列节流 | 暂不建议 |

本轮参数变更的核心是新增 `NAK_REPEAT_INTERVAL=800`。修改前 NAK 丢失后主要依赖 DATA timeout；修改后 ACK timer 在 `no_nak == 0` 时触发重复 NAK。300s 默认误码 flood 未下降，高误码 1200s 提升约 2.6 到 3 个百分点。

## 5. LOG02 之后的主要代码修改

### 5.1 新增限速重复 NAK

| 字段 | 内容 |
|---|---|
| 修改文件 | 两份 `datalink.c` |
| 涉及函数/宏 | `NAK_REPEAT_INTERVAL`; `send_nak`; `ACK_TIMEOUT`; `send_data_frame` |
| 修改前问题 | NAK 丢失/损坏后恢复慢，常等 DATA timeout |
| 修改后行为 | `send_nak(frame_expected)` 发送 NAK、置 `no_nak=0`、启动 `NAK_REPEAT_INTERVAL` ACK timer；`ACK_TIMEOUT` 在 `!no_nak` 时 repeat NAK |
| 证据 | `git diff` 显示仅两份当前 `datalink.c` 的 repeat NAK 逻辑；Linux 300s/1200s summary 支持 |
| 风险 | Windows 当前源码尚未重新编译复测 |

### 5.2 保留 `DATA_TIMER=2300`

LOG02 的 `DATA_TIMER=2300` 没有被本轮继续改小。本轮选择通过 repeat NAK 降低高误码恢复延迟，避免过度压缩 DATA timeout 造成默认误码或普通模式不稳。

### 5.3 Windows/Linux `datalink.c` 同步

两份 `datalink.c` SHA256 一致，当前协议逻辑完全同步。注意这只说明 `datalink.c` 一致，不代表平台工程文件没有差异。

### 5.4 测试脚本和结果目录增加

当前仓库保留多轮调参和验证结果。`verify-nak-repeat-matrix.sh` 含历史 patch 逻辑，且 `nak-repeat-matrix-20260512-121903` 的 repeat600/repeat800 曾出现 `tlast_nak_ms` 编译错误；后续不要直接把该脚本当最终验证脚本使用。

### 5.5 文档记录迁移

旧 LOG 当前实际在 `DEV_LOG/` 下；根目录旧 LOG 在 Git 状态中表现为删除/迁移。提交前需要人工确认是否接受该目录布局。

## 6. 当前核心数据结构说明

| 名称 | 类型 | 含义 | 使用位置 | 备注 |
|---|---|---|---|---|
| `struct FRAME` | 结构体 | 逻辑帧，含 kind/seq/ack/data | 收发解析 | 发送时手工序列化 |
| `FRAME_DATA` | 帧类型 | 数据帧 | `send_data_frame`; `FRAME_RECEIVED` | 携带 seq 和 piggyback ACK |
| `FRAME_ACK` | 帧类型 | 独立 ACK | `ACK_TIMEOUT` | 无 payload |
| `FRAME_NAK` | 帧类型 | NAK | `send_nak`; 收到 NAK 后重传 | 本轮 repeat NAK 使用 |
| `ack_expected` | `unsigned char` | 发送窗口左边界 | ACK 滑窗、timer 停止 | 0..15 |
| `next_frame_to_send` | `unsigned char` | 下一个发送序号 | 网络层 ready | 0..15 |
| `frame_expected` | `unsigned char` | 接收端期望序号 | DATA 接收、NAK 编号 | 0..15 |
| `too_far` | `unsigned char` | 接收窗口右边界后一位 | `between` 判断 | 随交付推进 |
| `nbuffered` | `unsigned char` | 未确认发送帧数 | `update_network_layer` | 0..8 |
| `no_nak` | `unsigned char` | 是否允许发送新 NAK | 错帧、乱序、ACK timeout | repeat NAK 时为 0 |
| `out_buf` | packet 缓存 | 发送缓存 | DATA timeout/NAK 重传 | 8 个 packet |
| `in_buf` | packet 缓存 | 接收乱序缓存 | 连续交付 | 8 个 packet |
| `arrived` | 标记数组 | 接收缓存占用 | DATA 接收/交付 | 0/1 |
| `NAK_REPEAT_INTERVAL` | 宏 | repeat NAK 间隔 | `send_nak`; `ACK_TIMEOUT` | 当前 800ms |

## 7. 当前核心函数说明

### 7.1 `main(int argc, char **argv)`

初始化协议框架、发送/接收窗口和缓存，进入事件循环。处理 `NETWORK_LAYER_READY`、`PHYSICAL_LAYER_READY`、`FRAME_RECEIVED`、`DATA_TIMEOUT`、`ACK_TIMEOUT`。相比 LOG02，事件框架未重构，主要变化在 ACK timeout 可触发 repeat NAK。

### 7.2 `between(unsigned char a, unsigned char b, unsigned char c)`

判断序号 `b` 是否在循环区间 `[a, c)` 内，用于 ACK 滑窗和接收窗口判断。相比 LOG02 未发现语义变化。

### 7.3 `update_network_layer(void)`

根据 `nbuffered < NR_BUFS` 开启或关闭网络层，防止发送窗口满后继续取包。相比 LOG02 未发现语义变化。

### 7.4 `send_data_frame(unsigned char kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[])`

按 `kind` 构造 DATA/ACK/NAK，设置 `seq` 和 `ack` 字段，手工序列化并发送。DATA 帧启动 DATA timer；ACK 帧停止 ACK timer；DATA 帧只有在 `no_nak` 为真时停止 ACK timer，避免未解除 NAK 状态被过早清掉。

### 7.5 `send_nak(unsigned char frame_expected)`

参数 `frame_expected` 是接收端请求补发的序号。函数发送 `FRAME_NAK`，设置 `no_nak=0`，若 `NAK_REPEAT_INTERVAL>0` 则启动 ACK timer 作为 repeat NAK 节流时钟，否则停止 ACK timer。

### 7.6 `FRAME_RECEIVED` 分支

收到帧后先检查长度和 CRC。坏帧时若 `no_nak` 为真则 `send_nak(frame_expected)`；收到 DATA 时缓存窗口内乱序帧，必要时对缺失帧 NAK；连续到达后交付网络层、滑动接收窗口并恢复 `no_nak=1`；收到 ACK 字段则释放发送窗口。收到 `FRAME_NAK` 时只重传被请求序号，不执行 ACK 滑窗。

### 7.7 `DATA_TIMEOUT`

单帧超时后重传对应序号的 DATA，仍是 SR，不是 GBN。

### 7.8 `ACK_TIMEOUT`

真实代码为：`if (!no_nak && NAK_REPEAT_INTERVAL > 0) send_nak(frame_expected); else send_data_frame(FRAME_ACK, 0, frame_expected, NULL);`。这是本轮高误码优化核心。

## 8. 当前事件驱动流程

| 事件 | 触发条件 | 状态修改 | 发送帧/timer | 与 LOG02 差异 |
|---|---|---|---|---|
| `NETWORK_LAYER_READY` | 网络层有包且窗口未满 | 写入 `out_buf`，`nbuffered++`，发送序号推进 | 发送 DATA，启动 DATA timer | 基本不变 |
| `PHYSICAL_LAYER_READY` | 物理层可继续发送 | `phl_ready=1` | 无直接帧 | 基本不变 |
| `FRAME_RECEIVED` | 收到物理层帧 | CRC/类型/序号驱动 ACK、NAK、缓存和交付 | 可能 DATA/ACK/NAK，可能启动 ACK timer | NAK 状态与 repeat NAK 关联增强 |
| `DATA_TIMEOUT` | DATA timer 到期 | 不回退窗口 | 单帧 DATA 重传 | 基本不变 |
| `ACK_TIMEOUT` | ACK timer 到期 | 根据 `no_nak` 判断 | ACK 或 repeat NAK | 新增 repeat NAK 分支 |

## 9. 当前测试记录总表

| 序号 | 测试场景 | 平台 | 运行时间 | A 利用率 | B 利用率 | Err/BER | Quit | Fatal scan | 日志/summary | 是否当前源码 | 结论 |
|---:|---|---|---:|---:|---:|---|---|---|---|---|---|
| 1 | 无误码普通 | Windows | 1200s | 51.70% | 96.97% | 0/0 | True | 0 | `performance-tests-20260511-212030/accepted-summary.csv` | 否，旧 exe | 接近参考，但需当前源码复测 |
| 2 | 默认误码普通 | Windows | 1200s | 49.52% | 93.40% | Err 49/96 | True | 0 | 同上 | 否，旧 exe | 低于参考，需复测 |
| 3 | 无误码 flood | Windows | 1200s | 96.97% | 96.96% | 0/0 | True | 0 | 同上 | 否，旧 exe | 达到参考约 97% |
| 4 | 默认误码 flood | Windows | 1200s | 93.20% | 92.80% | Err 97/95 | True | 0 | 同上 | 否，旧 exe | 距参考 95% 约 1-2 点 |
| 5 | 高误码 flood | Windows | 1200s | 55.02% | 51.73% | Err 821/817, BER `1e-4` | True | 0 | 同上 | 否，旧 exe | 旧高误码证据 |
| 6 | LOG02 default flood | Linux/WSL | 1200s | 93.24% | 92.58% | 默认误码 | yes | 0 | `final-validation-20260512-095621/summary.csv` | 是，当时源码；非 repeat800 | LOG02 稳定版对照 |
| 7 | LOG02 high flood | Linux/WSL | 1200s | 55.52% | 55.46% | `1e-4` | yes | 0 | 同上 | 是，当时源码；非 repeat800 | repeat800 对比基线 |
| 8 | repeat800 smoke default flood | Linux/WSL | 60s | 94.83% | 88.10% | 默认误码 | yes | 0 | `nak-repeat-smoke-20260512-135005/A.log,B.log` | 是，当前 repeat800 | 无 summary.csv，数值来自日志尾部 |
| 9 | baseline default/high 小矩阵 | Linux/WSL | 300s | 94.06% / 53.07% | 91.59% / 55.94% | 默认/`1e-4` | yes | 0 | `nak-repeat-clean-matrix-20260512-135157/summary.csv` | 是，矩阵编译产物 | 对照 |
| 10 | repeat600 default/high | Linux/WSL | 300s | 92.50% / 53.46% | 91.44% / 55.87% | 默认/`1e-4` | yes | 0 | 同上 | 是 | 默认误码下降，不选 |
| 11 | repeat800 default/high | Linux/WSL | 300s | 94.23% / 57.55% | 93.45% / 60.31% | 默认/`1e-4` | yes | 0 | 同上 | 是 | 当前候选 |
| 12 | repeat1000 default/high | Linux/WSL | 300s | 93.17% / 57.11% | 91.88% / 59.07% | 默认/`1e-4` | yes | 0 | 同上 | 是 | 高误码提升但默认弱于 repeat800 |
| 13 | 1200s high 首次终测 | Linux/WSL | 无效 | 无效 | 无效 | 端口错误 | no | A fatal 3 | `nak-repeat-final-1200-20260512-143617/summary.csv` | 否 | 随机端口 65696 溢出为 TCP port 160，环境参数错误 |
| 14 | 1200s high 合法端口复测 | Linux/WSL | 1200s | 58.14% | 58.44% | Err 935/940, BER `1e-4` | yes | 0 | `nak-repeat-final-1200-20260512-143701/summary.csv` | 是，当前 repeat800 | 相比 LOG02 约 +2.6 到 +3 点，达到合入门槛 |

说明：直接 `rg FATAL` 扫 `nak-repeat-clean-matrix-*` 会命中复制源码 `protocol.c` 中的字符串常量，不等价于运行 fatal；运行结果以 summary 的 `FatalMatches=0` 和日志尾部 Quit 为准。Windows `accepted-summary.csv` 的 `Log` 字段保留搬迁前 `C:\Users\28641\Desktop\计网\实验\...` 绝对路径；当前仓库中的 CSV/日志实际位于 `C:\Users\28641\Desktop\Experiment-1\...`。

## 10. 性能结论

| 场景 | 最好证据 | 当前结论 | 是否还要复测 |
|---|---|---|---|
| 无误码普通 | 旧 Windows 五组：A 51.70%，B 96.97% | 接近参考 A 53.9%、B 97.0% | 是，当前源码 Windows 重编译后复测 |
| 默认误码普通 | 旧 Windows 五组：A 49.52%，B 93.40% | 距参考 A 52.9%、B 95.1% 有差距 | 是 |
| 无误码 flood | 旧 Windows 五组：A/B 约 96.97% | 达到参考约 97% | 是，当前源码复测 |
| 默认误码 flood | 旧 Windows 93.20/92.80；Linux repeat800 300s 94.23/93.45 | 仍可能低于参考 95% | 是，建议 1200s 当前源码复测 |
| 高误码 `1e-4` flood | Linux repeat800 1200s：58.14/58.44 | 比 LOG02 约 55.5% 提升到约 58%，A/B 更均衡 | 可作为当前 Linux 高误码证据，但 Windows 需复测 |

高误码与参考图 A 42.0%、B 73.6% 的形态不同：当前实现没有复现 B 端很高的非对称表现，但 A/B 平均约 58.3%，接近参考平均约 57.8%。报告和答辩中应说明当前实现更均衡。

## 11. 当前已满足的实验要求

| 实验要求 | 当前是否满足 | 证据/备注 |
|---|---|---|
| 有噪音信道下无差错双工通信 | 基本满足 | 多组 default/high 误码 Quit 正常、fatal 0；当前 Windows 源码仍待复测 |
| 滑动窗口机制 | 满足 | `MAX_SEQ=15`; `NR_BUFS=8`; SR 发送/接收窗口 |
| 流量控制 | 满足 | `update_network_layer`; `PHL_QUEUE_LIMIT` |
| 差错控制 | 满足 | CRC、ACK、NAK、DATA timeout、单帧重传 |
| CRC | 满足 | CRC32 |
| 定时器管理 | 满足 | DATA timer、ACK timer、repeat NAK timer 复用 ACK timer |
| 缓存管理 | 满足 | `out_buf`; `in_buf`; `arrived` |
| 协议跟踪调试 | 满足 | `lprintf` 和大量日志 |
| Piggyback ACK | 满足 | DATA 帧 ACK 字段 |
| ACK timer | 满足 | `ACK_TIMEOUT` |
| NAK | 满足 | `send_nak`; repeat NAK |
| 15/20 分钟稳定运行 | 部分满足 | Linux high 当前源码 1200s；旧 Windows 五组 1200s；当前源码五组仍待补齐 |
| 性能记录 | 部分满足 | 有 summary，但需筛选当前源码/旧 exe |

## 12. 当前风险点与待办事项

| 优先级 | 风险 | 影响 | 建议动作 |
|---|---|---|---|
| P0 | Windows exe 早于当前源码 | Windows 五组不能证明 repeat800 | VS2019 重新编译并跑五组 20 分钟 |
| P0 | Git 工作区混有源码、脚本、日志、归档和删除 | 提交极易夹带大文件 | 提交前人工筛选 |
| P1 | Linux 当前源码五组 20 分钟未补齐 | 报告/验收证据不完整 | 补跑 Linux 五组 |
| P1 | 默认误码 flood 距参考约 1-2 点 | 性能表可能不够漂亮 | 先复测 1200s，再考虑微调 |
| P1 | 高误码 B 端不像参考那样高 | 答辩需解释 | 强调 A/B 平均接近且更均衡 |
| P1 | `verify-nak-repeat-matrix.sh` 是历史 patch 脚本 | 直接复用可能失败或跑非最终实现 | 使用前清理或新写脚本 |
| P2 | 大量日志和复制源码目录 | 仓库膨胀 | 加 `.gitignore` 或只提交 summary |
| P2 | 根目录旧 LOG 删除/迁移未确认 | 文档布局可能混乱 | 确认统一放 `DEV_LOG/` |

## 13. 给下一轮 Codex 的接手建议

1. 先读 LOG03，再执行 `git status --short`、`git diff --stat`，确认状态未漂移。
2. 验收优先级：先重新编译 Windows 当前源码，再跑五组 20 分钟。
3. 报告优先级：从 `accepted-summary.csv`、`final-validation-*`、`nak-repeat-clean-matrix-*`、`nak-repeat-final-1200-*` 提取表格，并标注当前源码/旧 exe。
4. 继续优化时不要先改窗口；优先复测默认误码 flood，或做 repeat NAK/ACK timer 小矩阵。
5. 修改 Windows `datalink.c` 后必须同步 Linux `datalink.c` 并重新算 SHA256。
6. 长测后保存 summary、命令、端口、运行时长、fatal scan 和日志尾部 Quit。
7. 大日志、编译产物和复制工程不要默认提交。

## 14. 建议提交/归档清单

| 类型 | 建议 |
|---|---|
| 应提交源码 | 两份 `datalink.c`；必要头文件/兼容修改需复核 staged diff |
| 应提交脚本 | 最终可复现脚本；历史失败脚本需标注或不提交 |
| 应提交记录 | `DEV_LOG/DEVELOPMENT_LOG00_RECORD.md` 到 `DEVELOPMENT_LOG03_RECORD.md`; `HIGH_BER_OPTIMIZATION_NOTES.md` |
| 可提交 summary | `accepted-summary.csv`; `final-validation-*/summary.csv`; `nak-repeat-clean-matrix-*/summary.csv`; `nak-repeat-final-1200-*/summary.csv` |
| 不建议提交 | 大型 `.log`、`.stdout`、`.stderr`、`*.o`、Linux `datalink`、复制工程目录 |
| 需人工确认 | `.rar` 删除/新增；根目录旧 LOG 迁移；所有 staged 大日志 |

## 15. LOG03 结论摘要

当前协议实现是 Selective Repeat，窗口参数为 `MAX_SEQ=15`、`NR_BUFS=8`，稳定参数为 `DATA_TIMER=2300`、`ACK_TIMER=300`，本轮新增并推荐保留 `NAK_REPEAT_INTERVAL=800`。Windows/Linux 两份 `datalink.c` 当前 SHA256 完全一致。

Linux 当前源码已有 repeat800 60s 冒烟、300s 小矩阵和 1200s 高误码复测证据；高误码 flood 从 LOG02 约 55.5% 提升到约 58.1/58.4%。Windows 五组结果目前对应旧 exe，不能证明当前源码。

下一步最推荐动作：重新编译 Windows 当前源码并跑五组 20 分钟；补齐 Linux 当前源码五组长测；整理 Git 工作区，避免把大量日志和编译产物混入最终提交。

## 16. 需要人工确认的点

- 是否确认旧 LOG 统一迁入 `Lab1-2024(Win+Linux)/DEV_LOG/`，并接受根目录旧 LOG 删除。
- 是否保留或删除 `Lab1-2024(Win+Linux)_normal.rar` 与旧 `.rar` 的删除状态。
- 是否把 repeat800 作为当前最终协议版本进入验收，还是仅作为候选保留。
- 是否需要提交完整长日志；若老师不要求，建议只提交 summary 和少量关键日志摘证。
- Windows VS2019 当前源码重新编译后，五组结果是否与 Linux repeat800 结论一致。

## 17. LOG03 二次严格核查记录

本节为 2026-05-12 对 LOG03 初稿再次核查后的补充，目的是降低上下文压缩导致的细节漂移风险。

| 核查项 | 复核结果 | 对 LOG03 的处理 |
|---|---|---|
| 当前工作目录 | `Get-Location` 为 `C:\Users\28641\Desktop\Experiment-1` | 原记录正确 |
| 最近提交 | `git log --oneline -5` 为 `eb3ec94 Update generated outputs`、`82bacc6 Initial commit` | 原记录正确 |
| Git 状态 | `DEV_LOG/` 仍为未跟踪目录；两份 `datalink.c` 为 `MM`；大量日志/脚本处于 staged 或 untracked | 原记录正确，仍需提交前人工筛选 |
| 两份 `datalink.c` SHA256 | Windows/Linux 均为 `627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43` | 原记录正确 |
| 当前宏参数 | `MAX_SEQ=15`、`NR_BUFS=8`、`DATA_TIMER=2300`、`ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800` 等均与 LOG03 一致 | 原记录正确 |
| `send_data_frame` 签名 | 真实签名为 `send_data_frame(unsigned char kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[])` | 已在第 7.4 节使用真实签名 |
| `send_nak` 签名 | 真实签名为 `send_nak(unsigned char frame_expected)` | 已在第 7.5 节使用真实签名 |
| ACK timeout repeat NAK | 真实代码为 `if (!no_nak && NAK_REPEAT_INTERVAL > 0) send_nak(frame_expected); else send_data_frame(FRAME_ACK, ...)` | 行为描述正确 |
| Windows exe 时间 | `datalink.exe` 为 2026/5/11 19:48:04，`datalink.c` 为 2026/5/12 14:35:46 | “Windows 旧 exe 不能证明当前源码”结论正确 |
| Windows accepted-summary 路径 | CSV 内部 `Log` 字段仍写搬迁前 `C:\Users\28641\Desktop\计网\实验\...` | 已在第 9 节说明这是旧绝对路径 |
| 60s smoke | 目录无 summary.csv；A/B 利用率来自 `A.log`/`B.log` 尾部，均 Quit | 已在第 9 节说明来源 |
| 1200s 失败终测 | `A.log` 显示 TCP port 160、bind fatal；原因来自非法端口 65696 溢出到 16-bit 端口 | “环境参数错误，不是协议结果”结论正确 |
| 历史 `verify-nak-repeat-matrix.sh` | 脚本确有 `last_nak_ms` patch 逻辑；`nak-repeat-matrix-20260512-121903` 的 repeat600/repeat800 构建失败含 `tlast_nak_ms` | “不要直接复用该脚本”结论正确 |

二次核查没有发现需要推翻 LOG03 主要结论的问题；修订集中在函数签名、测试证据来源和搬迁前绝对路径说明三类细节。
