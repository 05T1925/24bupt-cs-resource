# DHCP 正式抓包记录

| 项目 | 实际值 |
|---|---|
| 抓包文件 | `captures/dhcp.pcapng` |
| 抓包开始时间 | 2026-06-07 11:16:58.462584100 +08:00 |
| 抓包结束时间 | 2026-06-07 11:17:00.744677700 +08:00 |
| 接口名称 | WLAN |
| 接口编号 | 5（抓包前实时核对） |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 完整 NPF 接口 | `\Device\NPF_{887FB422-3CDD-474F-AB19-69DDE6D65A61}` |
| 捕获过滤器 | `udp port 67` |
| 触发命令 | `ipconfig /release "WLAN"`；`ipconfig /renew "WLAN"` |
| 抓包工具 | Wireshark/Dumpcap 4.6.6 |
| pcap 帧数 | 5 |
| 丢包 | 0（Wireshark 停止捕获时显示 0.0%） |
| pcap SHA-256 | `B07B87FB47930FC3A4D436D0E9EFF1F65102F6F94AEAC60123F0B24A42B6D1BC` |
| CSV 文件 | `exports/dhcp_fields.csv` |
| CSV 行数 | 5 |
| DHCP 验收 | PASS |

## 网络客户端状态

- Clash Verge 和系统代理在抓包期间保持开启。
- 系统代理为 `127.0.0.1:7897`，代理例外包含 `10.*`。
- 默认 IPv4 路由始终为 WLAN 网关 `10.21.128.1`。
- Sangfor aTrust VNIC 为断开状态。
- VMware VMnet1/VMnet8 没有默认网关。
- 抓包只绑定 WLAN GUID，且只对 WLAN 执行 release/renew。

## 报文概览

| Frame | 报文类型 | Transaction ID | 说明 |
|---:|---|---|---|
| 1 | Release | `0xdc2ff185` | 释放原租约 |
| 2 | Discover | `0xe83ab87a` | DORA 开始 |
| 3 | Offer | `0xe83ab87a` | 服务器提供地址 |
| 4 | Request | `0xe83ab87a` | 客户端请求所提供地址 |
| 5 | ACK | `0xe83ab87a` | 服务器确认租约 |
