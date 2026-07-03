# 数据链路层滑动窗口协议最终综合接手日志 LOG_SUM

## 0. LOG_SUM 生成背景

本文件是计算机网络实验一“数据链路层滑动窗口协议设计与实现”的最终综合接手日志。它不是实验报告正文，而是给队友和后续 AI/Codex 继续接手用的工程档案。它综合了 `DEVELOPMENT_LOG00_RECORD.md` 到 `DEVELOPMENT_LOG04_RECORD.md`、`HIGH_BER_OPTIMIZATION_NOTES.md`、当前源码、Git 状态、测试 summary、报告材料和归档状态。

旧日志只作为历史上下文。若旧日志与当前源码、当前 Git 状态、当前测试证据冲突，以当前仓库实证、LOG04 和本 LOG_SUM 为准。LOG00 的早期路径和 `DATA_TIMER=2500` 已过时；LOG01 是中间状态；LOG02 已把 `DATA_TIMER=2300` 落地但当时没有 repeat NAK；LOG03 引入 `NAK_REPEAT_INTERVAL=800`；LOG04 已记录 Windows/Linux 当前源码五组 1200s、参数矩阵和报告材料状态。本 LOG_SUM 是后续继续工作的最终入口。

本次生成只进行了只读核查，并最终只写入本文件。已核查：LOG00~LOG04、HIGH_BER、两份 `datalink.c`、Git 状态、Windows/Linux 五组 summary、参数矩阵 summary、候选 `2200/800` summary、性能测试记录表参考数据、空白性能表和报告首页。未修改协议源码、脚本、测试结果、报告、压缩包；未执行 `git add`、`git commit`、`git reset`、`git clean`；未删除、移动或重新打包。

## 1. 当前项目路径与仓库状态

| 项 | 当前实证 |
|---|---|
| 当前根目录 | `C:\Users\28641\Desktop\Experiment-1` |
| 主工程目录 | `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)` |
| Windows 源码路径 | `Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\datalink.c` |
| Linux 源码路径 | `Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-linux\datalink.c` |
| DEV_LOG 目录 | `Lab1-2024(Win+Linux)\DEV_LOG` |
| 报告/表格/测试结果目录 | 主工程根目录、Windows `performance-tests-*`、Linux `*summary.csv` 和测试目录 |
| 最近 git commit | `eb3ec94 Update generated outputs` |
| 当前 git status | 不干净；staged、unstaged、untracked、deleted、renamed 混合 |

Git 状态分类：

| 类别 | 当前状态 | 提醒 |
|---|---|---|
| 协议源码 | 两份 `datalink.c` 为 `MM`；当前工作区哈希一致 | 提交前同时查 `git diff` 和 `git diff --cached` |
| 头文件/框架兼容修改 | `datalink.h`、Linux `lprintf.h` 等有 staged 修改 | `lprintf.h` 是 GCC 兼容修复；框架文件不要继续大改 |
| 脚本 | 多个 `.ps1`、`.sh`、`.py` staged 或 untracked | 分清最终脚本和历史调参脚本 |
| 测试结果 | Windows/Linux 五组、矩阵、候选、历史长测很多 | 建议保留 summary 和必要摘证，不全量提交大日志 |
| 日志文档 | `DEV_LOG/` 为 untracked，根目录旧日志有迁移痕迹 | 建议统一保留 DEV_LOG 下 LOG00~LOG04、HIGH_BER、LOG_SUM |
| 报告材料 | 指导书、验收说明、报告首页、性能表、参考数据表存在 | 正文和性能表仍需按最终 summary 定稿 |
| 编译产物 | `datalink.exe`、Linux `datalink`、`.o`、VS Release 目录存在 | 除非课程要求，一般不提交 |
| 压缩包/归档目录 | 根目录有 `Lab1-2024(Win+Linux)_normal.rar`，旧 rar 为删除状态；Linux 有 archive/candidate/param/matrix 目录 | 不能直接全量提交或全量打包 |

当前工作区较复杂，不能直接 `git add .` 或全量压缩。必须筛选源码、必要工程文件、报告、性能表、关键 summary 和必要脚本，排除 `.git`、大量日志、临时矩阵目录、`.o`、旧压缩包和无效测试目录。

## 2. 前五份 LOG 的时间线整合

