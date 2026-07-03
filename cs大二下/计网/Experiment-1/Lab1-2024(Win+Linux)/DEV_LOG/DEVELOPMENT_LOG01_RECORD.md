# 数据链路层滑动窗口协议开发记录 LOG01

## 0. LOG01 时点说明

本文件是 `DEVELOPMENT_LOG00_RECORD.md` 之后、`DEVELOPMENT_LOG02_RECORD.md` 之前的一份阶段性工程接手记录。它记录的是 LOG01 生成时点的代码、脚本、测试证据和下一步计划，不是最终状态声明。

若同时阅读 LOG00/LOG01/LOG02，请按时间顺序理解：LOG00 记录早期稳定版本；LOG01 记录完成 Windows 五组测试、Linux 初步验证和参数候选筛选后的中间状态；LOG02 记录后续 `DATA_TIMER=2300` 已落地、300 秒矩阵和 1200 秒 Linux 最终验证后的最新状态。凡 LOG01 中的“当前”“尚未”“下一步”与 LOG02 的后续证据不同，应理解为已在 LOG02 阶段继续推进，并以 LOG02 为最新结论。

## 1. 项目背景与实验目标

本项目是计算机网络实验一：数据链路层滑动窗口协议的设计与实现。当前工作目录为：

```text
C:\Users\28641\Desktop\计网\实验
```

主要工程目录为：

```text
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)
```

实验核心目标：

- 在 8000 bps 全双工卫星信道上实现 A/B 两站点通信
- 单向传播时延 270 ms
- 默认误码率 1e-5
- 网络层分组长度固定为 256 字节，即 `PKT_LEN`
- 实现有噪音信道下的无差错双工通信
- 尽可能提高线路利用率
- 程序需稳定运行，满足验收和性能测试要求

本记录不是实验报告正文，而是给后续 Codex / ChatGPT 继续接手开发用的工程上下文记录。它基于 LOG01 生成时点的代码状态、Git 输出、终端输出、测试日志和前面多轮开发上下文整理；没有证据的地方标注为“待确认”。LOG02 生成后，本文件中的“当前状态”均应视为 LOG01 阶段快照。

## 2. 当前实现状态总览

截至 LOG01 生成时，`datalink.c` 的主要实现是 Selective Repeat 风格的滑动窗口协议，不是单独的 Go-Back-N 实现，也没有同时保留 Go-Back-N 独立版本。Windows 与 Linux 两份 `datalink.c` 当前 SHA256 一致：

```text
7594A138C43D7B7BA153EF4638936C9253D5474BB8B3FB39CAEDD96B475DBFA3
```

LOG01 阶段的协议支持全双工、CRC32、piggyback ACK、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable 流控，以及基于物理层发送队列长度的发送节流。到 LOG01 为止，已经完成五组 Windows 20 分钟验收性能测试；Linux 版已通过 WSL 编译和 60 秒运行短测。LOG02 阶段继续完成了 `DATA_TIMER=2300` 落地、300 秒矩阵和 Linux 1200 秒最终验证。

| 功能 | 当前状态 | 相关文件/函数 | 备注 |
|---|---|---|---|
| Selective Repeat 发送窗口 | 已完成 | `Lab1-Windows-VS2019\datalink.c`, `Lab1-linux\datalink.c`, `between`, `main` | 窗口大小 `NR_BUFS=8`，序号空间 0..15 |
| Go-Back-N 独立实现 | 未实现 | 无 | 当前项目交付代码是 SR，不是 GBN |
| 全双工通信 | 已完成 | `main`, `send_data_frame` | A/B 两端运行同一程序，可同时收发 |
| CRC32 生成 | 已完成 | `send_data_frame`, `crc32` | 手工序列化帧后追加 4 字节 CRC |
| CRC32 校验 | 已完成 | `FRAME_RECEIVED` 分支 | `crc32(frame, len) != 0` 判坏帧 |
| Piggyback ACK | 已完成 | `send_data_frame` | DATA 帧携带累计 ACK |
| ACK timer | 已完成 | `FRAME_RECEIVED`, `ACK_TIMEOUT` | 当前 `ACK_TIMER=300` ms |
| NAK | 已完成 | `FRAME_RECEIVED`, `FRAME_NAK` 处理 | NAK 的 `ack` 字段表示请求重传序号 |
| 乱序缓存 | 已完成 | `in_buf`, `arrived` | 接收窗口内乱序 DATA 会缓存 |
| 序号回绕 | 已完成 | `inc`, `between` | 环形序号比较 |
| 网络层 enable/disable 流控 | 已完成 | `update_network_layer` | 窗口满或物理队列偏高时关闭网络层 |
| 物理层发送队列控制 | 已完成 | `update_network_layer`, `PHL_QUEUE_LIMIT` | 当前阈值为 2 个数据帧长度 |
| Linux 编译 | 已完成 | `Lab1-linux\Makefile`, `lprintf.h` | WSL 下 `make clean && make` 已通过；曾修复 `lprintf.h` 返回类型 |
| Linux 运行 | 待验证/部分完成 | `verify-linux-run.sh` | 已做 60 秒默认误码 flood 短测；20 分钟 Linux 长测待确认 |
| Windows 五组验收测试 | 已完成 | `Run-FivePerformanceTests.ps1`, `accepted-summary.csv` | 最终采用 `accepted-summary.csv`；默认误码普通传输使用 rerun 结果 |
| 高误码长测 | 已完成 | `high-ber-longtest-20260511-234732` | Windows `--ber=1e-4 -f` 20 分钟复测通过 |
| 参数优化最终落地 | 部分完成 | `sweep-linux-params.sh`, `HIGH_BER_OPTIMIZATION_NOTES.md` | LOG01 阶段已筛出候选 `DATA_TIMER=2300`，但当时尚未改入稳定 `datalink.c`；LOG02 阶段已落地 |

LOG01 阶段风险点：

- LOG01 阶段稳定代码仍使用 `DATA_TIMER=2500`，高误码下吞吐约 52% 到 57%，仍有优化空间。LOG02 阶段已将 `DATA_TIMER=2300` 改入 Windows/Linux 两份源码并完成 Linux 复测。
- LOG01 阶段参数扫测显示 `DATA_TIMER=2300` 可能更好，但当时只做了 120 秒筛选，尚未做 300 秒/1200 秒最终确认。LOG02 已补充这些后续验证。
- `no_nak` 策略较简单；高误码下 NAK 若损坏，恢复可能退化到 DATA timeout。
- LOG01 阶段 Linux 只做了 60 秒短测，Linux 20 分钟正式长测待确认；LOG02 已补充 Linux 当前 `DATA_TIMER=2300` 的 1200 秒默认误码 flood 和高误码 flood 验证。
- 工作区当前有大量测试产物和未跟踪文件，继续开发前建议清理或提交必要记录。
- 当前 `PHYSICAL_LAYER_READY` 分支为空，依赖主循环末尾的 `update_network_layer()` 恢复网络层；目前测试未暴露问题，但仍是可观察点。

## 3. 仓库结构与关键文件

