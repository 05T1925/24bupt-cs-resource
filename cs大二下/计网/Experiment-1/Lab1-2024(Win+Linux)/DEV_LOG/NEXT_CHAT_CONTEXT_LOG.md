# 下一轮聊天完整接手上下文日志 NEXT_CHAT_CONTEXT_LOG

生成时间：2026-05-21  
当前工作区：`C:\Users\28641\Desktop\Experiment-1`

## 0. 使用方式

把本文件整体喂给下一次 ChatGPT/Codex 对话框，用作项目和先前对话的压缩上下文。它的目标是尽量避免上下文压缩后出现的典型问题：路径漂移、把旧日志当最新状态、误判 `Lab1-linux` 缺失、混淆旧 exe 测试和当前源码测试、混淆候选参数和最终参数、遗漏 Git 工作区混乱风险。

若本文件与更早的 `DEVELOPMENT_LOG00_RECORD.md`、`DEVELOPMENT_LOG01_RECORD.md`、`DEVELOPMENT_LOG02_RECORD.md`、`DEVELOPMENT_LOG03_RECORD.md`、`DEVELOPMENT_LOG04_RECORD.md`、`DEVELOPMENT_LOG_SUM_RECORD.md` 或 `HIGH_BER_OPTIMIZATION_NOTES.md` 冲突，以本文件和当前实证为准。更早日志仍有历史价值，但不能直接当作当前状态。

## 1. 用户在本轮明确补充的重要事实

用户明确说明：**不用在意 `Lab1-linux` 目录缺失，因为实验需要，用户已经手动移除了它。**

这条非常重要。下一轮不要再把 `Lab1-linux` 不存在或 Git 中 Linux 文件状态异常当成当前交付风险来反复提醒，除非用户重新要求恢复 Linux 版。历史日志里关于 Linux 编译、Linux 五组、Linux summary、Windows/Linux 两份 `datalink.c` 同步的内容，都属于历史开发和调参证据。当前交付语境下，`Lab1-linux` 被移除是用户有意操作。

## 2. 当前真实项目结构

当前根目录：

```text
C:\Users\28641\Desktop\Experiment-1
```

当前主要目录和文件：

```text
C:\Users\28641\Desktop\Experiment-1
├── .git
├── .vscode
├── 计算机网络实验一验收说明（学生版）.pdf
├── 实验一评分表（学生版）.xlsx
└── Lab1-2024(Win+Linux)
    ├── DEV_LOG
    │   ├── DEVELOPMENT_LOG00_RECORD.md
    │   ├── DEVELOPMENT_LOG01_RECORD.md
    │   ├── DEVELOPMENT_LOG02_RECORD.md
    │   ├── DEVELOPMENT_LOG03_RECORD.md
    │   ├── DEVELOPMENT_LOG04_RECORD.md
    │   ├── DEVELOPMENT_LOG_SUM_RECORD.md
    │   ├── HIGH_BER_OPTIMIZATION_NOTES.md
    │   └── NEXT_CHAT_CONTEXT_LOG.md
    └── Lab1-2024(Win+Linux)
        ├── 计算机网络实验一实验指导书.pdf
        ├── Lab1-DataLinkLayerDesign.pdf
        ├── 实验报告首页.docx
        ├── 性能测试记录表.docx
        ├── 性能测试记录表-参考数据.docx
        ├── rfc1662.txt
        └── Lab1-Windows-VS2019
```

当前主要源码目录：

```text
C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019
```

当前核心源码：

```text
Lab1-Windows-VS2019\datalink.c
Lab1-Windows-VS2019\datalink.h
Lab1-Windows-VS2019\protocol.c
Lab1-Windows-VS2019\protocol.h
Lab1-Windows-VS2019\crc32.c
Lab1-Windows-VS2019\lprintf.c
Lab1-Windows-VS2019\lprintf.h
Lab1-Windows-VS2019\getopt.c
Lab1-Windows-VS2019\getopt.h
Lab1-Windows-VS2019\Run-FivePerformanceTests.ps1
Lab1-Windows-VS2019\datalink.sln
Lab1-Windows-VS2019\datalink.vcxproj
```

当前没有可见顶层 `Lab1-linux` 目录；这是用户手动移除，不是误删风险。

## 3. 当前 Git 状态摘要

最近提交：

```text
eb3ec94 Update generated outputs
```

工作区非常不干净，有 staged、unstaged、untracked、deleted、renamed 混合状态。典型状态包括：

