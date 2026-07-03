# DHCP 报文捕获与分析

## 1. 抓包证据链

| 项目 | 值 |
|---|---|
| 抓包文件 | `captures/dhcp.pcapng` |
| 抓包时间 | 2026-06-07 10:05:21.513361000 至 10:05:24.829190000 +08:00 |
| 接口名称 | WLAN |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 捕获过滤器 | `udp port 67` |
| 显示过滤器 | `dhcp || bootp` |
| 触发命令 | `ipconfig /release "WLAN"`；`ipconfig /renew "WLAN"` |
| pcap SHA-256 | `990835E69AE6F0CDEB90EA7B7163A65DA4692584B08D1201E4AA28774379F0D2` |
| CSV 文件 | `exports/dhcp_fields.csv` |
| CSV 行数 | 5 |
| 验收结果 | PASS |

## 2. DHCP 报文识别结果

| 报文类型 | Frame | Time | Src IP | Dst IP | UDP Src Port | UDP Dst Port | Transaction ID | Client MAC | 说明 |
|---|---:|---:|---|---|---:|---:|---|---|---|
| Release | 1 | 0.000000000 | 10.21.225.70 | 10.3.9.2 | 68 | 67 | `0x7df8922e` | e8:b0:c5:2e:64:4a | 客户端向原 DHCP 服务器释放租约 |
| Discover | 2 | 3.282382000 | 0.0.0.0 | 255.255.255.255 | 68 | 67 | `0x4d8d939a` | e8:b0:c5:2e:64:4a | 客户端广播寻找 DHCP 服务器 |
| Offer | 3 | 3.297990300 | 10.3.9.2 | 255.255.255.255 | 67 | 68 | `0x4d8d939a` | e8:b0:c5:2e:64:4a | 服务器提供地址和网络配置 |
| Request | 4 | 3.299353100 | 0.0.0.0 | 255.255.255.255 | 68 | 67 | `0x4d8d939a` | e8:b0:c5:2e:64:4a | 客户端请求地址并选择服务器 |
| ACK | 5 | 3.315829000 | 10.3.9.2 | 255.255.255.255 | 67 | 68 | `0x4d8d939a` | e8:b0:c5:2e:64:4a | 服务器确认租约和配置 |

## 3. DHCP 配置字段

| 报文类型 | Frame | Your IP | Requested IP | Server Identifier | Subnet Mask | Router | DNS | Lease Time |
|---|---:|---|---|---|---|---|---|---|
| Release | 1 | 0.0.0.0 | 缺失 | 10.3.9.2 | 缺失 | 缺失 | 缺失 | 缺失 |
| Discover | 2 | 0.0.0.0 | 10.21.225.70 | 缺失 | 缺失 | 缺失 | 缺失 | 缺失 |
| Offer | 3 | 10.21.225.70 | 缺失 | 10.3.9.2 | 255.255.128.0 | 10.21.128.1 | 10.3.9.6、10.3.9.5、10.3.9.4 | 3600 秒 |
| Request | 4 | 0.0.0.0 | 10.21.225.70 | 10.3.9.2 | 缺失 | 缺失 | 缺失 | 缺失 |
| ACK | 5 | 10.21.225.70 | 缺失 | 10.3.9.2 | 255.255.128.0 | 10.21.128.1 | 10.3.9.6、10.3.9.5、10.3.9.4 | 3600 秒 |

表中“缺失”表示对应字段未出现在该报文的 TShark CSV 中，没有用理论值补写。

## 4. DHCP 地址分配过程说明

- Frame 1 是 DHCP Release。客户端从 10.21.225.70:68 向服务器 10.3.9.2:67 发送报文，释放原有租约。该报文使用独立 Transaction ID `0x7df8922e`。
- Frame 2 是 DHCP Discover。客户端尚无可用地址，以 0.0.0.0:68 向 255.255.255.255:67 广播，并请求地址 10.21.225.70。
- Frame 3 是 DHCP Offer。服务器 10.3.9.2 使用 Transaction ID `0x4d8d939a` 提供 10.21.225.70，同时携带子网掩码、默认网关、DNS 和 3600 秒租期。
- Frame 4 是 DHCP Request。客户端继续使用同一 Transaction ID 广播请求 10.21.225.70，并通过 Server Identifier 选择 10.3.9.2。
- Frame 5 是 DHCP ACK。服务器确认 10.21.225.70 的租约生效，并再次给出子网掩码 255.255.128.0、网关 10.21.128.1、DNS 服务器及 3600 秒租期。

Frame 2 至 Frame 5 的 Transaction ID 均为 `0x4d8d939a`，构成一组完整且可回查的 DORA。

## 5. 异常与重抓建议

本次 Discover、Offer、Request、ACK 和 Release 均存在，核心报文完整，验收为 PASS，不需要重抓。

Wireshark 图形界面的中文路径保存对话框未成功提交，但已将其原始临时 pcapng 原样复制到目标路径，并通过相同 SHA-256 和 TShark 5 帧读取结果验证文件一致性。该保存过程不影响报文字段真实性。

## 6. 截图清单

以下截图必须使用本次分析采用的 Frame 1 至 Frame 5：

| 文件 | 内容 | 状态 |
|---|---|---|
| `screenshots/01_dhcp/01_dhcp_packet_list.png` | 完整 DHCP 报文列表 | 已完成 |
| `screenshots/01_dhcp/02_dhcp_release_detail.png` | Frame 1 Release 详情 | 已完成 |
| `screenshots/01_dhcp/03_dhcp_discover_detail.png` | Frame 2 Discover 详情 | 已完成 |
| `screenshots/01_dhcp/04_dhcp_offer_detail.png` | Frame 3 Offer 详情 | 已完成 |
| `screenshots/01_dhcp/04b_dhcp_offer_options.png` | Frame 3 租期、子网掩码、网关和 DNS | 已完成 |
| `screenshots/01_dhcp/05_dhcp_request_detail.png` | Frame 4 Request 详情 | 已完成 |
| `screenshots/01_dhcp/06_dhcp_ack_detail.png` | Frame 5 ACK 详情 | 已完成 |
| `screenshots/01_dhcp/06b_dhcp_ack_options.png` | Frame 5 租期、子网掩码、网关和 DNS | 已完成 |

全部截图分辨率为 2560×1600，并同时保留关键 Frame Number、报文列表和 Wireshark 协议字段树。
