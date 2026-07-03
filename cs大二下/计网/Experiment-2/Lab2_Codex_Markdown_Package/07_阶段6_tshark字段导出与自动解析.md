# 阶段 6：tshark 字段导出与自动解析脚本

> 目标：编写自动化脚本，统一检查四个 pcap 文件，导出 CSV，并生成分析摘要。  
> 输入：`captures/*.pcapng`。  
> 输出：`scripts/extract_fields.py`、`scripts/validate_captures.py`、`exports/*.csv`、`exports/analysis_summary.md`。

---

## 1. 本阶段目标

前面阶段可以手动导出字段。本阶段要求 Codex 写脚本自动完成：

1. 检查 pcap 文件是否存在。
2. 检查 tshark 是否可用。
3. 检查 DHCP 字段名兼容性。
4. 导出 DHCP、ICMP、IP 分片、TCP 字段表。
5. 解析 CSV，生成汇总 Markdown。
6. 检查是否缺少关键报文。
7. 输出清晰的重新抓包建议。

---

## 2. 输入文件

```text
captures/dhcp.pcapng
captures/icmp.pcapng
captures/ip_fragment.pcapng
captures/tcp.pcapng
```

---

## 3. 输出文件

```text
exports/dhcp_fields.csv
exports/icmp_fields.csv
exports/ip_fragment_fields.csv
exports/tcp_fields.csv
exports/tcp_fields_rawseq.csv
exports/analysis_summary.md
scripts/extract_fields.py
scripts/validate_captures.py
```

---

## 4. extract_fields.py 功能要求

`scripts/extract_fields.py` 应支持：

```bat
python scripts\extract_fields.py
```

也支持指定 tshark 路径：

```bat
python scripts\extract_fields.py --tshark "C:\Program Files\Wireshark\tshark.exe"
```

功能：

```text
1. 确认 tshark 可执行。
2. 确认 captures 目录存在。
3. 分别检查四个 pcap 文件。
4. 自动选择 DHCP 字段前缀 dhcp 或 bootp。
5. 调用 tshark 导出 CSV。
6. 输出每个 CSV 的行数。
7. 若命令失败，打印完整错误信息。
8. 不吞掉错误，不生成虚假的空分析。
```

---

## 5. validate_captures.py 功能要求

`scripts/validate_captures.py` 应支持：

```bat
python scripts\validate_captures.py
```

功能：

```text
1. 读取 exports/dhcp_fields.csv，检查 Discover/Offer/Request/ACK 是否存在。
2. 读取 exports/icmp_fields.csv，检查 Echo Request/Reply 是否存在。
3. 读取 exports/ip_fragment_fields.csv，检查是否有同一 Identification 的多个分片。
4. 读取 exports/tcp_fields.csv，检查是否存在完整 SYN、SYN+ACK、ACK，以及 FIN/ACK。
5. 生成 exports/analysis_summary.md。
6. 对每个阶段输出 PASS/WARN/FAIL。
```

---

## 6. analysis_summary.md 结构

```markdown
# 实验二抓包分析汇总

## 1. 文件检查

| 文件 | 状态 | 备注 |
|---|---|---|
| captures/dhcp.pcapng | PASS/FAIL |  |
| captures/icmp.pcapng | PASS/FAIL |  |
| captures/ip_fragment.pcapng | PASS/FAIL |  |
| captures/tcp.pcapng | PASS/FAIL |  |

## 2. DHCP 分析摘要

| Message Type | Frame | Src IP | Dst IP | Transaction ID | 备注 |
|---|---:|---|---|---|---|

## 3. ICMP 分析摘要

| Pair | Request Frame | Reply Frame | Identifier | Sequence | 备注 |
|---:|---:|---:|---|---|---|

## 4. IP 分片分析摘要

| Identification | 分片数量 | 首片 Frame | 末片 Frame | 是否完整 | 备注 |
|---|---:|---:|---:|---|---|

## 5. TCP 分析摘要

| tcp.stream | Client | Server | 是否有握手 | 是否有释放 | 备注 |
|---:|---|---|---|---|---|

## 6. 缺失项和重抓建议
```

---

## 7. Python 实现注意事项

Codex 生成脚本时应遵守：

```text
1. 使用 subprocess.run 调用 tshark。
2. 使用 pathlib 管理路径。
3. 使用 csv 或 pandas 读取 CSV；若使用 pandas，应在无依赖时给出安装说明。
4. 处理 Windows 路径空格。
5. 对每个命令打印清晰日志。
6. 字段导出失败时不要静默继续。
7. 不对空 CSV 编造分析结果。
```

推荐不强依赖 pandas，优先使用 Python 标准库。

---

## 8. validate_captures.py 的判断逻辑

### 8.1 DHCP

如果 DHCP Message Type 是数字，常见映射为：

```text
1 = Discover
2 = Offer
3 = Request
4 = Decline
5 = ACK
6 = NAK
7 = Release
8 = Inform
```

脚本应同时兼容数字和文本。

### 8.2 ICMP

```text
Type 8 = Echo Request
Type 0 = Echo Reply
```

按 Identifier + Sequence 匹配请求与响应。

### 8.3 IP 分片

按以下字段分组：

```text
ip.src
ip.dst
ip.proto
ip.id
```

判断：

```text
分片数量 >= 2
至少一个 MF = 1
至少一个 offset > 0
最后一个分片通常 MF = 0 且 offset > 0
```

### 8.4 TCP

按 `tcp.stream` 分组，判断：

```text
SYN=1 ACK=0
SYN=1 ACK=1
ACK=1 SYN=0 FIN=0
FIN=1
```

优先选择同时包含握手和释放的 stream。

---

## 9. 本阶段验收标准

```text
[ ] scripts/extract_fields.py 存在
[ ] scripts/validate_captures.py 存在
[ ] 可以通过 python scripts\extract_fields.py 运行
[ ] 可以通过 python scripts\validate_captures.py 运行
[ ] exports/analysis_summary.md 生成
[ ] 每个协议给出 PASS/WARN/FAIL
[ ] 缺失数据有明确重抓建议
[ ] 没有虚构字段
```

---

## 10. 给 Codex 的提示语

```text
请执行阶段 6：tshark 字段导出与自动解析脚本。

任务：
1. 编写 scripts/extract_fields.py。
2. 编写 scripts/validate_captures.py。
3. extract_fields.py 要自动检查 tshark 路径，支持 --tshark 参数。
4. extract_fields.py 要导出 DHCP、ICMP、IP 分片、TCP 的 CSV 文件。
5. DHCP 字段必须兼容 dhcp 和 bootp 两种字段名前缀。
6. validate_captures.py 要读取 CSV 并生成 exports/analysis_summary.md。
7. analysis_summary.md 要包含文件检查、DHCP 摘要、ICMP 摘要、IP 分片摘要、TCP 摘要、缺失项和重抓建议。
8. 所有分析只能基于 CSV 实际内容，禁止伪造。
9. 代码要适配 Windows，路径使用 pathlib。
10. 完成后运行脚本，展示输出日志，并说明哪些阶段通过、哪些阶段需要补抓。

输出：
- scripts/extract_fields.py
- scripts/validate_captures.py
- exports/analysis_summary.md
- 运行结果摘要

禁止：
- 禁止在 CSV 为空时输出成功。
- 禁止写死示例 IP、示例帧号、示例 Seq/Ack。
- 禁止把理论值当作实验结果。
```
