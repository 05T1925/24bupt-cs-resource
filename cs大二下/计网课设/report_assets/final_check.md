# DNS 中继服务器课程设计报告终稿检查记录

## 输入文件确认

- 初稿：存在 - C:\Users\28641\Desktop\计网\计网课设\DNS中继服务器课程设计报告-初稿.docx
- 资料 README：存在 - C:\Users\28641\Desktop\计网\计网课设\readme.md
- 代码测试 README：存在 - C:\Users\28641\Desktop\计网\计网课设\code-test-readme.md
- 实验报告模板：存在 - C:\Users\28641\Desktop\计网\计网课设\资料\实验报告模板.docx
- 代码目录：存在 - C:\Users\28641\Desktop\计网\计网课设\dns-relay
- 域名-IP 对照表：存在 - C:\Users\28641\Desktop\计网\计网课设\资料\dnsrelay.txt

## 截图补充情况

| 截图名称 | 是否生成 | 是否插入 Word | 生成方式 | 未生成原因 |
|---|---|---|---|---|
| 01_startup_log.png | 是 | 是 | 终端输出转图片 | 无 |
| 02_local_ip_test.png | 是 | 是 | 终端输出转图片 | 无 |
| 03_block_test.png | 是 | 是 | 终端输出转图片 | 无 |
| 04_relay_test.png | 是 | 是 | 终端输出转图片 | 无 |
| 05_concurrency_test.png | 是 | 是 | 终端输出转图片 | 无 |
| 06_uppercase_failed_test.png | 是 | 是 | 终端输出转图片 | 无 |

## 流程图处理情况

| 流程图名称 | 是否渲染为图片 | 是否插入 Word | 说明 |
|---|---|---|---|
| 系统总体流程图 | 是 | 是 | 使用本地绘图脚本按流程定义生成 PNG，未使用 Mermaid 引擎。 |
| DNS 查询处理流程图 | 是 | 是 | 使用本地绘图脚本按流程定义生成 PNG，未使用 Mermaid 引擎。 |
| 本地响应构造流程图 | 是 | 是 | 使用本地绘图脚本按流程定义生成 PNG，未使用 Mermaid 引擎。 |
| 中继与 ID 转换流程图 | 是 | 是 | 使用本地绘图脚本按流程定义生成 PNG，未使用 Mermaid 引擎。 |
| 并发或连续查询处理流程图 | 是 | 是 | 使用本地绘图脚本按流程定义生成 PNG，未使用 Mermaid 引擎。 |

## 未自动完成或需人工确认

- 未生成原生终端窗口截图；本次使用真实日志转图片方式生成证据图。
- 未使用 Mermaid 引擎渲染流程图；本次使用本地绘图脚本生成等价流程图 PNG。
- 未执行畸形 DNS 报文、队列满、上游 DNS 超时、Wireshark 抓包测试。
- LibreOffice/soffice 未安装，无法执行 DOCX 渲染为页面 PNG 的视觉 QA。