LOG01 阶段 Git 状态：目录已经是 Git 仓库，`git log --oneline -5` 显示：

```text
eb3ec94 Update generated outputs
82bacc6 Initial commit
```

注意：早期 `DEVELOPMENT0_RECORD.md` 中曾记录“不是 Git 仓库”；这是当时上下文。当前实际状态已经有 Git 仓库。LOG01 阶段工作区未提交内容较多，`git status --short` 显示包括 Linux `lprintf.h` 修改、测试日志修改/新增、`HIGH_BER_OPTIMIZATION_NOTES.md`、Linux 验证脚本、参数扫测脚本和大量 `.o`/临时目录。

| 文件/目录 | 作用 | 是否建议修改 |
|---|---|---|
| `Lab1-Windows-VS2019\datalink.c` | Windows/VS 工程主协议实现 | 主要维护文件 |
| `Lab1-linux\datalink.c` | Linux 版主协议实现 | 主要维护文件，需与 Windows 同步 |
| `Lab1-Windows-VS2019\datalink.h`, `Lab1-linux\datalink.h` | 帧类型声明，`FRAME_DATA/ACK/NAK` | 谨慎修改，通常无需动 |
| `protocol.h` | 实验框架 API 声明，事件、网络层、物理层、timer、CRC | 实验库文件，不应随意修改 |
| `protocol.c` | 实验模拟框架实现，命令行参数、信道、事件驱动 | 实验库文件，不应随意修改 |
| `crc32.c` | CRC32 实现 | 实验库文件，不应修改 |
| `lprintf.c/.h` | 日志输出支持 | 实验库文件；Linux `lprintf.h` 已做最小兼容修复 |
| `getopt.c/.h` | Windows getopt 兼容 | Windows 实验库文件，不应修改 |
| `Lab1-linux\Makefile` | Linux 编译脚本 | 通常无需修改 |
| `Lab1-Windows-VS2019\datalink.sln`, `.vcxproj` | Visual Studio 工程 | 通常无需修改 |
| `Lab1-Windows-VS2019\datalink.exe` | Windows 可执行文件 | 编译产物 |
| `Run-FivePerformanceTests.ps1` | Windows 五组性能测试脚本 | 可维护脚本 |
| `Lab1-linux\verify-linux-run.sh` | Linux A/B 短测脚本 | 本轮新增 |
| `Lab1-linux\sweep-linux-params.sh` | Linux 参数小矩阵扫测脚本 | 本轮新增 |
| `Lab1-linux\validate-candidates-default.sh` | Linux 候选参数默认误码验证脚本 | 本轮新增 |
| `Lab1-linux\verify-minimal-matrix.sh` | Linux 300 秒最小矩阵验证脚本 | LOG01 阶段已存在，尚未运行确认；LOG02 阶段已有有效运行结果 |
| `HIGH_BER_OPTIMIZATION_NOTES.md` | 高误码长测与参数优化笔记 | 本轮新增/更新 |
| `DEVELOPMENT0_RECORD.md` | 前一版工程上下文记录 | 参考文档 |
| `DEVELOPMENT_LOG01_RECORD.md` | 当前这份接手记录 | 本轮生成 |
| `performance-tests-20260511-212030` | Windows 五组 20 分钟测试日志目录 | 测试证据 |
| `high-ber-longtest-20260511-234732` | Windows 高误码 20 分钟复测日志目录 | 测试证据 |
| `Lab1-linux\linux-run-verify-20260512` | Linux 60 秒短测日志目录 | 测试证据 |
| `Lab1-linux\param-sweep-20260512-002725` | Linux 参数扫测结果目录 | 测试证据 |

实验库提供、不应随意修改的文件主要是 `protocol.c/.h`、`crc32.c`、`lprintf.c`、`getopt.c/.h`。本轮唯一框架相关改动是 Linux `lprintf.h` 的函数返回类型同步，原因是其声明和 `lprintf.c` 实现不一致，导致新版 GCC 编译失败。

## 4. 本轮多轮迭代中做过的主要修改

### 4.1 Selective Repeat 主实现

- 修改文件：`Lab1-Windows-VS2019\datalink.c`, `Lab1-linux\datalink.c`
- 涉及函数：`between`, `update_network_layer`, `send_data_frame`, `main`
- 修改前问题：实验需要滑动窗口协议；早期基础实现无法满足高利用率、误码恢复、全双工稳定运行要求。
- 修改后行为：当前实现为 Selective Repeat with Piggybacking，窗口大小 8，序号空间 16，支持乱序缓存和单帧重传。
- 验证情况：Windows 五组 20 分钟测试通过；Linux 60 秒短测通过。
- 备注：这部分修改已经在当前提交历史中存在，非本轮最后一次未提交 diff 才出现。

### 4.2 帧结构与手工序列化

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED` 分支
- 修改前问题：直接发送结构体容易受 padding、对齐、平台差异影响。
- 修改后行为：发送端使用 `unsigned char frame[FRAME_MAX_LEN]` 手工写入 `kind/seq/ack/payload/crc`；接收端按字节解析到临时 `struct FRAME`。
- 解决的问题：避免 Windows/Linux 结构体布局差异影响 wire format。
- 验证情况：Windows 五组长测、Linux 编译和短测均未暴露帧格式问题。
- 备注：`struct FRAME` 仍保留 `padding` 字段，但实际发送不依赖结构体内存布局。

### 4.3 CRC32 生成与验证

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED`
- 修改前问题：噪音信道下必须检测坏帧。
- 修改后行为：发送时对不含 CRC 的字节流计算 `crc32(frame, len)`，低字节到高字节追加 4 字节；接收时对完整帧 `crc32(frame, len) != 0` 判坏。
- 解决的问题：误码帧不会交付网络层。
- 验证情况：默认误码、高误码测试均有 Err 计数但未出现 bad packet。

### 4.4 发送窗口逻辑

- 修改文件：`datalink.c`
- 涉及函数：`NETWORK_LAYER_READY`, ACK 处理, `between`
- 修改前问题：需要从单帧等待扩展为滑动窗口。
- 修改后行为：`ack_expected` 为发送窗口下界，`next_frame_to_send` 为下一个发送序号，`nbuffered` 记录未确认帧数；收到累计 ACK 后逐帧停止 timer 并滑动窗口。
- 解决的问题：提升链路利用率。
- 验证情况：无误码 flood 20 分钟 A/B 利用率约 96.97% / 96.96%。

### 4.5 接收窗口与乱序缓存

- 修改文件：`datalink.c`
- 涉及函数：`FRAME_RECEIVED`
- 修改前问题：误码下只按序接收会退化，后续正确帧被丢弃。
- 修改后行为：接收窗口 `[frame_expected, too_far)` 内的乱序 DATA 存入 `in_buf`，`arrived[]` 标记；只要窗口下界连续到达就批量 `put_packet`。
- 解决的问题：降低误码环境下重传量。
- 验证情况：默认误码 flood 20 分钟 A/B 利用率约 93.20% / 92.80%；无 bad packet。

