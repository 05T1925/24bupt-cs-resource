# 阶段 1：环境检查与 Wireshark / tshark 配置

> 目标：确认 Wireshark、Npcap、tshark 可用，确定活跃网卡，记录本机网络环境。  
> 输入：阶段 0 创建的项目目录。  
> 输出：环境信息文件、网卡列表、截图清单。

---

## 1. 前置要求

确保已经安装：

- Wireshark。
- Npcap。
- tshark 命令行工具。

Windows 安装 Wireshark 时建议勾选：

```text
Install Npcap
Install command line tools / tshark
```

---

## 2. 进入项目目录

PowerShell 或 CMD：

```bat
cd Lab2-PacketAnalysis
```

---

## 3. 检查 Wireshark 版本

```bat
wireshark -v
```

如果提示找不到命令，可尝试：

```bat
"C:\Program Files\Wireshark\Wireshark.exe" -v
```

---

## 4. 检查 tshark 版本

```bat
tshark -v
```

如果提示找不到命令，可尝试：

```bat
"C:\Program Files\Wireshark\tshark.exe" -v
```

如仍不可用，请把 Wireshark 安装目录加入 PATH，或让 Codex 后续脚本使用完整路径。

---

## 5. 导出网卡列表

```bat
tshark -D > exports\tshark_interfaces.txt
```

如果 `tshark` 不在 PATH：

```bat
"C:\Program Files\Wireshark\tshark.exe" -D > exports\tshark_interfaces.txt
```

打开文件：

```bat
notepad exports\tshark_interfaces.txt
```

确认活跃接口，例如：

```text
1. \Device\NPF_{...} (WLAN)
2. \Device\NPF_{...} (Ethernet)
```

记录正在联网的接口名称和编号。

---

## 6. 导出 IP 配置信息

```bat
ipconfig /all > exports\ipconfig_before.txt
```

打开检查：

```bat
notepad exports\ipconfig_before.txt
```

记录：

```text
IPv4 地址
子网掩码
默认网关
DNS 服务器
物理地址 MAC
DHCP 是否启用
DHCP 服务器
租约获得时间
租约过期时间
```

---

## 7. 截图要求

保存到 `screenshots/00_environment/`：

```text
01_wireshark_version.png
02_tshark_interfaces.png
03_ipconfig_all.png
04_wireshark_interface_selection.png
```

截图应能证明：

- Wireshark 已安装。
- tshark 可用。
- 当前联网接口已经确认。
- 本机网络参数已经记录。

---

## 8. Wireshark 手动确认步骤

1. 打开 Wireshark。
2. 在欢迎界面查看所有网络接口。
3. 观察哪个接口有明显流量波形。
4. 选中该接口，但先不要开始正式抓包。
5. 截图保存为：

```text
screenshots/00_environment/04_wireshark_interface_selection.png
```

---

## 9. 常见问题处理

### 9.1 tshark 找不到

处理方式：

```bat
where wireshark
where tshark
```

若找不到，确认安装路径：

```text
C:\Program Files\Wireshark\
```

后续命令使用完整路径：

```bat
"C:\Program Files\Wireshark\tshark.exe"
```

### 9.2 没有抓包权限

处理方式：

- Windows 重新安装 Npcap。
- 勾选支持普通用户抓包。
- 或以管理员身份运行 Wireshark。

### 9.3 网卡太多无法判断

处理方式：

1. 打开 Wireshark 欢迎页。
2. 观察接口右侧实时流量图。
3. 暂时断开/连接网络，看哪个接口变化。
4. 优先选择 WLAN 或 Ethernet 中有流量的接口。

---

## 10. 给 Codex 的提示语

```text
请执行阶段 1：环境检查与 Wireshark/tshark 配置。

任务：
1. 检查当前目录是否为 Lab2-PacketAnalysis。
2. 检查 wireshark -v 和 tshark -v 是否可用。
3. 如果 tshark 不在 PATH，请提示我使用 "C:\Program Files\Wireshark\tshark.exe" 完整路径。
4. 执行或指导我执行 tshark -D，将输出保存到 exports/tshark_interfaces.txt。
5. 执行或指导我执行 ipconfig /all，将输出保存到 exports/ipconfig_before.txt。
6. 读取这两个文件，提取网络接口、IPv4 地址、默认网关、DNS、MAC 地址、DHCP 状态。
7. 更新 exports/experiment_metadata.md 中的实验环境部分。
8. 给出应该选择哪个 Wireshark 接口的建议，但不要替我伪造结论。
9. 生成本阶段截图清单，要求保存到 screenshots/00_environment/。

输出：
- 环境检查结果。
- 推荐抓包接口名称或编号。
- 缺失项列表。
- 下一阶段 DHCP 抓包前需要确认的事项。

禁止：
- 不要虚构本机 IP、网关、DNS 或接口名称。
- 不要开始正式抓包。
```
