# 数据链路层滑动窗口协议开发记录

## 1. 项目背景与实验目标

本项目是计算机网络实验一：数据链路层滑动窗口协议的设计与实现。当前开发目录位于：

`C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)`

实验核心目标：

- 在 8000 bps 全双工卫星信道上实现 A/B 两站点通信
- 单向传播时延 270 ms
- 默认误码率 1e-5
- 网络层分组长度固定为 256 字节，即 `PKT_LEN`
- 实现有噪音信道下的无差错双工通信
- 尽可能提高线路利用率
- 程序需稳定运行，满足验收和性能测试要求

从已阅读的实验材料看，验收重点包括：可靠传输、滑动窗口、ACK piggyback、ACK timer、NAK、Selective Repeat、误码恢复、长时间稳定运行、性能指标记录与分析。

## 2. 当前实现状态总览

当前 `datalink.c` 的主要实现是 Selective Repeat 风格的滑动窗口协议，不是单独的 Go-Back-N 实现。它支持全双工、CRC32、piggyback ACK、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable 流控，以及基于物理层发送队列长度的发送节流。

当前整体已经具备实验基础功能，并完成过一次默认误码率 `-f` 双向 flood 20 分钟长测，A/B 两端日志均到达 `1200.018 Quit.`，最终利用率约 92.66%。但仍需要继续补齐五组标准测试，并进一步验证高误码率 `--ber=1e-4` 下的长时间稳定性。

| 功能 | 当前状态 | 相关文件/函数 | 备注 |
|---|---|---|---|
| Selective Repeat 发送窗口 | 已完成 | `datalink.c`, `main`, `between`, `send_data_frame` | 窗口大小为 8，序号空间 0..15 |
| Go-Back-N 独立实现 | 未实现 | 无 | 当前是 SR 实现，不是 GBN 版本 |
| 全双工通信 | 已完成 | `main`, `send_data_frame` | A/B 两端同一程序，可同时收发 |
| CRC32 生成 | 已完成 | `send_data_frame`, `crc32` | 手工序列化后追加 4 字节 CRC |
| CRC32 校验 | 已完成 | `FRAME_RECEIVED` 分支 | `crc32(frame, len) != 0` 时判坏帧 |
| Piggyback ACK | 已完成 | `send_data_frame` | DATA 帧携带当前累计 ACK |
| ACK timer | 已完成 | `FRAME_RECEIVED`, `ACK_TIMEOUT` | 当前 `ACK_TIMER=300` ms |
| NAK | 已完成 | `FRAME_RECEIVED`, `FRAME_NAK` 处理 | 使用 `no_nak` 防止对同一缺口重复 NAK |
| 乱序缓存 | 已完成 | `in_buf`, `arrived`, DATA 接收逻辑 | 在接收窗口内缓存乱序帧 |
| 序号回绕 | 已完成 | `inc`, `between` | 环形序号比较 |
| 网络层流控 | 已完成 | `update_network_layer` | 窗口满或物理队列偏高时 disable |
| 物理层发送队列控制 | 已完成 | `update_network_layer`, `PHL_QUEUE_LIMIT` | 当前阈值为两个数据帧长度 |
| Linux 版本同步 | 部分完成 | `Lab1-linux\datalink.c` | 当前与 Windows 版 `datalink.c` 内容一致；Linux 编译运行仍需验证 |
| 长时间默认误码 flood | 已完成 | `datalink-A.log`, `datalink-B.log` | 已确认 20 分钟，约 92.66% |
| 五组验收测试完整记录 | 部分完成 | 日志/记录表 | 当前只有部分场景有证据，仍需补齐 |

当前风险点：

- `PHYSICAL_LAYER_READY` 分支目前只 `break`，依赖循环末尾的 `update_network_layer()` 恢复网络层；短测和 20 分钟默认 flood 正常，但仍建议继续观察。
- `ACK_TIMER=300` 是当前稳定参数，不一定是全局最优。
- `DATA_TIMER=2500` 偏保守，高误码下可能牺牲恢复速度，但当前默认误码 20 分钟表现稳定。
- 后续参数优化应小范围进行，不要再大范围乱跳；前面已观察到 `DATA_TIMER=2000`、接收/发送窗口缩到 4、取消物理队列限制、物理队列阈值放到 4 帧等方向不理想。
- 当前没有 Git 仓库元数据，无法用 `git diff` 和提交历史复盘真实提交链；已递归查找，当前实验目录下未发现 `.git` 目录。
- `struct FRAME` 仍保留 `padding` 字段，但实际发送/接收已经使用手工字节序列化，不依赖结构体内存布局。

## 3. 仓库结构与关键文件

当前主要路径：

`C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)`