### 4.6 ACK、piggyback ACK 与 ACK timer

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED`, `ACK_TIMEOUT`
- 修改前问题：纯 ACK 太频繁会浪费带宽；没有 ACK timer 又可能导致 ACK 无限延迟。
- 修改后行为：DATA 帧携带 `(frame_expected + MAX_SEQ) % (MAX_SEQ + 1)` 作为累计 ACK；收到 DATA 后启动 `ACK_TIMER=300`，若超时则发送纯 ACK。
- 解决的问题：兼顾 piggyback 与确认及时性。
- 验证情况：五组长测均可正常退出。
- 备注：参数扫测中单独 `ACK_TIMER=250` 高误码表现较差，不建议单独采用。

### 4.7 NAK 处理与单帧快速重传

- 修改文件：`datalink.c`
- 涉及函数：`FRAME_RECEIVED`, `send_data_frame`
- 修改前问题：只依赖 DATA timeout 恢复误码会慢。
- 修改后行为：CRC 错误或乱序 DATA 触发 NAK，请求重传 `frame_expected`；收到 NAK 后仅重传该序号。
- 解决的问题：减少高误码环境下等待 timeout 的概率。
- 验证情况：高误码 `--ber=1e-4` 20 分钟能稳定跑完。
- 备注：当前 `no_nak` 只允许同一缺口发一次 NAK，后续可考虑带间隔的重复 NAK。

### 4.8 DATA timer 与超时重传

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `DATA_TIMEOUT`
- 修改前问题：需要在 ACK/NAK 丢失或坏帧时恢复。
- 修改后行为：每个 DATA 以序号启动 timer，超时时仅重传该序号，且先检查该序号仍在发送窗口内。
- 解决的问题：避免整窗重传。
- 验证情况：LOG01 阶段 `DATA_TIMER=2500` 稳定；Linux 参数扫测显示 `DATA_TIMER=2300` 可能更优，但当时尚未最终落地。LOG02 阶段已落地为 2300。

### 4.9 网络层 enable/disable 与物理队列节流

- 修改文件：`datalink.c`
- 涉及函数：`update_network_layer`
- 修改前问题：flood 模式下如果只看发送窗口，物理层队列可能堆积，ACK/NAK 被 DATA 压住。
- 修改后行为：仅当 `nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 时启用网络层。
- 解决的问题：改善默认误码 flood 长测性能。
- 验证情况：默认误码 flood 20 分钟约 93% 左右；早期记录中加入队列控制后从约 82% 提升到约 92.66%。

### 4.10 Windows 五组测试脚本

- 修改文件：`Lab1-Windows-VS2019\Run-FivePerformanceTests.ps1`
- 涉及函数：PowerShell 脚本逻辑
- 修改前问题：手工跑五组测试易出错，日志不集中。
- 修改后行为：脚本按五组测试创建目录、启动 A/B、保存 stdout/stderr/log，生成 `summary.csv`。
- 解决的问题：提高测试可复现性。
- 验证情况：已跑五组 20 分钟，最终可用汇总在 `accepted-summary.csv`。
- 备注：最初 stdout/stderr 重定向到同一文件导致 PowerShell 报错，已修复为分开保存。

### 4.11 高误码复测与优化笔记

- 修改文件：`HIGH_BER_OPTIMIZATION_NOTES.md`
- 涉及函数：无
- 修改前问题：高误码性能和调参方向缺少集中记录。
- 修改后行为：记录 Windows 高误码 20 分钟复测、Linux 小矩阵参数结果和下一步候选。
- 验证情况：复测日志 `high-ber-longtest-20260511-234732` 干净。

### 4.12 Linux 编译修复

- 修改文件：`Lab1-linux\lprintf.h`
- 涉及函数：`lprintf`, `__v_lprintf`
- 修改前问题：WSL `make` 失败，GCC 报 `lprintf.h` 中声明为 `int`，但 `lprintf.c` 中实现为 `size_t`。
- 修改后行为：Linux `lprintf.h` 声明改为 `size_t`，与 Windows 版和 `lprintf.c` 一致。
- 验证情况：WSL `make clean && make` 通过。
- Git diff：

```diff
-int lprintf(const char *format, ...);
-int __v_lprintf(const char *format, va_list arg_ptr);
+size_t lprintf(const char *format, ...);
+size_t __v_lprintf(const char *format, va_list arg_ptr);
```

### 4.13 Linux 运行验证与参数扫测脚本

- 修改文件：`verify-linux-run.sh`, `sweep-linux-params.sh`, `validate-candidates-default.sh`, `verify-minimal-matrix.sh`
- 涉及函数：bash 脚本逻辑
- 修改前问题：需要验证 Linux 运行状态，并缩小参数调整范围。
- 修改后行为：`verify-linux-run.sh` 跑 Linux A/B 短测；`sweep-linux-params.sh` 复制临时源码替换宏参数并测试；`validate-candidates-default.sh` 对候选参数跑默认误码验证；`verify-minimal-matrix.sh` 用于 baseline / `DATA_TIMER=2300` / `DATA_TIMER=2400,ACK_TIMER=250` 三组在默认误码和高误码下做最小矩阵验证。
- 验证情况：Linux 60 秒短测通过；120 秒高误码参数矩阵完成；候选默认误码验证完成。
- 备注：脚本调试过程中曾有 bash 正则和重定向 bug，已修复。`verify-minimal-matrix.sh` 当前存在于目录中，但尚未执行过，因此其结果待确认。

## 5. 核心数据结构说明

