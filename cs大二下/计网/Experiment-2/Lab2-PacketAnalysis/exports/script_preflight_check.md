# 脚本基础版复核结果

- 复核时间：2026-06-07 01:06:15 +08:00
- 项目目录：`C:\Users\28641\Desktop\计网\Experiment-2\Lab2-PacketAnalysis`
- TShark：4.6.6，`C:\Program Files\Wireshark\tshark.exe`
- 复核范围：字段导出与抓包校验脚本基础版；未进行正式抓包。

## 1. 读取的上下文文件

| 文件 | 结果 | 说明 |
|---|---|---|
| `PROJECT_STATE.md` | PASS | 已确认阶段 0、阶段 1 完成，阶段 2 尚未开始 |
| `PROJECT_EXECUTION_BASELINE.md` | PASS | 已确认真实性、证据链和执行顺序要求；未修改 |
| `README.md` | PASS | 已确认目录、命名和数据真实性要求 |
| `exports/environment_check.md` | PASS | 已确认 TShark 4.6.6 路径及历史字段预检结果 |
| `exports/experiment_metadata.md` | PASS | 已确认 WLAN 名称、接口编号和 GUID 记录 |

## 2. extract_fields.py 检查

| 检查项 | 结果 | 说明 |
|---|---|---|
| 仅使用 Python 标准库 | PASS | 使用 `argparse`、`subprocess`、`sys`、`pathlib` |
| 支持 `--tshark` | PASS | 可指定 TShark 完整路径 |
| 支持 `--protocol` | PASS | 支持 `all`、`dhcp`、`icmp`、`ip_fragment`、`tcp` |
| 支持 `--preflight` | PASS | 仅检查版本和字段兼容性，不要求 pcap |
| 默认 TShark 路径 | PASS | `C:\Program Files\Wireshark\tshark.exe` |
| 导出前检查字段 | PASS | 使用 `tshark -G fields` 并拒绝缺失字段 |
| DHCP 字段兼容性 | PASS | 自动选择 `dhcp.*` 或 `bootp.*` |
| 缺少正式 pcap 时拒绝导出 | PASS | 返回非零状态，不创建空 CSV；已做负向验证 |
| TShark 命令失败处理 | PASS | 抛出错误、打印 `[FAIL]` 并返回非零状态 |
| CSV 编码和格式 | PASS | UTF-8 写入；TShark 使用逗号分隔、双引号转义 |
| 四类协议支持 | PASS | DHCP、ICMP、IPv4 分片、TCP 均有独立定义 |
| TCP 两类序号导出 | PASS | 可生成相对序号 CSV 和 `tcp_fields_rawseq.csv` |

本次对脚本做了最小修改：缺少抓包时累计失败并返回非零状态；将 TShark 输出明确解码后以 UTF-8 写入。

## 3. validate_captures.py 检查

| 检查项 | 结果 | 说明 |
|---|---|---|
| 仅使用 Python 标准库 | PASS | 使用 `csv`、`collections`、`dataclasses`、`pathlib` |
| 协议独立状态 | PASS | DHCP、ICMP、IPv4 分片、TCP 分别输出 PASS/WARN/FAIL |
| DHCP DORA 检查 | PASS | 要求同一 Transaction ID 包含 Discover、Offer、Request、ACK，并记录 Release |
| ICMP 请求响应匹配 | PASS | 按 Identifier、Sequence 和反向源/目的地址匹配 |
| IPv4 分片分组与连续性 | PASS | 按源、目的、协议、Identification 分组，检查 MF、末片和连续覆盖 |
| `ip.hdr_len` 单位 | PASS | 本机 `tshark -G fields` 确认为 32-bit words，乘 4 计算字节数正确 |
| TCP 流检查 | PASS | 按 `tcp.stream`、方向和帧顺序检查 SYN、SYN+ACK、ACK；区分双向 FIN 与 RST |
| Frame Number 回查门槛 | PASS | 要求 Frame Number 为正整数且在 CSV 中唯一，同时要求对应 pcap 存在 |
| 缺失或不完整数据处理 | PASS | 缺失时 FAIL；RST 异常结束可 WARN，不输出伪 PASS |
| 分析汇总生成门槛 | PASS | 仅当至少存在一组真实 pcap/CSV 文件对时才写 `analysis_summary.md` |

本次对脚本做了最小修改：增加 pcap/CSV 来源门槛和 Frame Number 检查；DHCP 改为同事务 DORA；ICMP 支持重复键按顺序配对；TCP 增加方向与帧序检查；无真实文件对时禁止生成汇总。

## 4. TShark 字段预检

| 协议 | 字段数量 | 结果 | 说明 |
|---|---:|---|---|
| DHCP | 19 | PASS | 本机使用 `dhcp.*` 字段 |
| ICMP | 15 | PASS | 所需字段全部受支持 |
| IPv4 分片 | 13 | PASS | 所需字段全部受支持 |
| TCP | 15 | PASS | 所需字段全部受支持 |

总预检和四次按协议预检均使用 TShark 4.6.6 成功完成。预检期间所有正式 pcap 均不存在，脚本只报告 `capture not yet present`，没有导出 CSV。

## 5. 语法检查

| 脚本 | 结果 | 说明 |
|---|---|---|
| `scripts/extract_fields.py` | PASS | `python -m py_compile` 通过 |
| `scripts/validate_captures.py` | PASS | `python -m py_compile` 通过 |

附加负向检查：

- `extract_fields.py` 在缺少 `captures/dhcp.pcapng` 时退出码为 1，未创建 `exports/dhcp_fields.csv`。
- `validate_captures.py` 在没有正式 CSV 时四项均为 FAIL，退出码为 1，未创建 `exports/analysis_summary.md`。

## 6. 结论

**PASS：脚本基础版复核通过，可以进入阶段 2 DHCP。**

进入阶段 2 后仍需用首个真实 DHCP pcap 完成一次端到端导出和校验，以验证真实报文取值及校园网 DORA 完整性。开始 DHCP 前必须等待用户明确确认，并重新核对 WLAN 名称和 GUID。

本次未开始正式抓包，未创建示例 pcap、示例 CSV、协议分析或正式实验字段。