- `datalink.c` 为 `MM`，说明 staged 和 unstaged 都有变化。
- `datalink.h` 有 staged 修改。
- `crc32.c`、`protocol.c`、`datalink.sln`、`datalink.exe` 等有修改。
- `.vs/`、`x64-Debug/`、`Win32-Release/`、`.obj`、`.exe`、大量日志和测试目录处于 untracked 或 modified。
- Git 中仍显示大量 `Lab1-linux` 文件 deleted / added-deleted，但用户已说明 `Lab1-linux` 是为了实验需要手动移除，下一轮不要把它当异常重点。
- `DEV_LOG/` 当前为 untracked 或未整理状态。

后续如果要提交，绝对不要直接 `git add .`。应人工筛选：源码、必要工程文件、报告材料、关键 summary、最终接手日志可以考虑；`.vs`、`.obj`、临时日志、旧矩阵目录、编译产物、大型历史目录通常不要提交，除非课程明确要求。

## 4. 当前协议实现结论

当前 `Lab1-Windows-VS2019\datalink.c` 实现的是：

```text
Selective Repeat ARQ with Piggybacking + CRC32 + ACK timer + NAK + repeat NAK
```

它不是 Go-Back-N。

当前核心特性：

- 全双工 A/B 通信。
- 发送窗口和接收窗口。
- 接收端缓存窗口内乱序 DATA。
- 按序连续交付网络层。
- DATA/ACK/NAK 三种帧。
- DATA 帧携带 piggyback ACK。
- CRC32 检错。
- 坏 CRC 或乱序缺口触发 NAK。
- NAK 的 `ack` 字段表示“请求重传的缺失序号”，不是累计 ACK。
- 收到 NAK 或 DATA timeout 时只重传单帧，不回退整个窗口。
- ACK timer 同时承担 delayed ACK 和 repeat NAK 节奏。
- 物理层发送队列节流，避免 ACK/NAK 被 flood DATA 压在队尾。

当前重要宏参数来自 `datalink.c`：

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

参数含义：

| 参数 | 当前值 | 含义 |
|---|---:|---|
| `MAX_SEQ` | 15 | 序号空间 0..15 |
| `NR_BUFS` | 8 | SR 窗口大小，等于序号空间一半 |
| `DATA_TIMER` | 2300 ms | DATA 单帧重传 timeout |
| `ACK_TIMER` | 300 ms | 普通 delayed ACK timer |
| `NAK_REPEAT_INTERVAL` | 800 ms | NAK pending 时重复 NAK 间隔 |
| `PHL_QUEUE_LIMIT` | `FRAME_DATA_LEN * 2` | 物理层发送队列节流阈值 |

## 5. 核心代码阅读索引

以下行号基于当前 Windows `datalink.c`：

| 功能 | 位置 |
|---|---|
| 宏参数 | `datalink.c:45` 到 `datalink.c:54` |
| `struct FRAME` | `datalink.c` 帧结构体定义处 |
| `between()` | `datalink.c:147` |
| `update_network_layer()` | `datalink.c:162` |
| `send_nak()` | `datalink.c:189` |
| `send_data_frame()` | `datalink.c:224` |
| `NETWORK_LAYER_READY` | `main()` switch 内 |
| `FRAME_RECEIVED` | `datalink.c:406` |
| `DATA_TIMEOUT` | `datalink.c:530` |
| `ACK_TIMEOUT` | `datalink.c:549` |

关键逻辑：

```c
static int between(unsigned char a, unsigned char b, unsigned char c)
{
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) ||
           ((b < c) && (c < a));
}
```

```c
static void update_network_layer(void)
{
    if (nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT)
        enable_network_layer();
    else
        disable_network_layer();
}
```

```c
static void send_nak(unsigned char frame_expected)
{
    send_data_frame(FRAME_NAK, 0, frame_expected, NULL);
    no_nak = 0;
    if (NAK_REPEAT_INTERVAL > 0)
        start_ack_timer(NAK_REPEAT_INTERVAL);
    else
        stop_ack_timer();
}
```

ACK timeout 当前逻辑：

```c
case ACK_TIMEOUT:
    if (!no_nak && NAK_REPEAT_INTERVAL > 0)
        send_nak(frame_expected);
    else
        send_data_frame(FRAME_ACK, 0, frame_expected, NULL);
    break;
```

这就是 repeat NAK 的核心。

## 6. 帧格式与 ACK/NAK 语义

当前实际 wire format 是手工字节序列化，不直接发送 `struct FRAME` 内存。

DATA：

```text
+---------+--------+--------+----------------+--------+
| kind(1) | seq(1) | ack(1) | data[PKT_LEN]  | CRC(4) |
+---------+--------+--------+----------------+--------+
```

