# ICMP 报文捕获与分析

## 1. 抓包证据链

| 项目 | 值 |
|---|---|
| 抓包文件 | `captures/icmp.pcapng` |
| 抓包时间 | 2026-06-19 22:21:59.136920400 至 22:24:57.194813000 +08:00 |
| 接口名称 | WLAN |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 显示过滤器 | `icmp` |
| 触发命令 | `ping -4 -n 4 www.bupt.edu.cn` |
| 实际目标 | `www.bupt.edu.cn` 解析为 `10.3.19.2` |
| pcap SHA-256 | `E6FCEB96046289F85DD4461E0E2E952FD135385BB5DF84C3C2401503EF4A41AB` |
| CSV 文件 | `exports/icmp_fields.csv` |
| ICMP 行数 | 8 |
| 验收结果 | PASS |

## 2. Echo Request/Reply 匹配表

报告采用 Wireshark/TShark 显示的 `icmp.ident` 和 `icmp.seq`。

| 组 | Request Frame | Reply Frame | Request Src -> Dst | Reply Src -> Dst | Identifier | Sequence | Request TTL | Reply TTL | 数据长度 |
|---:|---:|---:|---|---|---:|---:|---:|---:|---:|
| 1 | 1108 | 1109 | 10.21.225.70 -> 10.3.19.2 | 10.3.19.2 -> 10.21.225.70 | 1 | 35 | 128 | 58 | 32 |
| 2 | 1122 | 1123 | 10.21.225.70 -> 10.3.19.2 | 10.3.19.2 -> 10.21.225.70 | 1 | 36 | 128 | 58 | 32 |
| 3 | 1174 | 1175 | 10.21.225.70 -> 10.3.19.2 | 10.3.19.2 -> 10.21.225.70 | 1 | 37 | 128 | 58 | 32 |
| 4 | 1186 | 1187 | 10.21.225.70 -> 10.3.19.2 | 10.3.19.2 -> 10.21.225.70 | 1 | 38 | 128 | 58 | 32 |

四组报文的 Identifier 和 Sequence 均一一对应，源地址和目的地址在应答中反向，因此匹配完整。

## 3. 关键字段分析

- Echo Request 的 `Type=8`、`Code=0`，表示回显请求。
- Echo Reply 的 `Type=0`、`Code=0`，表示回显应答。
- IPv4 `Protocol=1`，表示上层协议为 ICMP。
- Identifier 用于区分发起 ping 的进程；本次四组报文均显示为 1。
- Sequence Number 从 35 递增到 38，用于匹配每次请求与对应应答。
- Request TTL 为 128；Reply 到达本机时 TTL 为 58。报告只记录实际捕获值，不据此推断唯一的初始 TTL 或准确跳数。
- 每个 ICMP 报文携带 32 字节数据，IPv4 Total Length 为 60 字节。
- Request 与 Reply 的校验和不同，因为 Type 字段不同；每个报文的实际校验和值保存在 CSV 中。

## 4. 截图目标

| 文件 | 内容 | 推荐帧 |
|---|---|---:|
| `screenshots/02_icmp/01_icmp_packet_list.png` | 8 个 ICMP 报文完整列表 | 全部 |
| `screenshots/02_icmp/02_icmp_echo_request_detail.png` | Echo Request 的 Type、Code、Checksum、Identifier、Sequence、Data | 1108 |
| `screenshots/02_icmp/03_icmp_echo_reply_detail.png` | Echo Reply 的 Type、Code、Checksum、Identifier、Sequence、Data | 1109 |
| `screenshots/02_icmp/04_icmp_ipv4_header_detail.png` | IPv4 Source、Destination、TTL、Protocol、Total Length | 1108 |