| 记录名 | 当时背景 | 当时主要结论 | 现在是否仍成立 | LOG_SUM 最终修正 |
|---|---|---|---|---|
| LOG00 | 早期接手，旧路径在 `计网\实验` | SR 主线，`DATA_TIMER=2500`，五组/高误码待补 | 部分成立 | 实现类型成立；路径、参数、测试状态过时 |
| LOG01 | Windows 旧 exe 五组、Linux 初测、参数候选 | `DATA_TIMER=2300` 只是候选 | 部分过时 | 中间状态，不代表最终状态 |
| LOG02 | `DATA_TIMER=2300` 已落地 | `2300/300/Queue2` 主线，但无 repeat NAK | 参数部分成立 | 当前新增 `NAK_REPEAT_INTERVAL=800` |
| HIGH_BER | 高误码瓶颈分析 | NAK 损坏后等待 DATA timeout，建议 repeat NAK | 成立 | repeat NAK 已落地 |
| LOG03 | repeat NAK 引入 | `2300/800`，Windows 当前源码未复测，Linux 五组未补齐 | 核心参数成立 | LOG04 已补双平台五组 1200s |
| LOG04 | 当前源码双平台复测 | `2300/800` 为当前最终源码；双平台五组完整；`2200/800` 是候选 | 当前最可信 | 本 LOG_SUM 以 LOG04 和当前核查为准 |

## 3. 当前最终协议实现概览

当前实现是 **Selective Repeat ARQ with Piggybacking**，不是 Go-Back-N。接收端有 `in_buf[]` 和 `arrived[]` 乱序缓存，窗口内乱序帧可以先缓存；收到 NAK 或 DATA timeout 时只重传单帧，不回退整个窗口。

| 功能 | 当前状态 | 相关文件/函数 | 是否建议继续修改 | 风险备注 |
|---|---|---|---|---|
| 全双工 A/B 通信 | 已实现，双平台五组 1200s | `main()` | 不建议大改 | 普通模式 A/B 不对称属测试模型现象 |
| CRC32 校验 | 已实现 | `send_data_frame()`、`FRAME_RECEIVED` | 不建议 | 不要重写框架 CRC |
| 手工帧序列化 | 已实现 | `send_data_frame()` | 不建议 | 不直接发送结构体内存 |
| DATA / ACK / NAK | 已实现 | `FRAME_DATA/ACK/NAK` | 不建议 | NAK 的 `ack` 字段语义特殊 |
| Piggyback ACK | 已实现 | DATA 帧 `ack` 字段 | 不建议 | ACK 为最后一个按序收到的序号 |
| ACK timer | 已实现 | `ACK_TIMEOUT` | 谨慎 | 同时承担 delayed ACK 和 repeat NAK 节奏 |
| repeat NAK | 已实现 | `send_nak()`、`NAK_REPEAT_INTERVAL` | 可小矩阵微调 | 依赖 `no_nak` 状态机 |
| DATA timer | 已实现 | `DATA_TIMER=2300` | 保持主线 | `2200/800` 是候选，需双平台重测 |
| 单帧重传 | 已实现 | `DATA_TIMEOUT`、收到 NAK | 不建议改成整窗重传 | SR 区别于 GBN 的关键 |
| 发送窗口 | 已实现 | `ack_expected`、`next_frame_to_send`、`nbuffered` | 不建议 | 窗口大小必须不超过序号空间一半 |
| 接收窗口 | 已实现 | `frame_expected`、`too_far` | 不建议 | 依赖 `between()` |
| 乱序缓存 | 已实现 | `in_buf[]`、`arrived[]` | 不建议 | 报告中应重点说明 |
| 序号回绕 | 已实现 | `inc()`、`between()` | 不建议 | 普通整数比较会出错 |
| 网络层 enable/disable | 已实现 | `update_network_layer()` | 不建议 | 窗口满或物理队列高时关闭 |
| 物理层发送队列节流 | 已实现 | `phl_sq_len()`、`PHL_QUEUE_LIMIT` | 保持 | 防止 ACK/NAK 被 DATA 压住 |
| Windows 编译运行 | 当前源码已重编译并五组 1200s | `datalink.exe` 时间戳 2026-05-12 22:16:53 | 交付前可抽查 | exe 是否提交看课程要求 |
| Linux 编译运行 | 当前源码已五组 1200s | `linux-current-five-1200-summary.csv` | 交付前可抽查 | Linux 可执行和 `.o` 不建议提交 |
| 当前测试覆盖 | Windows/Linux 五组、矩阵、高误码、候选均有 summary | 多个 summary | 不需大规模补测，除非改参数 | 改参数后必须重测 |

