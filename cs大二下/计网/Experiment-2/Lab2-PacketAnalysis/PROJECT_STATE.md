# 计算机网络实验二：项目状态

> 本文件是后续恢复项目上下文的唯一入口。
> 每次继续工作时先读取本文件，再按其中引用读取执行基线和证据文件。
> 本文件记录的是截至 2026-06-06 的真实项目状态，不依赖聊天上下文。

## 1. 项目根目录

```text
C:\Users\28641\Desktop\计网\Experiment-2\Lab2-PacketAnalysis
```

上级目录中的原始阶段规划：

```text
C:\Users\28641\Desktop\计网\Experiment-2\Lab2_Codex_Markdown_Package
```

执行规则来源：

1. `PROJECT_STATE.md`：当前状态和恢复入口。
2. `PROJECT_EXECUTION_BASELINE.md`：实施顺序、风险控制和验收规则，不得覆盖。
3. `README.md`：目录、文件命名和数据真实性要求。
4. `Lab2_Codex_Markdown_Package/00-09`：原始阶段规划。
5. 实验指导书、真实 pcap、TShark CSV 和 Wireshark 截图。

## 2. 当前阶段

已完成：

- 阶段 0：项目初始化与目录结构，审计验收通过。
- 阶段 1：环境检查与 Wireshark/TShark/Npcap 配置，验收通过。
- 阶段 2：DHCP 正式抓包、字段导出、单项验收和截图，验收通过。

尚未开始：

- 阶段 3：ICMP 正式抓包与分析。
- 阶段 4：IPv4 分片正式抓包与分析。
- 阶段 5：TCP 建连和释放正式抓包与分析。
- 后续综合分析、报告生成与最终提交。

`captures/` 当前没有正式抓包文件，也没有正式协议字段 CSV。

上句为阶段 2 开始前的历史状态。当前已存在 DHCP 正式抓包和字段 CSV，其他三类正式抓包仍不存在。

## 3. 阶段 1 真实检查结果

阶段 1 结果：**PASS，可以进入正式抓包，但当前任务明确要求暂不进入阶段 2。**

- 检查时间：2026-06-06 21:50:29。
- Wireshark、TShark 和 Npcap 均可用。
- `npcap` 内核驱动服务状态为 `RUNNING`。
- TShark 可以列出真实网卡。
- Npcap 烟雾测试成功：
  - 文件：`exports/npcap_smoke_test.pcapng`
  - 帧数：789
  - SHA-256：`DFD78C2B857492CC2F471F744BE64D61D4934B3F7A7CE00A0FD143A3BC05B7D8`
  - 说明：仅用于证明抓包能力，不是正式协议抓包。
- 环境截图已存在 4 张：
  - `screenshots/00_environment/01_wireshark_version.png`
  - `screenshots/00_environment/02_tshark_interfaces.png`
  - `screenshots/00_environment/03_ipconfig_all.png`
  - `screenshots/00_environment/04_wireshark_interface_selection.png`
- 详细检查记录：`exports/environment_check.md`。

### 2026-06-07 复核

阶段 1 最终状态：**PASS（不是 WARN）**。

- `exports/environment_check.md`、`exports/tshark_interfaces.txt`、`exports/ipconfig_before.txt` 和 `exports/experiment_metadata.md` 均存在。
- 实时 `tshark -D` 仍确认 WLAN 为接口 5，GUID 为 `887FB422-3CDD-474F-AB19-69DDE6D65A61`。
- `npcap` 服务仍为 `RUNNING`。
- `exports/npcap_smoke_test.pcapng` 可由 TShark 读取，共 789 帧。
- `screenshots/00_environment/` 下 4 张要求截图均存在，环境截图为 4/4。
- 本次复核未开始 DHCP 或其他正式协议抓包。

## 4. 工具状态

| 工具 | 状态 | 版本/路径 |
|---|---|---|
| Wireshark | PASS | 4.6.6；`C:\Program Files\Wireshark\Wireshark.exe` |
| TShark | PASS | 4.6.6；`C:\Program Files\Wireshark\tshark.exe` |
| Npcap | PASS | 1.88；`npcap` 服务 `RUNNING` |
| Python | PASS | Python 3.13.7；自动化脚本语法检查通过 |

Wireshark 和 TShark 未加入 PATH，后续默认使用完整路径。

## 5. 当前 WLAN 接口

