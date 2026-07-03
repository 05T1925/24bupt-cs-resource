# 数据链路层滑动窗口协议开发记录 LOG02

## 1. LOG02 生成背景

本文件是在 `DEVELOPMENT_LOG00_RECORD.md`、`DEVELOPMENT_LOG01_RECORD.md` 和 `HIGH_BER_OPTIMIZATION_NOTES.md` 之后生成的第三份工程接手记录。它面向后续另起 ChatGPT/Codex 窗口继续开发，不是实验报告正文。

LOG02 的目标是把当前仓库真实状态、最近一轮参数落地、测试脚本改动、最终复测结果、未提交变更和后续风险点一次性记录清楚。旧记录中凡与当前代码和测试证据冲突的地方，以本文件为准。

本次未继续修改协议逻辑，只在读取现有记录、源码、脚本、summary/log、Git 状态后生成本记录。

## 2. 项目路径与当前仓库状态

当前工作目录：

```text
C:\Users\28641\Desktop\计网\实验
```

主工程目录：

```text
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)
```

主代码路径：

```text
Windows: C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\datalink.c
Linux:   C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-linux\datalink.c
```

Git 可用。最近提交：

```text
eb3ec94 Update generated outputs
82bacc6 Initial commit
```

当前工作区较脏，`git status --short` 显示有源码修改、历史记录新增、测试日志/summary 修改、新增测试目录和归档目录。主要类别如下：

| 类别 | 当前状态 | 说明 |
|---|---|---|
| 协议源码 | 已修改未提交 | Windows/Linux 两份 `datalink.c` 的 `DATA_TIMER` 已从 2500 改为 2300 |
| 帧声明注释 | 已修改未提交 | Windows/Linux 两份 `datalink.h` 的 ACK/NAK 注释已修正为 `kind/seq/ack/crc` |
| Linux 兼容修复 | 已修改未提交 | `Lab1-linux/lprintf.h` 返回类型改为 `size_t`，与实现一致 |
| 工程记录 | 新增未跟踪 | `DEVELOPMENT_LOG00_RECORD.md`、`DEVELOPMENT_LOG01_RECORD.md`、`HIGH_BER_OPTIMIZATION_NOTES.md`、本文件 |
| 测试脚本 | 新增未跟踪 | `verify-linux-run.sh`、`sweep-linux-params.sh`、`validate-candidates-default.sh`、`verify-minimal-matrix.sh`、`verify-final-dt2300.sh` |
| 测试证据 | 新增未跟踪 | `high-ber-longtest-*`、`param-sweep-*`、`minimal-matrix-*`、`final-validation-*`、`accepted-summary.csv` |
| 归档产物 | 新增未跟踪 | `archive-artifacts-*`，内含旧 sweep、空跑矩阵、`.o`、Linux 可执行文件等 |
| 旧文件状态 | 有删除/修改记录 | `DEVELOPMENT0_RECORD.md` 被删除，部分旧 Windows 测试日志/manifest 被改动 |

建议：下一步先决定哪些记录、脚本、summary 和关键日志要提交，哪些大型日志、归档目录、编译产物要 `.gitignore` 或移出仓库。

## 3. 与 LOG00 / LOG01 / 高误码笔记的关系

| 来源记录 | 当时结论 | 当前 LOG02 是否仍成立 | 当前修正 |
|---|---|---|---|
| LOG00 | 当时认为当前目录不是 Git 仓库 | 不成立 | 当前 Git 可用，最近提交为 `eb3ec94`、`82bacc6` |
| LOG00 | 五组验收测试未补齐，高误码长测待测 | 不成立 | Windows 五组 20 分钟已有 `accepted-summary.csv`；高误码 Windows 复测和 Linux 最终验证均已有日志 |
| LOG00 | `DATA_TIMER=2500` 是当前稳定值 | 已过时 | 当前代码真实值为 `DATA_TIMER=2300` |
| LOG01 | Windows/Linux 两份 `datalink.c` SHA256 一致 | 仍成立 | 当前两份 `datalink.c` SHA256 均为 `32AAD1BAA2C240B0E6F9E7689193AE9F04B7855F38D2D8FF1CD3E068121F1C23` |
| LOG01 | `DATA_TIMER=2300` 只是候选，尚未改入稳定源码 | 已过时 | 已同步改入 Windows/Linux 两份 `datalink.c` |
| LOG01 | `verify-minimal-matrix.sh` 尚未运行确认 | 已过时 | 有效 300 秒矩阵位于 `Lab1-linux/minimal-matrix-20260512-092212/summary.csv` |
| LOG01 | Linux 20 分钟正式长测待确认 | 部分修正 | Linux 已完成当前 `DATA_TIMER=2300` 的 1200s 默认误码 flood 和 1200s 高误码 flood；Linux 五组完整 20 分钟仍待确认 |
| HIGH_BER | `DATA_TIMER=2300` 是首选候选 | 已落地 | 当前最终参数为 `DATA_TIMER=2300, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2` |
| HIGH_BER | `DATA_TIMER=2300` 最终 1200 秒复测通过 | 仍成立 | 证据为 `Lab1-linux/final-validation-20260512-095621/summary.csv` |

