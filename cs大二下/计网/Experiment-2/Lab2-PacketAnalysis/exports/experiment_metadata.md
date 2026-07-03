# 实验元数据

## 学生信息

- 学院：计算机学院（国家示范性软件学院）
- 班级：304
- 报告文件名班级号：304
- 学号：2024212936
- 姓名：刘文涛

## 实验环境

- 封面实验日期：2026-06-19
- 实际环境配置与抓包日期：2026-06-06、2026-06-07、2026-06-19
- 操作系统：Windows 11 25H2，Build 26200.8457，64 位
- Wireshark 版本：4.6.6
- TShark 版本：4.6.6
- Npcap 版本：1.88
- Npcap 服务状态：RUNNING，实际抓包验证通过
- 网络接口编号：5（每次正式抓包前重新确认）
- 网络接口名称：WLAN
- 网络接口 GUID：887FB422-3CDD-474F-AB19-69DDE6D65A61
- 联网方式：WLAN
- WLAN 设备名称：Intel(R) Wi-Fi 6 AX201 160MHz
- 本机 IPv4 地址：10.21.225.70
- 子网掩码：255.255.128.0
- 默认网关：10.21.128.1
- DNS 服务器：10.3.9.4、10.3.9.5、10.3.9.6
- DHCP 是否启用：是
- DHCP 服务器：10.3.9.2
- 实验网络环境：校园网（BUPT-mobile）

## 环境验证记录

- `exports/tshark_interfaces.txt`：tshark 接口列表。
- `exports/ipconfig_before.txt`：实验前 IP 配置信息。
- `exports/tool_versions.txt`：Wireshark、tshark 和 Npcap 版本。
- `exports/npcap_smoke_test.pcapng`：接口 5 上的 8 秒抓包测试，共捕获 789 帧。

## 抓包文件清单

| 协议 | 文件路径 | 是否完成 | SHA-256 | 备注 |
|---|---|---|---|---|
| DHCP | captures/dhcp.pcapng | 已完成 | B07B87FB47930FC3A4D436D0E9EFF1F65102F6F94AEAC60123F0B24A42B6D1BC | PASS；2026-06-07 重新抓包，5 帧，Clash/系统代理保持开启 |
| ICMP | captures/icmp.pcapng | 已完成 | E6FCEB96046289F85DD4461E0E2E952FD135385BB5DF84C3C2401503EF4A41AB | PASS；4 组 Echo Request/Reply |
| IP 分片 | captures/ip_fragment.pcapng | 已完成 | B9C14AB424792FB14C75DAA8144EB708862A4CED1138D48AA379B0A37E5739F3 | PASS；请求和应答各 6 个完整分片 |
| TCP | captures/tcp.pcapng | 已完成 | 2046914206267F90A8CCDC8E7EF78BA78D2CE24AB2BD4FE94B24697B21B4E88F | PASS；采用 tcp.stream 2 |
| Npcap 烟雾测试 | exports/npcap_smoke_test.pcapng | 已完成 | DFD78C2B857492CC2F471F744BE64D61D4934B3F7A7CE00A0FD143A3BC05B7D8 | 2026-06-06 21:26:02，8 秒捕获 789 帧 |

## 截图清单

| 类别 | 截图目录 | 是否完成 | 备注 |
|---|---|---|---|
| 环境 | screenshots/00_environment | 已完成 | 版本、接口列表、WLAN 配置和 WLAN 选择截图已复拍并验收 |
| DHCP | screenshots/01_dhcp | 已完成 | 6 张正式截图验收通过 |
| ICMP | screenshots/02_icmp | 已完成 | 4 张截图验收通过 |
| IP 分片 | screenshots/03_ip_fragment | 已完成 | 4 张截图内容通过；用户接受两张图中的播放器悬浮条 |
| TCP | screenshots/04_tcp | 已完成 | 8 张截图验收通过 |

## 证据链记录规范

每个正式抓包阶段必须记录：

- 抓包文件路径
- 抓包时间
- 接口名称
- 接口 GUID
- 实际执行命令和目标
- 捕获过滤器
- 显示过滤器
- 文件 SHA-256
- CSV 路径和行数
- 采用的关键 Frame Number
- 截图文件名
- 验收结果：PASS / WARN / FAIL
