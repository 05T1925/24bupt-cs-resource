# 计算机网络实验二：IP 和 TCP 数据分组的捕获和解析

## 实验目标

- 捕获并分析 DHCP 报文。
- 捕获并分析 ICMP 报文。
- 捕获并分析 IPv4 数据报分片。
- 捕获并分析 TCP 建立连接和释放连接过程。

## 目录说明

- captures：保存 Wireshark 抓包文件。
- exports：保存 tshark 导出的字段表和分析摘要。
- screenshots：保存实验截图。
- report：保存实验报告草稿、图表和最终文件。
- scripts：保存自动化解析脚本。

## 文件命名规范

- DHCP 抓包：captures/dhcp.pcapng
- ICMP 抓包：captures/icmp.pcapng
- IP 分片抓包：captures/ip_fragment.pcapng
- TCP 抓包：captures/tcp.pcapng
- 报告草稿：report/draft.md
- 最终报告：report/计网实验2报告-班级-学号-姓名.pdf

## 数据真实性要求

所有协议字段值必须来自真实 pcap、tshark 导出的 CSV 或 Wireshark 截图，不允许虚构。

每个关键字段表必须保留 Frame Number，保证报告中的结论可以回查到具体报文。

如果某类抓包数据缺失，应明确标记缺失并重新抓包，不得用理论值或示例值替代真实实验数据。

## 当前执行基线

后续实施顺序、风险控制和验收标准见 `PROJECT_EXECUTION_BASELINE.md`。