| 文件/目录 | 作用 | 是否建议修改 |
|---|---|---|
| `Lab1-Windows-VS2019\datalink.c` | Windows/VS 工程当前主实现文件 | 是，当前主要维护文件 |
| `Lab1-linux\datalink.c` | Linux 版本实现文件 | 是，但需保持与 Windows 版本同步 |
| `datalink.h` | 帧类型等数据链路层声明 | 谨慎修改，当前一般不需要动 |
| `protocol.h` | 实验框架 API 声明，包括事件、网络层、物理层、定时器、CRC | 不应随意修改 |
| `protocol.c` | 实验模拟框架实现 | 不应修改 |
| `crc32.c` | CRC32 实现 | 不应修改 |
| `lprintf.c/.h` | 日志输出支持 | 不应修改 |
| `getopt.c/.h` | Windows 命令行参数兼容支持 | 不应修改 |
| `Lab1-linux\Makefile` | Linux 编译脚本 | 只在编译环境需要时谨慎修改 |
| `Lab1-Windows-VS2019\datalink.sln` | Visual Studio 解决方案 | 一般不修改 |
| `Lab1-Windows-VS2019\datalink.vcxproj` | VS 工程文件 | 一般不修改 |
| `Lab1-Windows-VS2019\datalink.exe` | 当前 Windows 可执行文件 | 编译产物 |
| `Lab1-Windows-VS2019\datalink-A.log` | 当前 A 端运行日志 | 测试证据 |
| `Lab1-Windows-VS2019\datalink-B.log` | 当前 B 端运行日志 | 测试证据 |
| `性能测试记录表.docx` | 学生填写性能记录表 | 实验文档 |
| `性能测试记录表-参考数据.docx` | 参考性能数据 | 实验文档 |
| `计算机网络实验一验收说明（学生版）.pdf` | 验收要求 | 实验文档 |
| `Lab1-DataLinkLayerDesign.pdf` | 数据链路层协议设计参考 | 实验文档 |
| `计算机网络实验一实验指导书.pdf` | 实验指导书 | 实验文档 |
| `rfc1662.txt` | PPP/HDLC framing 参考资料 | 参考文档 |

注意：当前 `C:\Users\28641\Desktop\计网\实验` 下执行 `git status --short` 和 `git log --oneline -5` 均返回 `fatal: not a git repository`，并且递归查找未发现 `.git` 目录。因此本记录不能基于真实 Git 提交历史，只能基于当前文件状态、终端输出、测试日志和前面开发过程上下文。

## 4. 本轮多轮迭代中做过的主要修改

以下记录按模块归纳。由于当前目录不是 Git 仓库，无法用提交历史逐项证明；已测试与否只按终端输出、日志和当前可见代码标注。

### 4.1 帧结构与手工序列化

- 修改文件：`Lab1-Windows-VS2019\datalink.c`, `Lab1-linux\datalink.c`
- 涉及函数：`send_data_frame`
- 修改前问题：直接发送结构体可能受结构体 padding、编译器对齐、平台差异影响。
- 修改后行为：使用 `unsigned char frame[FRAME_MAX_LEN]` 手工打包 `kind/seq/ack/payload/crc`。
- 验证情况：Windows 当前版本可运行 20 分钟 `-f` flood 默认误码长测。
- 备注：Windows 与 Linux 两份 `datalink.c` 当前内容一致。`struct FRAME` 仍存在，但只作为解析后的临时变量使用，不直接作为物理层发送格式。

### 4.2 CRC 生成与验证

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED`
- 修改前问题：需要确保发送前追加 CRC，接收时对完整帧校验。
- 修改后行为：发送时对头部和 payload 计算 `crc32(frame, len)`，追加 4 字节；接收时 `crc32(frame, len) != 0` 判为坏帧。
- 验证情况：默认误码长测日志显示有误码计数但程序未崩溃，能持续恢复。
- 备注：CRC 字节当前按低字节到高字节写入；与本实验框架的 `crc32` 使用方式匹配。

### 4.3 发送窗口逻辑

- 修改文件：`datalink.c`
- 涉及函数：`NETWORK_LAYER_READY`, `between`, `update_network_layer`
- 修改前问题：需要从 stop-and-wait 或初始框架扩展到滑动窗口。
- 修改后行为：`next_frame_to_send` 指向下一个发送序号，`ack_expected` 指向发送窗口下界，`nbuffered` 记录未确认帧数。
- 验证情况：20 分钟 `-f` 默认误码长测通过。
- 备注：窗口大小 `NR_BUFS=8`，序号空间 16，满足 SR 窗口不超过序号空间一半的要求。

### 4.4 接收窗口与乱序缓存

- 修改文件：`datalink.c`
- 涉及函数：`FRAME_RECEIVED`
- 修改前问题：只按序接收会退化为 GBN，误码恢复性能较差。
- 修改后行为：在 `[frame_expected, too_far)` 接收窗口内缓存乱序帧，`arrived[]` 标记到达状态，按序连续可交付时批量 `put_packet`。
- 验证情况：默认误码长测通过；高误码 20 分钟待验证。
- 备注：缓存下标使用 `seq % NR_BUFS`。

### 4.5 ACK 与 Piggyback

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED`, `ACK_TIMEOUT`
- 修改前问题：立即 ACK 会增加控制帧开销；无 ACK timer 又可能让对端等待过久。
- 修改后行为：DATA 帧携带 `ack_nr = last in-order frame`；收到 DATA 后启动 ACK timer，若短时间内有反向 DATA 则 piggyback，否则 ACK_TIMEOUT 发送纯 ACK。
- 验证情况：默认误码 20 分钟长测通过。
- 备注：当前 ACK timer 为 300 ms；之前曾考虑 100 ms，但最终稳定版本为 300 ms。

### 4.6 NAK 快速重传

- 修改文件：`datalink.c`
- 涉及函数：`FRAME_RECEIVED`, `send_data_frame`
- 修改前问题：只依赖 DATA timeout 恢复误码会明显降低性能。
- 修改后行为：坏 CRC 或收到接收窗口内的乱序 DATA 时，如果 `no_nak` 为真，立即发送 NAK 请求 `frame_expected`；收到 NAK 后只重传被请求的单帧。
- 验证情况：默认误码长测通过；高误码长测待补。
- 备注：修复过一个关键点：NAK 的 `ack` 字段表示“请求重传的缺失帧”，不能被当作累计 ACK 处理。因此当前代码在 `f.kind != FRAME_NAK` 时才执行累计 ACK 滑动。

