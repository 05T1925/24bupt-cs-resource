# 阶段 2：DHCP 报文捕获与分析

> 目标：捕获 DHCP Release、Discover、Offer、Request、ACK，分析 DHCP 动态地址分配过程。  
> 输入：活跃网卡、Wireshark/tshark 可用。  
> 输出：`captures/dhcp.pcapng`、`exports/dhcp_fields.csv`、DHCP 分析摘要、截图。

---

## 1. 实验依据

本阶段需要捕获 DHCP 报文。推荐捕获过滤器：

```text
udp port 67
```

推荐显示过滤器：

```text
dhcp
```

在 Windows 中通过以下命令触发 DHCP 地址释放和重新申请：

```bat
ipconfig /release
ipconfig /renew
```

---

## 2. 注意事项

执行 `ipconfig /release` 后会临时断网。执行前请：

```text
[ ] 保存所有文件
[ ] 确认 Wireshark 已经开始捕获
[ ] 确认使用的是活跃网卡
[ ] 确认没有正在进行的重要网络任务
```

如果使用校园网、VPN、热点或虚拟网卡，DHCP 行为可能不同。若抓不到完整 DORA 过程，需要更换网络环境或使用手机热点。

---

## 3. Wireshark 手动抓包步骤

1. 打开 Wireshark。
2. 选择阶段 1 确认的活跃网卡。
3. 在“捕获过滤器”中输入：

```text
udp port 67
```

4. 开始捕获。
5. 管理员身份打开 CMD。
6. 执行：

```bat
ipconfig /release
```

7. 等待 Wireshark 出现 DHCP Release 或相关报文。
8. 执行：

```bat
ipconfig /renew
```

9. 等待 Wireshark 出现 DHCP Discover、Offer、Request、ACK。
10. 停止捕获。
11. 保存为：

```text
captures/dhcp.pcapng
```

---

## 4. Wireshark 显示过滤器

打开 `captures/dhcp.pcapng` 后，在显示过滤器栏输入：

```text
dhcp
```

如果没有结果，尝试：

```text
bootp
```

或：

```text
udp.port == 67 || udp.port == 68
```

---

## 5. 需要截图的内容

保存到 `screenshots/01_dhcp/`：

```text
01_dhcp_packet_list.png
02_dhcp_release_detail.png
03_dhcp_discover_detail.png
04_dhcp_offer_detail.png
05_dhcp_request_detail.png
06_dhcp_ack_detail.png
```

截图要求：

- 包列表区能看到 DHCP 报文顺序。
- 协议解析区展开 Ethernet、IPv4、UDP、DHCP。
- 关键字段要清楚可见。
- DHCP Message Type、Transaction ID、Client MAC、Your IP Address、Requested IP、Server Identifier 等字段至少在相关截图中出现。

---

## 6. tshark 字段导出准备

不同 Wireshark 版本可能使用 `dhcp` 或 `bootp` 字段名。先检查：

```bat
tshark -G fields | findstr /i "dhcp.option bootp.option"
```

如果 `dhcp.option.dhcp` 存在，优先使用 `dhcp.*` 字段；如果只有 `bootp.option.dhcp`，则使用 `bootp.*` 字段。

---

## 7. DHCP 字段导出命令：dhcp 字段版本

```bat
tshark -r captures\dhcp.pcapng -Y "dhcp || bootp" -T fields ^
  -E header=y -E separator=, -E quote=d ^
  -e frame.number ^
  -e frame.time_relative ^
  -e eth.src ^
  -e eth.dst ^
  -e ip.src ^
  -e ip.dst ^
  -e udp.srcport ^
  -e udp.dstport ^
  -e dhcp.id ^
  -e dhcp.hw.mac_addr ^
  -e dhcp.ip.your ^
  -e dhcp.option.dhcp ^
  -e dhcp.option.requested_ip_address ^
  -e dhcp.option.dhcp_server_id ^
  -e dhcp.option.subnet_mask ^
  -e dhcp.option.router ^
  -e dhcp.option.domain_name_server ^
  -e dhcp.option.ip_address_lease_time > exports\dhcp_fields.csv
```

---

## 8. DHCP 字段导出命令：bootp 字段版本

如果上一个命令报字段不存在，改用：

