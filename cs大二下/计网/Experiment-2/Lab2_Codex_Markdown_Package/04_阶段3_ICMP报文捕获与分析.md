# 阶段 3：ICMP 报文捕获与分析

> 目标：通过 ping 捕获 ICMP Echo Request 和 Echo Reply，分析 ICMP 报文格式及主要字段功能。  
> 输入：活跃网卡、Wireshark/tshark 可用。  
> 输出：`captures/icmp.pcapng`、`exports/icmp_fields.csv`、ICMP 分析摘要、截图。

---

## 1. 实验目标

本阶段通过 `ping` 命令产生 ICMP 报文，捕获并分析：

```text
Echo Request
Echo Reply
ICMP Type
ICMP Code
Checksum
Identifier
Sequence Number
Data
IPv4 TTL
IPv4 Protocol
```

---

## 2. Wireshark 抓包方式

可选方式 A：不设置捕获过滤器，抓全量包，之后用显示过滤器筛选。

推荐显示过滤器：

```text
icmp
```

可选方式 B：设置捕获过滤器：

```text
icmp
```

如果担心漏包，优先使用方式 A。

---

## 3. ping 命令

优先使用 IPv4：

```bat
ping -4 -n 4 www.bupt.edu.cn
```

如果域名解析失败或无响应，使用公共 IP：

```bat
ping -4 -n 4 114.114.114.114
```

也可以用默认网关测试：

```bat
ping -4 -n 4 <默认网关IP>
```

默认网关 IP 可从 `exports/ipconfig_before.txt` 中读取。

---

## 4. 手动操作步骤

1. 打开 Wireshark。
2. 选择阶段 1 确认的活跃网卡。
3. 开始捕获。
4. 打开 CMD。
5. 执行：

```bat
ping -4 -n 4 www.bupt.edu.cn
```

6. 如果没有 Echo Reply，改用：

```bat
ping -4 -n 4 114.114.114.114
```

7. 停止 Wireshark 捕获。
8. 保存为：

```text
captures/icmp.pcapng
```

9. 在 Wireshark 显示过滤器输入：

```text
icmp
```

---

## 5. 需要截图的内容

保存到 `screenshots/02_icmp/`：

```text
01_icmp_packet_list.png
02_icmp_echo_request_detail.png
03_icmp_echo_reply_detail.png
04_icmp_ipv4_header_detail.png
```

截图要求：

- 包列表区能看到 Echo Request 和 Echo Reply 成对出现。
- 详情区展开 Internet Protocol Version 4。
- 详情区展开 Internet Control Message Protocol。
- Type、Code、Checksum、Identifier、Sequence Number 可见。

---

## 6. tshark 导出字段命令

```bat
tshark -r captures\icmp.pcapng -Y "icmp" -T fields ^
  -E header=y -E separator=, -E quote=d ^
  -e frame.number ^
  -e frame.time_relative ^
  -e ip.src ^
  -e ip.dst ^
  -e ip.ttl ^
  -e ip.proto ^
  -e ip.len ^
  -e icmp.type ^
  -e icmp.code ^
  -e icmp.checksum ^
  -e icmp.ident ^
  -e icmp.seq ^
  -e data.len > exports\icmp_fields.csv
```

如果 `data.len` 为空，可以接受；报告中以实际可见字段为准。

---

## 7. 需要分析的 ICMP 内容

### 7.1 Echo Request

通常字段含义：

```text
Type = 8：回显请求
Code = 0：无进一步子类型
Checksum：ICMP 报文校验和
Identifier：请求标识，用于匹配同一 ping 进程
Sequence Number：序列号，用于匹配请求和响应
```

### 7.2 Echo Reply

通常字段含义：

```text
Type = 0：回显应答
Code = 0：无进一步子类型
Identifier：应与对应请求一致
Sequence Number：应与对应请求一致
```

### 7.3 IPv4 头部相关字段

需要说明：

```text
Protocol = 1：表示上层协议是 ICMP
TTL：生存时间，每经过一台路由器减 1
Source IP：发送方地址
Destination IP：接收方地址
Total Length：IP 数据报总长度
```

---

## 8. 建议生成的分析表

| Frame | Time | Src IP | Dst IP | Type | Code | Identifier | Sequence | TTL | 说明 |
|---|---:|---|---|---:|---:|---|---|---:|---|
| 实际帧号 | 实际时间 | 实际值 | 实际值 | 实际值 | 实际值 | 实际值 | 实际值 | 实际值 | Echo Request/Reply |

Codex 必须从 CSV 读取实际值填表。

---

## 9. ICMP 阶段验收标准

```text
[ ] captures/icmp.pcapng 存在
[ ] 能看到至少 1 个 Echo Request
[ ] 能看到至少 1 个 Echo Reply，或明确说明目标无响应
[ ] exports/icmp_fields.csv 存在
[ ] Type、Code、Checksum、Identifier、Sequence Number 可读
[ ] 至少 3 张关键截图已保存
[ ] 分析中没有虚构字段
```

---

## 10. 常见失败处理

### 10.1 ping 域名失败

改用：

```bat
ping -4 -n 4 114.114.114.114
```

### 10.2 抓不到 ICMP

处理：

1. 确认抓的是正确网卡。
2. 不设置捕获过滤器，抓全量包。
3. 用显示过滤器：

```text
icmp
```

4. 检查是否 ping 使用了 IPv6。如果是，强制 IPv4：

```bat
ping -4 -n 4 目标
```

### 10.3 只有请求没有响应

可能是目标禁 ping 或网络阻断。换目标：

```bat
ping -4 -n 4 默认网关
ping -4 -n 4 114.114.114.114
```

---

## 11. 给 Codex 的提示语

```text
请执行阶段 3：ICMP 报文捕获与分析。

任务：
1. 检查 captures/icmp.pcapng 是否存在。
2. 如果不存在，请指导我打开 Wireshark 对活跃网卡开始捕获。
3. 指导我执行 ping -4 -n 4 www.bupt.edu.cn；如果失败，改用 ping -4 -n 4 114.114.114.114 或默认网关。
4. 要求我停止捕获并保存为 captures/icmp.pcapng。
5. 使用 tshark 导出 exports/icmp_fields.csv。
6. 读取 CSV，找出 Echo Request 和 Echo Reply 对。
7. 整理每组请求/响应的 Frame Number、Time、Src IP、Dst IP、ICMP Type、Code、Checksum、Identifier、Sequence Number、TTL。
8. 生成 exports/icmp_analysis.md，包含捕获过程、字段表、ICMP 字段功能说明、请求响应匹配关系、截图清单。
9. 如果只有请求没有响应，请明确说明并给出换目标重新抓包方案。
10. 不得虚构任何 ICMP 字段。

输出：
- ICMP 报文数量。
- Echo Request/Reply 匹配表。
- ICMP 分析摘要路径。
- 截图清单。

禁止：
- 禁止编造 Type、Code、Checksum、Identifier、Sequence Number。
- 禁止在无 Echo Reply 时假装捕获成功。
```