### 4.7 DATA timer 与超时重传

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `DATA_TIMEOUT`
- 修改前问题：需要避免整窗重传造成性能下降。
- 修改后行为：每个 DATA 帧使用自身序号启动定时器；超时时仅重传 `arg` 对应单帧。
- 验证情况：默认误码长测通过。
- 备注：当前 `DATA_TIMER=2500` ms，偏保守但稳定。

### 4.8 网络层 enable/disable 与物理队列节流

- 修改文件：`datalink.c`
- 涉及函数：`update_network_layer`
- 修改前问题：flood 模式下如果只看发送窗口，物理层队列可能堆积，影响稳定性和性能。
- 修改后行为：仅当 `nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 时启用网络层，否则关闭。
- 验证情况：加入队列控制后，默认误码 20 分钟 `-f` 长测约 92.66%。
- 备注：当前 `PHL_QUEUE_LIMIT = FRAME_DATA_LEN * 2`。这是本轮最关键的性能优化方向：限制物理层发送队列积压，避免 ACK/NAK 被大量 DATA 帧压在后面。先前版本 20 分钟长测约 82%，该优化后提升到约 92.66%，接近参考 SR flood 的 95% 水平。后续只建议在 2 帧和 3 帧阈值之间做窄范围验证；4 帧阈值已观察到无误码和误码性能都有下降，不建议继续作为重点。

### 4.9 初始化清理

- 修改文件：`datalink.c`
- 涉及函数：`main`
- 修改前问题：长测前需要避免脏内存导致未定义行为。
- 修改后行为：进入事件循环前显式重置窗口变量、NAK 状态、发送/接收缓存和 `arrived` 数组。
- 验证情况：当前长测版本已包含该初始化。
- 备注：初始化后调用 `enable_network_layer()`。

### 4.10 日志与调试输出

- 修改文件：`datalink.c`
- 涉及函数：`send_data_frame`, `FRAME_RECEIVED`, `DATA_TIMEOUT`
- 修改前问题：调试时难以定位 ACK/NAK/DATA 行为。
- 修改后行为：使用 `dbg_frame` 输出发送/接收 DATA/ACK/NAK，使用 `dbg_event` 输出 CRC 错误和 timeout。
- 验证情况：可在 `-d` 参数打开不同等级日志。
- 备注：性能长测时日志等级不宜太高，避免输出影响结果。

## 5. 核心数据结构说明

| 名称 | 类型 | 含义 | 取值/范围 | 使用位置 |
|---|---|---|---|---|
| `MAX_SEQ` | 宏 | 最大序号 | 15，即 0..15 | 序号空间、回绕 |
| `NR_BUFS` | 宏 | 发送/接收窗口大小 | 8 | `out_buf`, `in_buf`, 窗口判断 |
| `DATA_TIMER` | 宏 | DATA 重传定时器 | 2500 ms | `send_data_frame`, `DATA_TIMEOUT` |
| `ACK_TIMER` | 宏 | 延迟 ACK 定时器 | 300 ms | `FRAME_RECEIVED`, `ACK_TIMEOUT` |
| `FRAME_HEAD_LEN` | 宏 | 帧头长度 | 3 字节 | 序列化/解析 |
| `FRAME_CRC_LEN` | 宏 | CRC 长度 | 4 字节 | 序列化/解析 |
| `FRAME_DATA_LEN` | 宏 | DATA 帧不含 CRC 长度 | 259 字节 | `send_data_frame` |
| `FRAME_MAX_LEN` | 宏 | 最大物理帧长度 | 263 字节 | 接收缓冲、发送缓冲 |
| `PHL_QUEUE_LIMIT` | 宏 | 物理队列节流阈值 | `FRAME_DATA_LEN * 2` | `update_network_layer` |
| `FRAME_DATA` | 枚举/宏 | 数据帧类型 | 来自 `datalink.h` | `send_data_frame`, `FRAME_RECEIVED` |
| `FRAME_ACK` | 枚举/宏 | ACK 帧类型 | 来自 `datalink.h` | `ACK_TIMEOUT`, `FRAME_RECEIVED` |
| `FRAME_NAK` | 枚举/宏 | NAK 帧类型 | 来自 `datalink.h` | 坏帧/乱序恢复 |
| `struct FRAME.kind` | `unsigned char` | 帧类型 | DATA/ACK/NAK | 接收后解析 |
| `struct FRAME.seq` | `unsigned char` | 数据帧序号 | 0..15 | DATA 收发 |
| `struct FRAME.ack` | `unsigned char` | ACK 或 NAK 请求号 | 0..15 | ACK/NAK 处理 |
| `struct FRAME.data` | `unsigned char[PKT_LEN]` | 网络层 payload | 256 字节 | DATA 帧 |
| `ack_expected` | `unsigned char` | 发送窗口下界 | 0..15 | ACK 处理、timeout 判断 |
| `next_frame_to_send` | `unsigned char` | 下一个发送序号 | 0..15 | NETWORK_LAYER_READY |
| `frame_expected` | `unsigned char` | 接收窗口下界/期望序号 | 0..15 | DATA 接收、ACK/NAK 发送 |
| `too_far` | `unsigned char` | 接收窗口上界的后一位 | 0..15 | 接收窗口判断 |
| `nbuffered` | `unsigned char` | 当前未确认发送帧数 | 0..8 | 流控、ACK 处理 |
| `no_nak` | `unsigned char` | 是否允许对当前缺口发送 NAK | 0/1 | CRC 错误、乱序 DATA |
| `out_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 发送缓存 | 8 个分组 | 发送、重传 |
| `in_buf` | `unsigned char[NR_BUFS][PKT_LEN]` | 接收缓存 | 8 个分组 | 乱序缓存、交付 |
| `arrived` | `unsigned char[NR_BUFS]` | 接收缓存占用标记 | 0/1 | DATA 接收、按序交付 |