ACK / NAK：

```text
+---------+--------+--------+--------+
| kind(1) | seq(1) | ack(1) | CRC(4) |
+---------+--------+--------+--------+
```

注意：

- DATA/ACK 帧中的 `ack` 是累计 ACK，表示最后一个按序收到的帧。
- NAK 帧中的 `ack` 是请求重传的缺失序号。
- 所以当前代码只在 `f.kind != FRAME_NAK` 时执行累计 ACK 滑动。
- 如果把 NAK 的 `ack` 当累计 ACK，会错误滑动发送窗口，可能导致未收到的数据被误确认。

## 7. 历史开发时间线

### LOG00

早期接手记录。主要总结 SR 实现、帧序列化、CRC、ACK/NAK、窗口、物理队列节流、默认 `DATA_TIMER=2500`。当时路径是旧的 `C:\Users\28641\Desktop\计网\实验...`，且认为 Git 不可用。该状态现在已经过时。

### LOG01

记录 Windows 五组测试、Linux 初步编译和短测、Linux 参数 sweep、`DATA_TIMER=2300` 候选。它仍是历史有效记录，但“2300 只是候选”“Linux 需要补测”等内容后续被 LOG02/LOG03/LOG04 更新。

### LOG02

记录 `DATA_TIMER=2300` 落地，Linux 300 秒矩阵和 1200 秒默认/高误码 flood 验证。此时还没有最终 repeat NAK，或者 repeat NAK 状态不是当前最终形态。

### HIGH_BER_OPTIMIZATION_NOTES

分析高误码瓶颈：NAK 本身也可能损坏，若唯一 NAK 损坏，恢复会退化到 DATA timeout。建议收敛 `DATA_TIMER` 和研究 repeat NAK。该分析仍然成立。

### LOG03

记录新增 `NAK_REPEAT_INTERVAL=800` 和 repeat NAK 逻辑。Linux repeat800 小矩阵和 1200 秒高误码验证显示高误码利用率提升到约 58%。当时仍强调 Windows 当前源码需要重新编译复测。

### LOG04

记录 Windows/Linux 当前源码五组 1200 秒、参数矩阵和报告材料状态。它是旧综合日志前的最新阶段记录之一。

### LOG_SUM

综合 LOG00 到 LOG04、HIGH_BER、源码、Git、summary、报告材料形成最终大汇总。它非常有用，但有一个当前新增事实需要覆盖：`Lab1-linux` 现在是用户为了实验需要手动移除，不要继续当缺失风险。

### 本文件 NEXT_CHAT_CONTEXT_LOG

本文件结合用户最新说明和当前只读核查，作为下一轮聊天的最优入口。

## 8. 测试证据总览

当前最重要的 Windows 五组 1200 秒完整 summary：

```text
C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\performance-tests-20260512-221730\summary.csv
```

该组是当前工作区可见的最完整 Windows 1200 秒五组证据。结果如下：

| 场景 | A 利用率 | B 利用率 | A Err | B Err | Quit | FatalMatches |
|---|---:|---:|---:|---:|---|---:|
| `01_plain_ber0` | 51.65% | 96.96% | 0 | 0 | True/True | 0 |
| `02_plain_ber1e-5` | 49.43% | 91.89% | 53 | 97 | True/True | 0 |
| `03_flood_ber0` | 96.96% | 96.97% | 0 | 0 | True/True | 0 |
| `04_flood_ber1e-5` | 92.90% | 92.77% | 97 | 97 | True/True | 0 |
| `05_flood_ber1e-4` | 58.19% | 57.70% | 941 | 936 | True/True | 0 |

manifest：

```text
performance-tests-20260512-221730\manifest.txt
Duration per test: 1200 seconds
Executable: ...\Lab1-Windows-VS2019\datalink.exe
```

`performance-tests-20260515-221741` 是 60 秒短测级数据，不能替代 1200 秒验收结果。它显示高误码 B 端偏低，说明短测波动较大。

`performance-tests-20260516-012343` 只有 manifest，没有完整 summary，并且 manifest 里的路径指向 `Experiment-10\Experiment-1`，不作为最终证据。

`Win32-Release\datalink-A.log` / `datalink-B.log` 出现过 `System too busy`，且长时间睡眠导致统计掉下去，这类日志不宜作为正式性能表数据。

## 9. 当前测试结论

基于 `performance-tests-20260512-221730\summary.csv`：

