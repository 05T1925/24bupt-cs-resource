# 高误码长测与参数优化笔记

时间：2026-05-12

## 当前基线

当前 Windows 与 Linux 两份 `datalink.c` 内容一致，基线参数为：

```c
#define MAX_SEQ 15
#define NR_BUFS ((MAX_SEQ + 1) / 2)
#define DATA_TIMER 2500
#define ACK_TIMER 300
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

实现类型：Selective Repeat with Piggybacking。支持 CRC32、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable 和物理层发送队列节流。

## 本次高误码复测

命令：

```powershell
datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l high-ber-longtest-20260511-234732\A.log A
datalink.exe -t 1200 -f --ber=1e-4 -p 63588 -l high-ber-longtest-20260511-234732\B.log B
```

结果：

| Side | Packets | Bps | Utilization | Errors | Observed BER | Quit | Fatal scan |
|---|---:|---:|---:|---:|---:|---|---|
| A | 2654 | 4542 | 56.78% | 825 | 1.0e-004 | yes | clean |
| B | 2533 | 4337 | 54.22% | 817 | 1.0e-004 | yes | clean |

日志尾部均到达 `1200.043 Quit.`。扫描以下关键字无命中：

- `Network Layer received a bad packet`
- `incorrect packet length`
- `Physical Layer Sending Queue overflow`
- `recv_frame(): Receiving Queue is empty`
- `get_packet(): Network layer is not ready`
- `start_timer(): timer`
- `overflow`
- `bad packet`
- `System too busy`
- `FATAL`
- `Abort`

## 与已有五组测试对照

已有五组 20 分钟测试中，高误码 `--ber=1e-4` flood 结果为：

| Side | Utilization | Errors |
|---|---:|---:|
| A | 55.02% | 821 |
| B | 51.73% | 817 |

本次复测为：

| Side | Utilization | Errors |
|---|---:|---:|
| A | 56.78% | 825 |
| B | 54.22% | 817 |

结论：高误码场景可以稳定跑满 20 分钟，且结果可重复；当前性能大致稳定在 52% 到 57% 区间。

## 参数优化判断

### 1. 不建议改窗口大小

`MAX_SEQ=15`、`NR_BUFS=8` 满足 SR 窗口不超过序号空间一半。记录中已经提到窗口缩到 4 后无误码和误码性能都下降，因此窗口不应作为下一轮优化重点。

### 2. 第一优先级：DATA_TIMER

`DATA_TIMER=2500` 在默认误码下稳定，但高误码下如果 NAK 丢失或 NAK 被破坏，就会等待 data timer 才能恢复，2.5 秒会明显拖低吞吐。

建议只做窄范围测试：

- 2300 ms
- 2400 ms
- 2500 ms
- 2600 ms

不建议回到 2000 ms，因为记录中已经观察到严重停顿。

预期：2300/2400 可能提高高误码恢复速度，但过低会造成不必要重传。若默认误码 flood 明显下降，应放弃。

### 3. 第二优先级：PHL_QUEUE_LIMIT

当前 `FRAME_DATA_LEN * 2` 对默认误码 flood 表现很好，能避免 ACK/NAK 被大量 DATA 帧压住。高误码下 ACK/NAK 也更容易损坏，适度放宽到 `FRAME_DATA_LEN * 3` 可能提高连续发送能力，但也可能重新引入控制帧排队。

建议只比较：

- `FRAME_DATA_LEN * 2`
- `FRAME_DATA_LEN * 3`

不建议测试 `* 4` 或无限制，记录中已说明这些方向表现不佳。

### 4. 第三优先级：ACK_TIMER

`ACK_TIMER=300` 对 piggyback 友好，但在高误码下，反向数据或 ACK 损坏时，ACK 延迟会放大恢复等待。

建议只比较：

- 250 ms
- 300 ms
- 350 ms

不建议回到 100/150 ms 这类激进值，控制帧过多可能抵消收益。

### 5. 可能的代码级优化：NAK 触发策略

当前 `no_nak` 防止同一缺口重复 NAK，逻辑简单且稳定。但在 `1e-4` 下，NAK 本身也很容易损坏；如果唯一一次 NAK 损坏，恢复会退化到 DATA timeout。

可考虑的低风险方向：

- 收到同一个缺口后的后续乱序 DATA 时，允许在间隔超过一个小阈值后再次 NAK。
- 阈值可以小于 DATA_TIMER，例如 500 ms 到 800 ms。
- 需要新增 `last_nak_time`，避免 NAK 风暴。

这个方向比单纯调 timer 更复杂，应在参数小矩阵测试后再做。

## 推荐下一轮测试矩阵

先做 300 秒筛选，不直接跑 1200 秒：

| 轮次 | DATA_TIMER | ACK_TIMER | PHL_QUEUE_LIMIT |
|---|---:|---:|---|
| baseline | 2500 | 300 | `FRAME_DATA_LEN * 2` |
| A | 2400 | 300 | `FRAME_DATA_LEN * 2` |
| B | 2300 | 300 | `FRAME_DATA_LEN * 2` |
| C | 2500 | 250 | `FRAME_DATA_LEN * 2` |
| D | 2500 | 300 | `FRAME_DATA_LEN * 3` |
| E | 2400 | 250 | `FRAME_DATA_LEN * 2` |

每轮至少跑：

```powershell
datalink.exe -t 300 -f --ber=1e-4 A
datalink.exe -t 300 -f --ber=1e-4 B
```

筛选标准：

1. A/B 都必须 `Quit.`。
2. 无 fatal/error 关键字。
3. 两端利用率都高于当前 52% 到 57% 区间，或至少低端不低于 54%。
4. 候选参数还要再跑默认误码 `-f` 300 秒，不能牺牲现有 92% 到 93% 的默认误码表现。

最终只对最佳 1 到 2 组跑 1200 秒。

## 2026-05-12 Linux 编译与运行验证

### 编译问题

在 WSL 中执行：

```bash
cd /mnt/c/Users/28641/Desktop/计网/实验/Lab1-2024\(Win+Linux\)/Lab1-2024\(Win+Linux\)/Lab1-linux
make clean
make
```

首次失败点不是 `datalink.c`，而是 `lprintf.h` 与 `lprintf.c` 返回类型不一致：

- `lprintf.h` 原声明：`int lprintf(...)` / `int __v_lprintf(...)`
- `lprintf.c` 实现：`size_t lprintf(...)` / `size_t __v_lprintf(...)`

Windows 版 `lprintf.h` 已经是 `size_t`，因此将 Linux 版 `lprintf.h` 同步为 `size_t`。修复后 WSL `make clean && make` 通过。

### 运行验证

新增脚本：

```bash
Lab1-linux/verify-linux-run.sh
```

执行 60 秒 Linux 版默认误码双向 flood：

```bash
bash verify-linux-run.sh 60 63620 linux-run-verify-20260512
```

结果：

| Side | Packets | Bps | Utilization | Errors | Quit | Fatal scan |
|---|---:|---:|---:|---:|---|---|
| A | 219 | 7600 | 95.00% | 3 | yes | clean |
| B | 214 | 7459 | 93.24% | 5 | yes | clean |

结论：Linux 版能编译、能运行、A/B 能互联，短测日志正常。

## 2026-05-12 Linux 参数小矩阵

新增脚本：

```bash
Lab1-linux/sweep-linux-params.sh
```

该脚本不会修改稳定源码，而是复制一份临时源码，替换宏参数后编译运行。120 秒高误码 `--ber=1e-4` flood 结果位于：

```text
Lab1-linux/param-sweep-20260512-002725/summary.csv
```

结果摘要：

| Variant | DATA_TIMER | ACK_TIMER | Queue frames | A util | B util | Avg | Min | Fatal |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dt2300 | 2300 | 300 | 2 | 57.39% | 60.22% | 58.80% | 57.39% | 0 |
| dt2400_ack250 | 2400 | 250 | 2 | 57.50% | 58.04% | 57.77% | 57.50% | 0 |
| queue3 | 2500 | 300 | 3 | 52.35% | 59.16% | 55.76% | 52.35% | 0 |
| dt2400 | 2400 | 300 | 2 | 58.69% | 49.66% | 54.18% | 49.66% | 0 |
| baseline | 2500 | 300 | 2 | 55.14% | 51.54% | 53.34% | 51.54% | 0 |
| ack250 | 2500 | 250 | 2 | 53.39% | 49.05% | 51.22% | 49.05% | 0 |

额外用默认误码 flood 对前两名做 120 秒验证：

```text
Lab1-linux/param-sweep-20260512-002725/default-validation.csv
```

| Variant | A util | B util | Fatal |
|---|---:|---:|---:|
| dt2300 | 93.64% | 91.40% | 0 |
| dt2400_ack250 | 93.43% | 90.37% | 0 |

## 进一步缩小后的参数范围

基于 record0、20 分钟 Windows 高误码复测、Linux 120 秒高误码矩阵、Linux 120 秒默认误码候选验证，下一步范围可以收窄为：

### 首选候选

```c
#define DATA_TIMER 2300
#define ACK_TIMER 300
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

