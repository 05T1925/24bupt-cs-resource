# 计算机网络实验二：Codex 总控说明

> 用途：本文件作为整个实验的总控 Prompt。先把本文件喂给 Codex，再按阶段依次喂入 `01` 到 `09` 的 Markdown 文件。  
> 实验主题：IP 和 TCP 数据分组的捕获和解析。  
> 核心原则：真实抓包、真实字段、可追溯帧号，禁止虚构任何实验数据。

---

## 1. 实验目标

本实验需要完成以下内容：

1. 捕获并分析 DHCP 报文。
2. 捕获并分析 ICMP 报文。
3. 捕获并分析 IPv4 数据报分片过程。
4. 捕获并分析 TCP 建立连接和释放连接过程。
5. 整理字段表、截图、序列图、报告初稿和最终 PDF。

实验报告必须覆盖：

- 实验内容和实验环境描述。
- 捕获方法和过程。
- DHCP 通信过程。
- ICMP 报文格式及主要字段功能。
- IP 数据报分片原理及关键字段值。
- TCP 建立连接和释放连接过程及关键字段值。
- 实验总结、遇到的问题和解决方法。

---

## 2. 实验数据真实性规则

Codex 必须遵守：

1. 所有协议字段值必须来自真实 `.pcapng` 文件、Wireshark 截图或 `tshark` 导出的 CSV。
2. 不允许用理论值替代抓包值。
3. 不允许编造 IP 地址、MAC 地址、Seq、Ack、Identification、Offset、Flags 等字段。
4. 如果抓包缺失某类数据，必须明确指出“缺失”，并给出重新抓包方案。
5. 所有表格必须保留 Frame Number，保证字段可回查。
6. 报告中的关键结论必须对应具体帧号或截图。

---

## 3. 推荐项目目录结构

请在本机创建以下目录：

```text
Lab2-PacketAnalysis/
  captures/
    dhcp.pcapng
    icmp.pcapng
    ip_fragment.pcapng
    tcp.pcapng
  exports/
    ipconfig_before.txt
    tshark_interfaces.txt
    dhcp_fields.csv
    icmp_fields.csv
    ip_fragment_fields.csv
    tcp_fields.csv
    analysis_summary.md
  screenshots/
    00_environment/
    01_dhcp/
    02_icmp/
    03_ip_fragment/
    04_tcp/
  report/
    figures/
    tables/
    draft.md
    计网实验2报告-班级-学号-姓名.docx
    计网实验2报告-班级-学号-姓名.pdf
  scripts/
    extract_fields.py
    draw_tcp_sequence.py
    validate_captures.py
  README.md
```

---

## 4. 全流程顺序

建议按以下顺序完成：

1. 阶段 0：项目初始化与目录结构。
2. 阶段 1：环境检查与 Wireshark/tshark 配置。
3. 阶段 2：DHCP 报文捕获与分析。
4. 阶段 3：ICMP 报文捕获与分析。
5. 阶段 4：IPv4 数据报分片捕获与分析。
6. 阶段 5：TCP 建立连接和释放连接捕获与分析。
7. 阶段 6：tshark 字段导出与自动解析。
8. 阶段 7：报告生成与截图整理。
9. 阶段 8：最终自检与 PDF 提交。

---

## 5. 通用 Codex 工作模式

每一阶段都按以下模式执行：

1. 先检查前置文件是否存在。
2. 给出用户需要手动执行的操作。
3. 等待用户确认或提供抓包文件。
4. 对抓包文件进行自动化分析。
5. 输出该阶段的 CSV、Markdown 摘要、截图清单。
6. 明确该阶段是否通过验收。

---

## 6. 总控 Prompt：直接喂给 Codex

```text
你是我的计算机网络实验二全流程实验助手。请严格按照我提供的分阶段 Markdown 文件执行实验辅助工作。

实验主题：IP 和 TCP 数据分组的捕获和解析。

总规则：
1. 所有字段值必须来自真实 Wireshark/tshark 抓包结果，禁止虚构。
2. 每一项分析都要能追溯到具体 pcap 文件、Frame Number 或截图。
3. 需要我手动操作的地方，你必须给出明确命令、点击路径、预期现象和失败处理方案。
4. 你负责创建目录、生成脚本、检查环境、指导抓包、导出字段、整理 CSV、生成序列图、撰写报告初稿。
5. 如果某个阶段抓包不完整，你必须指出缺失内容，并要求我重新抓包或补充截图。
6. 最终报告应包含实验环境、捕获方法、协议分析、字段表、截图说明、TCP 序列图、问题解决和心得总结。
7. 最终文件名按：计网实验2报告-班级-学号-姓名.pdf。班级、学号、姓名由我最后提供或你在报告模板中保留占位符。

请先读取本总控说明，然后等待我继续提供阶段 0 文件。
```

---

## 7. 用户需要准备的信息

请在开始实验前准备：

```text
班级：
学号：
姓名：
操作系统版本：
Wireshark 版本：
是否安装 tshark：
联网方式：WLAN / 以太网
实验地点网络环境：校园网 / 家庭网络 / 手机热点 / 其他
```

---

## 8. 注意事项

- DHCP 阶段执行 `ipconfig /release` 会临时断网，必须提前保存文件。
- TCP 阶段应优先使用 `curl` 访问明确的 HTTP 地址，避免浏览器默认 HTTPS 导致 `tcp port 80` 捕获不到。
- IP 分片阶段如果目标主机不响应大包或路径禁止分片，需要更换目标或降低/调整数据长度。
- 所有截图需展示：包列表区、协议解析区、关键字段展开区。
