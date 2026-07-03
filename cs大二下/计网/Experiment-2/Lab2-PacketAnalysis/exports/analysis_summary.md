# 实验二抓包分析汇总

## 1. 文件检查

| 文件 | 状态 |
|---|---|
| captures/dhcp.pcapng | PASS |
| captures/icmp.pcapng | PASS |
| captures/ip_fragment.pcapng | PASS |
| captures/tcp.pcapng | PASS |

## 2. DHCP 分析摘要

| Message Type | Frame | Src IP | Dst IP | Transaction ID |
|---|---:|---|---|---|
| Release | 1 | 10.21.225.70 | 10.3.9.2 | 0xdc2ff185 |
| Discover | 2 | 0.0.0.0 | 255.255.255.255 | 0xe83ab87a |
| Offer | 3 | 10.3.9.2 | 255.255.255.255 | 0xe83ab87a |
| Request | 4 | 0.0.0.0 | 255.255.255.255 | 0xe83ab87a |
| ACK | 5 | 10.3.9.2 | 255.255.255.255 | 0xe83ab87a |

## 3. ICMP 分析摘要

| Pair | Request Frame | Reply Frame | Identifier | Sequence |
|---:|---:|---:|---|---|
| 1 | 1108 | 1109 | 1 | 35 |
| 2 | 1122 | 1123 | 1 | 36 |
| 3 | 1174 | 1175 | 1 | 37 |
| 4 | 1186 | 1187 | 1 | 38 |

## 4. IP 分片分析摘要

| Identification | 分片数量 | Frames | 完整性 |
|---|---:|---|---|
| 0x299e | 6 | 29, 30, 31, 32, 33, 34 | PASS |
| 0x82f8 | 6 | 35, 36, 37, 38, 39, 40 | PASS |

## 5. TCP 分析摘要

| tcp.stream | Client | Server | 握手 | 正常释放 |
|---:|---|---|---|---|
| 0 | 10.21.225.70:60471 | 14.17.27.213:80 | PASS | FAIL |
| 1 | 10.21.225.70:57615 | 14.116.242.250:80 | FAIL | FAIL |
| 2 | 10.21.225.70:60497 | 104.20.23.154:80 | PASS | PASS |
| 3 | 10.21.225.70:57664 | 221.238.41.33:80 | PASS | FAIL |

## 6. 验收结果和重抓建议

- DHCP: **PASS** - Complete DORA in 1 transaction(s); Release present in frame(s) 1
- ICMP: **PASS** - 4 matched Echo pair(s)
- IPv4 fragments: **PASS** - 2 complete fragment group(s)
- TCP: **PASS** - 1 complete TCP stream(s)

> 本汇总仅依据现有 pcap/CSV。缺失或不完整的数据必须重抓，不得用理论值或示例值替代。