理由：

- 高误码 120 秒矩阵平均最好，且两端都高于基线。
- 默认误码 120 秒验证未出现明显退化。
- 只改一个参数，风险最低。

### 备选候选

```c
#define DATA_TIMER 2400
#define ACK_TIMER 250
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)
```

理由：

- 高误码两端更均衡。
- 默认误码略低于 dt2300，暂列第二。

### 暂时不优先

- `ACK_TIMER=250` 单独使用：120 秒高误码最差，不建议单独采用。
- `PHL_QUEUE_LIMIT=FRAME_DATA_LEN*3`：两端不均衡，低端只到 52.35%，不应优先。
- `DATA_TIMER=2400, ACK_TIMER=300`：A 端高但 B 端低，不如 dt2300 稳。

### 下一步最小验证矩阵

建议只跑 3 组 300 秒，继续收敛：

| Variant | DATA_TIMER | ACK_TIMER | Queue |
|---|---:|---:|---|
| baseline | 2500 | 300 | 2 |
| candidate-1 | 2300 | 300 | 2 |
| candidate-2 | 2400 | 250 | 2 |

每组跑：

```powershell
datalink.exe -t 300 -f --ber=1e-4 A/B
datalink.exe -t 300 -f A/B
```

如果 `candidate-1` 在 300 秒高误码和默认误码都继续领先，再将 Windows 与 Linux 两份 `datalink.c` 的 `DATA_TIMER` 从 2500 改为 2300，并跑最终 1200 秒验收级复测。