## 4. 当前最终参数与宏配置

两份 `datalink.c` 当前宏：

| 宏 | 当前值 | 含义 |
|---|---|---|
| `MAX_SEQ` | `15` | 序号空间 0..15 |
| `NR_BUFS` | `((MAX_SEQ + 1) / 2)`，即 8 | SR 窗口大小，为序号空间一半 |
| `DATA_TIMER` | `2300` | DATA 单帧重传 timer，ms |
| `ACK_TIMER` | `300` | delayed ACK timer，ms |
| `NAK_REPEAT_INTERVAL` | `800` | NAK pending 时 repeat NAK 间隔，ms |
| `FRAME_HEAD_LEN` | `3` | `kind/seq/ack` 三字节 |
| `FRAME_CRC_LEN` | `4` | CRC32 四字节 |
| `FRAME_DATA_LEN` | `(FRAME_HEAD_LEN + PKT_LEN)`，即 259 | DATA 不含 CRC 长度 |
| `FRAME_MAX_LEN` | `(FRAME_DATA_LEN + FRAME_CRC_LEN)`，即 263 | DATA 最大物理帧长 |
| `PHL_QUEUE_LIMIT` | `(FRAME_DATA_LEN * 2)` | 物理层发送队列阈值 |

Windows/Linux 两份 `datalink.c` SHA256 完全一致：

```text
627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43
```

结论：当前工作区版本完全同步。注意：Git 状态中两份文件是 `MM`，提交前仍需确认最终提交的是当前工作区版本。

## 5. 当前核心代码逻辑说明

### 5.1 `struct FRAME` 和 wire format

`struct FRAME` 包含 `kind`、`seq`、`ack`、`data[PKT_LEN]` 和 `padding`。发送时不直接发送结构体内存，而是手工写入 `frame[0]=kind`、`frame[1]=seq`、`frame[2]=ack`，DATA 帧再复制 256 字节 payload，最后追加 4 字节 CRC。这样避免 Windows/Linux 结构体 padding 和对齐差异。

### 5.2 `send_data_frame()`

该函数统一发送 DATA、ACK、NAK。普通 ACK 字段为 `(frame_expected + MAX_SEQ) % (MAX_SEQ + 1)`，表示最后一个按序收到的帧。NAK 帧特殊设置 `ack_nr=frame_expected`，表示请求重传该缺失序号。DATA 帧发送后启动 `DATA_TIMER`。ACK 会停止 ACK timer；DATA 仅在没有 NAK pending 时停止 ACK timer，避免打断 repeat NAK。

### 5.3 `send_nak()`

`send_nak(frame_expected)` 发送 NAK，请求对端重传当前缺失序号。发送后置 `no_nak=0`，并用 `NAK_REPEAT_INTERVAL=800` 启动 ACK timer；若缺口没有被填补，ACK timeout 会再次发送 NAK。

### 5.4 `between()`

`between(a,b,c)` 判断环形序号 `b` 是否位于 `[a,c)`。它用于 ACK 滑窗、接收窗口判断、NAK/timeout 合法性检查。序号回绕时不能用普通整数比较。

### 5.5 `update_network_layer()`

当 `nbuffered < NR_BUFS` 且 `phl_sq_len() < PHL_QUEUE_LIMIT` 时启用网络层，否则关闭网络层。它同时控制发送窗口容量和物理层队列长度，防止 flood 下 DATA 堆积导致 ACK/NAK 被压住。

### 5.6 `NETWORK_LAYER_READY`

网络层有新包时，将分组写入 `out_buf[next_frame_to_send % NR_BUFS]`，`nbuffered++`，发送 DATA，并推进 `next_frame_to_send`。事件结束后用 `update_network_layer()` 决定是否继续启用网络层。

### 5.7 `FRAME_RECEIVED`

收到帧后先检查长度和 CRC。CRC 错误时，如果 `no_nak` 为真，则 NAK 当前 `frame_expected`。CRC 正确时解析 `kind/seq/ack`。非 NAK 帧按累计 ACK 滑动发送窗口；NAK 帧只解释为请求重传某序号，不参与累计 ACK；DATA 帧在接收窗口内则缓存，若乱序则 NAK 缺失帧，连续到达后批量 `put_packet()`。