## 4. 当前实现状态总览

| 功能 | 当前状态 | 相关文件/函数 | LOG02 备注 |
|---|---|---|---|
| Selective Repeat 发送窗口 | 已完成 | `datalink.c`, `between`, `main` | 窗口 `NR_BUFS=8`，序号空间 0..15 |
| Go-Back-N 独立实现 | 未实现 | 无 | 当前交付实现是 SR，不是 GBN |
| 全双工通信 | 已完成 | `main`, `send_data_frame` | A/B 两端运行同一程序 |
| CRC32 | 已完成 | `send_data_frame`, `FRAME_RECEIVED`, `crc32` | 发送追加 CRC，接收 `crc32(frame,len)!=0` 判坏 |
| 手工帧序列化 | 已完成 | `send_data_frame` | 使用 `kind/seq/ack/payload/crc` 字节流，不直接发送结构体 |
| Piggyback ACK | 已完成 | `send_data_frame` | DATA 帧携带累计 ACK |
| ACK timer | 已完成 | `FRAME_RECEIVED`, `ACK_TIMEOUT` | 当前 `ACK_TIMER=300` |
| NAK | 已完成 | `FRAME_RECEIVED`, `FRAME_NAK` 处理 | 当前仍是单次 NAK + `no_nak` 去重，尚未实现限速重复 NAK |
| 乱序缓存 | 已完成 | `in_buf`, `arrived` | SR 接收窗口内缓存乱序 DATA |
| 序号回绕 | 已完成 | `inc`, `between` | 所有窗口判断使用环形比较 |
| DATA timer | 已完成 | `send_data_frame`, `DATA_TIMEOUT` | 当前 `DATA_TIMER=2300`，LOG01 的 2500 已过时 |
| 单帧重传 | 已完成 | `FRAME_NAK`, `DATA_TIMEOUT` | 收 NAK 或单帧 timeout 均只重传目标帧 |
| 网络层 enable/disable | 已完成 | `update_network_layer` | 窗口满或物理队列偏高时关闭网络层 |
| 物理层发送队列节流 | 已完成 | `PHL_QUEUE_LIMIT` | 当前阈值为两个 DATA 帧长度 |
| Linux 编译 | 已验证 | `Makefile`, `lprintf.h` | 最终验证脚本会先 `make clean && make`，构建通过 |
| Windows 编译/运行 | 历史已验证 | `datalink.exe`, VS 工程 | 当前 Windows 源码改为 2300 后，尚未用 VS 重新编译生成新 exe |
| 自动化测试脚本 | 已有多份 | `.ps1`, `.sh` | Linux 脚本较完整，Windows 五组脚本已有 |
| 当前最终参数组合 | 已落地 | `datalink.c` 宏定义 | `2300/300/Queue2` |
| 当前测试覆盖 | 较完整 | summary/log 目录 | Windows 五组基于旧 2500 exe；Linux 最终 1200s 覆盖当前 2300 的默认误码 flood 和高误码 flood |

## 5. 当前最终参数与协议配置

以下来自当前两份 `datalink.c` 真实内容：