| 名称 | 类型 | 含义 | 取值/范围 | 使用位置 |
|---|---|---|---|---|
| `MAX_SEQ` | 宏 | 最大序号 | 15，即 0..15 | 序号空间、回绕 |
| `NR_BUFS` | 宏 | 发送/接收窗口大小 | `(MAX_SEQ + 1) / 2 = 8` | `out_buf`, `in_buf`, 窗口判断 |
| `DATA_TIMER` | 宏 | DATA 重传定时器 | 当前 2500 ms | `send_data_frame`, `DATA_TIMEOUT` |
| `ACK_TIMER` | 宏 | 延迟 ACK 定时器 | 当前 300 ms | `FRAME_RECEIVED`, `ACK_TIMEOUT` |
| `FRAME_HEAD_LEN` | 宏 | 帧头长度 | 3 字节 | 序列化/解析 |
| `FRAME_CRC_LEN` | 宏 | CRC 长度 | 4 字节 | 序列化/解析 |
| `FRAME_DATA_LEN` | 宏 | DATA 帧不含 CRC 的长度 | `3 + PKT_LEN = 259` | 发送长度、队列阈值 |
| `FRAME_MAX_LEN` | 宏 | 最大帧长度 | 263 字节 | 接收/发送缓冲 |
| `PHL_QUEUE_LIMIT` | 宏 | 物理层发送队列阈值 | `FRAME_DATA_LEN * 2` | `update_network_layer` |
| `PKT_LEN` | 宏 | 网络层分组长度 | 256 字节 | `protocol.h`, payload |
| `FRAME_DATA` | 宏 | 数据帧类型 | 1 | `datalink.h`, `send_data_frame` |
| `FRAME_ACK` | 宏 | ACK 帧类型 | 2 | ACK timer |
| `FRAME_NAK` | 宏 | NAK 帧类型 | 3 | NAK 快速重传 |
| `struct FRAME.kind` | `unsigned char` | 帧类型 | DATA/ACK/NAK | 接收解析 |
| `struct FRAME.seq` | `unsigned char` | DATA 序号 | 0..15 | DATA 收发 |
| `struct FRAME.ack` | `unsigned char` | ACK 或 NAK 字段 | 0..15 | ACK 滑窗 / NAK 请求 |
| `struct FRAME.data` | `unsigned char[PKT_LEN]` | payload | 256 字节 | DATA 帧 |
| `struct FRAME.padding` | `unsigned int` | 残留 padding 字段 | 不用于 wire format | 仅结构体临时变量 |
| `ack_expected` | `static unsigned char` | 发送窗口下界 | 0..15 | ACK 处理、timeout 判断 |
| `next_frame_to_send` | `static unsigned char` | 下一个发送序号 | 0..15 | 网络层取包发送 |
| `frame_expected` | `static unsigned char` | 接收窗口下界 | 0..15 | DATA 接收、ACK/NAK 生成 |
| `too_far` | `static unsigned char` | 接收窗口上界后一位 | 0..15 | 接收窗口判断 |
| `nbuffered` | `static unsigned char` | 未确认发送帧数量 | 0..8 | 流控、ACK 滑动 |
| `no_nak` | `static unsigned char` | 是否允许对当前缺口发送 NAK | 0/1 | CRC 错误、乱序 DATA |
| `out_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 发送缓存 | 8 个 256 字节分组 | DATA 重传 |
| `in_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 接收缓存 | 8 个 256 字节分组 | 乱序缓存 |
| `arrived` | `unsigned char[NR_BUFS]` | 接收缓存占用标记 | 0/1 | 按序交付 |
| `phl_sq_len()` | 框架函数 | 物理层发送队列长度 | 字节数 | `update_network_layer` |

## 6. 核心函数说明

### `main(int argc, char **argv)`

- 参数含义：命令行参数透传给实验框架。
- 返回值含义：正常事件循环不返回。
- 主要功能：初始化协议状态，进入事件驱动主循环。
- 关键逻辑：`protocol_init` 后清零窗口变量和缓存，`enable_network_layer()`，然后循环 `wait_for_event(&arg)`。
- 调用时机：程序入口。
- 注意事项：不要在事件循环中加入阻塞等待；两端 A/B 都运行同一程序。

### `between(unsigned char a, unsigned char b, unsigned char c)`

- 参数含义：环形序号区间 `[a, c)` 中判断 `b` 是否在内。
- 返回值含义：真表示 `b` 位于当前窗口区间。
- 主要功能：处理序号回绕。
- 关键逻辑：覆盖普通区间和跨 0 回绕区间。
- 调用时机：ACK 滑动、接收窗口判断、NAK/timeout 是否有效。
- 注意事项：SR 窗口必须不超过序号空间一半；当前 8/16 满足。

### `update_network_layer(void)`