### 5.8 `DATA_TIMEOUT`

某个 DATA timer 超时后，只检查该序号是否仍在发送窗口内；若在，只重传该单帧，不重传整个窗口。

### 5.9 `ACK_TIMEOUT`

若 `!no_nak && NAK_REPEAT_INTERVAL > 0`，说明存在 NAK pending，则重复发送 NAK；否则发送普通 ACK。ACK timer 因此有 delayed ACK 和 repeat NAK 两种用途。

### 5.10 ACK/NAK 语义区别

DATA/ACK 帧的 `ack` 字段是累计 ACK，表示最后一个按序收到的帧。NAK 帧的 `ack` 字段是请求重传的缺失序号。当前代码只在 `f.kind != FRAME_NAK` 时执行累计 ACK 滑窗，这是关键正确性点。

### 5.11 `no_nak` 状态机

`no_nak=1` 表示当前没有未解决缺口，可以发送新的 NAK 或普通 delayed ACK。CRC 错误或乱序缺帧触发 `send_nak()` 后，`no_nak=0`。当缺失帧到达并连续交付后，`no_nak=1`。repeat NAK 依赖这个状态判断是否继续重发。

### 5.12 repeat NAK 如何依赖 ACK timer

发送 NAK 后调用 `start_ack_timer(NAK_REPEAT_INTERVAL)`。ACK timeout 到期时如果缺口仍未解决，就再次 NAK 并重新启动 timer。这样避免 NAK 风暴，又降低高误码下唯一 NAK 损坏后等待 DATA timeout 的概率。

### 5.13 为什么 NAK 帧的 ack 字段不能作为累计 ACK 处理

NAK 的 `ack` 字段表示缺失序号，不是“已收到到哪里”。如果把它当累计 ACK，发送窗口可能错误滑动，提前停止未确认帧 timer，造成数据丢失。

### 5.14 为什么当前是 SR，不是 GBN

当前接收端缓存乱序帧，只按序连续交付；发送端收到 NAK 或 DATA timeout 只重传目标帧。GBN 通常丢弃乱序帧并整窗回退重传，因此当前实现是 SR。

## 6. LOG03 / LOG04 后的最终有效测试证据

### 6.1 Windows 当前源码五组 1200s

目录：`Lab1-Windows-VS2019\performance-tests-20260512-221730`

summary：`performance-tests-20260512-221730\summary.csv`

可执行文件：`Lab1-Windows-VS2019\datalink.exe`，时间戳 `2026-05-12 22:16:53`。LOG04 记录该 exe 对应当前源码重编译后复测。

| 场景 | 命令选项 | 运行时间 | A 利用率 | B 利用率 | Quit | Fatal scan |
|---|---|---:|---:|---:|---|---:|
| 普通无误码 | `-t 1200 -u` | 1200s | 51.65% | 96.96% | True/True | 0 |
| 普通默认误码 | `-t 1200` | 1200s | 49.43% | 91.89% | True/True | 0 |
| flood 无误码 | `-t 1200 -f -u` | 1200s | 96.96% | 96.97% | True/True | 0 |
| flood 默认误码 | `-t 1200 -f` | 1200s | 92.90% | 92.77% | True/True | 0 |
| flood 高误码 | `-t 1200 -f --ber=1e-4` | 1200s | 58.19% | 57.70% | True/True | 0 |

### 6.2 Linux 当前源码五组 1200s

summary：`Lab1-linux\linux-current-five-1200-summary.csv`

| 场景 | 命令选项 | 运行时间 | A 利用率 | B 利用率 | Quit | Fatal scan |
|---|---|---:|---:|---:|---|---:|
| 普通无误码 | `./datalink -t 1200 -u` | 1200s | 51.70% | 96.97% | yes/yes | 0 |
| 普通默认误码 | `./datalink -t 1200` | 1200s | 49.74% | 92.14% | yes/yes | 0 |
| flood 无误码 | `./datalink -t 1200 -f -u` | 1200s | 96.97% | 96.97% | yes/yes | 0 |
| flood 默认误码 | `./datalink -t 1200 -f` | 1200s | 93.05% | 92.80% | yes/yes | 0 |
| flood 高误码 | `./datalink -t 1200 -f --ber=1e-4` | 1200s | 59.35% | 58.41% | yes/yes | 0 |

### 6.3 高误码 `--ber=1e-4` flood 长测