## 6. 核心函数说明

### `main(int argc, char **argv)`

- 参数含义：命令行参数透传给实验框架。
- 返回值含义：正常情况下不返回。
- 主要功能：初始化协议、清理状态、启用网络层、进入事件驱动主循环。
- 关键逻辑：通过 `wait_for_event(&arg)` 获取事件，用 `switch(event)` 处理网络层、物理层、接收、DATA timeout、ACK timeout。
- 调用时机：程序入口。
- 注意事项：严格单线程事件驱动，不应引入多线程或阻塞等待。

### `between(unsigned char a, unsigned char b, unsigned char c)`

- 参数含义：判断环形序号 `b` 是否位于 `[a, c)`。
- 返回值含义：真表示在区间内。
- 主要功能：处理序号回绕下的窗口区间判断。
- 关键逻辑：分三种环形区间情况判断。
- 调用时机：ACK 滑动、DATA 接收窗口判断、NAK/timeout 是否有效判断。
- 注意事项：SR 窗口大小必须不超过序号空间一半，否则新旧帧可能混淆。

### `update_network_layer(void)`

- 参数含义：无。
- 返回值含义：无。
- 主要功能：根据发送窗口和物理层队列状态启用或关闭网络层。
- 关键逻辑：`nbuffered < NR_BUFS && phl_sq_len() < PHL_QUEUE_LIMIT` 时 `enable_network_layer()`，否则 `disable_network_layer()`。
- 调用时机：事件处理后、NETWORK_LAYER_READY 后。
- 注意事项：这是当前性能优化的关键点之一，避免 flood 模式下物理层队列过度堆积。

### `send_data_frame(unsigned char kind, unsigned char frame_nr, unsigned char frame_expected, unsigned char buffer[])`

- 参数含义：
  - `kind`：帧类型，DATA/ACK/NAK。
  - `frame_nr`：DATA 帧序号；ACK/NAK 中通常为 0。
  - `frame_expected`：本端当前期望接收的序号，用于计算 ACK 或 NAK。
  - `buffer`：DATA payload 指针；ACK/NAK 可为 `NULL`。
- 返回值含义：无。
- 主要功能：构造物理层字节流，追加 CRC32，调用 `send_frame()`。
- 关键逻辑：
  - 默认 ACK 字段为 `(frame_expected + MAX_SEQ) % (MAX_SEQ + 1)`，表示最后一个按序收到的帧。
  - NAK 帧特殊处理，ACK 字段改为 `frame_expected`，表示请求重传的缺失帧。
  - DATA 帧发送后启动该序号的数据定时器。
  - 任意发送后停止 ACK timer，因为 ACK 已被发出或 piggyback。
- 调用时机：发送 DATA、ACK timeout、坏 CRC 发送 NAK、乱序发送 NAK、收到 NAK 后重传、DATA timeout 重传。
- 注意事项：当前函数名叫 `send_data_frame`，实际也发送 ACK/NAK。

### `FRAME_RECEIVED` 内部处理逻辑

当前代码没有单独拆成 `handle_data_frame/handle_ack/handle_nak` 函数，而是在 `switch` 的 `FRAME_RECEIVED` 分支中完成。

- 参数含义：从 `recv_frame(frame, sizeof frame)` 得到原始字节。
- 返回值含义：无。
- 主要功能：校验 CRC，解析帧头，处理 ACK/NAK/DATA。
- 关键逻辑：
  - 坏 CRC：若 `no_nak` 为真，发送 NAK 请求 `frame_expected`。
  - 非 NAK 帧：先处理累计 ACK 并滑动发送窗口。
  - NAK 帧：只重传被请求的单帧。
  - DATA 帧：若在接收窗口内且未缓存，缓存；乱序时可能发 NAK；按序连续到达时批量交付网络层。
- 调用时机：事件 `FRAME_RECEIVED`。
- 注意事项：NAK 的 `ack` 字段不是累计 ACK，不能参与发送窗口滑动。

### `DATA_TIMEOUT` 内部处理逻辑

- 参数含义：`arg` 是超时定时器编号，即对应 DATA 序号。
- 返回值含义：无。
- 主要功能：选择性重传该序号的 DATA 帧。
- 关键逻辑：只有当 `arg` 位于当前发送窗口内才重传。
- 调用时机：事件 `DATA_TIMEOUT`。
- 注意事项：不会整窗重传，符合 SR 的性能目标。

### `ACK_TIMEOUT` 内部处理逻辑

- 参数含义：无额外参数。
- 返回值含义：无。
- 主要功能：发送纯 ACK，避免反向无数据时 ACK 被无限延迟。
- 关键逻辑：调用 `send_data_frame(FRAME_ACK, 0, frame_expected, NULL)`。
- 调用时机：事件 `ACK_TIMEOUT`。
- 注意事项：发送 ACK 后 `send_data_frame` 会停止 ACK timer。

### CRC 相关函数

- `crc32(unsigned char *buf, int len)` 由实验框架提供。
- 发送时：先对不含 CRC 的字节流计算 CRC，再追加到末尾。
- 接收时：对含 CRC 的完整帧计算 CRC，结果为 0 表示通过。
- 注意事项：不要重新实现 `crc32`。

### 日志相关函数