- 参数含义：无。
- 返回值含义：无。
- 主要功能：根据发送窗口和物理层队列决定启用/关闭网络层。
- 关键逻辑：`nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 时启用。
- 调用时机：多数事件分支结束后，以及 `NETWORK_LAYER_READY` 分支内。
- 注意事项：这是当前性能优化关键点，避免 ACK/NAK 被 DATA 队列压住。

### `send_data_frame(unsigned char kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[])`

- 参数含义：`kind` 为帧类型；`frame_nr` 为 DATA 序号；`frame_expected` 用于生成 ACK/NAK 字段；`buffer` 为 DATA payload。
- 返回值含义：无。
- 主要功能：统一封装 DATA/ACK/NAK，追加 CRC 后调用框架 `send_frame`。
- 关键逻辑：DATA 帧写入 3 字节头和 256 字节 payload；ACK 字段默认为最后一个按序收到的序号；NAK 特殊使用 `ack=frame_expected` 表示请求该帧。
- 调用时机：网络层有新包、ACK timeout、NAK 触发、收到 NAK、DATA timeout。
- 注意事项：函数名叫 `send_data_frame`，实际也发送 ACK/NAK。

### `FRAME_RECEIVED` 内部 ACK 处理

- 参数含义：从 `recv_frame` 读到原始字节。
- 返回值含义：事件分支无返回。
- 主要功能：对非 NAK 帧中的累计 ACK 滑动发送窗口。
- 关键逻辑：`while (between(ack_expected, f.ack, next_frame_to_send))` 停止对应 timer，`nbuffered--`，`inc(ack_expected)`。
- 调用时机：收到 ACK 或 DATA 时。
- 注意事项：当前代码刻意不对 NAK 帧执行累计 ACK 处理，因为 NAK 的 `ack` 字段含义不同。

### `FRAME_RECEIVED` 内部 NAK 处理

- 参数含义：`f.ack` 是请求重传的序号。
- 主要功能：选择性重传被请求单帧。
- 关键逻辑：若请求序号在当前发送窗口内，则用 `out_buf[n % NR_BUFS]` 重发。
- 注意事项：高误码下 NAK 本身可能损坏，当前只发一次 NAK 的策略可能导致等待 DATA timeout。

### `FRAME_RECEIVED` 内部 DATA 处理

- 参数含义：`f.seq` 为收到 DATA 序号，`f.data` 为 payload。
- 主要功能：接收窗口判断、乱序缓存、按序交付网络层。
- 关键逻辑：在窗口内且未缓存则写入 `in_buf`；如果不是期望帧且 `no_nak` 为真，发送 NAK；随后循环交付连续到达的数据。
- 注意事项：收到 DATA 后启动 ACK timer，等待 piggyback 机会。

### `DATA_TIMEOUT` 内部处理

- 参数含义：`arg` 是超时 timer 编号，即 DATA 序号。
- 主要功能：单帧超时重传。
- 关键逻辑：仅当超时序号仍在 `[ack_expected, next_frame_to_send)` 窗口内才重传。
- 注意事项：LOG01 阶段 `DATA_TIMER=2500`，候选优化为 2300；LOG02 阶段 2300 已成为当前源码值。

### `ACK_TIMEOUT` 内部处理

- 参数含义：无额外参数。
- 主要功能：发送纯 ACK，避免 ACK 被无限 piggyback 延迟。
- 关键逻辑：`send_data_frame(FRAME_ACK, 0, frame_expected, NULL)`。
- 注意事项：发送后 `send_data_frame` 会停止 ACK timer。

### CRC 相关函数

- `crc32` 由实验框架提供。
- 发送：对不含 CRC 的帧计算 CRC 并追加。
- 接收：对含 CRC 的帧整体计算，结果为 0 表示通过。
- 注意事项：不要重写 `crc32`。

### 日志相关函数

- `dbg_frame` 打印帧级调试信息。
- `dbg_event` 打印事件级调试信息。
- `lprintf` 打印协议启动和统计信息。
- 注意事项：性能测试不要开高 debug，输出会影响吞吐。

## 7. 事件驱动主循环说明

主循环结构：

```c
for (;;) {
    event = wait_for_event(&arg);
    switch (event) {
    case NETWORK_LAYER_READY:
        ...
    case PHYSICAL_LAYER_READY:
        ...
    case FRAME_RECEIVED:
        ...
    case DATA_TIMEOUT:
        ...
    case ACK_TIMEOUT:
        ...
    }
    update_network_layer();
}
```

### `NETWORK_LAYER_READY`

实际行为：

1. 若 `nbuffered < NR_BUFS`，从网络层 `get_packet()` 到 `out_buf[next_frame_to_send % NR_BUFS]`。
2. `nbuffered++`。
3. 调用 `send_data_frame(FRAME_DATA, next_frame_to_send, frame_expected, ...)`。
4. `inc(next_frame_to_send)`。
5. 调用 `update_network_layer()`。

可能修改：`out_buf`、`nbuffered`、`next_frame_to_send`、DATA timer、ACK timer、网络层启停状态。

### `PHYSICAL_LAYER_READY`

实际行为：当前只 `break`。真正的网络层恢复依赖主循环末尾统一调用 `update_network_layer()`。

可能修改：无直接修改。

注意事项：目前测试未发现问题，但可考虑在该分支显式调用 `update_network_layer()` 作为可读性改进。

### `FRAME_RECEIVED`

实际行为：

1. `recv_frame(frame, sizeof frame)` 读入字节。
2. 检查长度和 CRC。
3. 坏帧时，若 `no_nak` 为真，发送 NAK 请求 `frame_expected` 并置 `no_nak=0`。
4. 解析 `kind/seq/ack`，DATA 帧复制 payload。
5. 非 NAK 帧处理累计 ACK 并滑动发送窗口。
6. ACK 帧仅输出调试信息。
7. NAK 帧按 `f.ack` 重传指定 DATA。
8. DATA 帧在接收窗口内缓存，乱序时可能发送 NAK；连续到达时交付网络层并移动 `frame_expected/too_far`。
9. 收到 DATA 后启动 ACK timer。

可能修改：`ack_expected`、`nbuffered`、timer、`arrived`、`in_buf`、`frame_expected`、`too_far`、`no_nak`。

### `DATA_TIMEOUT`

实际行为：

1. 输出 timeout 调试信息。
2. 若 `arg` 仍在发送窗口内，重传该序号 DATA。

可能修改：DATA timer、ACK timer、物理发送队列。

### `ACK_TIMEOUT`

实际行为：发送纯 ACK。

可能修改：ACK timer、物理发送队列。

## 8. 协议工作流程

### 8.1 正常发送流程

1. 网络层准备好分组，触发 `NETWORK_LAYER_READY`。
2. 协议从网络层取 256 字节分组到 `out_buf`。
3. 构造 DATA 帧，序号为 `next_frame_to_send`，ACK 字段 piggyback 当前累计 ACK。
4. 追加 CRC32，调用物理层 `send_frame`。
5. 启动对应 DATA timer。
6. `next_frame_to_send` 环形递增。
7. 收到对端 ACK 或 DATA 中的 piggyback ACK 后，滑动发送窗口。
8. 每确认一帧就停止该帧 timer，`nbuffered--`。
9. 窗口空位出现后，`update_network_layer()` 可重新启用网络层。

### 8.2 正常接收流程

1. 物理层收到帧，触发 `FRAME_RECEIVED`。
2. 读取原始字节并做 CRC 校验。
3. 如果是 DATA 且序号在接收窗口内，则缓存到 `in_buf`。
4. 如果该帧正好是 `frame_expected`，循环交付所有连续已到达分组。
5. 每交付一帧，清除 `arrived`，移动 `frame_expected` 和 `too_far`。
6. 启动 ACK timer，等待可能的反向 DATA piggyback；若超时则发纯 ACK。

### 8.3 误码/丢帧恢复流程

CRC 错误：

- 丢弃该帧。
- 若 `no_nak==1`，发送 NAK 请求 `frame_expected`。
- 设置 `no_nak=0`，避免同一缺口 NAK 风暴。

乱序 DATA：

- 如果在接收窗口内则先缓存。
- 如果 `seq != frame_expected` 且 `no_nak==1`，发送 NAK 请求缺失帧。

收到 NAK：

- `f.ack` 被解释为请求重传的序号。
- 若该序号仍在发送窗口内，重传单帧。

ACK 丢失：

- 后续 DATA/ACK 的累计 ACK 仍可确认旧帧。
- 若一直没有确认，对应 DATA timer 超时后重传。

DATA timeout：

- 只重传超时序号对应帧，不整窗重传。

### 8.4 序号回绕处理

当前序号空间是 0..15，窗口大小是 8。所有窗口判断使用 `between(a,b,c)`，而不是普通整数比较。`inc(k)` 在 `MAX_SEQ` 后回到 0。因为 SR 窗口大小等于序号空间一半，满足避免新旧帧混淆的基本要求。

## 9. 当前参数选择与理由

| 参数 | 当前值 | 理由 | 是否建议继续调整 |
|---|---:|---|---|
| `MAX_SEQ` | 15 | 序号空间 16，配合 SR 窗口 8 | 不建议调整 |
| `NR_BUFS` | 8 | SR 窗口不超过序号空间一半，且利于填满带宽时延积 | 不建议调整 |
| `DATA_TIMER` | 2500 ms | 当前稳定，默认误码和高误码 20 分钟均可跑完 | 建议继续优化，候选 2300 |
| `ACK_TIMER` | 300 ms | 兼顾 piggyback 和及时 ACK | 小范围验证，250 单独表现较差 |
| `PHL_QUEUE_LIMIT` | `FRAME_DATA_LEN * 2` | 限制物理队列，避免 ACK/NAK 被 DATA 压住 | 只建议与 `*3` 对比，当前 `*2` 更稳 |
| `FRAME_HEAD_LEN` | 3 | `kind/seq/ack` | 不建议调整 |
| `FRAME_CRC_LEN` | 4 | CRC32 | 不建议调整 |
| `PKT_LEN` | 256 | 实验框架固定 | 不应修改 |

参数已经针对 8000 bps、270 ms 单向时延、256 字节分组做过经验调优。当前低误码/默认误码表现较好，高误码仍是经验值，待进一步 300 秒和 1200 秒性能测试优化。

Linux 120 秒参数扫测中，当前最值得继续验证的候选是：

```c
#define DATA_TIMER 2300
#define ACK_TIMER 300
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

该候选尚未写入稳定版 `datalink.c`。

## 10. 测试命令与运行方式

### 编译

Windows 当前主要使用已生成的：

```powershell
cd "C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019"
.\datalink.exe ...
```

也可用 Visual Studio 工程 `datalink.sln` 构建，当前未在本轮重新用 VS 编译，待确认。

Linux 已在 WSL 验证：

```bash
cd "/mnt/c/Users/28641/Desktop/计网/实验/Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux"
make clean
make
```

### 基础运行

Windows：

```powershell
.\datalink.exe -d3 A
.\datalink.exe -d3 B
```