| 平台 | 证据 | A 利用率 | B 利用率 | 稳定跑完 | 异常 |
|---|---|---:|---:|---|---|
| Windows 当前源码 | `performance-tests-20260512-221730\summary.csv` | 58.19% | 57.70% | 是，1200s Quit | FatalMatches 0 |
| Linux 当前源码 | `linux-current-five-1200-summary.csv` | 59.35% | 58.41% | 是，1200s Quit | FatalMatches 0 |
| Linux 候选 `2200/800` | `candidate-dt2200-nri800-valid-1200-summary.csv` | 59.04% | 59.36% | 是，1200s Quit | FatalMatches 0；不是当前源码 |

当前高误码性能大致在 58% 到 59% 区间，能稳定跑完 1200s。当前有效 summary 未显示 bad packet、overflow、System too busy、FATAL。

### 6.4 参数矩阵

当前最终源码仍为 `DATA_TIMER=2300`、`NAK_REPEAT_INTERVAL=800`。`2200/800` 曾作为候选出现，并在 Linux 300s 小矩阵和 1200s 候选验证中显示高误码略有潜力。但不建议立即改成 `2200/800`，因为当前 `2300/800` 已有 Windows/Linux 当前源码五组 1200s 完整证据；`2200/800` 只有 Linux 候选证据，缺 Windows 同步、重编译、五组复测和报告同步。

`protocol-matrix-20260513-000052\summary.csv` 关键结果：

| Variant | 默认 A/B | 高误码 A/B | 结论 |
|---|---|---|---|
| `dt2200_nri800` | 93.98% / 92.79% | 60.09% / 59.66% | 小矩阵最好之一，但不是当前源码 |
| `dt2300_nri800` | 93.80% / 93.03% | 58.72% / 59.62% | 当前源码主线，默认误码更均衡 |
| `dt2400_nri800` | 93.95% / 92.49% | 58.97% / 57.87% | 不优先 |
| `dt2300_nri700` | 93.66% / 93.39% | 59.23% / 55.29% | 高误码 B 端低，不优先 |
| `dt2300_nri900` | 93.67% / 92.15% | 57.80% / 59.49% | 不优先 |
| `dt2300_nri1000` | 92.87% / 92.39% | 58.85% / 60.12% | 默认误码较弱，不优先 |

若队友决定继续尝试 `2200/800`，必须同步修改 Windows/Linux 两份 `datalink.c`，重新编译，至少跑 Windows/Linux 五组 1200s，并更新报告、性能表、LOG 和最终打包清单。

### 6.5 与参考性能表的差距

已只读提取 `性能测试记录表-参考数据.docx`，参考数据包括：

| 场景 | GoBackN A/B | Selective A/B | 当前 Windows A/B | 当前 Linux A/B | 说明 |
|---|---|---|---|---|---|
| 无误码普通 | 51.6 / 97.0 | 53.9 / 97.0 | 51.65 / 96.96 | 51.70 / 96.97 | A 端低于 Selective 参考，B 端接近 97 |
| 默认误码普通 | 47.7 / 86.9 | 52.9 / 95.1 | 49.43 / 91.89 | 49.74 / 92.14 | 介于 GBN 和 Selective 参考之间 |
| 无误码 flood | 97.0 / 97.0 | 97.0 / 97.0 | 96.96 / 96.97 | 96.97 / 96.97 | 基本达到参考 |
| 默认误码 flood | 88.1 / 87.7 | 95.0 / 95.1 | 92.90 / 92.77 | 93.05 / 92.80 | 明显优于 GBN，低于 Selective 参考约 2 点 |
| 高误码 flood | 23.1 / 46.8 | 42.0 / 73.6 | 58.19 / 57.70 | 59.35 / 58.41 | A/B 更均衡；平均接近或高于参考均值 |

没有证据的地方不要虚构。若改参数，当前表格数据全部需要重新确认。

## 7. 当前已完成事项