- `dbg_frame`：输出帧级调试信息，如 DATA/ACK/NAK 收发。
- `dbg_event`：输出事件级调试信息，如 CRC 错误、timeout。
- `lprintf`：输出启动构建信息。
- 注意事项：性能测试时调试等级过高可能影响吞吐。

## 7. 事件驱动主循环说明

当前主循环严格使用：

```c
for (;;) {
    event = wait_for_event(&arg);
    switch (event) {
        ...
    }
    update_network_layer();
}
```

### `NETWORK_LAYER_READY`

实际行为：

- 如果 `nbuffered < NR_BUFS`，调用 `get_packet()` 取一个 256 字节分组到 `out_buf[next_frame_to_send % NR_BUFS]`。
- `nbuffered++`。
- 调用 `send_data_frame(FRAME_DATA, next_frame_to_send, frame_expected, ...)` 发送 DATA，并 piggyback 当前 ACK。
- `inc(next_frame_to_send)`。
- 调用 `update_network_layer()` 根据窗口和物理队列控制网络层。

可能修改：

- `out_buf`
- `nbuffered`
- `next_frame_to_send`
- DATA timer
- ACK timer
- 网络层 enable/disable 状态

### `PHYSICAL_LAYER_READY`

实际行为：

- 当前分支只 `break`。
- 循环末尾仍会调用 `update_network_layer()`，因此当物理队列下降到阈值以下时，网络层可能被重新启用。

可能修改：

- 直接不修改状态。
- 间接通过循环末尾 `update_network_layer()` 影响网络层开关。

### `FRAME_RECEIVED`

实际行为：

- 调用 `recv_frame()` 得到原始帧。
- 检查最小长度和 CRC。
- CRC 错误时根据 `no_nak` 发送一次 NAK，然后丢弃。
- CRC 正确时解析 `kind/seq/ack`。
- 对非 NAK 帧处理累计 ACK，停止对应 timer，滑动 `ack_expected`，减少 `nbuffered`。
- 对 ACK 帧只记录日志。
- 对 NAK 帧检查请求序号是否在发送窗口内，若在则立即重传对应 DATA。
- 对 DATA 帧检查是否在接收窗口内：
  - 在窗口内且未到达：缓存。
  - 若乱序且当前允许 NAK：发送 NAK 请求 `frame_expected`。
  - 若 `arrived[frame_expected % NR_BUFS]` 连续为真：依次 `put_packet()`，滑动 `frame_expected/too_far`，并重置 `no_nak=1`。
- DATA 帧处理后启动 ACK timer。

可能修改：

- `ack_expected`
- `nbuffered`
- DATA timers
- `f`
- `in_buf`
- `arrived`
- `frame_expected`
- `too_far`
- `no_nak`
- ACK timer

### `DATA_TIMEOUT`

实际行为：

- `arg` 是超时序号。
- 若 `arg` 仍在当前发送窗口内，则从 `out_buf[arg % NR_BUFS]` 重构并重传该 DATA。
- `send_data_frame` 会重启该序号 timer。

可能修改：

- 对应 DATA timer
- ACK timer
- 物理发送队列

### `ACK_TIMEOUT`

实际行为：

- 发送纯 ACK。
- ACK 字段为当前最后一个按序收到的帧。
- `send_data_frame` 内部停止 ACK timer。

可能修改：

- ACK timer
- 物理发送队列

## 8. 协议工作流程

### 8.1 正常发送流程

1. 网络层准备好分组，触发 `NETWORK_LAYER_READY`。
2. 数据链路层将分组保存到 `out_buf[next_frame_to_send % NR_BUFS]`。
3. `nbuffered++`，表示发送窗口内多了一个未确认帧。
4. 发送 DATA 帧，帧头包含：
   - `kind = FRAME_DATA`
   - `seq = next_frame_to_send`
   - `ack = last in-order received`
5. 启动该序号的 DATA timer。
6. `next_frame_to_send` 环形递增。
7. 收到对端 ACK 或 piggyback ACK 后，通过 `between(ack_expected, f.ack, next_frame_to_send)` 滑动发送窗口。
8. 每滑过一个已确认帧，停止对应 timer，`nbuffered--`。
9. 窗口打开后，`update_network_layer()` 重新启用网络层。

### 8.2 正常接收流程

1. 物理层收到帧，触发 `FRAME_RECEIVED`。
2. 调用 `recv_frame()` 读取原始字节。
3. 对完整帧执行 CRC 校验。
4. CRC 正确且为 DATA：
   - 若序号在接收窗口内且尚未缓存，则复制 payload 到 `in_buf`。
   - 若该帧正好是 `frame_expected`，进入连续交付循环。
   - 若之前缓存过后续乱序帧，也会在循环中一并交付。
5. 每交付一个分组：
   - 调用 `put_packet()`。
   - 清除 `arrived`。
   - `inc(frame_expected)`。
   - `inc(too_far)`。
   - 重置 `no_nak=1`。
6. 启动 ACK timer，等待可能的反向 DATA 进行 piggyback；若超时则发纯 ACK。

### 8.3 误码/丢帧恢复流程

CRC 错误：

- 当前帧被丢弃。
- 若 `no_nak == 1`，立即发送 NAK 请求 `frame_expected`。
- 设置 `no_nak = 0`，避免对同一个缺口重复发 NAK。

乱序 DATA：

- 如果序号在接收窗口内，先缓存。
- 如果 `seq != frame_expected` 且 `no_nak == 1`，发送 NAK 请求缺失的 `frame_expected`。

收到 NAK：

- NAK 的 `ack` 字段被解释为请求重传的序号。
- 如果该序号在当前发送窗口内，只重传这一帧。