| 字段 | 真实值 |
|---|---|
| 当前接口编号 | 5 |
| 接口名称 | WLAN |
| 接口 GUID | `887FB422-3CDD-474F-AB19-69DDE6D65A61` |
| 完整 NPF 接口 | `\Device\NPF_{887FB422-3CDD-474F-AB19-69DDE6D65A61}` |
| WLAN 设备 | Intel(R) Wi-Fi 6 AX201 160MHz |
| 本机 IPv4 | 10.21.225.70 |
| 子网掩码 | 255.255.128.0 |
| 默认网关 | 10.21.128.1 |
| DHCP 服务器 | 10.3.9.2 |
| DNS 服务器 | 10.3.9.4、10.3.9.5、10.3.9.6 |
| 网络环境 | 校园网 BUPT-mobile |

接口编号可能因重启或驱动变化而改变。每次正式抓包前必须重新运行：

```cmd
"C:\Program Files\Wireshark\tshark.exe" -D
```

优先按接口名称和 GUID 核对，不能仅依赖编号 5。

## 6. 导出与校验脚本状态

### `scripts/extract_fields.py`

已存在，基础版已完成：

- 支持 `--tshark` 和 `--protocol`。
- 支持 `--preflight`，可在没有正式 pcap 时检查字段兼容性。
- 自动兼容 `dhcp.*` / `bootp.*`。
- 当前 TShark 4.6.6 的 DHCP、ICMP、IPv4 分片和 TCP 字段预检通过。
- 正式 pcap 不存在时不会生成伪造或空 CSV。
- TCP 抓包存在后会同时导出相对序号和原始序号 CSV。

### `scripts/validate_captures.py`

已存在，基础版已完成：

- DHCP：检查 Discover、Offer、Request、ACK，并记录 Release 是否存在。
- ICMP：按 Identifier、Sequence 和反向地址匹配请求/响应。
- IPv4 分片：按源、目的、协议和 Identification 分组，检查偏移连续性。
- TCP：按 `tcp.stream` 检查握手、FIN 正常释放和 RST。
- 生成 `exports/analysis_summary.md`。
- 正式数据缺失或不完整时返回 FAIL，不输出伪成功。

脚本基础版已建立，但在每种真实抓包完成后仍需运行单项导出和校验。

### 2026-06-07 脚本基础版复核

- 复核时间：2026-06-07 01:06:15 +08:00。
- `scripts/extract_fields.py`：**PASS，已做最小补强**。
  - 字段预检、DHCP `dhcp.*` / `bootp.*` 兼容、UTF-8 CSV 和 TCP 双序号导出逻辑通过。
  - 缺少正式 pcap 时返回非零状态且不创建 CSV，负向检查通过。
- `scripts/validate_captures.py`：**PASS，已做最小补强**。
  - 增加真实 pcap/CSV 来源门槛、Frame Number 有效性和唯一性检查。
  - DHCP 按同一 Transaction ID 检查 DORA；ICMP、IPv4 分片和 TCP 检查逻辑已复核。
  - 没有真实 pcap/CSV 文件对时不生成 `exports/analysis_summary.md`，负向检查通过。
- TShark 4.6.6 字段预检：**PASS**。
  - DHCP：19 个字段。
  - ICMP：15 个字段。
  - IPv4 分片：13 个字段。
  - TCP：15 个字段。
- Python 语法检查：两份脚本均 **PASS**。
- 详细报告：`exports/script_preflight_check.md`。
- 阶段判断：脚本基础版允许进入阶段 2 DHCP，但必须等待用户明确确认。
- 本次未开始正式抓包，未生成正式协议 CSV 或分析文件。

## 6.1 阶段 2 DHCP 真实结果

阶段 2 DHCP 结果：**PASS，不需要重抓。**

- 抓包时间：2026-06-07 10:05:21.513361000 至 10:05:24.829190000 +08:00。
- 正式抓包：`captures/dhcp.pcapng`。
- 接口：WLAN，GUID `887FB422-3CDD-474F-AB19-69DDE6D65A61`。
- 捕获过滤器：`udp port 67`。
- 触发命令：`ipconfig /release "WLAN"`、`ipconfig /renew "WLAN"`。
- pcap 帧数：5。
- pcap SHA-256：`990835E69AE6F0CDEB90EA7B7163A65DA4692584B08D1201E4AA28774379F0D2`。
- 字段 CSV：`exports/dhcp_fields.csv`，5 行。
- DHCP 验收：
  - Frame 1：Release，Transaction ID `0x7df8922e`。
  - Frame 2：Discover，Transaction ID `0x4d8d939a`。
  - Frame 3：Offer，Transaction ID `0x4d8d939a`。
  - Frame 4：Request，Transaction ID `0x4d8d939a`。
  - Frame 5：ACK，Transaction ID `0x4d8d939a`。