- SR 协议主实现完成。
- 双平台 `datalink.c` 当前工作区完全同步。
- Linux `lprintf.h` 兼容修复完成。
- `DATA_TIMER` 从 2500 收敛到 2300。
- repeat NAK 方案落地，`NAK_REPEAT_INTERVAL=800`。
- `PHL_QUEUE_LIMIT` 保持 `FRAME_DATA_LEN*2`。
- Windows 当前源码已重编译并完成五组 1200s 复测。
- Linux 当前源码已完成五组 1200s 长测。
- 高误码 `--ber=1e-4` 长测已完成，双平台当前源码均稳定跑完。
- 参数矩阵已完成，`2200/800` 候选有记录但未落地。
- 报告材料和表格材料已存在，但仍需按最终数据定稿。
- DEV_LOG 已累计 LOG00~LOG04、HIGH_BER，并新增本 LOG_SUM。

## 8. 当前未完成 / 仍需队友继续做的事项

### P0：交付前必须做

- 重新确认 Windows/Linux `datalink.c` SHA256 一致。
- 重新确认最终源码参数仍为 `2300/300/800`。
- 重新跑或至少抽查 Windows/Linux 五组 summary。
- 清理 Git 状态。
- 筛选提交/打包文件。
- 不要把所有巨大 log、临时目录、`.o`、旧压缩包全部提交。
- 确认最终压缩包结构能直接打开工程并编译运行。

### P1：报告和表格必须补

- 填写性能测试记录表。
- 补实验报告首页信息。
- 补实验内容、设计思想、核心算法、帧格式、状态变量、事件处理流程。
- 补测试结果表和分析。
- 补参数选择依据。
- 补与 GoBackN / Selective 参考程序对比。
- 补个人分工和心得。
- 补验收问答准备。

### P2：可选性能优化

- 是否尝试 `DATA_TIMER=2200 + NAK_REPEAT_INTERVAL=800`。
- 是否重新验证 `ACK_TIMER` 或 `NAK_REPEAT_INTERVAL`。
- 是否保持 `2300/800` 作为稳妥最终版。
- 任何性能优化必须重新双平台同步、重编译、跑五组、更新报告与表格。

当前建议是不立即改。`2300/800` 已有完整双平台证据，`2200/800` 仍需更充分验证。

### P3：代码可读性和答辩准备

- 可加注释，但不要大改逻辑。
- 准备讲解 SR、ACK、NAK、CRC、timer、缓存、窗口回绕。
- 准备解释为什么不是 GBN。
- 准备解释高误码性能为什么低于默认误码。

## 9. 给队友的 AI/Codex 后续工作提示语

### 9.1 检查最终打包文件

```text
请在 C:\Users\28641\Desktop\Experiment-1 只读检查计算机网络实验一最终打包候选。不要修改源码，不要 git add/commit/reset/clean，不要删除或移动文件。请检查目录结构、Windows/Linux datalink.c SHA256 是否一致、必要源码和工程文件是否齐全、报告/性能表/评分表是否存在、summary 是否能证明当前源码五组 1200s，通过后给出建议打包清单和排除清单。重点排除 .git、大量 log/stdout/stderr、param-sweep 临时目录、.o、旧 exe/旧 rar/重复压缩包、System too busy 无效测试目录。
```

### 9.2 填实验报告初稿

```text
请基于 DEV_LOG/DEVELOPMENT_LOG_SUM_RECORD.md、实验指导书、验收说明、性能测试记录表参考数据、当前 Windows/Linux summary，生成实验报告正文草稿。不要改源码。报告要说明当前实现是 Selective Repeat ARQ with Piggybacking，不是 Go-Back-N；覆盖帧格式、状态变量、ACK/NAK、repeat NAK、DATA timer、ACK timer、CRC32、窗口回绕、缓存、物理层队列节流、参数选择和测试分析。没有证据的数据标注待确认。
```

### 9.3 填性能测试记录表

```text
请只读读取 Windows performance-tests-20260512-221730/summary.csv 和 Linux linux-current-five-1200-summary.csv，提取五组 1200s A/B 利用率、Err、Quit、FatalMatches，并填入性能测试记录表草稿或生成可复制表格。标明每个数据来源路径。不要使用旧 exe 的 accepted-summary，除非用于历史对照。没有证据的项目标注待确认。
```

### 9.4 做验收问答准备

```text
请基于 LOG_SUM 和当前 datalink.c 生成验收问答速查。问题覆盖：为什么选择 SR、为什么不是 GBN、窗口为什么是 8、MAX_SEQ 为什么是 15、帧格式、ACK 字段含义、NAK 字段含义、为什么 NAK 不参与累计 ACK、DATA_TIMER=2300、ACK_TIMER=300、NAK_REPEAT_INTERVAL=800、PHL_QUEUE_LIMIT、CRC32、如何保证无差错按序交付、高误码下利用率下降原因、Windows/Linux 如何同步、System too busy 如何处理。
```