ACK 丢失：

- 如果后续 DATA 或 ACK 携带更高累计 ACK，发送窗口仍可滑动。
- 如果一直没有收到 ACK，对应 DATA timer 超时后会重传单帧。

DATA timeout：

- 只重传超时序号对应的 DATA，不整窗重传。

### 8.4 序号回绕处理

当前序号空间为 0..15，窗口大小为 8。`inc(k)` 在 `MAX_SEQ` 后回到 0。所有窗口内判断使用 `between(a,b,c)`，避免普通整数比较在回绕时失效。

因为 SR 协议窗口大小不超过序号空间一半，当前 `NR_BUFS = (MAX_SEQ + 1) / 2 = 8`，满足避免新旧帧混淆的基本条件。

## 9. 当前参数选择与理由

| 参数 | 当前值 | 理由 | 是否建议继续调整 |
|---|---:|---|---|
| `MAX_SEQ` | 15 | 序号空间 16，配合 SR 窗口 8 | 可保持 |
| `NR_BUFS` | 8 | 满足 SR 窗口不超过序号空间一半，提升带宽利用 | 可保持 |
| `DATA_TIMER` | 2500 ms | 当前稳定参数，默认误码 20 分钟长测通过 | 只建议窄范围测试 2300/2400/2500/2600/2700；`2000` 已观察到严重停顿，不建议再测 |
| `ACK_TIMER` | 300 ms | 当前稳定参数，兼顾 piggyback 与及时 ACK | 只建议窄范围测试 250/300/350；不建议再回到 100/150 这类过激参数 |
| `PHL_QUEUE_LIMIT` | `FRAME_DATA_LEN * 2` | 限制 flood 下物理层队列堆积，当前 20 分钟默认误码表现稳定 | 只建议测试 `FRAME_DATA_LEN * 2` 或 `* 3`；`* 4` 和无限制方向已不建议 |
| DATA 帧头 | 3 字节 | `kind/seq/ack` | 可保持 |
| CRC 长度 | 4 字节 | CRC32 | 可保持 |
| Payload | 256 字节 | 实验框架固定 `PKT_LEN` | 不应修改 |

当前参数针对 8000 bps、270 ms 单向时延、256 字节分组做过经验调优。已经确认的最佳方向不是继续扩大窗口或放宽物理队列，而是控制物理层排队，让 ACK/NAK 不被大量 DATA 帧堵住。由于 20 分钟长测只确认了默认误码双向 flood，参数仍应标注为“待进一步性能测试优化”，尤其是高误码率 `--ber=1e-4` 场景。

## 10. 测试命令与运行方式

### 编译

Windows 当前主要通过 VS 工程或已生成的 `datalink.exe` 运行。此前从 VS/终端可以看到 Windows 可执行文件路径：

```powershell
cd "C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019"
.\datalink.exe ...
```

Linux 推荐方式，待在 Linux 环境验证：

```bash
cd "Lab1-2024(Win+Linux)/Lab1-2024(Win+Linux)/Lab1-linux"
make
```

注意：当前 PowerShell 环境直接使用 Linux `make` 的验证情况待确认。

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

以下命令按实验要求整理。A/B 两端需要同时运行，端口可用默认，也可显式使用同一 `-p` 端口。

#### 1. 无误码普通传输

Windows：

```powershell
.\datalink.exe -u A
.\datalink.exe -u B
```

Linux：

```bash
./datalink -u A
./datalink -u B
```

#### 2. 默认误码普通传输

Windows：

```powershell
.\datalink.exe A
.\datalink.exe B
```

Linux：

```bash
./datalink A
./datalink B
```

#### 3. 无误码双向 flood

Windows：

```powershell
.\datalink.exe -f -u A
.\datalink.exe -f -u B
```

Linux：

```bash
./datalink -f -u A
./datalink -f -u B
```

#### 4. 默认误码双向 flood

Windows：

```powershell
.\datalink.exe -f A
.\datalink.exe -f B
```

Linux：

```bash
./datalink -f A
./datalink -f B
```

已完成 20 分钟长测时使用的命令：

```powershell
.\datalink.exe -t 1200 -p 62820 -f A
.\datalink.exe -t 1200 -p 62820 -f B
```

#### 5. ber=1e-4 双向 flood

Windows：

```powershell
.\datalink.exe -f --ber=1e-4 A
.\datalink.exe -f --ber=1e-4 B
```

Linux：

```bash
./datalink -f --ber=1e-4 A
./datalink -f --ber=1e-4 B
```

若要长测 20 分钟：

```powershell
.\datalink.exe -t 1200 -f --ber=1e-4 A
.\datalink.exe -t 1200 -f --ber=1e-4 B
```

## 11. 已完成测试记录

以下只记录当前有日志或终端证据的项目；没有证据的不编造。

| 序号 | 测试场景 | A 命令 | B 命令 | 运行时间 | A 利用率 | B 利用率 | 是否通过 | 备注 |
|---:|---|---|---|---:|---:|---:|---|---|
| 1 | 默认误码双向 flood | `datalink.exe -t 1200 -p 62820 -f A` | `datalink.exe -t 1200 -p 62820 -f B` | 1200 s | 92.66% | 92.66% | 已通过本次运行 | `datalink-A.log`/`datalink-B.log` 均到 `1200.018 Quit.` |
| 2 | 无误码双向 flood 短测 | `datalink.exe -f -u A` | `datalink.exe -f -u B` | 约 10 s | 约 96.89% | 约 97.07% | 仅短测 | 待 10/20 分钟正式测试 |
| 3 | 默认误码双向 flood 短测 | `datalink.exe -f A` | `datalink.exe -f B` | 约 10 s | 约 88.04% | 约 93.46% | 仅短测 | 早期短测，最终长测见序号 1 |
| 4 | `PHL_QUEUE_LIMIT` 参数短测 | 多组 flood 命令 | 多组 flood 命令 | 30/60 s | 待确认 | 待确认 | 参数探索 | 只作为调参参考；后续重点只保留 2/3 帧阈值，4 帧方向已不建议继续重点测试 |
| 5 | `--ber=1e-4` 双向 flood | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 高误码长测仍需补齐 |
| 6 | 无误码普通传输 | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 五组验收测试之一 |
| 7 | 默认误码普通传输 | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 待测试 | 五组验收测试之一 |

