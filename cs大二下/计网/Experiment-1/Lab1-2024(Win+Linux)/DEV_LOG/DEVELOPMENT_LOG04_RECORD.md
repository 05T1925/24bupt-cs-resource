# 数据链路层滑动窗口协议开发记录 LOG04

## 0. LOG04 生成背景

本记录生成于 `DEVELOPMENT_LOG00_RECORD.md`、`DEVELOPMENT_LOG01_RECORD.md`、`DEVELOPMENT_LOG02_RECORD.md`、`DEVELOPMENT_LOG03_RECORD.md` 与 `HIGH_BER_OPTIMIZATION_NOTES.md` 之后，记录的是 LOG03 之后多轮 Codex 代码迭代、参数验证、Windows/Linux 复测和仓库整理后的当前状态。

LOG04 不是实验报告正文，而是后续另起聊天窗口继续开发、验收、整理报告和提交前清理的接手档案。本文所有结论优先依据当前仓库实证：当前源码、Git 状态、summary、日志尾部、文件哈希和可读输出。若旧日志与当前源码、Git 状态或测试证据冲突，以 LOG04 的当前实证为准。

本次生成 LOG04 的过程中只进行了只读核查，并最终只写入本文档 `DEVELOPMENT_LOG04_RECORD.md`。未主动修改协议源码、脚本、测试数据、工程文件、Git 暂存区或压缩包；未执行 `git add`、`git commit`、`git reset`、`git clean`、删除、移动或重命名操作。

路径修订说明：LOG04 初稿曾按用户临时指令写到 `C:\Users\28641\Desktop\计网\实验\DEVELOPMENT_LOG04_RECORD.md`；用户随后已手动转移到 `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\DEV_LOG\DEVELOPMENT_LOG04_RECORD.md`。本次修订只更正 LOG04 内部路径表述，最终日志位置为仓库内 `Lab1-2024(Win+Linux)/DEV_LOG/DEVELOPMENT_LOG04_RECORD.md`。

## 1. 项目路径与当前仓库状态

| 项目 | 当前实证 |
|---|---|
| 当前核查仓库目录 | `C:\Users\28641\Desktop\Experiment-1` |
| 本轮 Codex shell 工作目录 | `C:\Users\28641\Desktop\计网\实验`（工具会话工作目录，不是核查仓库） |
| 当前 LOG04 文件位置 | `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\DEV_LOG\DEVELOPMENT_LOG04_RECORD.md` |
| 主工程目录 | `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)` |
| Windows 源码路径 | `Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-Windows-VS2019/datalink.c` |
| Linux 源码路径 | `Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux/datalink.c` |
| 仓库内 DEV_LOG 目录 | `Lab1-2024(Win+Linux)/DEV_LOG`；当前 LOG04 已位于该目录 |
| 最近 Git commit | `eb3ec94 Update generated outputs`；`82bacc6 Initial commit` |
| 当前工作区是否干净 | 不干净；存在 staged、unstaged、untracked 混合状态 |
| 本轮 Git 操作 | 只读检查；未暂存、未提交、未重置、未清理 |

### Git 状态摘要

| 类别 | 当前状态 | 关键文件/目录 | 风险 |
|---|---|---|---|
| 源码 | Windows/Linux `datalink.c` 同时存在 staged 与 unstaged 修改；当前工作区版本哈希一致 | `Lab1-Windows-VS2019/datalink.c`、`Lab1-linux/datalink.c`、`datalink.h` | 提交前必须区分 staged 的 `DATA_TIMER=2300` 与 unstaged 的 repeat NAK 修改，避免漏提交或误提交 |
| 脚本 | 大量测试、汇总、矩阵脚本已 staged 或 untracked | `run_current_five_1200.sh`、`protocol_matrix_300s.sh`、`summarize_current_five.py` 等 | 部分脚本属于历史调参脚本，提交前需筛选 |
| 测试结果 | 大量 summary、stdout、stderr、log、运行目录存在 | Windows `performance-tests-20260512-221730/`；Linux `linux-current-five-1200-summary.csv`、`protocol-matrix-20260513-000052/`、`candidate-dt2200-nri800-*` | 大日志和中间目录很多，不宜全部提交 |
| 日志文档 | `DEV_LOG` 目录为 untracked；旧根目录日志存在 staged add/delete/rename 状态；LOG04 已由用户手动迁入 `DEV_LOG` | `DEV_LOG/DEVELOPMENT_LOG00_RECORD.md` 至 `LOG04`、`HIGH_BER_OPTIMIZATION_NOTES.md` | 日志目录迁移尚未通过 Git 清理确认 |
| 编译产物 | Windows exe、Linux 可执行文件、`.o`、VS Release 目录存在 | `datalink.exe`、`datalink`、`protocol.o`、`Win32-Release/` | 一般不建议提交编译产物，除非课程要求 |
| 压缩包 | 旧 rar 删除、多个 zip/rar/工程复制目录存在 | `Lab1-2024(Win+Linux).rar`、`Lab1-2024-Windows&Linux-handin.zip`、submission candidate 目录 | 需要人工确认最终打包口径 |
| 报告材料 | 存在报告、图片、性能表等材料 | `计算机网络实验一报告.docx`、`report/`、`performance_and_parameter_tables.md` | 需与最终源码和测试证据对齐后再定稿 |

## 2. 与 LOG00 / LOG01 / LOG02 / LOG03 / HIGH_BER 的关系