- 普通无误码：A 约 51.65%，B 约 96.96%。
- 普通默认误码：A 约 49.43%，B 约 91.89%。
- flood 无误码：A/B 约 96.96% / 96.97%，基本达到 97%。
- flood 默认误码：A/B 约 92.90% / 92.77%。
- flood 高误码 `--ber=1e-4`：A/B 约 58.19% / 57.70%。
- 所有五组均 `Quit=True`，`FatalMatches=0`。

从报告角度可以说：

- 当前实现比 Go-Back-N 风格更适合误码信道，因为它缓存乱序帧并单帧重传。
- 默认误码 flood 约 93%，略低于某些参考 SR 约 95%，但明显优于典型 GBN 约 88%。
- 高误码下 A/B 更均衡，平均约 58%，控制帧和 DATA 都会被误码影响，吞吐下降合理。

## 10. 对源码正确性的阅读结论

当前 `datalink.c` 的设计整体合理：

- SR 窗口大小 8，序号空间 16，满足 SR 窗口不超过序号空间一半的要求。
- `between()` 正确处理回绕。
- 手工序列化避免结构体 padding。
- CRC 检查失败后丢弃坏帧。
- 乱序帧缓存于 `in_buf`，`arrived` 标记，到达连续区间后按序 `put_packet`。
- NAK 请求缺失 `frame_expected`。
- DATA timeout 和 NAK 都是单帧重传。
- `update_network_layer()` 同时考虑发送窗口和物理层队列，避免 flood 时底层队列堆积太深。
- `ACK_TIMEOUT` 可在 NAK pending 时触发 repeat NAK，减少唯一 NAK 损坏后的等待。

需要小心的点：

- `ACK timer` 被复用为 delayed ACK 和 repeat NAK timer，后续修改时要非常谨慎。
- `no_nak` 的语义是“当前是否没有未解决 NAK 缺口”。收到连续期望帧并交付后会恢复为 1。
- 发送 DATA 时，只有 `no_nak` 为真才停止 ACK timer，避免中断 repeat NAK。
- NAK 帧不能参与累计 ACK 处理。
- 当前大量中文注释可以帮助答辩，但提交前要确认老师是否接受。

## 11. 当前交付/报告材料状态

当前可见材料：

```text
计算机网络实验一验收说明（学生版）.pdf
实验一评分表（学生版）.xlsx
Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\计算机网络实验一实验指导书.pdf
Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-DataLinkLayerDesign.pdf
Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\实验报告首页.docx
Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\性能测试记录表.docx
Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\性能测试记录表-参考数据.docx
```

尚未在本轮确认存在完整最终实验报告正文。之前 LOG_SUM 也提到只确认报告首页、性能表和参考数据表。

如果下一轮任务是写报告，建议报告结构：

1. 实验目的和环境。
2. 协议类型：Selective Repeat ARQ with Piggybacking。
3. 帧格式。
4. 状态变量和窗口大小。
5. 事件驱动流程。
6. ACK / NAK / repeat NAK / timer 机制。
7. CRC32 检错。
8. 物理层队列节流。
9. 参数选择依据。
10. 五组性能结果表。
11. 与 Go-Back-N / Selective Repeat 参考对比。
12. 高误码性能分析。
13. 遇到的问题和解决方法。
14. 总结和心得。

## 12. 验收问答速查

| 问题 | 建议回答 |
|---|---|
| 当前实现是什么协议？ | Selective Repeat ARQ with Piggybacking，不是 Go-Back-N。 |
| 为什么不是 GBN？ | 接收端缓存乱序正确帧，发送端收到 NAK 或 timeout 只重传单帧，不整窗回退。 |
| 窗口为什么是 8？ | `MAX_SEQ=15`，序号空间 16；SR 窗口不能超过序号空间一半，所以取 8。 |
| ACK 字段含义？ | DATA/ACK 帧中表示最后一个按序收到的序号，是累计 ACK。 |
| NAK 字段含义？ | NAK 帧中 `ack` 字段表示请求重传的缺失序号。 |
| 为什么 NAK 不参与累计 ACK？ | NAK 的 `ack` 不是确认进度，若当累计 ACK 会错误滑动发送窗口。 |
| DATA timer 为什么是 2300？ | 从 2500 收敛而来，兼顾默认误码稳定和高误码恢复速度；历史长测和矩阵支持。 |
| ACK timer 为什么是 300？ | 给 piggyback 留机会，同时避免 ACK 无限延迟。 |
| NAK repeat 为什么是 800？ | 高误码下 NAK 也可能损坏，800ms 重发能减少等待 DATA timeout，又避免 NAK 过密。 |
| CRC 怎么做？ | 发送时对头部和 payload 算 CRC32 并追加 4 字节；接收时对整帧算 CRC32，非 0 判坏帧。 |
| 如何保证按序交付？ | 正确乱序帧先缓存，只有从 `frame_expected` 开始连续到达才 `put_packet()`。 |
| 高误码下为什么利用率下降？ | DATA、ACK、NAK 都可能损坏，重传和等待 timer 增多。 |
| 为什么限制物理队列？ | flood 模式下避免 ACK/NAK 被大量 DATA 堵在物理发送队列后面。 |