- Frame 2 至 Frame 5 构成同一事务的完整 DORA。
- 分析文件：`exports/dhcp_analysis.md`。
- 抓包日志：`exports/dhcp_capture_log.md`。
- 截图：`screenshots/01_dhcp/`，共 8 张，规定的 6 张及 Offer/ACK 配置补图均已完成。
- 所有数值均来自本次真实 pcap 和 CSV；未使用烟雾测试 pcap。

### 2026-06-07 截图审计修正

- DHCP pcap、CSV、字段分析和单项验收仍为 **PASS**，不需要重抓网络报文。
- DHCP 现有 8 张截图的协议内容覆盖为 PASS，但包含控制提示、光标高亮或助手悬浮标记，最终提交状态改为 **WARN，待使用同一真实 pcap 无提示复拍**。
- 环境截图总体为 WARN/FAIL：
  - 三张命令结果图不是原生 CMD 窗口截图。
  - `04_wireshark_interface_selection.png` 选中的是“本地连接* 10”，不是 WLAN。
- 详细审计和复拍清单：`exports/screenshot_audit.md`。
- 不通过图像修改伪装操作来源；复拍必须继续使用真实 `captures/dhcp.pcapng` 和相同 Frame 1–5。

## 7. 已存在文件清单

### 根文件

- `README.md`
- `PROJECT_EXECUTION_BASELINE.md`
- `PROJECT_STATE.md`
- `npcap-1.88-official.exe`

### 环境与元数据

- `exports/environment_check.md`
- `exports/experiment_metadata.md`
- `exports/ipconfig_before.txt`
- `exports/tshark_interfaces.txt`
- `exports/tool_versions.txt`
- `exports/npcap_smoke_test.pcapng`
- `exports/dhcp_capture_log.md`
- `exports/dhcp_fields.csv`
- `exports/dhcp_analysis.md`
- `exports/analysis_summary.md`

### DHCP 正式数据与截图

- `captures/dhcp.pcapng`
- `screenshots/01_dhcp/01_dhcp_packet_list.png`
- `screenshots/01_dhcp/02_dhcp_release_detail.png`
- `screenshots/01_dhcp/03_dhcp_discover_detail.png`
- `screenshots/01_dhcp/04_dhcp_offer_detail.png`
- `screenshots/01_dhcp/04b_dhcp_offer_options.png`
- `screenshots/01_dhcp/05_dhcp_request_detail.png`
- `screenshots/01_dhcp/06_dhcp_ack_detail.png`
- `screenshots/01_dhcp/06b_dhcp_ack_options.png`

### 环境截图

- `screenshots/00_environment/01_wireshark_version.png`
- `screenshots/00_environment/02_tshark_interfaces.png`
- `screenshots/00_environment/03_ipconfig_all.png`
- `screenshots/00_environment/04_wireshark_interface_selection.png`

### 正式实验脚本

- `scripts/extract_fields.py`
- `scripts/validate_captures.py`
- `scripts/render_environment_screenshots.py`
- `scripts/capture_wireshark_interface_page.ps1`

### 环境修复和诊断留档

- `scripts/cleanup_npcap_residuals.ps1`
- `scripts/diagnose_network_stack.ps1`
- `scripts/remove_broken_vmware_bridge.ps1`
- `scripts/rebuild_vmware_bridge.ps1`
- `exports/network_stack_diagnostics.txt`
- `exports/npcap_cleanup.log`
- `exports/npcap_failed_install_logs/`
- `exports/vmware_bridge_cleanup.log`
- `exports/vmware_bridge_rebuild.log`

### 已存在目录

- `captures/`
- `exports/`
- `screenshots/00_environment/`
- `screenshots/01_dhcp/`
- `screenshots/02_icmp/`
- `screenshots/03_ip_fragment/`
- `screenshots/04_tcp/`
- `report/figures/`
- `report/tables/`
- `scripts/`

## 8. 未完成事项