| 记录 | 旧记录核心结论 | 当前是否仍成立 | LOG04 新增/修正 |
|---|---|---|---|
| LOG00 | 早期实现 SR；当时路径在 `C:\Users\28641\Desktop\计网\实验`；`DATA_TIMER=2500`；高误码待补 | 部分成立 | 当前实际核查仓库在 `C:\Users\28641\Desktop\Experiment-1`；最终源码为 `DATA_TIMER=2300`、`NAK_REPEAT_INTERVAL=800`；高误码已有 1200s 证据 |
| LOG01 | Windows 五组多为旧 exe 证据；Linux 短测；`DATA_TIMER=2300` 成为候选 | 部分成立 | Windows 已用当前源码重新编译并完成五组 1200s；Linux 也补齐当前源码五组 1200s |
| LOG02 | `DATA_TIMER=2300` 收敛；当时无 repeat NAK；Linux final validation 证据 | 部分过时 | 当前已加入 repeat NAK；Linux 旧 final validation 不能直接代表当前源码 |
| HIGH_BER | 高误码主要风险为 NAK 丢失后恢复慢；建议优化 timer/NAK | 仍成立为问题背景 | 当前实现采用 repeat NAK 缩短恢复路径，并完成矩阵和长测验证 |
| LOG03 | `2300/800` repeat NAK 为基线；Windows 当前源码尚未复测；Linux 五组未补齐 | 核心参数仍成立，风险状态已变化 | Windows 当前源码五组 1200s 已补；Linux 当前源码五组 1200s 已补；新增 `2200/800` 小矩阵候选但未改入最终源码 |

### LOG03 中仍成立的结论

| LOG03 结论 | 当前状态 |
|---|---|
| 协议实现是 Selective Repeat，不是 Go-Back-N | 仍成立；`out_buf`、`in_buf`、`arrived`、`between()`、单帧重传和乱序缓存仍存在 |
| Windows/Linux 两份 `datalink.c` 应保持同步 | 当前工作区两份 `datalink.c` SHA256 完全一致 |
| 主线参数 `MAX_SEQ=15`、`NR_BUFS=8`、`DATA_TIMER=2300`、`ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800`、`PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2` | 当前源码仍为这些值 |
| `PHL_QUEUE_LIMIT = FRAME_DATA_LEN * 2` 作为主线 | 当前源码仍保持 |

### LOG03 中已解决的风险

| LOG03 风险 | 当前证据 |
|---|---|
| Windows 当前源码未重新编译复测 | `Lab1-Windows-VS2019/datalink.exe` 时间为 `2026-05-12 22:16:53`；`performance-tests-20260512-221730/summary.csv` 有五组 1200s |
| Linux 当前源码五组长测未补齐 | `Lab1-linux/linux-current-five-1200-summary.csv` 有五组 1200s |
| repeat800 只有部分 Linux 证据 | 当前已有 Windows 五组、Linux 五组、300s 矩阵、`2200/800` 候选长测证据 |

### LOG03 后仍存在的风险

| 风险 | 当前状态 |
|---|---|
| Git 工作区混乱 | 仍存在；大量 staged/untracked 日志、脚本、产物、压缩包、复制目录 |
| 提交候选文件筛选 | 仍待人工确认；不能直接全量提交 |
| 默认误码性能是否达到参考 | 当前约 92-93%，接近但可能低于某些参考数据，需报告中客观描述 |
| 是否采用 `DATA_TIMER=2200` | 小矩阵和候选长测显示有潜力，但当前最终源码仍是 2300，需人工决定是否改动 |

## 3. 当前实现状态总览