```c
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define DATA_TIMER 2300
#define ACK_TIMER 300
#define FRAME_HEAD_LEN 3
#define FRAME_CRC_LEN 4
#define FRAME_DATA_LEN (FRAME_HEAD_LEN + PKT_LEN)
#define FRAME_MAX_LEN (FRAME_DATA_LEN + FRAME_CRC_LEN)
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

| 参数 | 当前值 | 来源位置 | 作用 | 是否建议继续调整 |
|---|---:|---|---|---|
| `MAX_SEQ` | 15 | `datalink.c:7` | 序号空间 0..15 | 不建议调，SR 窗口依赖该空间 |
| `NR_BUFS` | 8 | `datalink.c:8` | 发送/接收窗口大小 | 不建议调，窗口 4 方向已观察到变差 |
| `DATA_TIMER` | 2300 ms | `datalink.c:9` | DATA 单帧重传超时 | 当前最终值；继续优化应优先看重复 NAK，而不是继续盲调 |
| `ACK_TIMER` | 300 ms | `datalink.c:10` | 延迟 ACK timer | 暂不调；`ACK_TIMER=250` 单独表现差 |
| `FRAME_HEAD_LEN` | 3 bytes | `datalink.c:11` | `kind/seq/ack` | 不调 |
| `FRAME_CRC_LEN` | 4 bytes | `datalink.c:12` | CRC32 | 不调 |
| `FRAME_DATA_LEN` | 259 bytes | `datalink.c:13` | DATA 不含 CRC 长度 | 不调 |
| `FRAME_MAX_LEN` | 263 bytes | `datalink.c:14` | DATA 最大物理帧长度 | 不调 |
| `PHL_QUEUE_LIMIT` | `FRAME_DATA_LEN*2` | `datalink.c:15` | 物理发送队列节流 | 当前稳定；`*3` 两端不均衡 |

`DATA_TIMER=2300` 的依据：

- 120 秒高误码小矩阵中 `dt2300` 平均最好：A 57.39%，B 60.22%。
- 300 秒最小矩阵中 `candidate_dt2300` 高误码 A/B 均优于基线，默认误码只小幅波动。
- 最终 1200 秒 Linux 当前代码复测：默认误码 A 93.24%、B 92.58%；高误码 A 55.52%、B 55.46%；Quit 均 yes，fatal 为 0。

## 6. 本轮 LOG01 之后的主要代码修改

### 6.1 DATA_TIMER 从 2500 调整为 2300

- 修改文件：`Lab1-Windows-VS2019/datalink.c`, `Lab1-linux/datalink.c`
- 涉及宏：`DATA_TIMER`
- 修改前问题：LOG01 中 `DATA_TIMER=2500` 高误码下偏保守，NAK 损坏后等待 timeout 时间长。
- 修改后行为：DATA 单帧重传 timer 缩短到 2300 ms。
- 解决的问题：高误码下恢复等待略缩短，同时避免 2000 ms 那类过激参数。
- 是否已验证：已验证 Linux 当前代码。
- 相关测试：`Lab1-linux/final-validation-20260512-095621/summary.csv`
- 风险或待确认：Windows `datalink.exe` 尚未用 VS 重新编译，因此 Windows 当前 exe 不能证明 2300 版结果。

### 6.2 修正 ACK/NAK 帧格式注释

- 修改文件：`Lab1-Windows-VS2019/datalink.h`, `Lab1-linux/datalink.h`
- 涉及内容：ACK/NAK 注释。
- 修改前问题：注释写作 `KIND(1) | ACK(1) | CRC(4)`，与实际 `send_data_frame` 发送的 3 字节头不一致。
- 修改后行为：注释改为 `KIND(1) | SEQ(1) | ACK(1) | CRC(4)`。
- 解决的问题：避免报告/答辩/后续维护中 wire format 误解。
- 是否已验证：注释改动不影响运行；当前 `datalink.c` 仍按 3 字节头发送。
- 风险或待确认：无协议行为变化。

### 6.3 增强 300 秒最小矩阵脚本

- 修改文件：`Lab1-linux/verify-minimal-matrix.sh`
- 涉及逻辑：A 端启动失败检测、端口重试。
- 修改前问题：一次 300 秒矩阵出现 A 端 `failed to bind TCP port`，导致 summary 把环境端口失败混入参数结果。
- 修改后行为：A 端若因 bind/FATAL/Abort 立即退出，自动换端口重试，最多 5 次。
- 解决的问题：减少端口占用对参数矩阵的污染。
- 是否已验证：有效矩阵 `minimal-matrix-20260512-092212` 全部 Quit，fatal 为 0。
- 风险或待确认：只处理 A 端启动早期 bind 失败；若 B 端连接异常仍需人工看日志。

### 6.4 新增最终验证脚本

- 修改文件：`Lab1-linux/verify-final-dt2300.sh`
- 涉及逻辑：编译当前 Linux 源码，连续跑 1200s 默认误码 flood 和 1200s 高误码 flood，生成 summary。
- 修改前问题：最终复测命令散落，不利于复现。
- 修改后行为：脚本统一执行 `make clean`、`make`、A/B 启动、fatal 扫描和 summary 提取。
- 解决的问题：当前最终参数复测可复现。
- 是否已验证：已生成 `final-validation-20260512-095621/summary.csv`。
- 风险或待确认：只覆盖 Linux flood 两组；Windows 当前 2300 版仍需重新编译复测。

### 6.5 Linux lprintf.h 兼容修复

- 修改文件：`Lab1-linux/lprintf.h`
- 涉及声明：`lprintf`, `__v_lprintf`
- 修改前问题：头文件声明为 `int`，`lprintf.c` 实现为 `size_t`，GCC 编译失败。
- 修改后行为：声明改为 `size_t`。
- 解决的问题：WSL `make` 可通过。
- 是否已验证：Linux 60 秒短测、120 秒矩阵、300 秒矩阵、1200 秒最终复测均依赖该修复。
- 风险或待确认：这是框架头文件的最小兼容修复；不建议继续改框架代码。

### 6.6 未改变的协议逻辑

以下模块相对 LOG01 没有协议行为级改动：

- NAK 策略：仍是 `no_nak` 单次去重，尚未实现限速重复 NAK。
- ACK 处理：仍是非 NAK 帧处理累计 ACK，NAK 的 `ack` 字段不参与累计 ACK 滑窗。
- 接收窗口/乱序缓存：仍使用 `in_buf` 与 `arrived`。
- 物理队列节流：仍为 `FRAME_DATA_LEN*2`。
- `PHYSICAL_LAYER_READY`：仍为空分支，依赖循环末尾 `update_network_layer()`。

## 7. 当前核心数据结构说明

| 名称 | 类型 | 含义 | 当前取值/范围 | 使用位置 | LOG02 备注 |
|---|---|---|---|---|---|
| `struct FRAME` | 结构体 | 接收解析临时帧 | `kind/seq/ack/data/padding` | `FRAME_RECEIVED` | 不作为 wire format 直接发送 |
| `FRAME_DATA` | 宏 | DATA 帧类型 | 1 | `datalink.h` | DATA 总长 263 字节 |
| `FRAME_ACK` | 宏 | ACK 帧类型 | 2 | `datalink.h` | 实际头为 3 字节 |
| `FRAME_NAK` | 宏 | NAK 帧类型 | 3 | `datalink.h` | `ack` 字段表示请求重传序号 |
| `ack_expected` | `static unsigned char` | 发送窗口下界 | 0..15 | ACK 滑窗、timeout 判断 | 收到累计 ACK 后递增 |
| `next_frame_to_send` | `static unsigned char` | 下一个发送序号 | 0..15 | 网络层取包发送 | 每发新 DATA 后递增 |
| `frame_expected` | `static unsigned char` | 接收窗口下界 | 0..15 | DATA 接收、ACK/NAK 生成 | NAK 请求该序号 |
| `too_far` | `static unsigned char` | 接收窗口右边界后一位 | 0..15 | 接收窗口判断 | 初始化为 `NR_BUFS` |
| `nbuffered` | `static unsigned char` | 未确认发送帧数 | 0..8 | 流控、ACK 滑窗 | 决定网络层开关 |
| `no_nak` | `static unsigned char` | 是否允许对当前缺口发 NAK | 0/1 | CRC 错误、乱序 DATA | 高误码下仍是潜在优化点 |
| `out_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 发送缓存 | 8 个分组 | 新发、NAK/timeout 重传 | 下标 `seq % NR_BUFS` |
| `in_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 接收乱序缓存 | 8 个分组 | DATA 接收、交付 | 下标 `seq % NR_BUFS` |
| `arrived` | `unsigned char[NR_BUFS]` | 接收缓存占用标记 | 0/1 | 按序交付循环 | 交付后清 0 |

当前没有新增 NAK 重复控制变量，也没有新增统计变量。

## 8. 当前核心函数说明

### `main(int argc, char **argv)`

- 参数含义：命令行参数透传给实验框架。
- 主要功能：初始化协议状态并进入事件循环。
- 关键逻辑：调用 `protocol_init`，清零窗口和缓存，`enable_network_layer()`，然后 `wait_for_event(&arg)`。
- 调用时机：程序入口。
- 相比 LOG01：协议结构未变，宏参数 `DATA_TIMER` 已变为 2300。
- 注意事项：事件循环内不要阻塞。

### `between(unsigned char a, unsigned char b, unsigned char c)`

- 参数含义：判断环形序号 `b` 是否位于 `[a,c)`。
- 主要功能：处理 ACK、NAK、DATA 窗口判断中的回绕。
- 关键逻辑：覆盖普通区间与跨 0 区间。
- 调用时机：ACK 滑窗、NAK/timeout 合法性检查、接收窗口判断。
- 相比 LOG01：无变化。

### `update_network_layer(void)`

- 参数含义：无。
- 主要功能：根据发送窗口和物理发送队列启停网络层。
- 关键逻辑：`nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 时启用。
- 调用时机：事件分支结束后，`NETWORK_LAYER_READY` 分支内也会调用。
- 相比 LOG01：无变化。