- 脚本基础版复核已完成；真实数据端到端验证将在各协议首次正式抓包后执行。
- ICMP 正式抓包、CSV、分析和截图。
- IPv4 分片正式抓包、CSV、分析和截图。
- TCP 正式抓包、CSV、分析、序列图和截图。
- `exports/analysis_summary.md`。
- 报告初稿、DOCX、PDF 和最终检查清单。
- 学生班级、学号、姓名仍为 `【待填写】`。

## 9. 下一步计划

下一次继续时必须按以下顺序：

1. 先读取本文件和 `PROJECT_EXECUTION_BASELINE.md`。
2. 复核或补强 `scripts/extract_fields.py` 与 `scripts/validate_captures.py` 基础版。
3. 运行脚本预检，不生成正式协议 CSV。
4. 重新运行 `tshark -D`，按 WLAN 名称和 GUID 确认接口。
5. 用户明确同意后，才进入阶段 2 DHCP 正式抓包。
6. DHCP 抓包完成后立即导出、校验，再决定重抓和截图。

### 2026-06-07 复核后的下一步

1. 等待用户明确确认开始阶段 2 DHCP。
2. 开始前重新运行 `tshark -D`，按 WLAN 名称和 GUID 确认正式抓包接口。
3. 完成真实 DHCP 抓包后，立即运行字段导出和 DHCP 单项校验。
4. 只有真实 pcap、CSV 和关键 Frame Number 通过验收后，才生成 DHCP 分析和截图。

### 阶段 2 完成后的下一步

1. 阶段 2 DHCP 已通过，不需要重抓。
2. 当前停止在阶段 2，不自动进入其他协议。
3. 等待用户明确确认后，进入阶段 3 ICMP 前置检查和正式抓包。
4. 阶段 3 开始前仍需重新核对 WLAN 名称和 GUID。

### 2026-06-07 阶段 2 重新抓包

- 旧阶段 2 证据已归档到 `archive/stage2-dhcp-20260607-111518/`。
- Clash Verge、系统代理和普通后台程序保持开启。
- 系统代理为 `127.0.0.1:7897`，`10.*` 为直连例外。
- Sangfor aTrust VNIC 断开；VMware VMnet1/VMnet8 无默认网关。
- 唯一默认路由为 WLAN 网关 `10.21.128.1`。
- 新正式抓包：`captures/dhcp.pcapng`。
- 抓包时间：2026-06-07 11:16:58.462584100 至 11:17:00.744677700 +08:00。
- SHA-256：`B07B87FB47930FC3A4D436D0E9EFF1F65102F6F94AEAC60123F0B24A42B6D1BC`。
- CSV：`exports/dhcp_fields.csv`，5 行。
- Frame 1：Release，Transaction ID `0xdc2ff185`。
- Frame 2 至 Frame 5：完整 DORA，Transaction ID `0xe83ab87a`。
- DHCP 数据验收：**PASS**。
- 截图状态：**待用户手动复拍**。助手只负责定位 Wireshark，定位完成后停止控制，由用户截图。

### 2026-06-07 阶段 3 开始前交接

- 阶段 3 开始前交接日志已生成：`NEXT_CHAT_HANDOFF_STAGE3.md`。
- 当前 DHCP 正式 pcap/CSV 数据验收：**PASS**。
- 阶段 2 完整交付状态：**WARN**，原因仅为当前正式抓包的无提示截图尚未复拍。
- `exports/analysis_summary.md` 仍包含旧 DHCP 抓包事务号，恢复上下文时不得将其作为当前 DHCP 数值来源。
- 当前正式 DHCP 证据应以 `captures/dhcp.pcapng`、`exports/dhcp_fields.csv`、`exports/dhcp_capture_log.md` 和 `exports/dhcp_analysis.md` 为准。
- `captures/icmp.pcapng`、`exports/icmp_fields.csv` 和 `exports/icmp_analysis.md` 尚不存在。
- 可以在下一个聊天框进入阶段 3 ICMP，但必须先读取交接日志、重新核对 WLAN GUID，并等待用户明确确认后抓包。
- 本次仅生成交接文件和更新项目状态；未开始 ICMP，未修改任何 pcap、CSV 或截图。

## 10. 禁止事项