| 项 | 当前实现状态 | 文件/函数证据 |
|---|---|---|
| Selective Repeat / Go-Back-N | 当前为 Selective Repeat | `between()`、`ack_expected`、`next_frame_to_send`、`frame_expected`、`too_far`、`out_buf`、`in_buf`、`arrived` |
| 双工通信 | 支持 A/B 双向发送与接收 | `main()` 事件循环；五组测试 A/B 双端利用率 |
| CRC | 使用 `crc32()` 对整帧校验 | `send_data_frame()` 写 CRC；`FRAME_RECEIVED` 分支校验 CRC |
| 手工帧序列化 | 使用 `struct FRAME` 字段和 `memcpy` 手工拼接 | `struct FRAME`、`FRAME_HEAD_LEN`、`FRAME_CRC_LEN` |
| ACK | 支持单独 ACK 与 piggyback ACK | `FRAME_ACK`、`send_data_frame(FRAME_ACK, ...)`、DATA 帧 `ack` 字段 |
| piggyback ACK | DATA 帧携带 `ack=(frame_expected+MAX_SEQ)%(MAX_SEQ+1)` | `send_data_frame()` |
| ACK timer | 用于延迟 ACK 或 repeat NAK 节奏 | `ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800`、`ACK_TIMEOUT` 分支 |
| NAK | 支持 NAK | `FRAME_NAK`、`send_nak()`、CRC 错误/乱序缺帧触发 |
| repeat NAK | 当前存在 repeat NAK | `ACK_TIMEOUT` 中 `!no_nak && NAK_REPEAT_INTERVAL > 0` 时再次 `send_nak()` |
| DATA timer | 每个发送窗口帧独立启动/停止 | `start_timer(frame_nr, DATA_TIMER)`、`stop_timer(...)` |
| 单帧重传 | 超时仅重传 `event_arg` 对应帧 | `DATA_TIMEOUT` 分支调用 `send_data_frame(FRAME_DATA, event_arg, ...)` |
| 接收乱序缓存 | 支持缓存窗口内乱序 DATA | `in_buf[]`、`arrived[]`、`while(arrived[frame_expected])` |
| 发送缓存 | 网络层包进入 `out_buf[next_frame_to_send]` | `NETWORK_LAYER_READY` 分支 |
| 序号回绕 | 使用 `MAX_SEQ+1` 模运算 | 多处分支取模，`between()` 判断窗口区间 |
| 网络层 enable/disable | 根据 `nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 控制 | `update_network_layer()` |
| 物理层队列节流 | 限制底层队列，避免 ACK/NAK 被 DATA 压住 | `PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)` |
| Windows 编译运行 | 当前源码已重新编译并跑五组 1200s | `datalink.exe` 时间戳与 `performance-tests-20260512-221730/summary.csv` |
| Linux 编译运行 | 当前源码已跑五组 1200s | `linux-current-five-1200-summary.csv` |
| 自动化测试脚本 | 存在多类脚本 | `run_current_five_1200.sh`、`run_performance_tests.ps1`、`protocol_matrix_300s.sh` 等 |
| 当前最终参数 | 源码仍为 `DATA_TIMER=2300`、`NAK_REPEAT_INTERVAL=800` | 两份 `datalink.c` 宏定义一致 |
| 当前验收证据完整度 | 核心长测已补齐；仓库整理仍未完成 | Windows/Linux 五组 summary 完整，Git 状态仍复杂 |

## 4. 当前最终参数与协议配置

### datalink.c 哈希与同步状态

| 文件 | SHA256 |
|---|---|
| `Lab1-Windows-VS2019/datalink.c` | `627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43` |
| `Lab1-linux/datalink.c` | `627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43` |

结论：当前工作区两份 `datalink.c` 完全一致。注意：Git 状态中两份文件同时有 staged 与 unstaged 修改，说明“工作区当前版本一致”，不等价于“已提交版本一致”。

### 当前宏参数

| 宏 | 当前值 | 作用 | 相比 LOG03 | 建议 |
|---|---|---|---|---|
| `MAX_SEQ` | `15` | 序号空间 0-15 | 未变 | 保持 |
| `NR_BUFS` | `((MAX_SEQ + 1) / 2)`，即 8 | SR 窗口大小 | 未变 | 保持；不要先动窗口 |
| `DATA_TIMER` | `2300` | DATA 帧重传超时 | 未变 | 当前最终源码保持；`2200/800` 可作为候选另行确认 |
| `ACK_TIMER` | `300` | 普通延迟 ACK 时间 | 未变 | 保持 |
| `NAK_REPEAT_INTERVAL` | `800` | NAK 已挂起时 repeat NAK 间隔 | LOG03 已引入，当前仍存在 | 保持；矩阵显示 800 均衡 |
| `FRAME_HEAD_LEN` | `3` | kind/ack/seq 三字节头 | 未变 | 保持 |
| `FRAME_CRC_LEN` | `4` | CRC32 长度 | 未变 | 保持 |
| `FRAME_DATA_LEN` | `(FRAME_HEAD_LEN + PKT_LEN)` | DATA 帧不含 CRC 长度 | 未变 | 保持 |
| `FRAME_MAX_LEN` | `(FRAME_DATA_LEN + FRAME_CRC_LEN)` | 最大帧长 | 未变 | 保持 |
| `PHL_QUEUE_LIMIT` | `(FRAME_DATA_LEN * 2)` | 物理层队列节流阈值 | 未变 | 保持主线；此前 *3 表现不佳 |

## 5. LOG03 之后的主要代码修改

### 5.1 DATA_TIMER 从 staged 候选收敛到 2300

| 字段 | 内容 |
|---|---|
| 修改文件 | Windows/Linux `datalink.c` |
| 涉及函数/宏 | `DATA_TIMER` |
| 修改前问题 | 旧阶段存在 `DATA_TIMER=2500` 或其他候选，默认误码和高误码恢复空间有限 |
| 修改后行为 | 当前源码为 `DATA_TIMER=2300` |
| 与 LOG03 的差异 | LOG03 已把 2300 作为主线；当前仓库继续保持 |
| 测试证据 | Windows 五组、Linux 五组、矩阵均包含 2300 主线证据 |
| 风险/备注 | `2200/800` 候选长测显示高误码略有潜力，但未改入当前最终源码 |

### 5.2 repeat NAK 逻辑进入当前源码

| 字段 | 内容 |
|---|---|
| 修改文件 | Windows/Linux `datalink.c` |
| 涉及函数/宏 | `NAK_REPEAT_INTERVAL`、`send_nak()`、`send_data_frame()`、`FRAME_RECEIVED`、`ACK_TIMEOUT` |
| 修改前问题 | NAK 丢失后只能等 DATA timeout，尤其高误码下恢复慢 |
| 修改后行为 | 当 `no_nak==0` 且 ACK timer 到期时，按 `NAK_REPEAT_INTERVAL=800` 重新发送 NAK；收到期望 DATA 并交付后恢复 `no_nak=1` |
| 与 LOG03 的差异 | LOG03 已记录 repeat NAK；当前仓库确认该逻辑仍在最终源码中 |
| 测试证据 | Linux 300s 矩阵和 1200s 高误码；Windows/Linux 当前源码五组 1200s |
| 风险/备注 | 当前实现仍是单一 `no_nak` 状态，不是更精细的 `nak_pending_seq + last_nak_ms` 模型 |

### 5.3 ACK timer 同时承担 delayed ACK 与 repeat NAK 节奏

| 字段 | 内容 |
|---|---|
| 修改文件 | Windows/Linux `datalink.c` |
| 涉及函数/宏 | `send_data_frame()`、`send_nak()`、`ACK_TIMEOUT`、`ACK_TIMER`、`NAK_REPEAT_INTERVAL` |
| 修改前问题 | 只有 delayed ACK 时，NAK 丢失恢复慢；若无节制发送 NAK，可能增加控制帧拥塞 |
| 修改后行为 | 普通 ACK 使用 `ACK_TIMER=300`；NAK pending 时使用 `NAK_REPEAT_INTERVAL=800` 驱动 repeat NAK |
| 与 LOG03 的差异 | 当前代码证实 LOG03 的设计被保留 |
| 测试证据 | 高误码 flood 1200s Windows 约 58%；Linux 约 58-59%；候选 2200/800 约 59% |
| 风险/备注 | ACK timer 语义变复合，后续修改需谨慎，避免破坏 delayed ACK |

### 5.4 Windows/Linux 源码同步

| 字段 | 内容 |
|---|---|
| 修改文件 | 两份 `datalink.c` |
| 涉及函数/宏 | 全文件 |
| 修改前问题 | 多平台实验容易出现 Windows/Linux 版本漂移 |
| 修改后行为 | 当前两份文件 SHA256 完全一致 |
| 与 LOG03 的差异 | LOG03 认为两份源码当时同步；LOG04 再次实证同步 |
| 测试证据 | 哈希一致；两端均有当前源码五组 1200s |
| 风险/备注 | Git index/worktree 状态复杂，提交时仍需再次确认 |

### 5.5 Windows 当前源码重新编译并复测

| 字段 | 内容 |
|---|---|
| 修改文件 | `Lab1-Windows-VS2019/datalink.exe` 有变更；源码未在本轮改动 |
| 涉及函数/宏 | 当前 `datalink.c` 全部逻辑 |
| 修改前问题 | LOG03 明确 Windows 当前源码尚未重新编译复测 |
| 修改后行为 | 当前 exe 时间戳为 `2026-05-12 22:16:53`，随后有五组 1200s summary |
| 与 LOG03 的差异 | 该风险已解决 |
| 测试证据 | `performance-tests-20260512-221730/summary.csv` |
| 风险/备注 | `datalink.exe` 是否提交需按课程要求人工确认 |

### 5.6 Linux 当前源码五组 1200s 补齐

| 字段 | 内容 |
|---|---|
| 修改文件 | Linux 可执行文件和测试输出目录；源码本轮未改 |
| 涉及函数/宏 | 当前 `datalink.c` 全部逻辑 |
| 修改前问题 | LOG03 中 Linux repeat800 只有部分长测证据，默认误码 flood 1200s尤其需要补 |
| 修改后行为 | 当前存在五组 1200s summary |
| 与 LOG03 的差异 | 五组长测证据补齐 |
| 测试证据 | `Lab1-linux/linux-current-five-1200-summary.csv` |
| 风险/备注 | 日志目录较多，提交前需筛选 summary 与必要尾部证据 |

### 5.7 小矩阵扫描与候选 `2200/800`

| 字段 | 内容 |
|---|---|
| 修改文件 | 测试目录与 summary；当前最终源码未改成 2200 |
| 涉及函数/宏 | `DATA_TIMER`、`NAK_REPEAT_INTERVAL` |
| 修改前问题 | repeat NAK 改变恢复路径后，原 `DATA_TIMER=2300` 可能不是最优 |
| 修改后行为 | 扫描 `DATA_TIMER=2200/2300/2400` 与 `NAK_REPEAT_INTERVAL=700/800/900/1000` 的小矩阵 |
| 与 LOG03 的差异 | LOG03 后新增矩阵实证；`dt2200_nri800` 在 300s 高误码中表现最好 |
| 测试证据 | `protocol-matrix-20260513-000052/summary.csv`；`candidate-dt2200-nri800-valid-1200-summary.csv` |
| 风险/备注 | 2200 候选尚未进入最终源码；首次 high 长测遇到 `System too busy`，已通过 rerun 获得有效 high 结果 |

### 5.8 脚本、summary 和报告材料显著增加

| 字段 | 内容 |
|---|---|
| 修改文件 | 多个 `.sh`、`.ps1`、`.py`、`.csv`、`.md`、报告目录 |
| 涉及函数/宏 | 不直接涉及协议逻辑 |
| 修改前问题 | LOG03 指出日志/产物筛选和验收材料整理风险 |
| 修改后行为 | 当前仓库已有完整得多的测试 summary、参数表、报告辅助材料 |
| 与 LOG03 的差异 | 证据更充分，但 Git 状态更复杂 |
| 测试证据 | Windows/Linux 五组 summary、矩阵 summary、候选 summary |
| 风险/备注 | 需要人工决定哪些脚本和 summary 进入最终提交，避免提交大型日志和临时目录 |

## 6. 当前核心数据结构说明

| 名称 | 类型/语义 | 作用 | 使用位置 | 相比 LOG03 |
|---|---|---|---|---|
| `struct FRAME` | 帧结构，含 `kind`、`ack`、`seq`、`data`、`padding` | 手工帧序列化载体 | `send_data_frame()`、`FRAME_RECEIVED` | 未发现结构性变化 |
| `FRAME_DATA` | 帧类型常量 1 | 数据帧 | `datalink.h`、主循环 | 未变 |
| `FRAME_ACK` | 帧类型常量 2 | 单独 ACK | `datalink.h`、`send_data_frame()` | 未变 |
| `FRAME_NAK` | 帧类型常量 3 | 否定确认 | `datalink.h`、`send_nak()` | LOG03 后仍为主线 |
| `ack_expected` | unsigned char | 发送窗口左边界，最早未确认帧 | `main()`、ACK 滑动逻辑 | 未变 |
| `next_frame_to_send` | unsigned char | 下一个可发送序号 | `NETWORK_LAYER_READY` | 未变 |
| `frame_expected` | unsigned char | 接收端期望交付序号 | `FRAME_RECEIVED`、`send_nak()` | 未变 |
| `too_far` | unsigned char | 接收窗口右边界后一位 | `FRAME_RECEIVED` | 未变 |
| `nbuffered` | int | 当前发送窗口内未确认帧数 | 发送、ACK、网络层控制 | 未变 |
| `no_nak` | int/bool 语义 | 表示当前是否没有 NAK pending | CRC 错误、乱序、交付、ACK_TIMEOUT | 变得更重要；repeat NAK 依赖它 |
| `out_buf[NR_BUFS][PKT_LEN]` | 发送缓存 | 保存已发送未确认包，供超时重传 | `NETWORK_LAYER_READY`、`DATA_TIMEOUT` | 未变 |
| `in_buf[NR_BUFS][PKT_LEN]` | 接收缓存 | 缓存乱序 DATA | `FRAME_RECEIVED` | 未变 |
| `arrived[NR_BUFS]` | 接收缓存标记 | 标识某序号数据是否已到达 | `FRAME_RECEIVED` | 未变 |
| `DATA_TIMER` | 宏 | 每帧数据超时 | `start_timer(frame_nr, DATA_TIMER)` | 当前保持 LOG03 的 2300 |
| `ACK_TIMER` | 宏 | delayed ACK 基础超时 | DATA 接收路径 | 未变 |
| `NAK_REPEAT_INTERVAL` | 宏 | repeat NAK 间隔 | `send_nak()`、`ACK_TIMEOUT` | LOG03 后关键新增/保留变量 |

## 7. 当前核心函数说明

| 函数/分支 | 当前真实签名或事件 | 当前行为 | 与 LOG03 的差异 |
|---|---|---|---|
| `main` | `int main(int argc, char **argv)` | 初始化协议状态，持续等待事件并按五类事件驱动 SR 逻辑 | LOG03 后主结构未发现颠覆性变化 |
| `between` | `static int between(unsigned char a, unsigned char b, unsigned char c)` | 判断 `b` 是否位于循环序号区间 `[a,c)` 内 | 未变，是 SR 的核心判断 |
| `update_network_layer` | `static void update_network_layer(void)` | 当 `nbuffered < NR_BUFS` 且 `phl_sq_len() < PHL_QUEUE_LIMIT` 时启用网络层，否则禁用 | LOG03 后仍保留物理层队列节流 |
| `send_data_frame` | `static void send_data_frame(unsigned char kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[])` | 组帧、填 ACK、复制 payload、计算 CRC、发送；DATA 启动 DATA timer；ACK 或可 piggyback 时管理 ACK timer | 结合 repeat NAK 后，`no_nak` 会影响 ACK timer 是否停止 |
| `send_nak` | `static void send_nak(unsigned char frame_expected)` | 发送 `FRAME_NAK`，设置 `no_nak=0`，并以 `NAK_REPEAT_INTERVAL` 启动 ACK timer | LOG03 的新增机制，当前仍存在 |
| `FRAME_RECEIVED` | 事件分支 | 读帧、CRC 校验、处理 NAK/ACK/DATA、缓存乱序、交付连续数据、滑动发送窗口 | CRC 错误和缺帧路径会触发 `send_nak()`；DATA 收到后根据 `no_nak` 决定 ACK timer |
| `DATA_TIMEOUT` | 事件分支 | 只重传超时序号 `event_arg` 对应缓存帧 | 未变，仍是单帧重传而非 GBN |
| `ACK_TIMEOUT` | 事件分支 | 若 NAK pending 且 repeat 间隔启用，则重发 NAK；否则发送 ACK | LOG03 后关键变化 |
| CRC 相关 | `crc32()` API | 发送前写入 CRC；接收后校验整帧 CRC | 未变；运行 fatal 不应由源码字符串误判 |

简化伪流程：

```text
NETWORK_LAYER_READY: 从网络层取包 -> 存 out_buf -> 发 DATA -> next_frame_to_send++ -> nbuffered++
FRAME_RECEIVED: 校 CRC -> 处理 NAK/ACK -> DATA 入接收窗口 -> 连续交付 -> 滑动窗口
DATA_TIMEOUT: 重传 event_arg 对应 DATA
ACK_TIMEOUT: 若 NAK pending 则 repeat NAK，否则发 ACK
update_network_layer: 结合发送窗口和物理层队列长度启停网络层
```

## 8. 当前事件驱动流程

| 事件 | 触发条件 | 修改状态变量 | 可能发送帧 | timer 行为 | 与 LOG03 相比 |
|---|---|---|---|---|---|
| `NETWORK_LAYER_READY` | 网络层有新包且被启用 | `out_buf`、`next_frame_to_send`、`nbuffered` | `FRAME_DATA` | DATA 帧启动对应 DATA timer；可能停止 ACK timer | 主体未变 |
| `PHYSICAL_LAYER_READY` | 物理层可发送 | 通常只调用 `update_network_layer()` | 无 | 无直接 timer 变化 | 未发现关键变化 |
| `FRAME_RECEIVED` | 收到物理层帧 | ACK 滑动、`arrived`、`in_buf`、`frame_expected`、`too_far`、`no_nak` | ACK、NAK、DATA 重传可被触发 | CRC 错/乱序可启动 NAK repeat timer；正常 DATA 可启动 ACK timer | repeat NAK 路径是关键变化 |
| `DATA_TIMEOUT` | 某数据帧超时 | 无窗口整体回退 | 单帧 `FRAME_DATA` 重传 | 重启该帧 DATA timer | 仍为 SR 单帧重传 |
| `ACK_TIMEOUT` | ACK timer 到期 | 通常不改窗口；repeat NAK 时保持 `no_nak=0` | `FRAME_ACK` 或 `FRAME_NAK` | ACK 后停止/重置；NAK 后按 800ms 再启动 | LOG03 后从纯 ACK timeout 扩展为 repeat NAK 驱动 |

## 9. 当前测试记录总表

| 序号 | 测试场景 | 平台 | 运行时间 | A 利用率 | B 利用率 | Err/BER | Quit | Fatal scan | 日志/summary | 是否当前源码 | 结论 |
|---|---|---:|---:|---:|---:|---|---|---:|---|---|---|
| 1 | 普通无误码 | Windows | 1200s | 51.65% | 96.96% | 0/0；BER 0 | True | 0 | `performance-tests-20260512-221730/summary.csv` | 是 | 通过；普通模式双向速率不对称属发送模型影响 |
| 2 | 普通默认误码 | Windows | 1200s | 49.43% | 91.89% | 53/97；BER `1e-5` | True | 0 | 同上 | 是 | 通过；存在误码恢复 |
| 3 | flood 无误码 | Windows | 1200s | 96.96% | 96.97% | 0/0；BER 0 | True | 0 | 同上 | 是 | 通过，接近满速 |
| 4 | flood 默认误码 | Windows | 1200s | 92.90% | 92.77% | 97/97；BER `1e-5` | True | 0 | 同上 | 是 | 通过，约 93% |
| 5 | flood 高误码 | Windows | 1200s | 58.19% | 57.70% | 941/936；BER `1e-4` | True | 0 | 同上 | 是 | 通过但性能明显受误码影响 |
| 6 | 普通无误码 | Linux | 1200s | 51.70% | 96.97% | 0/0；BER 0 | yes | 0 | `linux-current-five-1200-summary.csv` | 是 | 通过 |
| 7 | 普通默认误码 | Linux | 1200s | 49.74% | 92.14% | 54/94；BER `1e-5` | yes | 0 | 同上 | 是 | 通过 |
| 8 | flood 无误码 | Linux | 1200s | 96.97% | 96.97% | 0/0；BER 0 | yes | 0 | 同上 | 是 | 通过 |
| 9 | flood 默认误码 | Linux | 1200s | 93.05% | 92.80% | 96/98；BER `1e-5` | yes | 0 | 同上 | 是 | 通过 |
| 10 | flood 高误码 | Linux | 1200s | 59.35% | 58.41% | 935/934；BER `1e-4` | yes | 0 | 同上 | 是 | 通过，略高于 Windows 当前结果 |
| 11 | `dt2200_nri800` 默认 flood | Linux | 1200s | 93.22% | 92.71% | 98/97；BER `1e-5` | yes | 0 | `candidate-dt2200-nri800-valid-1200-summary.csv` | 候选源码 | 可作为候选证据，不是当前最终源码 |
| 12 | `dt2200_nri800` 高误码 flood | Linux | 1200s | 59.04% | 59.36% | 937/940；BER `1e-4` | yes | 0 | 同上 | 候选源码 | 高误码略优；首次 high 因 `System too busy` 无效，已 rerun |
| 13 | 小矩阵 300s | Linux | 300s | 多组 | 多组 | 默认/高误码 | 多数 yes | 0 | `protocol-matrix-20260513-000052/summary.csv` | 候选矩阵 | 用于参数判断，不能替代最终 1200s 验收 |

说明：运行 fatal 以 summary/log 扫描为准，未把源码中的 `FATAL` 字符串常量误判为运行 fatal。本轮生成 LOG04 未重新运行测试，只整理已有结果。

## 10. 性能结论

| 场景 | Windows 当前源码 | Linux 当前源码 | 最可信证据 | 相比 LOG03 |
|---|---|---|---|---|
| 无误码普通 | A 51.65%，B 96.96%，Quit True，Fatal 0 | A 51.70%，B 96.97%，Quit yes，Fatal 0 | Windows `performance-tests-20260512-221730/summary.csv`；Linux `linux-current-five-1200-summary.csv` | Windows 风险已补；Linux 五组已补 |
| 默认误码普通 | A 49.43%，B 91.89% | A 49.74%，B 92.14% | 同上 | 当前源码已有双平台 1200s 证据 |
| 无误码 flood | A/B 约 96.96-96.97% | A/B 96.97% | 同上 | 稳定接近满速 |
| 默认误码 flood | A/B 约 92.8-92.9% | A 93.05%，B 92.80% | 同上 | 证据显著强于 LOG03；是否达到参考需报告中客观比较 |
| 高误码 flood `--ber=1e-4` | A 58.19%，B 57.70% | A 59.35%，B 58.41% | 同上 | 高误码长测证据补齐；性能受 BER 限制明显 |
| 候选 `2200/800` | 未见 Windows 当前证据 | 默认 93.22/92.71；高误码 59.04/59.36 | `candidate-dt2200-nri800-valid-1200-summary.csv` | 只作为候选，不代表当前最终源码 |

结论：当前最终源码 `2300/800` 已具备 Windows/Linux 五组 1200s 证据；无误码 flood 接近满速，默认误码 flood 约 93%，高误码 flood 约 58-59%。`2200/800` 在 Linux 候选验证中高误码略有优势，但尚未被改为最终源码。

## 11. 当前已满足的实验要求

| 实验要求 | 当前是否满足 | 证据/备注 |
|---|---|---|
| 有噪音信道下无差错传输 | 基本满足 | 默认误码和高误码 1200s 均 Quit 且 Fatal 0；需报告中结合完整日志说明无 fatal |
| 双工通信 | 满足 | A/B 双端均有发送利用率和 Quit |
| 滑动窗口 | 满足 | `MAX_SEQ=15`、`NR_BUFS=8`、SR 窗口变量 |
| 流量控制 | 满足 | `update_network_layer()` 根据窗口和 `phl_sq_len()` 控制 |
| 差错控制 | 满足 | CRC、ACK、NAK、DATA timer、repeat NAK |
| CRC | 满足 | `crc32()` 写入与校验 |
| ACK | 满足 | `FRAME_ACK` 与 ACK 字段处理 |
| NAK | 满足 | `FRAME_NAK`、`send_nak()` |
| piggyback ACK | 满足 | DATA 帧带 `ack` 字段 |
| ACK timer | 满足 | `ACK_TIMER=300`，ACK timeout 分支 |
| DATA timer | 满足 | `start_timer(frame_nr, DATA_TIMER)`，单帧重传 |
| 缓存管理 | 满足 | `out_buf`、`in_buf`、`arrived` |
| 序号回绕 | 满足 | `between()` 与 `% (MAX_SEQ + 1)` |
| 协议跟踪调试 | 基本满足 | 有日志、stdout/stderr、summary；大日志未建议全量提交 |
| 15/20 分钟稳定运行 | 满足 20 分钟级证据 | 五组 1200s 即 20min |
| 性能测试记录 | 基本满足 | Windows/Linux 五组 summary、矩阵 summary、候选 summary |
| Windows/Linux 一致性 | 当前满足 | 两份 `datalink.c` SHA256 一致 |
| 验收材料完整性 | 部分满足 | 源码和 summary 充分；提交清单、报告、压缩包仍需人工确认 |

## 12. 当前风险点与待办事项

| 优先级 | 风险/待办 | 影响 | 建议动作 |
|---|---|---|---|
| P0 | Git 工作区混有源码、日志、脚本、产物、压缩包、复制目录 | 直接提交会带入大量无关或大型文件 | 提交前先 `git status --short`、`git diff --stat`，人工筛选 |
| P0 | `datalink.c` 有 staged 与 unstaged 两层修改 | 可能只提交 `DATA_TIMER` 而漏掉 repeat NAK，或反过来 | 最终提交前确认 `git diff` 与 `git diff --cached`，必要时重新整理补丁 |
| P0 | 是否采用 `2300/800` 作为最终版本 | 当前源码是 2300/800，但 2200/800 有候选优势 | 若追求稳妥，保持 2300/800；若想再优化，需明确改参并补 Windows/Linux 长测 |
| P1 | Windows/Linux 五组虽已补齐，但大日志很多 | 报告可引用 summary，但提交包可能臃肿 | 保留 summary、命令、端口、Quit、Fatal scan；不提交全文大日志 |
| P1 | 默认误码性能约 93%，可能低于某些参考 | 报告评分或验收时需解释 | 用 SR、repeat NAK、队列节流和实测数据客观说明 |
| P1 | 高误码存在环境波动 | 候选 first high 曾出现 `System too busy` | 长测时保留 stderr 和 fatal scan；必要时 rerun 异常组 |
| P1 | 脚本多且历史性质不同 | 新窗口可能误用旧调参脚本 | 在 README/提交清单中区分 final 脚本与历史脚本 |
| P2 | 报告材料可能尚未与最终 summary 对齐 | 报告正文可能引用旧结果 | 以 LOG04 表格和最终 summary 更新报告 |
| P2 | 压缩包/复制目录状态不明确 | 可能误提交或漏交 | 人工确认课程要求后重新打包最终工程 |
| P2 | `.gitignore` 或提交筛选策略待定 | 编译产物和日志可能再次混入 | 提交前考虑添加/调整忽略规则，或手工选择文件 |

## 13. 给下一轮 Codex / ChatGPT 的接手建议

1. 先读 LOG04，再回看 LOG03；LOG04 记录的是当前仓库实证，不要直接沿用 LOG03 的未完成风险判断。
2. 任何继续操作前先运行 `git status --short` 和 `git diff --stat`，确认工作区是否相较 LOG04 漂移。
3. 如果要继续优化协议，先确认 Windows/Linux `datalink.c` 哈希一致；不要先动窗口或队列。
4. 若继续参数优化，只建议围绕 `DATA_TIMER=2200/2300/2400` 与 `NAK_REPEAT_INTERVAL=700/800/900/1000` 的小矩阵；当前源码仍是 `2300/800`。
5. 若采纳 `2200/800`，必须改 Windows/Linux 两份源码并重新编译复测 Windows/Linux 五组 1200s，不能只引用候选 Linux 数据。
6. 长测后保留 summary、端口、命令、运行时长、Quit、Fatal scan 和日志尾部；不必提交全文大日志。
7. 提交前筛选：源码、必要头文件、最终脚本、DEV_LOG、精简 summary 优先；大型 log、stdout/stderr、编译产物、复制目录、压缩包需人工确认。
8. `protocol-matrix-20260513-000052` 是调参证据，不是最终验收五组；报告中应分开表述。
9. `candidate-dt2200-nri800-valid-1200-summary.csv` 是候选证据，不代表当前最终源码。
10. 不要把源码里的 `FATAL` 字符串当作运行 fatal；以 summary/log scan 为准。

## 14. 建议提交/归档清单

| 类型 | 建议 |
|---|---|
| 应提交源码 | Windows/Linux `datalink.c`，并确保两份最终一致 |
| 应提交头文件 | 若 `datalink.h`、`protocol.h`、`lprintf.h` 有课程要求或真实修改，应纳入；提交前核查 diff |
| 应提交脚本 | 可提交最终可复用的 Windows/Linux 五组测试脚本与 summary 脚本；历史矩阵脚本可选 |
| 应提交 DEV_LOG | 建议提交 `DEV_LOG/DEVELOPMENT_LOG00_RECORD.md` 至 `LOG04` 与 `HIGH_BER_OPTIMIZATION_NOTES.md`，前提是课程允许开发记录进入仓库 |
| 可提交 summary | Windows/Linux 当前源码五组 1200s summary、参数矩阵 summary、候选 summary 可作为证据材料 |
| 不建议提交大型日志 | 原始 `.log`、`.stdout`、`.stderr` 大量文件一般不建议全量提交，只保留必要摘要或压缩归档 |
| 不建议提交编译产物 | `.exe`、Linux 可执行文件、`.o`、VS Release 目录通常不提交，除非实验提交要求包含可执行文件 |
| 需人工确认压缩包 | `*.zip`、`*.rar`、submission candidate 目录需确认最终交付口径后再保留/重打包 |
| 需人工确认报告材料 | `计算机网络实验一报告.docx`、`report/`、性能表需用最终 summary 更新后再归档 |

## 15. LOG04 结论摘要

当前协议版本是 Selective Repeat 滑动窗口协议，支持双工、CRC、ACK、piggyback ACK、NAK、repeat NAK、DATA timer 单帧重传、乱序接收缓存、发送缓存、序号回绕和物理层队列节流。当前最终源码参数为 `MAX_SEQ=15`、`NR_BUFS=8`、`DATA_TIMER=2300`、`ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800`、`PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2`。

Windows 与 Linux 两份 `datalink.c` 当前工作区 SHA256 完全一致：`627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43`。LOG03 后最重要的变化是 Windows 当前源码已重新编译并完成五组 1200s，Linux 当前源码也补齐五组 1200s；同时完成了参数小矩阵和 `2200/800` 候选 1200s 验证。

最可信的当前验收证据是 Windows `performance-tests-20260512-221730/summary.csv` 和 Linux `linux-current-five-1200-summary.csv`。这两份 summary 显示五组 1200s 均 Quit，Fatal scan 为 0；无误码 flood 接近 97%，默认误码 flood 约 93%，高误码 flood 约 58-59%。

当前最缺的不是协议代码，而是提交前整理：Git 工作区混有大量 staged/untracked 日志、脚本、编译产物、复制目录和压缩包。下一步最推荐做的是人工确认最终参数是否保持 `2300/800`，然后筛选提交清单、更新报告性能表、必要时重新打包最终工程。

## 16. 需要人工确认的点

| 序号 | 需要确认的问题 | 原因 |
|---|---|---|
| 1 | 是否确认 `DATA_TIMER=2300`、`NAK_REPEAT_INTERVAL=800` 为最终验收版本 | 当前源码如此；`2200/800` 候选有一定性能空间但未改入最终源码 |
| 2 | 是否需要采用 `2200/800` 并重新跑 Windows/Linux 五组 1200s | 只有 Linux 候选证据，不足以代表最终双平台版本 |
| 3 | 哪些 summary 可进入报告或提交包 | 当前 summary 很多，需区分最终验收、候选矩阵、历史调参 |
| 4 | 是否保留或提交大型日志、stdout/stderr | 大量日志会污染仓库；但验收可能需要部分尾部证据 |
| 5 | 是否提交编译产物 `datalink.exe`、Linux `datalink` | 取决于课程提交要求 |
| 6 | 是否删除或保留旧压缩包、复制目录、submission candidate 目录 | 当前 Git 状态显示多种归档材料，需人工定夺 |
| 7 | DEV_LOG 目录布局是否最终采用 `Lab1-2024(Win+Linux)/DEV_LOG` | LOG04 已手动迁入该目录；但根目录旧日志仍有迁移痕迹，Git 状态仍需清理 |
| 8 | 报告正文是否已经使用最新 Windows/Linux 五组数据 | 需要打开报告或性能表逐项核对 |
| 9 | 是否需要重新打包最终工程 | 当前压缩包可能对应旧状态 |
| 10 | 提交前是否需要 `.gitignore` 或手工筛选 | 防止 `.o`、exe、log、复制目录混入提交 |

## 17. LOG04 自检记录

| 核查项 | 复核结果 | LOG04 处理 |
|---|---|---|
| 路径关系 | 核查对象为 `C:\Users\28641\Desktop\Experiment-1` 仓库；Codex shell 工作目录为 `C:\Users\28641\Desktop\计网\实验`；LOG04 当前文件位置为 `C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\DEV_LOG\DEVELOPMENT_LOG04_RECORD.md` | 已在第 0、1 节修正路径差异，避免把临时输出位置误认为最终仓库内路径 |
| 最近提交 | `eb3ec94 Update generated outputs`；`82bacc6 Initial commit` | 已写入第 1 节 |
| Git 状态 | 工作区不干净，staged/unstaged/untracked 混合 | 已列风险和摘要 |
| 两份 `datalink.c` 哈希 | 两份均为 `627AD8629A4EE4EC7761C0E6C65543C226AA82EB9AF9480D5753B7641C2A1D43` | 已写入第 4 节 |
| 当前宏参数 | `MAX_SEQ=15`、`NR_BUFS=8`、`DATA_TIMER=2300`、`ACK_TIMER=300`、`NAK_REPEAT_INTERVAL=800`、`PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2` | 已写入第 4 节 |
| 关键函数签名 | `main`、`between`、`update_network_layer`、`send_data_frame`、`send_nak` 等均已核查 | 已写入第 7 节 |
| Windows/Linux 同步情况 | 当前工作区源码完全一致 | 已写入第 4 节和第 15 节 |
| 当前测试 summary 来源 | Windows 五组、Linux 五组、矩阵、候选 summary 均已整理 | 已写入第 9、10 节 |
| 是否发现 fatal | 当前五组 summary Fatal scan 均为 0；候选首次 high 有 `System too busy` 无效记录，已标注 | 已保守记录 |
| 是否发现旧 exe / 旧源码测试结果 | 旧日志中存在，但 LOG04 当前结论优先使用当前源码 summary | 已在第 2、9 节区分 |
| 是否执行过破坏性操作 | 否 | 已在第 0 节声明 |
| 是否本轮重新运行测试 | 否，只整理已有结果 | 已在第 9 节声明 |

