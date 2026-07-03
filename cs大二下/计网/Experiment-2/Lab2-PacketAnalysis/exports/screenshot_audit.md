# 实验截图审计

- 审计日期：2026-06-07
- 审计范围：`screenshots/00_environment/`、`screenshots/01_dhcp/`
- 原则：截图必须保持真实字段、真实 Frame Number 和真实界面，不通过图像修改掩盖操作来源。

## 1. 环境截图

| 文件 | 结果 | 说明 |
|---|---|---|
| `01_wireshark_version.png` | WARN | 版本 4.6.6 信息正确，但画面是整理后的命令展示样式，不是原生 CMD 窗口；若教师要求原始操作截图，应复拍 |
| `02_tshark_interfaces.png` | WARN | 接口列表和 WLAN GUID 信息完整，但不是原生 CMD 窗口截图；建议复拍真实 `tshark -D` 输出 |
| `03_ipconfig_all.png` | WARN | WLAN 的 IPv4、掩码、网关、DHCP 和 DNS 信息齐全，但不是原生 CMD 窗口截图；建议复拍真实 `ipconfig /all` 输出 |
| `04_wireshark_interface_selection.png` | FAIL | Wireshark 界面真实，但画面中选中的是“本地连接* 10”，不是 WLAN，不能证明正式抓包接口选择正确 |

环境截图总体结论：**WARN/FAIL，需要复拍后才能作为完整环境证据。**

建议复拍：

1. 管理员 CMD 显示 `"C:\Program Files\Wireshark\Wireshark.exe" -v`。
2. 管理员 CMD 显示 `"C:\Program Files\Wireshark\tshark.exe" -D`，确保 WLAN 和完整 GUID 可见。
3. 管理员 CMD 显示 `ipconfig /all`，滚动到完整 WLAN 段。
4. Wireshark 欢迎页单击并高亮 `WLAN`，确认不是虚拟网卡、蓝牙或“本地连接*”。

## 2. DHCP 截图

八张截图均为 2560×1600，协议内容来自真实 `captures/dhcp.pcapng`，字段和关键 Frame Number 可回查：

| 文件 | 内容完整性 | 提交适用性 |
|---|---|---|
| `01_dhcp_packet_list.png` | PASS：Frame 1–5 列表完整 | WARN：有控制提示、光标高亮和助手悬浮标记 |
| `02_dhcp_release_detail.png` | PASS：Frame 1 Release、Transaction ID、客户端地址和服务器标识可见 | WARN：有控制提示和助手标记 |
| `03_dhcp_discover_detail.png` | PASS：Frame 2 Discover、Transaction ID、客户端 MAC 和请求地址可见 | WARN：有控制提示和助手标记 |
| `04_dhcp_offer_detail.png` | PASS：Frame 3 Offer、Your IP、服务器标识、租期和掩码可见 | WARN：有控制提示和助手标记 |
| `04b_dhcp_offer_options.png` | PASS：Frame 3 网关和三台 DNS 可见 | WARN：有控制提示和助手标记 |
| `05_dhcp_request_detail.png` | PASS：Frame 4 Request、Requested IP 和 Server Identifier 可见 | WARN：有控制提示和助手标记 |
| `06_dhcp_ack_detail.png` | PASS：Frame 5 ACK、Your IP、服务器标识、租期、掩码和网关可见 | WARN：有控制提示和助手标记 |
| `06b_dhcp_ack_options.png` | PASS：Frame 5 网关和三台 DNS 可见 | WARN：有控制提示和助手标记 |

DHCP 截图内容覆盖结论：**PASS**。

DHCP 截图最终提交结论：**WARN，必须使用同一真实 pcap 无提示复拍。** 不需要重抓网络报文，也不能修改字段值；只需重新打开现有 `captures/dhcp.pcapng`。

## 3. 无提示复拍清单

在 Wireshark 中手动打开：

`C:\Users\28641\Desktop\计网\Experiment-2\Lab2-PacketAnalysis\captures\dhcp.pcapng`

设置显示过滤器：

`dhcp || bootp`

逐张复拍：

1. `01_dhcp_packet_list.png`：完整显示 Frame 1–5、Source、Destination、Protocol 和 Info。
2. `02_dhcp_release_detail.png`：选 Frame 1，展开 DHCP，显示 Release、Transaction ID、Client IP、Client MAC、Server Identifier。
3. `03_dhcp_discover_detail.png`：选 Frame 2，显示 Discover、Transaction ID、Client MAC、Requested IP。
4. `04_dhcp_offer_detail.png`：选 Frame 3，显示 Offer、Transaction ID、Your IP、Server Identifier、Lease Time、Subnet Mask。
5. `04b_dhcp_offer_options.png`：继续在 Frame 3 展开 Router 和 Domain Name Server，显示网关和三台 DNS。
6. `05_dhcp_request_detail.png`：选 Frame 4，显示 Request、Transaction ID、Requested IP、Server Identifier。
7. `06_dhcp_ack_detail.png`：选 Frame 5，显示 ACK、Transaction ID、Your IP、Server Identifier、Lease Time、Subnet Mask。
8. `06b_dhcp_ack_options.png`：继续在 Frame 5 展开 Router 和 Domain Name Server，显示网关和三台 DNS。

复拍时避免出现其他程序、自动化提示、通知、悬浮头像或鼠标高亮；保持包列表、协议树和 Frame Number 同屏。