## 2026-05-12 300 秒最小矩阵复测

增强 `Lab1-linux/verify-minimal-matrix.sh`：若 A 端因端口 bind 失败在启动后立即退出，自动换端口重试，避免把环境端口问题误判为协议参数失败。

有效结果目录：

```text
Lab1-linux/minimal-matrix-20260512-092212/summary.csv
```

结果摘要：

| Variant | Scenario | DATA_TIMER | ACK_TIMER | Queue | A util | B util | Fatal |
|---|---|---:|---:|---:|---:|---:|---:|
| baseline | default | 2500 | 300 | 2 | 93.93% | 91.52% | 0 |
| baseline | high | 2500 | 300 | 2 | 52.28% | 55.30% | 0 |
| candidate_dt2300 | default | 2300 | 300 | 2 | 93.41% | 91.36% | 0 |
| candidate_dt2300 | high | 2300 | 300 | 2 | 54.79% | 56.52% | 0 |
| candidate_dt2400_ack250 | default | 2400 | 250 | 2 | 93.88% | 93.09% | 0 |
| candidate_dt2400_ack250 | high | 2400 | 250 | 2 | 50.64% | 54.80% | 0 |

结论：

- `candidate_dt2300` 在高误码两端均优于基线，默认误码仅小幅波动。
- `candidate_dt2400_ack250` 默认误码较好，但高误码低端下降明显，不适合落地。
- 已将 Windows/Linux 两份 `datalink.c` 的 `DATA_TIMER` 从 `2500` 调整为 `2300`，进入 1200 秒最终复测阶段。

## 2026-05-12 DATA_TIMER=2300 最终 1200 秒复测

新增脚本：

```text
Lab1-linux/verify-final-dt2300.sh
```

脚本会编译当前 Linux 源码，然后连续运行 1200 秒默认误码 flood 与 1200 秒高误码 flood。有效结果目录：

```text
Lab1-linux/final-validation-20260512-095621/summary.csv
```

结果：

| Scenario | Side | Packets | Bps | Utilization | Errors | Observed BER | Quit | Fatal |
|---|---|---:|---:|---:|---:|---:|---|---:|
| default | A | 4364 | 7459 | 93.24% | 93 | 9.9e-06 | yes | 0 |
| default | B | 4333 | 7406 | 92.58% | 95 | 1.0e-05 | yes | 0 |
| high | A | 2597 | 4442 | 55.52% | 838 | 1.0e-04 | yes | 0 |
| high | B | 2589 | 4437 | 55.46% | 840 | 1.0e-04 | yes | 0 |

最终结论：

- `DATA_TIMER=2300` 保留了默认误码 flood 的 92% 到 93% 水平。
- 高误码 `--ber=1e-4` 两端更加均衡，低端从此前约 51.73%/54.22% 提升到 55.46%。
- 当前建议将 `DATA_TIMER=2300, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2` 作为最终参数组合。