已核对的 20 分钟长测日志尾部：

```text
A: 1198.277 .... 4335 packets received, 7413 bps, 92.66%, Err 96 (1.0e-005)
A: 1200.018 Quit.
B: 1198.261 .... 4335 packets received, 7413 bps, 92.66%, Err 98 (1.0e-005)
B: 1200.018 Quit.
```

错误/警告记录：

| 时间/场景 | 问题 | 可能原因 | 当前处理状态 |
|---|---|---|---|
| 早期 VS 启动 | `datalink.exe does not exist` | 当时编译失败或 VS 启动路径不对 | 后续已生成 `datalink.exe`，当前不再是阻塞 |
| 早期编译 | `undefined reference to dbg_frame/put_packet/stop_timer/...` | 当时可能只单独编译 `datalink.c`，未链接实验框架 | 使用 VS 工程/完整工程编译后解决 |
| 当前 Git 检查 | `fatal: not a git repository` | 当前工作目录没有 `.git` | 本记录不使用提交历史作为证据 |
| 默认误码长测 | 日志中存在 `Err 96/98` | 信道误码，符合默认 BER | 协议已恢复并跑满 20 分钟 |

已对当前 `datalink-A.log` 和 `datalink-B.log` 扫描以下关键字，未发现致命错误命中：`Network Layer received a bad packet`、`incorrect packet length`、`Physical Layer Sending Queue overflow`、`failed`、`recv_frame(): Receiving Queue is empty`、`get_packet(): Network layer is not ready`、`start_timer(): timer`、`overflow`、`bad packet`。扫描 `error` 只命中了日志开头的信道参数 `bit error rate 1.0E-005`，不是程序错误。

## 12. 当前性能结论

当前有充分证据的性能结论：

- 默认 `1e-5` 误码、双向 flood、20 分钟：A/B 均约 92.66%，程序稳定退出。

当前暂无充分性能结论的项目：

- 无误码普通传输 10/20 分钟正式结果：待测试。
- 默认误码普通传输 10/20 分钟正式结果：待测试。
- 无误码 flood 10/20 分钟正式结果：待测试。
- `--ber=1e-4` 高误码 flood 10/20 分钟正式结果：待测试。

对照参考数据，当前默认误码双向 flood 的 92.66% 明显高于 Go-Back-N 参考水平约 88%，接近但略低于 Selective Repeat 参考水平约 95%。这版相比之前约 82% 的 20 分钟长测已有明显提升，关键优化是限制物理层发送队列积压，让 ACK/NAK 不再被大量 DATA 帧压在后面。最可能继续影响利用率的瓶颈：

- DATA timer 偏保守，错误恢复时等待较长；但 `DATA_TIMER=2000` 已观察到严重停顿，不应再向过低方向大幅调整。
- ACK timer 当前 300 ms，可能在某些场景下略延迟 ACK；后续只建议在 250/300/350 间比较。
- 物理层队列阈值较低，稳定性好但可能抑制瞬时吞吐；后续只建议比较 2 帧与 3 帧阈值。
- NAK 去重策略较简单，高误码下可能仍有恢复空窗。
- 调试日志等级过高时会影响性能，正式测试应降低日志输出。

## 13. 已知问题与风险点

| 问题 | 影响 | 复现方式 | 建议修复方向 | 优先级 |
|---|---|---|---|---|
| 五组标准测试未全部完成 | 性能表和验收证据不足 | 按第 10 节运行五组测试 | 补齐每组至少 10 分钟，最好 20 分钟 | P0 |
| 高误码 `--ber=1e-4` 长测待验证 | 可能隐藏 NAK/timeout 交互问题 | `-t 1200 -f --ber=1e-4` | 先跑 5 分钟，再 20 分钟，扫描日志 | P0 |
| Linux 编译运行待确认 | 跨平台验收可能有风险 | Linux 目录执行 `make` | 两份 `datalink.c` 当前内容一致；下一步重点是验证 Linux 编译和运行 | P0 |
| 当前没有 Git 仓库 | 难以回滚和追踪修改 | `git status` | 若允许，初始化 Git 或备份当前稳定文件 | P0 |
| `ACK_TIMER=300` 未必最优 | 可能降低 piggyback/ACK 响应效率 | 只扫测 250/300/350 | 用 600 秒筛选，再选最佳组合跑 1200 秒 | P1 |
| `DATA_TIMER=2500` 偏保守 | 错误恢复可能慢 | 只扫测 2300/2400/2500/2600/2700；不要再测 2000 | 默认误码和高误码分别测试 | P1 |
| `PHL_QUEUE_LIMIT=2 frames` 可能限制吞吐 | 无误码或低误码场景可能未达最佳 | 只比较 2/3 帧阈值；4 帧和无限制方向已排除 | 比较无误码 flood 和默认 flood | P1 |
| `PHYSICAL_LAYER_READY` 分支较空 | 极端队列状态下恢复逻辑依赖循环末尾 | 人工提高 flood 压力观察 | 可在该事件中显式 `update_network_layer()` | P2 |
| 日志过多影响性能 | 调试等级高时吞吐下降 | 使用 `-d3` 长测 | 正式性能测试避免高 debug 等级 | P2 |
| `struct FRAME.padding` 残留 | 易误导后续维护者 | 阅读源码 | 可加注释说明实际不用于 wire format | P2 |

