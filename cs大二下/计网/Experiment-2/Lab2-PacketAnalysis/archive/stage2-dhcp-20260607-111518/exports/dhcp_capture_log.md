# DHCP 正式抓包记录

| 项目 | 实际值 |
|---|---|
| 抓包文件 | `captures/dhcp.pcapng` |
| 抓包开始时间 | 2026-06-07 10:05:21.513361000 +08:00 |
| 抓包结束时间 | 2026-06-07 10:05:24.829190000 +08:00 |
| 接口名称 | WLAN |
| 接口编号 | 5（抓包前实时核对） |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 完整 NPF 接口 | `\Device\NPF_{887FB422-3CDD-474F-AB19-69DDE6D65A61}` |
| 捕获过滤器 | `udp port 67` |
| 触发命令 | `ipconfig /release "WLAN"`；`ipconfig /renew "WLAN"` |
| 抓包工具 | Wireshark/Dumpcap 4.6.6 |
| pcap 帧数 | 5 |
| 丢包 | 0（Wireshark 停止捕获时显示 0.0%） |
| pcap SHA-256 | `990835E69AE6F0CDEB90EA7B7163A65DA4692584B08D1201E4AA28774379F0D2` |
| CSV 文件 | `exports/dhcp_fields.csv` |
| CSV 行数 | 5 |
| DHCP 验收 | PASS |

## 文件保存说明

Wireshark 捕获停止后原始文件位于：

`C:\Users\28641\AppData\Local\Temp\wireshark_WLANY74WQ3.pcapng`

由于 Wireshark 的中文路径“另存为”对话框未成功提交，使用文件系统原样复制到 `captures/dhcp.pcapng`。复制前后 SHA-256 均为：

`990835E69AE6F0CDEB90EA7B7163A65DA4692584B08D1201E4AA28774379F0D2`

TShark 已从目标文件成功读取 Frame 1 至 Frame 5，未对 pcap 内容进行重编码或修改。

## 报文概览

| Frame | 报文类型 | Transaction ID | 说明 |
|---:|---|---|---|
| 1 | Release | `0x7df8922e` | 释放原租约 |
| 2 | Discover | `0x4d8d939a` | DORA 开始 |
| 3 | Offer | `0x4d8d939a` | 服务器提供地址 |
| 4 | Request | `0x4d8d939a` | 客户端请求所提供地址 |
| 5 | ACK | `0x4d8d939a` | 服务器确认租约 |