1. 不得虚构任何协议字段、帧号、IP、MAC、Seq、Ack、Identification、Offset 或 Flags。
2. 不得依赖聊天上下文恢复项目；必须以本文件和项目内证据为准。
3. 不得覆盖或重写 `PROJECT_EXECUTION_BASELINE.md`。
4. 不得把烟雾测试 pcap 当作正式实验数据。
5. 不得在正式 pcap 缺失时生成示例 CSV、截图或分析结论。
6. 不得把不同 `tcp.stream` 的报文混合分析。
7. 不得用理论计算值替代真实抓包字段；计算只能用于一致性校验。
8. 未经用户明确指示，不得提前开始 DHCP 或其他正式抓包。
9. 不得删除、覆盖或修改已有 pcap、CSV 和截图。

## 11. 2026-06-19 四类协议完成状态

- DHCP：正式 pcap、CSV、分析和截图内容验收 PASS。
- ICMP：正式 pcap、CSV、分析和 4 张截图验收 PASS。
- IPv4 分片：正式 pcap、CSV 和分析验收 PASS；请求、应答各有一组完整的 6 个分片。
- TCP：正式采用重抓文件，`tcp.stream == 2` 同时包含三次握手和正常 FIN 释放；相对/原始序号 CSV、分析文件、序列图和 8 张截图验收 PASS。
- 综合自动校验：DHCP、ICMP、IPv4 fragments、TCP 全部 PASS。

### 正式 TCP 证据

- 正式文件：`captures/tcp.pcapng`
- SHA-256：`2046914206267F90A8CCDC8E7EF78BA78D2CE24AB2BD4FE94B24697B21B4E88F`
- 采用流：`tcp.stream == 2`
- 三次握手：Frame 16、17、18
- 连接释放：Frame 24、25、26、27
- 首次不合格文件：`captures/tcp_failed_20260619_230511.pcapng`

### 报告前遗留项

1. `screenshots/03_ip_fragment/03_middle_fragment_ipv4_detail.png` 和 `04_last_fragment_ipv4_detail.png` 左下角存在播放器悬浮条，需无悬浮控件复拍。
2. `screenshots/00_environment/` 仍沿用旧截图审计结果：三张命令图为 WARN，Wireshark 接口选择图选错接口为 FAIL，需复拍 WLAN 环境证据。
3. 学生班级、学号、姓名仍待提供。
4. 完成上述事项后进入报告初稿、DOCX/PDF 和最终反查阶段。

## 12. 2026-06-19 最终报告完成

- 学院：计算机学院（国家示范性软件学院）
- 班级：304
- 学号：2024212936
- 姓名：刘文涛
- 实验日期：2026-06-19
- DOCX：`report/计网实验2报告-304-2024212936-刘文涛.docx`
- PDF：`report/计网实验2报告-304-2024212936-刘文涛.pdf`
- PDF 页数：13
- PDF SHA-256：`3D321FE774031093070081E7AC3BD58EA5E9DD5EEAB996930EE07747C459737E`
- 四类协议自动验收：全部 PASS。
- 报告占位符检查：PASS。
- Word 导出 PDF：PASS。
- PDF 逐页渲染与视觉检查：PASS。
- 最终检查清单：`report/final_checklist.md`

当前状态：最终 PDF 已具备提交条件。

## 13. 2026-06-20 提示语优化终稿

- 已依据用户提供的详细提示语重构报告，补全实验目的、实验内容、实验环境、总体方法、四类协议分析、问题与解决方法、心得和材料审计附录。
- 最终 Word：`report/计网实验2报告-304-2024212936-刘文涛.docx`
- Markdown 源稿：`report/计网实验2报告-304-2024212936-刘文涛.md`
- 文档结构校验：138 个正文段落、13 张表格、13 幅插图、3 个显式分页符。
- TCP 三次握手和连接释放小节已分别整页起排，修复首轮 PDF 中表格跨页不美观的问题。
- 四类正式抓包自动校验再次全部 PASS。
- 最终 DOCX 占位符扫描：PASS。
- 最终 DOCX SHA-256：`408A36C0E2403AC350DD84D58CE22A0EA6627FDDB54B403B34F65D463C5E9FBA`
- 分页优化前 PDF 已移至 `report/archive/分页优化前-计网实验2报告-304-2024212936-刘文涛.pdf`，避免误交。
- 第二轮 PDF 渲染因系统工具额度限制未能执行；提交主文件以最终 DOCX 为准。

当前状态：提示语优化后的最终 Word 已完成并通过内容、结构和抓包证据复核。