### `send_data_frame(...)`

- 参数含义：`kind` 为 DATA/ACK/NAK；`frame_nr` 为 DATA 序号；`frame_expected` 用于生成 ACK/NAK 字段；`buffer` 为 DATA payload。
- 主要功能：统一封装 DATA/ACK/NAK，追加 CRC，调用 `send_frame`。
- 关键逻辑：ACK 字段默认为最后一个按序收到的序号；NAK 特殊将 `ack` 设为 `frame_expected`；DATA 帧启动 `DATA_TIMER`。
- 调用时机：新发 DATA、ACK_TIMEOUT、坏帧/乱序触发 NAK、收到 NAK、DATA_TIMEOUT。
- 相比 LOG01：函数未变，但 DATA timer 参数由 2500 变为 2300。

### `FRAME_RECEIVED` 分支

- 主要功能：接收物理帧、CRC 检查、解析、ACK/NAK/DATA 处理。
- ACK 处理：若 `f.kind != FRAME_NAK`，使用 `between(ack_expected, f.ack, next_frame_to_send)` 滑动发送窗口。
- NAK 处理：`f.ack` 解释为请求重传序号，若该序号仍在发送窗口内，只重传该帧。
- DATA 处理：接收窗口内未缓存帧存入 `in_buf`，乱序且 `no_nak` 为真时发送 NAK，连续到达后批量 `put_packet`。
- 相比 LOG01：逻辑无变化。
- 注意事项：NAK 不参与累计 ACK 滑窗；这是关键正确性点。