### 9.5 继续性能优化

```text
请在 C:\Users\28641\Desktop\Experiment-1 继续性能优化，但必须先备份/记录当前 2300/800 稳定版本。一次只改一个参数，优先验证 DATA_TIMER=2200 + NAK_REPEAT_INTERVAL=800。必须同步修改 Windows/Linux 两份 datalink.c，确认 SHA256 一致，重新编译，先跑 300s 小矩阵，再跑 Windows/Linux 五组 1200s。不要盲目改窗口、队列、CRC、protocol 框架。改完必须更新报告、性能表和 LOG。
```

## 10. 最终建议的交付/打包清单

建议包含：

- Windows 工程源码和必要工程文件。
- Linux 源码和 Makefile。
- `datalink.c` / `datalink.h` / `protocol.c/h` / `crc32.c` / `lprintf.c/h` / `getopt.c/h` 等必要文件。
- README 或 LOG_SUM。
- 必要测试 summary。
- 实验报告 docx。
- 性能测试记录表 docx。
- 评分表 xlsx 如课程要求。
- 必要脚本，不包含巨大日志。

建议不包含或谨慎包含：

- `.git` 目录。
- 大量中间 log。
- `param-sweep-*`、`protocol-matrix-*`、`archive-artifacts-*` 临时目录，除非只保留 summary。
- `.o` 文件。
- 旧 exe、旧 rar、多份重复压缩包。
- 过时根目录 LOG。
- 临时复制目录。
- `System too busy` 的无效测试目录。

当前根目录只有 `Lab1-2024(Win+Linux)_normal.rar` 一个可见 rar，但本次未解包确认内容，不能证明它是最终版。建议人工确认后重新打最终包。

## 11. 报告填写建议

建议报告结构：

1. 实验目的与环境。
2. 协议类型：Selective Repeat ARQ。
3. 帧格式设计。
4. 主要数据结构。
5. 事件驱动流程。
6. ACK / NAK / Piggyback / Timer 机制。
7. CRC32 检错。
8. 流量控制与物理队列节流。
9. 参数选择与理论分析。
10. 测试方法。
11. 测试结果表。
12. 性能分析。
13. 高误码优化说明。
14. 遇到的问题和解决方法。
15. 团队分工。
16. 总结与心得。
17. 源程序清单或核心代码说明。

报告中应明确：当前实现不是 GBN；默认误码 flood 约 93%，无误码 flood 约 97%，高误码 flood 约 58-59%；`2200/800` 只是候选，除非最终改参并复测，否则不要写成最终源码参数。

## 12. 验收问答速查

| 问题 | 简答 |
|---|---|
| 为什么选择 SR？ | SR 能缓存乱序正确帧，只重传丢失或损坏的单帧，误码信道下比 GBN 更高效。 |
| 窗口为什么是 8？ | `MAX_SEQ=15` 时序号空间 16，SR 窗口不能超过序号空间一半，因此取 8。 |
| `MAX_SEQ` 为什么是 15？ | 使用 0..15 序号空间，配合窗口 8 可处理回绕且避免新旧帧混淆。 |
| ACK 字段含义是什么？ | 在 DATA/ACK 帧中表示最后一个按序收到的帧，是累计 ACK。 |
| NAK 字段含义是什么？ | 在 NAK 帧中 `ack` 字段表示请求重传的缺失序号。 |
| 为什么 NAK 不参与累计 ACK？ | NAK 的 `ack` 字段不是累计确认，若滑动窗口会错误确认未收到的帧。 |
| `DATA_TIMER` 为什么是 2300？ | 由 2500 向下收敛，兼顾默认误码稳定和高误码恢复速度；已有双平台五组证据。 |
| `NAK_REPEAT_INTERVAL` 为什么是 800？ | 高误码下 NAK 可能损坏，800ms repeat NAK 能减少等待 DATA timeout，同时避免 NAK 风暴。 |
| `ACK_TIMER` 为什么是 300？ | 用于 delayed ACK，给 piggyback 留机会，同时不会过久延迟纯 ACK。 |
| `PHL_QUEUE_LIMIT` 为什么是两个数据帧？ | 限制物理层队列，避免 ACK/NAK 被 flood DATA 堵在队列后面。 |
| CRC 怎么生成和验证？ | 发送时对头部和 payload 计算 CRC32 并追加 4 字节；接收时对整帧计算 CRC32，结果非 0 判坏帧。 |
| 如何保证无差错按序交付？ | 坏帧丢弃并 NAK，乱序正确帧缓存，只有从 `frame_expected` 开始连续到达才 `put_packet()`。 |
| 高误码下为什么利用率下降？ | DATA/ACK/NAK 都可能损坏，重传和等待 timer 增多，控制帧也会被误码影响。 |
| Windows 和 Linux 如何保证同步？ | 两份 `datalink.c` 当前 SHA256 一致；改动必须双平台同步并重测。 |
| 如果出现 `System too busy` 怎么处理？ | 视为环境无效结果，不填性能表；换端口/重跑，并保留有效 rerun summary。 |