```bat
tshark -r captures\dhcp.pcapng -Y "bootp" -T fields ^
  -E header=y -E separator=, -E quote=d ^
  -e frame.number ^
  -e frame.time_relative ^
  -e eth.src ^
  -e eth.dst ^
  -e ip.src ^
  -e ip.dst ^
  -e udp.srcport ^
  -e udp.dstport ^
  -e bootp.id ^
  -e bootp.hw.mac_addr ^
  -e bootp.ip.your ^
  -e bootp.option.dhcp ^
  -e bootp.option.requested_ip_address ^
  -e bootp.option.dhcp_server_id ^
  -e bootp.option.subnet_mask ^
  -e bootp.option.router ^
  -e bootp.option.domain_name_server ^
  -e bootp.option.ip_address_lease_time > exports\dhcp_fields.csv
```

---

## 9. 需要分析的 DHCP 内容

报告中至少分析：

### 9.1 DHCP 的功能

DHCP 用于为主机自动分配 IP 地址及相关网络参数，包括：

```text
IP 地址
子网掩码
默认网关
DNS 服务器
租约时间
DHCP 服务器地址
```

### 9.2 DHCP 地址分配过程

需要结合抓包结果说明：

```text
DHCP Discover：客户端广播寻找 DHCP 服务器。
DHCP Offer：服务器提供可用地址和配置。
DHCP Request：客户端请求使用某个地址。
DHCP ACK：服务器确认租约生效。
DHCP Release：客户端释放已有地址。
```

### 9.3 需要记录的字段

| 报文类型 | 必填字段 |
|---|---|
| Release | Frame Number、Src IP、Dst IP、Transaction ID、Client MAC |
| Discover | Frame Number、Src IP、Dst IP、Transaction ID、Client MAC、Requested IP 如果存在 |
| Offer | Frame Number、Server IP、Your IP Address、Subnet Mask、Router、DNS、Lease Time |
| Request | Frame Number、Requested IP、Server Identifier、Client MAC |
| ACK | Frame Number、Your IP Address、Subnet Mask、Router、DNS、Lease Time |

---

## 10. DHCP 阶段验收标准

```text
[ ] captures/dhcp.pcapng 存在
[ ] 能看到 DHCP Release 或说明为什么没有 Release
[ ] 能看到 Discover、Offer、Request、ACK 至少四个核心报文
[ ] exports/dhcp_fields.csv 存在
[ ] DHCP Message Type 可以识别
[ ] 每个关键报文有 Frame Number
[ ] 至少 4 张关键截图已保存
[ ] 分析中没有虚构字段
```

---

## 11. 常见失败处理

### 11.1 抓不到 Discover/Offer/Request/ACK

处理：

1. 确认 Wireshark 捕获的是当前联网接口。
2. 确认以管理员权限执行 CMD。
3. 重新执行：

```bat
ipconfig /release
ipconfig /renew
```

4. 尝试不设置捕获过滤器，改为抓全量包，之后显示过滤：

```text
dhcp
```

或：

```text
udp.port == 67 || udp.port == 68
```

### 11.2 执行 release 后无法 renew

处理：

```bat
ipconfig /renew
ipconfig /flushdns
ipconfig /all
```

必要时断开并重新连接 Wi-Fi。

### 11.3 字段名不兼容

让 Codex 先运行：

```bat
tshark -G fields | findstr /i "bootp dhcp"
```

再选择可用字段，不能强行使用不存在字段。

---

## 12. 给 Codex 的提示语

```text
请执行阶段 2：DHCP 报文捕获与分析。

任务：
1. 检查 captures/dhcp.pcapng 是否已经存在。
2. 如果不存在，请指导我在 Wireshark 中使用捕获过滤器 udp port 67 抓包，并让我依次执行 ipconfig /release 和 ipconfig /renew。
3. 明确提醒我：ipconfig /release 会临时断网。
4. 抓包完成后，要求我保存为 captures/dhcp.pcapng。
5. 使用 tshark 检查 DHCP/BOOTP 字段名兼容性。
6. 导出 exports/dhcp_fields.csv。
7. 读取 CSV，识别 Release、Discover、Offer、Request、ACK。
8. 为每个关键报文整理 Frame Number、时间、源 IP、目的 IP、UDP 端口、Transaction ID、Client MAC、Your IP、Requested IP、Server Identifier、Subnet Mask、Router、DNS、Lease Time。
9. 生成 exports/dhcp_analysis.md，内容包括捕获过程、DHCP DORA 流程、关键字段表、结论、截图清单。
10. 如果缺少任一关键报文，请指出缺失内容，并给出重新抓包方案，不要编造数据。

输出：
- DHCP 报文识别结果。
- DHCP 字段表路径。
- DHCP 分析摘要路径。
- 需要我补充的截图清单。

禁止：
- 禁止虚构 DHCP Server、租约时间、分配 IP、Transaction ID。
- 禁止用理论值替代实际抓包值。
```