### `DATA_TIMEOUT`

- 主要功能：单帧超时重传。
- 关键逻辑：只有 `arg` 仍位于当前发送窗口内才重传。
- 相比 LOG01：逻辑无变化；timer 时限为 2300。

### `ACK_TIMEOUT`

- 主要功能：发送纯 ACK。
- 关键逻辑：调用 `send_data_frame(FRAME_ACK, 0, frame_expected, NULL)`。
- 相比 LOG01：无变化。

## 9. 当前事件驱动流程

当前主循环结构仍为：

```c
for (;;) {
    event = wait_for_event(&arg);
    switch (event) {
        ...
    }
    update_network_layer();
}
```

| 事件 | 触发条件 | 状态修改 | 发送帧/timer | 是否调用 `update_network_layer` | 与 LOG01 差异 |
|---|---|---|---|---|---|
| `NETWORK_LAYER_READY` | 网络层有包可取 | 写 `out_buf`，`nbuffered++`，`next_frame_to_send++` | 发送 DATA，启动 DATA timer，停止 ACK timer | 分支内和循环末尾都会调用 | 无逻辑差异 |
| `PHYSICAL_LAYER_READY` | 物理层发送队列降至阈值以下 | 无 | 无 | 循环末尾调用 | 仍为空分支 |
| `FRAME_RECEIVED` | 收到物理帧 | 可能滑动 ACK、缓存 DATA、推进接收窗口、更新 `no_nak` | 可能发 NAK/DATA 重传，收到 DATA 后启动 ACK timer | 循环末尾调用 | 无逻辑差异 |
| `DATA_TIMEOUT` | 某 DATA timer 超时 | 无直接窗口移动 | 若序号仍有效则重传该 DATA | 循环末尾调用 | timer 时限为 2300 |
| `ACK_TIMEOUT` | ACK timer 超时 | 无 | 发送纯 ACK | 循环末尾调用 | 无逻辑差异 |

## 10. 当前测试记录总表

| 序号 | 测试场景 | 平台 | 命令/脚本 | 运行时间 | A 利用率 | B 利用率 | Err/BER | Quit | Fatal scan | 日志路径 | 结论 |
|---:|---|---|---|---:|---:|---:|---|---|---:|---|---|
| 1 | 无误码普通传输 | Windows | `Run-FivePerformanceTests.ps1` | 1200s | 51.70% | 96.97% | 0 / 0 | yes | 0 | `performance-tests-20260511-212030/accepted-summary.csv` | 旧 Windows exe 证据，可用于五组表 |
| 2 | 默认误码普通传输 | Windows | rerun 结果 | 1200s | 49.52% | 93.40% | A 49, B 96 / 1e-5 | yes | 0 | `02_plain_ber1e-5-rerun` | 首跑有 System too busy，最终用 rerun |
| 3 | 无误码双向 flood | Windows | `Run-FivePerformanceTests.ps1` | 1200s | 96.97% | 96.96% | 0 / 0 | yes | 0 | `03_flood_ber0` | 接近参考 97% |
| 4 | 默认误码双向 flood | Windows | `Run-FivePerformanceTests.ps1` | 1200s | 93.20% | 92.80% | A 97, B 95 / 1e-5 | yes | 0 | `04_flood_ber1e-5` | 旧 2500 参数证据 |
| 5 | 高误码 `1e-4` flood | Windows | `Run-FivePerformanceTests.ps1` | 1200s | 55.02% | 51.73% | A 821, B 817 / 1e-4 | yes | 0 | `05_flood_ber1e-4` | 旧 2500 参数证据 |
| 6 | 高误码 `1e-4` flood 复测 | Windows | 手动 A/B | 1200s | 56.78% | 54.22% | A 825, B 817 / 1e-4 | yes | clean | `high-ber-longtest-20260511-234732` | 旧 2500 参数独立复测 |
| 7 | 默认误码 flood 短测 | Linux/WSL | `verify-linux-run.sh 60 63620` | 60s | 95.00% | 93.24% | A 3, B 5 / 约 1e-5 | yes | clean | `linux-run-verify-20260512` | 编译运行短测通过 |
| 8 | 高误码小矩阵 | Linux/WSL | `sweep-linux-params.sh 120 63700` | 120s/组 | 见 CSV | 见 CSV | 1e-4 | yes | 0 | `param-sweep-20260512-002725/summary.csv` | 筛出 `dt2300` |
| 9 | 候选默认误码验证 | Linux/WSL | `validate-candidates-default.sh` | 120s/组 | dt2300 A 93.64% | dt2300 B 91.40% | 约 1e-5 | yes | 0 | `default-validation.csv` | dt2300 默认误码未明显退化 |
| 10 | 300 秒最小矩阵 | Linux/WSL | `verify-minimal-matrix.sh 300 52000` | 300s/组 | 见 CSV | 见 CSV | default/high | yes | 0 | `minimal-matrix-20260512-092212/summary.csv` | 确认 `dt2300` 优于基线高误码 |
| 11 | 当前最终默认误码 flood | Linux/WSL | `verify-final-dt2300.sh 1200 53000` | 1200s | 93.24% | 92.58% | A 93, B 95 / 1e-5 | yes | 0 | `final-validation-20260512-095621/default` | 当前 2300 参数最终证据 |
| 12 | 当前最终高误码 flood | Linux/WSL | `verify-final-dt2300.sh 1200 53000` | 1200s | 55.52% | 55.46% | A 838, B 840 / 1e-4 | yes | 0 | `final-validation-20260512-095621/high` | 当前 2300 参数最终证据 |