## 13. 风险警告

- 当前仓库 staged / unstaged / untracked 混合，不能直接 `git add .`。
- 旧日志中的路径和参数可能过时。
- 旧测试结果不一定对应当前源码。
- 只看 exe 结果不够，要确认 exe 时间戳与源码匹配。
- 修改参数后必须更新报告和表格。
- 性能表不能填没有证据的数据。
- 高误码测试偶发波动，单次结果不能代表最终性能。
- 大日志目录不要全部打包给队友，除非课程要求。
- 当前 `2300/800` 是稳妥最终版；`2200/800` 是候选，不要混淆。
- `FATAL` 字符串出现在源码中不等于运行 fatal；以 summary 的 `FatalMatches` 和日志扫描为准。

## 14. 最终结论

当前最终实现为 **Selective Repeat with Piggybacking + CRC32 + ACK timer + NAK + repeat NAK**。当前主线参数为 `DATA_TIMER=2300`、`ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800`、`PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2`。Windows/Linux 两份 `datalink.c` 当前工作区 SHA256 完全一致，应继续保持完全一致。

核心功能和长测证据基本齐全：Windows/Linux 当前源码五组 1200s 均有 summary，Quit 正常，Fatal scan 为 0。后续主要任务不是大改协议，而是清理交付、完善报告、填表、准备验收；如继续优化，必须严格小步验证。

## 15. 本次实际读取过的文件

- `DEV_LOG\DEVELOPMENT_LOG00_RECORD.md`
- `DEV_LOG\DEVELOPMENT_LOG01_RECORD.md`
- `DEV_LOG\DEVELOPMENT_LOG02_RECORD.md`
- `DEV_LOG\DEVELOPMENT_LOG03_RECORD.md`
- `DEV_LOG\DEVELOPMENT_LOG04_RECORD.md`
- `DEV_LOG\HIGH_BER_OPTIMIZATION_NOTES.md`
- `Lab1-Windows-VS2019\datalink.c`
- `Lab1-linux\datalink.c`
- `Lab1-Windows-VS2019\performance-tests-20260512-221730\summary.csv`
- `Lab1-Windows-VS2019\performance-tests-20260512-221730\manifest.txt`
- `Lab1-linux\linux-current-five-1200-summary.csv`
- `Lab1-linux\protocol-matrix-20260513-000052\summary.csv`
- `Lab1-linux\candidate-dt2200-nri800-valid-1200-summary.csv`
- `性能测试记录表-参考数据.docx`
- `性能测试记录表.docx`
- `实验报告首页.docx`

同时执行了只读目录和 Git 核查：`rg --files`、`git status --short`、`git log -1 --oneline`、`git diff --stat`、`git diff --cached --stat`、`Get-FileHash`、`Get-Item`。

## 16. 没有读取到但建议人工确认的文件

- `计算机网络实验一报告.docx` 或最终报告正文：当前清单中未确认有完整最终报告正文，只确认有报告首页、指导书、设计 PDF、性能表。
- `report/` 目录内若存在的图表和 Markdown：本次只通过文件清单层面识别，未逐项读取。
- 最终压缩包 `Lab1-2024(Win+Linux)_normal.rar`：本次未解包确认内容，不能证明它是最终版。
- 所有大型 `.log`、`.stdout`、`.stderr` 尾部：本次优先依据 summary，没有逐个打开全部巨大日志。
- 验收说明 PDF 和实验指导书 PDF 正文：本次确认存在，但未全文 OCR/提取。
- `.gitignore` 和最终提交清单：建议提交前单独核查。