## 14. 后续开发建议

### P0：必须完成

- 跑完五组性能测试，每组至少 10 分钟，推荐 20 分钟。
- 对每组测试保留 A/B 两端命令、运行时间、最终利用率、错误计数、日志文件名。
- 扫描日志是否出现：
  - `Network Layer received a bad packet`
  - `incorrect packet length`
  - `Physical Layer Sending Queue overflow`
  - `failed`
  - `recv_frame(): Receiving Queue is empty`
  - `get_packet(): Network layer is not ready`
- 验证 `--ber=1e-4` 高误码场景能恢复，不死锁。
- 确认 Linux 版本能编译运行。
- 在继续大改前备份当前稳定版本。

### P1：建议优化

- 不要再大范围乱跳参数。后续只测试这些组合附近：
  - `DATA_TIMER`：2300、2400、2500、2600、2700 ms。
  - `ACK_TIMER`：250、300、350 ms。
  - `PHL_QUEUE_LIMIT`：`FRAME_DATA_LEN * 2` 或 `FRAME_DATA_LEN * 3`。
- 已排除或不建议继续重点测试：
  - `DATA_TIMER=2000`：出现严重停顿，不稳。
  - 窗口 4：无误码和误码性能都下降。
  - 不限制物理队列：默认误码长期性能明显低。
  - `PHL_QUEUE_LIMIT=FRAME_DATA_LEN * 4`：短测表现不如 2/3 帧方向。
- 下一轮建议用 600 秒做参数筛选，最后只对最佳组合跑 1200 秒。当前版本已经可以作为报告中的正式 `-f` 20 分钟性能数据。
- 优化 NAK 去重策略，观察高误码下是否存在长时间等待 timeout 的情况。
- 减少正式测试中的调试输出。

### P2：可选加分

- 将 `FRAME_RECEIVED` 内部逻辑拆成 `handle_ack`、`handle_nak`、`handle_data_frame`、`handle_timeout`，提高可读性。
- 增强调试输出，统计 NAK 数、timeout 数、重传数。
- 写 PowerShell 或 Bash 自动化测试脚本，自动启动 A/B、等待结束、提取利用率。
- 生成性能测试 Markdown/CSV，方便填入 `性能测试记录表.docx`。
- 添加 wire format 注释，明确帧头 3 字节、CRC 4 字节、DATA 总长 263 字节。

## 15. 给下一个聊天窗口的接手提示

```text
这是计算机网络实验一：数据链路层滑动窗口协议项目。当前主要代码在：
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-Windows-VS2019\datalink.c
以及 Linux 同步文件：
C:\Users\28641\Desktop\计网\实验\Lab1-2024(Win+Linux)\Lab1-2024(Win+Linux)\Lab1-linux\datalink.c

当前实现是 Selective Repeat with Piggybacking，支持 CRC32、ACK timer、NAK、乱序缓存、序号回绕、网络层 enable/disable 和物理队列节流。当前关键参数：
MAX_SEQ=15, NR_BUFS=8, DATA_TIMER=2500, ACK_TIMER=300, PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2。

请优先阅读 datalink.c 中的：
between()
update_network_layer()
send_data_frame()
main() 的 switch(event) 五个分支

不要随意修改 protocol.c/protocol.h/crc32.c/lprintf.c/getopt.c 等实验框架文件。主要维护 datalink.c。

已经完成一次默认误码双向 flood 20 分钟长测：
datalink.exe -t 1200 -p 62820 -f A
datalink.exe -t 1200 -p 62820 -f B
日志 datalink-A.log 和 datalink-B.log 均到 1200.018 Quit.，A/B 最终约 92.66%。

下一步优先任务：
1. 补齐五组验收性能测试，最好每组 20 分钟。
2. 跑 --ber=1e-4 双向 flood 长测并扫描错误。
3. 验证 Linux make 编译和运行。
4. 小范围调参 DATA_TIMER、ACK_TIMER、PHL_QUEUE_LIMIT，提高性能但不要破坏当前稳定版本。注意不要再大范围乱跳参数；建议只测 DATA_TIMER=2300/2400/2500/2600/2700，ACK_TIMER=250/300/350，PHL_QUEUE_LIMIT=FRAME_DATA_LEN*2 或 *3。已排除 DATA_TIMER=2000、窗口 4、不限制物理队列、PHL_QUEUE_LIMIT=*4。

当前目录没有 .git，git status/log 不可用。继续大改前请先备份当前稳定 datalink.c。
```

## 16. 附录：关键代码摘要

### 帧与参数定义

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

### 序号递增与区间判断

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

### 初始化摘要

```c
next_frame_to_send = 0;
ack_expected = 0;
frame_expected = 0;
too_far = NR_BUFS;
nbuffered = 0;
no_nak = 1;
memset(out_buf, 0, sizeof out_buf);
memset(in_buf, 0, sizeof in_buf);
memset(arrived, 0, sizeof arrived);
enable_network_layer();
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

### 超时重传核心

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

建议将本文件保存为 `DEVELOPMENT0_RECORD.md`，作为后续新聊天窗口继续接手开发的工程上下文记录。