备注：

- Windows 五组测试日志对应的是当时 Windows `datalink.exe`，不是当前修改后重新编译的 2300 版 exe。
- 当前代码版本的强证据是 Linux `final-validation-20260512-095621`，因为脚本会先编译当前 Linux 源码。
- 对最新有效目录的 fatal 扫描未发现协议致命错误；旧 Windows 首跑目录中曾有 `System too busy`，已不作为最终表数据。

## 11. 性能结论

### 无误码普通传输

当前最好证据来自 Windows 五组 20 分钟：A 51.70%，B 96.97%。满足验收稳定性。该结果来自旧 Windows exe，当前 2300 源码理论上不影响无误码普通模式太多，但若报告要完全严格，需重新编译 Windows exe 后复测。

### 默认误码普通传输

当前最好证据来自 Windows rerun：A 49.52%，B 93.40%。Quit 和 fatal 均正常。该结果用于五组表格可接受。

### 无误码双向 flood

当前最好证据来自 Windows 五组：A 96.97%，B 96.96%，基本达到参考 97%。

### 默认误码双向 flood

旧 Windows 2500 参数：A 93.20%，B 92.80%。当前 Linux 2300 参数最终复测：A 93.24%，B 92.58%。结论是 `DATA_TIMER=2300` 保持了默认误码 flood 的 92% 到 93% 水平。

### 高误码 `--ber=1e-4` 双向 flood

旧 Windows 2500 参数五组：A 55.02%，B 51.73%；旧 Windows 复测：A 56.78%，B 54.22%。当前 Linux 2300 参数最终复测：A 55.52%，B 55.46%。结论是 2300 让两端更均衡，低端从约 51.73%/54.22% 提升到 55.46%。

### Linux 版测试

Linux 已经完成编译、60 秒短测、120 秒参数扫测、300 秒最小矩阵、1200 秒最终默认误码 flood 和高误码 flood。Linux 五组完整 20 分钟尚未跑完。

### Windows 版测试

Windows 已完成五组 20 分钟和高误码复测，但这些结果基于当时 exe。当前源码改为 2300 后，尚未确认 VS 重新编译和 Windows 2300 版复测。

## 12. 当前已知问题与风险点

| 问题 | 影响 | 当前证据 | 建议处理 | 优先级 |
|---|---|---|---|---|
| Windows 当前源码已改但 exe 未重新编译 | Windows 运行可能仍是旧 2500 参数 | 未见 VS 编译新 exe 的证据 | 用 VS 重新构建，至少跑默认误码 flood 和高误码 flood 1200s | P0 |
| Git 工作区很脏 | 后续 review 和提交困难 | `git status --short` 大量 M/??/D | 分组提交或清理/归档日志产物 | P0 |
| 测试产物和归档目录很多 | 仓库体积和搜索噪声增大 | `archive-artifacts-*`, `param-sweep-*`, `minimal-matrix-*` | 保留关键 summary/log，其他移出或 gitignore | P0 |
| Windows/Linux `datalink.h` 哈希不同 | 可能误以为内容不同 | 逐行 Compare-Object 无差异；hash 不同 | 视为换行/文件结尾差异，可统一行尾 | P2 |
| Linux 五组完整 20 分钟未完成 | Linux 平台完整验收证据不足 | 当前只有 flood 最终 1200s 和短测/矩阵 | 若需 Linux 验收，补跑五组 | P1 |
| NAK 仍只发一次 | 高误码下 NAK 损坏后仍可能等 timeout | 当前代码仍使用 `no_nak` 布尔去重 | 后续可设计限速重复 NAK，先小矩阵验证 | P1 |
| `PHYSICAL_LAYER_READY` 分支为空 | 可读性和极端场景恢复依赖循环末尾 | 当前代码分支仍 `break` | 可显式调用 `update_network_layer()`，但要回归测试 | P2 |
| 调试输出可能影响性能 | `-d3` 长测会拖慢吞吐 | 正式脚本未开 debug | 正式测试继续保持 debug mask 0 | P2 |
| 临时 sweep 源码可能混淆主源码 | 后续读错文件会误判参数 | `param-sweep-*` 内有复制源码 | 明确主源码只看 Windows/Linux 顶层 `datalink.c` | P1 |
| `DEVELOPMENT0_RECORD.md` 删除但 `DEVELOPMENT_LOG00_RECORD.md` 存在 | 记录命名混乱 | Git 显示 `DEVELOPMENT0_RECORD.md` D，LOG00 新增 | 后续统一保留 `DEVELOPMENT_LOG00/01/02_RECORD.md` | P1 |

