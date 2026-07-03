# IPv4 数据报分片捕获与分析

## 1. 抓包证据链

| 项目 | 值 |
|---|---|
| 抓包文件 | `captures/ip_fragment.pcapng` |
| 抓包时间 | 2026-06-19 22:44:15.485420800 至 22:44:22.811151800 +08:00 |
| 接口名称 | WLAN |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 触发命令 | `ping -4 -n 1 -l 8000 10.21.128.1` |
| 显示过滤器 | `ip.flags.mf == 1 || ip.frag_offset > 0` |
| pcap SHA-256 | `B9C14AB424792FB14C75DAA8144EB708862A4CED1138D48AA379B0A37E5739F3` |
| CSV 文件 | `exports/ip_fragment_fields.csv` |
| 分片行数 | 12 |
| 验收结果 | PASS |

## 2. ICMP Echo Request 分片组

源地址为 `10.21.225.70`，目的地址为 `10.21.128.1`，协议号为 1，Identification 为 `0x299e`。

| 分片 | Frame | Total Length | IP 头长度 | 数据长度 | DF | MF | Fragment Offset | 实际字节偏移 | 是否末片 |
|---:|---:|---:|---:|---:|---|---|---:|---:|---|
| 1 | 29 | 1500 | 20 | 1480 | 0 | 1 | 0 | 0 | 否 |
| 2 | 30 | 1500 | 20 | 1480 | 0 | 1 | 185 | 1480 | 否 |
| 3 | 31 | 1500 | 20 | 1480 | 0 | 1 | 370 | 2960 | 否 |
| 4 | 32 | 1500 | 20 | 1480 | 0 | 1 | 555 | 4440 | 否 |
| 5 | 33 | 1500 | 20 | 1480 | 0 | 1 | 740 | 5920 | 否 |
| 6 | 34 | 628 | 20 | 608 | 0 | 0 | 925 | 7400 | 是 |

前五片每片携带 1480 字节数据，因此下一片偏移量增加 `1480 / 8 = 185`。末片从第 7400 字节开始，携带 608 字节数据。六片合计承载 8008 字节 IP 负载，其中包括 8 字节 ICMP 头和命令指定的 8000 字节数据。

## 3. ICMP Echo Reply 分片组

源地址为 `10.21.128.1`，目的地址为 `10.21.225.70`，协议号为 1，Identification 为 `0x82f8`。

| 分片 | Frame | Total Length | IP 头长度 | 数据长度 | DF | MF | Fragment Offset | 实际字节偏移 | 是否末片 |
|---:|---:|---:|---:|---|---|---:|---:|---|
| 1 | 35 | 1500 | 20 | 1480 | 0 | 1 | 0 | 0 | 否 |
| 2 | 36 | 1500 | 20 | 1480 | 0 | 1 | 185 | 1480 | 否 |
| 3 | 37 | 1500 | 20 | 1480 | 0 | 1 | 370 | 2960 | 否 |
| 4 | 38 | 1500 | 20 | 1480 | 0 | 1 | 555 | 4440 | 否 |
| 5 | 39 | 1500 | 20 | 1480 | 0 | 1 | 740 | 5920 | 否 |
| 6 | 40 | 628 | 20 | 608 | 0 | 0 | 925 | 7400 | 是 |

## 4. 完整性判断

- 每组内的源地址、目的地址、协议号和 Identification 保持一致。
- 两组均包含 6 个分片。
- 前五片均为 `MF=1`，表示后续仍有分片。
- 最后一片为 `MF=0` 且 Fragment Offset 为 925。
- 所有分片均为 `DF=0`，允许分片。
- Offset 按 8 字节换算后与前一片数据结束位置连续，不存在缺口或重叠。
- 请求和应答均被完整捕获，适合用于实验报告。

## 5. 截图目标

| 文件 | 内容 | 推荐帧 |
|---|---|---:|
| `screenshots/03_ip_fragment/01_ip_fragment_packet_list.png` | 请求和应答的 12 个分片列表 | 全部 |
| `screenshots/03_ip_fragment/02_first_fragment_ipv4_detail.png` | 请求首片的 Length、Identification、DF、MF、Offset | 29 |
| `screenshots/03_ip_fragment/03_middle_fragment_ipv4_detail.png` | 请求中间片的相同字段 | 30 |
| `screenshots/03_ip_fragment/04_last_fragment_ipv4_detail.png` | 请求末片的 Length、Identification、DF、MF、Offset | 34 |
| `screenshots/03_ip_fragment/05_fragment_filter_condition.png` | 显示过滤器及完整分片列表 | 全部 |