Linux：

```bash
./datalink -d3 A
./datalink -d3 B
```

### 性能测试命令

#### 1. 无误码普通传输

Windows：

```powershell
.\datalink.exe -t 1200 -u -p 61439 -l .\performance-tests-...\01_plain_ber0\A.log A
.\datalink.exe -t 1200 -u -p 61439 -l .\performance-tests-...\01_plain_ber0\B.log B
```

Linux：

```bash
./datalink -t 1200 -u -p 61439 -l A.log A
./datalink -t 1200 -u -p 61439 -l B.log B
```

#### 2. 默认误码普通传输

Windows：

```powershell
.\datalink.exe -t 1200 -p 61450 -l .\performance-tests-...\02_plain_ber1e-5-rerun\A.log A
.\datalink.exe -t 1200 -p 61450 -l .\performance-tests-...\02_plain_ber1e-5-rerun\B.log B
```

Linux：

```bash
./datalink -t 1200 -p 61450 -l A.log A
./datalink -t 1200 -p 61450 -l B.log B
```

#### 3. 无误码双向 flood

Windows：

```powershell
.\datalink.exe -t 1200 -f -u -p 61441 -l .\performance-tests-...\03_flood_ber0\A.log A
.\datalink.exe -t 1200 -f -u -p 61441 -l .\performance-tests-...\03_flood_ber0\B.log B
```

Linux：

```bash
./datalink -t 1200 -f -u -p 61441 -l A.log A
./datalink -t 1200 -f -u -p 61441 -l B.log B
```

#### 4. 默认误码双向 flood

Windows：

```powershell
.\datalink.exe -t 1200 -f -p 61442 -l .\performance-tests-...\04_flood_ber1e-5\A.log A
.\datalink.exe -t 1200 -f -p 61442 -l .\performance-tests-...\04_flood_ber1e-5\B.log B
```

Linux：

```bash
./datalink -t 1200 -f -p 61442 -l A.log A
./datalink -t 1200 -f -p 61442 -l B.log B
```

#### 5. `ber=1e-4` 双向 flood

Windows：

```powershell
.\datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l .\high-ber-longtest-20260511-234732\A.log A
.\datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l .\high-ber-longtest-20260511-234732\B.log B
```

Linux：

```bash
./datalink -t 1200 -f --ber=1e-4 -p 63588 -l A.log A
./datalink -t 1200 -f --ber=1e-4 -p 63588 -l B.log B
```

自动跑 Windows 五组：

```powershell
powershell -ExecutionPolicy Bypass -File .\Run-FivePerformanceTests.ps1 -DurationSeconds 1200
```

Linux 60 秒验证：

```bash
bash verify-linux-run.sh 60 63620 linux-run-verify-20260512
```

Linux 参数扫测：

```bash
bash sweep-linux-params.sh 120 63700
bash validate-candidates-default.sh param-sweep-20260512-002725 120 63800
bash verify-minimal-matrix.sh 300 63900
```

## 11. 已完成测试记录

Windows 五组 20 分钟正式可用汇总：

```text
Lab1-Windows-VS2019\performance-tests-20260511-212030\accepted-summary.csv
```

注意：同一目录下原始 `02_plain_ber1e-5` 日志保留了首跑结果，出现过 `System too busy`，不作为最终表格数据；最终表格使用 `02_plain_ber1e-5-rerun` 日志。

| 序号 | 测试场景 | A 命令 | B 命令 | 运行时间 | A 利用率 | B 利用率 | 是否通过 | 备注 |
|---:|---|---|---|---:|---:|---:|---|---|
| 1 | 无误码普通传输 | `datalink.exe -t 1200 -u -p 61439 ... A` | `datalink.exe -t 1200 -u -p 61439 ... B` | 1200 s | 51.70% | 96.97% | 通过 | A/B 均 `Quit.`，无 fatal 扫描命中 |
| 2 | 默认误码普通传输 | `datalink.exe -t 1200 -p 61450 ... A` | `datalink.exe -t 1200 -p 61450 ... B` | 1200 s | 49.52% | 93.40% | 通过 | 使用 rerun 结果；原 61440 首跑出现 `System too busy`，不用作最终表 |
| 3 | 无误码双向 flood | `datalink.exe -t 1200 -f -u -p 61441 ... A` | `datalink.exe -t 1200 -f -u -p 61441 ... B` | 1200 s | 96.97% | 96.96% | 通过 | A/B 均 4536 packets |
| 4 | 默认误码双向 flood | `datalink.exe -t 1200 -f -p 61442 ... A` | `datalink.exe -t 1200 -f -p 61442 ... B` | 1200 s | 93.20% | 92.80% | 通过 | Err 97/95 |
| 5 | `ber=1e-4` 双向 flood | `datalink.exe -t 1200 -f --ber=1e-4 -p 61443 ... A` | `datalink.exe -t 1200 -f --ber=1e-4 -p 61443 ... B` | 1200 s | 55.02% | 51.73% | 通过 | Err 821/817 |
| 6 | `ber=1e-4` 双向 flood 复测 | `datalink.exe -t 1200 -f --ber=1e-4 -p 63588 ... A` | `datalink.exe -t 1200 -f --ber=1e-4 -p 63588 ... B` | 1200 s | 56.78% | 54.22% | 通过 | 独立复测，A/B exit 0 |
| 7 | Linux 默认误码 flood 短测 | `./datalink -t 60 -f -p 63620 ... A` | `./datalink -t 60 -f -p 63620 ... B` | 60 s | 95.00% | 93.24% | 通过 | WSL，短测，不可替代正式长测 |
| 8 | Linux 高误码参数小矩阵 | `sweep-linux-params.sh 120 63700` | 脚本自动 A/B | 120 s/组 | 见 `summary.csv` | 见 `summary.csv` | 通过 | 用于筛选参数，不是验收正式数据 |

错误/警告记录：

| 时间/场景 | 问题 | 可能原因 | 当前处理状态 |
|---|---|---|---|
| Windows 五组首跑 `02_plain_ber1e-5` | 日志出现 `System too busy`，最后统计停在约 784 秒 | 主机调度/系统忙 | 已单独 rerun，并用 `02_plain_ber1e-5-rerun` 作为最终结果 |
| WSL 首次 `make` | `lprintf.h` 声明 `int` 与 `lprintf.c` 实现 `size_t` 冲突 | Linux 头文件与实现不一致 | 已修复 Linux `lprintf.h`，`make` 通过 |
| WSL 命令输出 | 出现 localhost/NAT 乱码提示 | WSL 环境提示 | 不影响编译运行，已记录 |
| 参数扫测脚本调试 | bash 正则和重定向写法报错 | 脚本 bug | 已修复，成功生成 `param-sweep-20260512-002725` |
| Git 工作区 | 大量测试产物未跟踪/已修改 | 长测和扫测生成日志、二进制和临时源码 | 后续需要决定保留、提交或清理 |

## 12. 当前性能结论

基于已有 Windows 20 分钟数据：