## 13. 后续开发建议

### P0：必须完成

- 用 Visual Studio 重新编译 Windows `datalink.exe`，确认 exe 确实包含 `DATA_TIMER=2300`。
- 对 Windows 2300 版至少补跑 1200s 默认误码 flood 和 1200s 高误码 flood；若时间充足，重跑五组 20 分钟。
- 清理或归档 Git 工作区：保留 `DEVELOPMENT_LOG02_RECORD.md`、优化笔记、关键脚本和关键 summary；把大型日志、`.o`、临时 sweep 源码纳入 `.gitignore` 或移出。
- 提交当前稳定源码和关键测试脚本，避免后续丢失 2300 版本。

### P1：建议优化

- 研究 NAK 丢失后的限速重复 NAK：例如 `NAK_REPEAT_INTERVAL=600/800ms`，不改变窗口大小。
- 对重复 NAK 方案先跑 300s 默认误码 flood + 300s 高误码 flood，小矩阵只测 baseline/repeat600/repeat800。
- 若继续写报告，整理“当前指标 vs Selective 参考表现”的表格：无误码 flood 基本贴近，默认误码 flood 约低 2%，高误码平均接近但 B 端历史参考不完全对齐。
- 可以统一 Windows/Linux 测试脚本输出格式，减少手工汇总。

### P2：可选加分

- 拆分 `FRAME_RECEIVED` 大分支为 `handle_ack`、`handle_nak`、`handle_data`。
- 增加统计字段：NAK 次数、timeout 次数、重传次数、重复 NAK 次数，便于解释高误码性能。
- 添加清理脚本，自动移动 `.o`、临时源码、失败空跑目录。
- 给 `struct FRAME.padding` 增加注释，说明不参与 wire format。

## 14. 给下一个聊天窗口的接手提示

```text
这是计算机网络实验一：数据链路层滑动窗口协议项目。请优先阅读 DEVELOPMENT_LOG02_RECORD.md，它是最新工程接手记录，优先级高于 DEVELOPMENT_LOG00_RECORD.md、DEVELOPMENT_LOG01_RECORD.md 和 HIGH_BER_OPTIMIZATION_NOTES.md 中的旧状态。

当前目录：
C:\Users\28641\Desktop\计网\实验

主代码：
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\datalink.c
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-linux\datalink.c

当前实现是 Selective Repeat with Piggybacking，不是 Go-Back-N。支持 CRC32、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable 和物理队列节流。当前最终参数：
MAX_SEQ=15, NR_BUFS=8, DATA_TIMER=2300, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2。

Windows/Linux 两份 datalink.c 内容一致。不要随意修改 protocol.c/protocol.h/crc32.c/lprintf.c/getopt.c；Linux lprintf.h 仅做过返回类型 size_t 的最小兼容修复。

已验证：
1. Windows 五组 20 分钟旧 exe 结果：Lab1-Windows-VS2019/performance-tests-20260511-212030/accepted-summary.csv。
2. Windows 高误码旧 exe 复测：high-ber-longtest-20260511-234732。
3. Linux 300 秒最小矩阵：Lab1-linux/minimal-matrix-20260512-092212/summary.csv。
4. Linux 当前 DATA_TIMER=2300 最终 1200 秒复测：Lab1-linux/final-validation-20260512-095621/summary.csv，默认误码 A/B=93.24%/92.58%，高误码 A/B=55.52%/55.46%，fatal=0。

下一步 P0：
1. 用 Visual Studio 重新编译 Windows datalink.exe，让 Windows exe 包含 DATA_TIMER=2300。
2. 补跑 Windows 2300 版 1200s 默认误码 flood 和高误码 flood，必要时重跑五组。
3. 清理/归档当前大量测试产物，提交稳定源码、脚本和关键记录。

下一步优化方向不是调窗口，而是理论设计并小矩阵验证“限速重复 NAK”，解决高误码下唯一 NAK 损坏后等待 DATA_TIMEOUT 的问题。
```