## 13. 下一轮如果继续做代码，应注意

默认不要大改协议。当前实现和测试证据已经比较完整，后续主要应围绕交付、报告、性能表、打包清理。

如必须优化：

- 不要先改窗口大小。
- 不要随意改 `protocol.c`、`protocol.h`、`crc32.c` 等实验框架。
- 一次只改一个参数或一个机制。
- 改后必须重跑至少关键场景：默认误码 flood、高误码 flood。
- 任何代码改动后，测试结果和报告里的参数要同步更新。

如要重跑测试，优先用：

```powershell
cd "C:\Users\28641\Desktop\Experiment-1\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019"
powershell -ExecutionPolicy Bypass -File .\Run-FivePerformanceTests.ps1 -DurationSeconds 1200
```

如果手动跑：

```powershell
.\datalink.exe -t 1200 -f -p 62827 -l A.log A
.\datalink.exe -t 1200 -f -p 62827 -l B.log B
```

高误码：

```powershell
.\datalink.exe -t 1200 -f --ber=1e-4 -p 62828 -l A.log A
.\datalink.exe -t 1200 -f --ber=1e-4 -p 62828 -l B.log B
```

## 14. 下一轮如果做打包/提交，应注意

推荐包含：

- `Lab1-Windows-VS2019` 必要源码和 VS 工程文件。
- `datalink.c`、`datalink.h`、`protocol.c/h`、`crc32.c`、`lprintf.c/h`、`getopt.c/h`。
- 实验报告、性能测试记录表、评分表、必要说明文档。
- 关键 summary，如 `performance-tests-20260512-221730\summary.csv`。
- 必要的 `DEV_LOG`，尤其本文件和 `DEVELOPMENT_LOG_SUM_RECORD.md`。

谨慎或排除：

- `.git`
- `.vs`
- `x64-Debug`
- `Win32-Release` 中的大量产物，除非老师要求可执行文件
- `.obj`
- 过多 `.log` / `.stdout` / `.stderr`
- 失败或无效测试目录
- 旧压缩包和重复压缩包
- Git 中已被用户手动移除的 `Lab1-linux`，除非用户明确要求恢复

## 15. 给下一轮对话框的开场提示词

可以直接复制下面这段作为下一轮第一条消息：

```text
请先阅读这份 NEXT_CHAT_CONTEXT_LOG.md。当前项目在 C:\Users\28641\Desktop\Experiment-1，是计算机网络实验一数据链路层滑动窗口协议。当前主要交付目录是 Lab1-Windows-VS2019。注意：Lab1-linux 目录是我因为实验需要手动移除的，不要把它当作缺失风险。

当前 datalink.c 实现是 Selective Repeat ARQ with Piggybacking + CRC32 + ACK timer + NAK + repeat NAK，不是 Go-Back-N。核心参数为 MAX_SEQ=15、NR_BUFS=8、DATA_TIMER=2300、ACK_TIMER=300、NAK_REPEAT_INTERVAL=800、PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2。重点读 between、update_network_layer、send_nak、send_data_frame、FRAME_RECEIVED、DATA_TIMEOUT、ACK_TIMEOUT。

当前最完整 Windows 1200 秒五组证据是 Lab1-Windows-VS2019/performance-tests-20260512-221730/summary.csv：普通无误码 51.65/96.96，普通默认误码 49.43/91.89，flood 无误码 96.96/96.97，flood 默认误码 92.90/92.77，flood 高误码 58.19/57.70，Quit 均 True，FatalMatches 均 0。

当前 Git 工作区很脏，千万不要 git add .。不要随意改 protocol.c/protocol.h/crc32.c 等实验框架。后续优先做报告、性能表、交付清理、答辩准备；如果继续改代码，必须小步验证并同步更新测试证据。
```

## 16. 当前最终一句话结论

当前项目已经具备一个较完整的 Windows 版 Selective Repeat 数据链路层协议实现，核心逻辑和五组 1200 秒测试证据基本完整；`Lab1-linux` 已由用户为实验需要手动移除，不应再作为当前缺失风险。下一步重点不是重新理解协议，而是整理交付、报告、性能表、Git/打包清单和验收答辩材料。