- 无误码 flood 场景：A/B 约 96.97% / 96.96%。
- 默认 `1e-5` 误码 flood 场景：A/B 约 93.20% / 92.80%；另有早期默认误码 flood 20 分钟约 92.66%。
- `1e-4` 高误码 flood 场景：五组正式数据 A/B 约 55.02% / 51.73%；独立复测约 56.78% / 54.22%。
- 默认误码 flood 性能明显优于典型 GBN 参考水平，接近 Selective Repeat 参考水平，但仍低于理想/参考 SR 高值。
- 高误码下性能瓶颈主要可能是 NAK 损坏后等待 DATA timer、DATA timeout 偏保守、NAK 去重策略过强、误码造成控制帧和数据帧频繁损坏。

LOG01 阶段 Linux 只完成 60 秒短测和 120 秒参数扫测；Linux 20 分钟正式性能结论待确认。LOG02 阶段已补充当前 2300 参数的 Linux 1200 秒默认误码 flood 和高误码 flood 结论。

## 13. 已知问题与风险点

| 问题 | 影响 | 复现方式 | 建议修复方向 | 优先级 |
|---|---|---|---|---|
| LOG01 阶段 `DATA_TIMER=2500` 高误码下偏保守 | NAK 丢失后恢复慢，高误码利用率约 52% 到 57% | `-t 1200 -f --ber=1e-4` | 当时建议验证 `DATA_TIMER=2300` 300s/1200s；LOG02 已落地并复测 | P1 |
| `no_nak` 只发一次 NAK | NAK 损坏后等待 timeout | 高误码、开启调试观察 NAK/timeout | 增加带时间间隔的重复 NAK，避免风暴 | P1 |
| LOG01 阶段 Linux 20 分钟长测未完成 | Linux 正式验收证据不足 | WSL 或真实 Linux 跑五组/至少 flood 长测 | LOG02 已补 Linux 1200s 默认误码 flood 和高误码 flood；完整五组仍可按需补 | P0 |
| 工作区测试产物很多 | 后续 diff 噪声大，难以 review | `git status --short` | 清理 `.o`、临时 sweep src，保留关键 summary/log | P0 |
| `PHYSICAL_LAYER_READY` 分支为空 | 可读性差，极端队列恢复依赖循环末尾 | 压力 flood 测试 | 可显式调用 `update_network_layer()`，但需测试 | P2 |
| 参数扫测样本短 | 120 秒结果有随机性 | 重复跑 sweep | 先 300 秒复测，再 1200 秒终测 | P0 |
| `PHL_QUEUE_LIMIT=3` 两端不均衡 | 可能让控制帧排队变差 | 参数扫测 `queue3` | 暂不优先；继续保留 2 帧 | P1 |
| ACK timer 单独降到 250 表现差 | 控制帧变多或节奏不佳 | 参数扫测 `ack250` | 不单独采用 ACK=250 | P1 |
| 日志过多影响性能 | debug 输出拖慢程序 | 使用 `-d3` 长测 | 正式测试 debug mask 用 0 | P2 |
| Windows/Linux 行为差异 | 跨平台结果可能不同 | 同参数分别跑长测 | 保持 `datalink.c` 同步并分别记录 | P1 |

## 14. 后续开发建议

### P0：必须完成

- 清理或归档当前大量测试产物，避免 Git 工作区被日志和 `.o` 文件淹没。
- LOG01 当时建议对稳定参数和候选参数做 300 秒最小验证矩阵；当时已有脚本 `Lab1-linux\verify-minimal-matrix.sh`，但尚未运行确认。LOG02 阶段已完成有效矩阵，保留本表作为当时计划：

| Variant | DATA_TIMER | ACK_TIMER | Queue |
|---|---:|---:|---|
| baseline | 2500 | 300 | 2 |
| candidate-1 | 2300 | 300 | 2 |
| candidate-2 | 2400 | 250 | 2 |

- 每组至少跑 `300s 高误码 flood` 和 `300s 默认误码 flood`。
- 若 `DATA_TIMER=2300` 持续领先，再同步修改 Windows/Linux 两份 `datalink.c` 并跑 1200 秒最终复测。此项已在 LOG02 阶段推进：2300 已写入源码，Linux 1200 秒最终复测已完成。
- 跑 Linux 20 分钟正式长测，至少包括默认误码 flood 和 `--ber=1e-4` flood。LOG02 阶段已补当前 2300 参数的这两项 flood 长测；Linux 完整五组仍可按需补。

### P1：建议优化

- 继续聚焦 `DATA_TIMER`，范围缩小到 2300、2400、2500；不要再测 2000。
- 暂不优先调整窗口大小；窗口 4 已观察到变差。
- 暂不优先采用 `PHL_QUEUE_LIMIT=FRAME_DATA_LEN*3`，除非 300 秒重复测试证明稳定。
- 研究 NAK 去重策略：可增加 `last_nak_time`，同一缺口超过 500 到 800 ms 可重复 NAK。
- 对候选参数同时比较默认误码和高误码，避免高误码提升但默认误码下降。

### P2：可选加分

- 将 `FRAME_RECEIVED` 大分支拆成 `handle_ack`、`handle_nak`、`handle_data_frame`、`handle_timeout`，提升可读性。
- 增加统计：NAK 数、timeout 数、重传数、重复 NAK 数。
- 生成更干净的 CSV/Markdown 性能表，便于填写实验表格。
- 为测试产物目录添加命名规范和清理脚本。
- 为 wire format 添加注释，明确 DATA 总长 263 字节，ACK/NAK 总长 7 字节。

## 15. 给 LOG02 之前下一聊天窗口的接手提示