## 15. 附录 A：关键代码摘要

### 参数宏

```c
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define DATA_TIMER 2300
#define ACK_TIMER 300
#define FRAME_HEAD_LEN 3
#define FRAME_CRC_LEN 4
#define FRAME_DATA_LEN (FRAME_HEAD_LEN + PKT_LEN)
#define FRAME_MAX_LEN (FRAME_DATA_LEN + FRAME_CRC_LEN)
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

### 帧结构和状态变量

```c
struct FRAME {
    unsigned char kind;
    unsigned char seq;
    unsigned char ack;
    unsigned char data[PKT_LEN];
    unsigned int padding;
};

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

### 发送帧封装

```c
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
        send_data_frame(FRAME_DATA, n, frame_expected, out_buf[n % NR_BUFS]);
}

if (f.kind == FRAME_DATA) {
    if (between(frame_expected, f.seq, too_far) && !arrived[f.seq % NR_BUFS]) {
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

### Timeout 核心

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

## 16. 附录 B：测试命令与复现方式

### Windows

进入目录：

```powershell
cd "C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019"
```

建议先用 Visual Studio 重新编译 `datalink.sln`，确认 `datalink.exe` 更新到 `DATA_TIMER=2300`。

五组测试脚本：

```powershell
powershell -ExecutionPolicy Bypass -File .\Run-FivePerformanceTests.ps1 -DurationSeconds 1200
```

默认误码 flood：

```powershell
.\datalink.exe -t 1200 -f -p 54000 -l .\final-default-A.log A
.\datalink.exe -t 1200 -f -p 54000 -l .\final-default-B.log B
```

高误码 flood：

```powershell
.\datalink.exe -t 1200 -f --ber=1e-4 -p 54020 -l .\final-high-A.log A
.\datalink.exe -t 1200 -f --ber=1e-4 -p 54020 -l .\final-high-B.log B
```

### Linux / WSL

进入目录：

```bash
cd "/mnt/c/Users/28641/Desktop/计网/实验/Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux"
```

编译：

```bash
make clean
make
```

默认误码 flood：

```bash
./datalink -t 1200 -f -p 53000 -l A.log A
./datalink -t 1200 -f -p 53000 -l B.log B
```

高误码 flood：

```bash
./datalink -t 1200 -f --ber=1e-4 -p 53020 -l A.log A
./datalink -t 1200 -f --ber=1e-4 -p 53020 -l B.log B
```

参数矩阵：

```bash
bash sweep-linux-params.sh 120 63700
bash validate-candidates-default.sh param-sweep-20260512-002725 120 63800
bash verify-minimal-matrix.sh 300 52000
```

当前最终验证：

```bash
bash verify-final-dt2300.sh 1200 53000
```

## 17. 附录 C：文件清理/提交建议

建议提交：

- `Lab1-Windows-VS2019/datalink.c`
- `Lab1-linux/datalink.c`
- `Lab1-Windows-VS2019/datalink.h`
- `Lab1-linux/datalink.h`
- `Lab1-linux/lprintf.h`
- `Lab1-linux/verify-linux-run.sh`
- `Lab1-linux/sweep-linux-params.sh`
- `Lab1-linux/validate-candidates-default.sh`
- `Lab1-linux/verify-minimal-matrix.sh`
- `Lab1-linux/verify-final-dt2300.sh`
- `HIGH_BER_OPTIMIZATION_NOTES.md`
- `DEVELOPMENT_LOG00_RECORD.md`
- `DEVELOPMENT_LOG01_RECORD.md`
- `DEVELOPMENT_LOG02_RECORD.md`

建议保留的关键证据：

- `Lab1-Windows-VS2019/performance-tests-20260511-212030/accepted-summary.csv`
- `Lab1-Windows-VS2019/high-ber-longtest-20260511-234732/`
- `Lab1-linux/param-sweep-20260512-002725/summary.csv`
- `Lab1-linux/param-sweep-20260512-002725/default-validation.csv`
- `Lab1-linux/minimal-matrix-20260512-092212/summary.csv`
- `Lab1-linux/final-validation-20260512-095621/summary.csv`

建议删除或 gitignore：

- `*.o`
- Linux 生成的 `datalink` 可执行文件
- 失败/空跑矩阵目录，如已归档的 `minimal-matrix-20260512-085539`、`minimal-matrix-20260512-085832`
- 临时 sweep 源码目录中的 `src/*.o` 和复制源码
- 大量 stdout/stderr/log，如果不需要完整审计，可只保留 summary 和最终 A/B log

不要误删：

- `accepted-summary.csv`
- `final-validation-20260512-095621/summary.csv`
- `minimal-matrix-20260512-092212/summary.csv`
- `HIGH_BER_OPTIMIZATION_NOTES.md`
- 三份 `DEVELOPMENT_LOG*_RECORD.md`
