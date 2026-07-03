# 阶段 1 环境检查结果

- 检查时间：2026-06-06 21:50:29
- 项目目录：`C:\Users\28641\Desktop\计网\Experiment-2\Lab2-PacketAnalysis`
- 说明：本次为阶段 1 复核，没有执行 DHCP、ICMP、IPv4 分片或 TCP 正式抓包。

## 1. 工具检查

| 项目 | 结果 | 详情 |
|---|---|---|
| Wireshark | PASS | `C:\Program Files\Wireshark\Wireshark.exe`；版本 4.6.6。未加入 PATH，本次使用完整路径 |
| TShark | PASS | `C:\Program Files\Wireshark\tshark.exe`；版本 4.6.6。未加入 PATH，本次使用完整路径 |
| Npcap | PASS | Npcap 1.88；`npcap` 服务存在且状态为 `RUNNING` |

## 2. 网络接口确认

| 项目 | 结果 |
|---|---|
| 当前推荐接口编号 | 5 |
| 当前推荐接口名称 | WLAN |
| 当前推荐接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 完整 NPF 接口 | `\Device\NPF_{887FB422-3CDD-474F-AB19-69DDE6D65A61}` |
| 历史 GUID 是否仍匹配 | PASS |
| WLAN 设备描述 | Intel(R) Wi-Fi 6 AX201 160MHz |

> 接口编号可能在重启或驱动变更后变化。正式抓包前必须重新运行 `tshark -D`，并优先按 WLAN 名称和 GUID 核对。

## 3. IP 配置信息

以下内容仅来自当前活跃的 `Wireless LAN adapter WLAN`，未采用 VMware、蓝牙、VPN 或其他虚拟接口的数据。

| 字段 | 值 |
|---|---|
| 适配器名称 | WLAN |
| 设备描述 | Intel(R) Wi-Fi 6 AX201 160MHz |
| 物理地址 MAC | E8-B0-C5-2E-64-4A |
| IPv4 地址 | 10.21.225.70 |
| 子网掩码 | 255.255.128.0 |
| 默认网关 | 10.21.128.1 |
| DNS 服务器 | 10.3.9.4、10.3.9.5、10.3.9.6 |
| DHCP 是否启用 | 是 |
| DHCP 服务器 | 10.3.9.2 |
| 租约获得时间 | 2026-06-06 20:39:10 |
| 租约过期时间 | 2026-06-06 23:31:59 |

原始命令输出：

- `exports/tshark_interfaces.txt`
- `exports/ipconfig_before.txt`
- `exports/tool_versions.txt`

## 4. 烟雾抓包检查

| 文件 | 帧数 | SHA-256 | 结果 |
|---|---:|---|---|
| `exports/npcap_smoke_test.pcapng` | 789 | `DFD78C2B857492CC2F471F744BE64D61D4934B3F7A7CE00A0FD143A3BC05B7D8` | PASS |

该文件可由 TShark 正常读取且帧数大于 0，证明 Npcap 当前具备实际抓包能力。它仅是环境烟雾测试，不是任何正式协议抓包。

## 5. 环境截图清单

| 截图 | 是否存在 | 说明 |
|---|---|---|
| `screenshots/00_environment/01_wireshark_version.png` | PASS | 基于实际 Wireshark `-v` 输出 |
| `screenshots/00_environment/02_tshark_interfaces.png` | PASS | 基于最新 `tshark -D` 输出，可见 WLAN 和完整 GUID |
| `screenshots/00_environment/03_ipconfig_all.png` | PASS | 基于最新 `ipconfig /all`，仅展示当前 WLAN 配置 |
| `screenshots/00_environment/04_wireshark_interface_selection.png` | PASS | 实际 Wireshark 欢迎页，可见 WLAN 接口；未开始抓包 |

## 6. 字段导出和抓包验证脚本

已建立：

- `scripts/extract_fields.py`
- `scripts/validate_captures.py`

基础版检查结果：

- Python 3.13.7 语法检查：PASS。
- TShark 字段预检：PASS。
- DHCP：19 个字段受支持。
- ICMP：15 个字段受支持。
- IPv4 分片：13 个字段受支持。
- TCP：15 个字段受支持。
- 正式 pcap 尚不存在，因此未生成任何协议 CSV。

后续每完成一种正式抓包，应立即运行字段导出与单项校验；空抓包或缺少关键报文不得输出伪成功结果。

## 7. 阶段 1 验收

| 验收项 | 结果 | 说明 |
|---|---|---|
| 当前目录为 Lab2-PacketAnalysis | PASS | 路径已核对 |
| Wireshark 可用 | PASS | 4.6.6 |
| TShark 可用 | PASS | 4.6.6 |
| Npcap 服务可用 | PASS | 1.88，RUNNING |
| `tshark_interfaces.txt` 已生成 | PASS | 已刷新 |
| `ipconfig_before.txt` 已生成 | PASS | 已刷新 |
| WLAN 编号、名称、GUID 已确认 | PASS | 5、WLAN、历史 GUID 仍匹配 |
| `experiment_metadata.md` 已更新 | PASS | 已保存真实环境值 |
| 烟雾抓包可读且帧数大于 0 | PASS | 789 帧 |
| 环境截图清单已补齐 | PASS | 4/4 |
| 未开始正式协议抓包 | PASS | `captures/` 仍为空 |

## 8. 阶段 1 总体结论

**PASS：环境可以进入正式抓包。**

进入阶段 2 前，仍应先确认关闭或暂停 VPN、代理和可能切换网络的虚拟网络客户端，并再次运行 `tshark -D` 核对 WLAN GUID。

## 9. 进入阶段 2 DHCP 抓包前需要确认的事项

- [ ] 已关闭或暂停 VPN、代理和虚拟网络客户端。
- [ ] 已重新确认当前正式抓包接口为 WLAN，GUID 未变化。
- [ ] 已保存所有文件，了解 `ipconfig /release` 会临时断网。
- [ ] 已准备以管理员身份运行 CMD。
- [ ] 已准备使用捕获过滤器 `udp port 67`。
- [x] 已同意先建立字段导出/校验脚本基础版；该项已完成。

## 10. 2026-06-07 阶段 1 复核

| 复核项 | 结果 | 真实检查结果 |
|---|---|---|
| `exports/environment_check.md` | PASS | 文件存在 |
| `exports/tshark_interfaces.txt` | PASS | 文件存在 |
| `exports/ipconfig_before.txt` | PASS | 文件存在 |
| `exports/experiment_metadata.md` | PASS | 文件存在，环境截图状态已同步 |
| 当前 WLAN 接口 | PASS | 编号 5，名称 WLAN，GUID `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| Npcap 服务 | PASS | `RUNNING` |
| `exports/npcap_smoke_test.pcapng` | PASS | TShark 可读，共 789 帧 |
| `screenshots/00_environment/` | PASS | 4/4 张要求截图存在 |

复核结论：**阶段 1 为完整 PASS，不需要标记 WARN。** 本次仅复核并固化环境状态，没有开始 DHCP 抓包，也没有修改任何 pcap、CSV 或截图。
