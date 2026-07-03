# TCP 建立连接与释放连接捕获分析

## 1. 抓包证据链

| 项目 | 值 |
|---|---|
| 正式抓包 | `captures/tcp.pcapng` |
| 原始重抓文件 | `captures/tcp_retry.pcapng` |
| 首次不合格抓包归档 | `captures/tcp_failed_20260619_230511.pcapng` |
| 接口 | WLAN，GUID `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 捕获过滤器 | `tcp port 80` |
| 触发命令 | `curl.exe --noproxy "*" -4 -v --http1.1 -H "Connection: close" http://example.com/ -o NUL` |
| pcap SHA-256 | `2046914206267F90A8CCDC8E7EF78BA78D2CE24AB2BD4FE94B24697B21B4E88F` |
| 相对序号 CSV | `exports/tcp_fields.csv` |
| 原始序号 CSV | `exports/tcp_fields_rawseq.csv` |
| 采用的 TCP 流 | `tcp.stream == 2` |
| 验收结果 | PASS |

报告默认使用 Wireshark 相对序号。原始序号保存在 `tcp_fields_rawseq.csv` 中，可用于反查。

## 2. 连接端点

- Client：`10.21.225.70:60497`
- Server：`104.20.23.154:80`

## 3. TCP 三次握手

| 步骤 | Frame | Time | 方向 | Flags | Seq | Ack | Len | 含义 |
|---:|---:|---:|---|---|---:|---:|---:|---|
| 1 | 16 | 13.759490700 | Client -> Server | SYN | 0 | 0 | 0 | 客户端请求建立连接 |
| 2 | 17 | 13.800060500 | Server -> Client | SYN, ACK | 0 | 1 | 0 | 服务器确认并同步序号 |
| 3 | 18 | 13.800163300 | Client -> Server | ACK | 1 | 1 | 0 | 客户端确认，连接建立 |

原始序号关系：

- Frame 16：Seq `2115761771`
- Frame 17：Seq `4262144966`，Ack `2115761772`
- Frame 18：Seq `2115761772`，Ack `4262144967`

SYN 各占用一个序号，因此确认号均为对方初始序号加 1。

## 4. 数据传输

- Frame 19：客户端发送 94 字节 HTTP 请求数据。
- Frame 21 和 Frame 22：服务器发送 863 字节和 5 字节响应数据。
- Frame 23：客户端确认服务器数据，Ack 为 869。

## 5. TCP 连接释放

本次由服务器首先发起关闭。

| 步骤 | Frame | Time | 方向 | Flags | Seq | Ack | Len | 含义 |
|---:|---:|---:|---|---|---:|---:|---:|---|
| 1 | 24 | 13.849259800 | Server -> Client | FIN, ACK | 869 | 95 | 0 | 服务器请求关闭发送方向 |
| 2 | 25 | 13.849294000 | Client -> Server | ACK | 95 | 870 | 0 | 客户端确认服务器 FIN |
| 3 | 26 | 13.866547100 | Client -> Server | FIN, ACK | 95 | 870 | 0 | 客户端请求关闭发送方向 |
| 4 | 27 | 13.923362900 | Server -> Client | ACK | 870 | 96 | 0 | 服务器最终确认 |

FIN 同样占用一个序号，因此 Frame 25 的 Ack 从 869 增加到 870，Frame 27 的 Ack 从 95 增加到 96。该流未出现 RST，正常释放完整。

## 6. 序列图

序列图源文件：`exports/tcp_sequence.mmd`。

## 7. 截图目标

| 文件 | 内容 | 推荐帧/过滤器 |
|---|---|---|
| `screenshots/04_tcp/01_tcp_packet_list_port80.png` | 端口 80 报文列表 | `tcp.port == 80` |
| `screenshots/04_tcp/02_tcp_stream_filter.png` | 完整采用流 | `tcp.stream == 2` |
| `screenshots/04_tcp/03_tcp_syn_detail.png` | SYN、Seq、Ack、Flags | Frame 16 |
| `screenshots/04_tcp/04_tcp_syn_ack_detail.png` | SYN+ACK、Seq、Ack、Flags | Frame 17 |
| `screenshots/04_tcp/05_tcp_ack_detail.png` | 第三次握手 ACK | Frame 18 |
| `screenshots/04_tcp/06_tcp_fin_ack_detail.png` | 首个 FIN+ACK | Frame 24 |
| `screenshots/04_tcp/07_tcp_connection_release_list.png` | Frame 24-27 释放过程 | `tcp.stream == 2` |
| `screenshots/04_tcp/08_follow_tcp_stream.png` | HTTP 请求与响应 | Follow TCP Stream |
