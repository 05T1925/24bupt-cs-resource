# 实验二最终检查清单

- 检查日期：2026-06-20
- 学生：刘文涛
- 班级：304
- 学号：2024212936
- 学院：计算机学院（国家示范性软件学院）
- 报告封面日期：2026 年 6 月 19 日

## 最终 Word

| 项目 | 状态 |
|---|---|
| DOCX 已生成且结构可正常解析 | PASS |
| 正文段落 138 个、表格 13 张、插图 13 幅 | PASS |
| 封面身份信息与实验日期完整 | PASS |
| DHCP、ICMP、IPv4 分片、TCP 四部分齐全 | PASS |
| 实验目的、内容、环境、方法、问题解决、心得、附录齐全 | PASS |
| 无 TODO、待填写、示例值或提示语残留 | PASS |
| TCP 握手与连接释放小节已整页起排 | PASS |
| 首轮 17 页 PDF 已逐页视觉检查 | PASS |

## 协议证据

| 协议 | pcap/CSV | 字段分析 | Frame Number | 截图 | 结果 |
|---|---|---|---|---|---|
| DHCP | 完整 | Release + DORA | 1-5 | 已插入 | PASS |
| ICMP | 完整 | 4 组 Echo Request/Reply | 1108-1187 | 已插入 | PASS |
| IPv4 分片 | 完整 | 请求/应答各 6 片 | 29-40 | 已插入 | PASS |
| TCP | 完整 | stream 2 握手与 FIN 释放 | 16-27 | 已插入 | PASS |

## 文件与哈希

- 最终 DOCX：`report/计网实验2报告-304-2024212936-刘文涛.docx`
- Markdown 源稿：`report/计网实验2报告-304-2024212936-刘文涛.md`
- DOCX SHA-256：`408A36C0E2403AC350DD84D58CE22A0EA6627FDDB54B403B34F65D463C5E9FBA`
- DHCP SHA-256：`B07B87FB47930FC3A4D436D0E9EFF1F65102F6F94AEAC60123F0B24A42B6D1BC`
- ICMP SHA-256：`E6FCEB96046289F85DD4461E0E2E952FD135385BB5DF84C3C2401503EF4A41AB`
- IPv4 分片 SHA-256：`B9C14AB424792FB14C75DAA8144EB708862A4CED1138D48AA379B0A37E5739F3`
- TCP SHA-256：`2046914206267F90A8CCDC8E7EF78BA78D2CE24AB2BD4FE94B24697B21B4E88F`

说明：分页优化前的 PDF 已归档到 `report/archive/`。由于第二轮本地渲染授权被系统额度限制拒绝，最终提交主文件为上述 DOCX。