```text
注意：这是 LOG01 阶段的接手提示。如果 DEVELOPMENT_LOG02_RECORD.md 同时存在，请优先阅读 LOG02，因为 LOG02 已记录 DATA_TIMER=2300 落地和后续复测结果。

这是计算机网络实验一：数据链路层滑动窗口协议项目。当前目录：
C:\Users\28641\Desktop\计网\实验

主代码：
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\datalink.c
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-linux\datalink.c

LOG01 阶段实现是 Selective Repeat with Piggybacking，不是 Go-Back-N。支持 CRC32、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable、物理队列节流。LOG01 阶段稳定参数：
MAX_SEQ=15, NR_BUFS=8, DATA_TIMER=2500, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2。
LOG02 阶段已将 DATA_TIMER 改为 2300，并以 LOG02 为最新状态。

优先阅读：
1. datalink.c: between()
2. datalink.c: update_network_layer()
3. datalink.c: send_data_frame()
4. datalink.c: main() 的 NETWORK_LAYER_READY / FRAME_RECEIVED / DATA_TIMEOUT / ACK_TIMEOUT 分支
5. HIGH_BER_OPTIMIZATION_NOTES.md
6. DEVELOPMENT_LOG01_RECORD.md

不要随意修改 protocol.c/protocol.h/crc32.c/lprintf.c/getopt.c。Linux lprintf.h 已做最小修复：lprintf 和 __v_lprintf 返回 size_t，与 lprintf.c 一致。

已完成 Windows 五组 20 分钟测试，结果在：
Lab1-Windows-VS2019\performance-tests-20260511-212030\accepted-summary.csv

已完成 Windows 高误码 --ber=1e-4 flood 20 分钟复测：
Lab1-Windows-VS2019\high-ber-longtest-20260511-234732
A 56.78%, B 54.22%，日志干净。

Linux 已在 WSL 下 make clean && make 通过，并完成 60 秒默认误码 flood 短测：
Lab1-linux\linux-run-verify-20260512
A 95.00%, B 93.24%。

参数扫测在：
Lab1-linux\param-sweep-20260512-002725\summary.csv
当前最值得继续验证的候选是 DATA_TIMER=2300, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2。
300 秒最小矩阵脚本在 LOG01 阶段已存在但尚未运行：
Lab1-linux\verify-minimal-matrix.sh
LOG02 阶段已完成有效 300 秒矩阵，结果以 LOG02 记录为准。

下一步建议：
1. 清理/归档当前大量测试产物和 .o 文件。
2. 跑 300 秒最小矩阵：baseline(2500/300/2)、candidate-1(2300/300/2)、candidate-2(2400/250/2)。
3. 每组同时跑默认误码 flood 和 --ber=1e-4 flood。
4. 若 DATA_TIMER=2300 继续最好，再同步修改 Windows/Linux 两份 datalink.c，并跑 1200 秒最终复测。
5. LOG01 阶段 Linux 20 分钟正式长测仍待确认；LOG02 阶段已补当前 2300 参数的 Linux 1200 秒默认误码 flood 和高误码 flood。

Windows 运行示例：
cd "C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019"
.\datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l A.log A
.\datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l B.log B

Linux 运行示例：
cd "/mnt/c/Users/28641/Desktop/计网/实验/Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux"
make clean && make
./datalink -t 1200 -f --ber=1e-4 -p 63588 -l A.log A
./datalink -t 1200 -f --ber=1e-4 -p 63588 -l B.log B
```

## 16. 附录：关键代码摘要

### 参数与帧结构

```c
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define DATA_TIMER 2500
#define ACK_TIMER 300
#define FRAME_HEAD_LEN 3
#define FRAME_CRC_LEN 4
#define FRAME_DATA_LEN (FRAME_HEAD_LEN + PKT_LEN)
#define FRAME_MAX_LEN (FRAME_DATA_LEN + FRAME_CRC_LEN)
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)

struct FRAME {
    unsigned char kind;
    unsigned char seq;
    unsigned char ack;
    unsigned char data[PKT_LEN];
    unsigned int padding;
};
```

### 窗口状态

```c
static unsigned char ack_expected = 0;
static unsigned char next_frame_to_send = 0;
static unsigned char frame_expected = 0;
static unsigned char too_far = NR_BUFS;
static unsigned char nbuffered = 0;
static unsigned char no_nak = 1;

static unsigned char out_buf[NR_BUFS][PKT_LEN];
static unsigned char in_buf[NR_BUFS][PKT_LEN];
static unsigned char arrived[NR_BUFS];
```

### 序号递增与窗口判断

```c
#define inc(k)                 \
    do {                       \
        if ((k) < MAX_SEQ)     \
            (k)++;             \
        else                   \
            (k) = 0;           \
    } while (0)

static int between(unsigned char a, unsigned char b, unsigned char c)
{
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) ||
           ((b < c) && (c < a));
}
```

### 网络层流控

```c
static void update_network_layer(void)
{
    if (nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT)
        enable_network_layer();
    else
        disable_network_layer();
}
```

### 发送封装

```c
static void send_data_frame(unsigned char kind, unsigned char frame_nr,
                            unsigned char frame_expected,
                            unsigned char buffer[])
{
    unsigned char frame[FRAME_MAX_LEN];
    unsigned char ack_nr;
    unsigned int crc;
    int len;

    memset(frame, 0, sizeof frame);
    ack_nr = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    if (kind == FRAME_NAK)
        ack_nr = frame_expected;

    frame[0] = kind;
    frame[1] = frame_nr;
    frame[2] = ack_nr;

    if (kind == FRAME_DATA) {
        memcpy(frame + FRAME_HEAD_LEN, buffer, PKT_LEN);
        len = FRAME_DATA_LEN;
    } else {
        len = FRAME_HEAD_LEN;
    }

    crc = crc32(frame, len);
    frame[len] = (unsigned char)(crc & 0xff);
    frame[len + 1] = (unsigned char)((crc >> 8) & 0xff);
    frame[len + 2] = (unsigned char)((crc >> 16) & 0xff);
    frame[len + 3] = (unsigned char)((crc >> 24) & 0xff);
    send_frame(frame, len + FRAME_CRC_LEN);

    if (kind == FRAME_DATA)
        start_timer(frame_nr, DATA_TIMER);
    stop_ack_timer();
}
```

### ACK/NAK/DATA 接收核心

```c
if (len < FRAME_HEAD_LEN + FRAME_CRC_LEN || crc32(frame, len) != 0) {
    if (no_nak) {
        send_data_frame(FRAME_NAK, 0, frame_expected, NULL);
        no_nak = 0;
    }
    break;
}

if (f.kind != FRAME_NAK) {
    while (between(ack_expected, f.ack, next_frame_to_send)) {
        nbuffered--;
        stop_timer(ack_expected);
        inc(ack_expected);
    }
}

if (f.kind == FRAME_NAK) {
    n = f.ack;
    if (between(ack_expected, n, next_frame_to_send))
        send_data_frame(FRAME_DATA, n, frame_expected,
                        out_buf[n % NR_BUFS]);
}

if (f.kind == FRAME_DATA) {
    if (between(frame_expected, f.seq, too_far) &&
        !arrived[f.seq % NR_BUFS]) {
        arrived[f.seq % NR_BUFS] = 1;
        memcpy(in_buf[f.seq % NR_BUFS], f.data, PKT_LEN);
        if (f.seq != frame_expected && no_nak) {
            send_data_frame(FRAME_NAK, 0, frame_expected, NULL);
            no_nak = 0;
        }

        while (arrived[frame_expected % NR_BUFS]) {
            put_packet(in_buf[frame_expected % NR_BUFS], PKT_LEN);
            arrived[frame_expected % NR_BUFS] = 0;
            inc(frame_expected);
            inc(too_far);
            no_nak = 1;
        }
    }
    start_ack_timer(ACK_TIMER);
}
```

### 超时处理

```c
case DATA_TIMEOUT:
    if (between(ack_expected, (unsigned char)arg, next_frame_to_send))
        send_data_frame(FRAME_DATA, (unsigned char)arg,
                        frame_expected,
                        out_buf[((unsigned char)arg) % NR_BUFS]);
    break;

case ACK_TIMEOUT:
    send_data_frame(FRAME_ACK, 0, frame_expected, NULL);
    break;
```

本文件保存为 `DEVELOPMENT_LOG01_RECORD.md`，作为 LOG00 到 LOG02 之间的阶段性工程上下文记录。若后续新聊天窗口同时看到 LOG02，应以 `DEVELOPMENT_LOG02_RECORD.md` 作为最新接手记录。
