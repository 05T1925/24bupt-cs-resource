# v3_LOG00：Logistics V3 Network 启动规划日志

## 1. 已阅读材料

本次 V3 规划基于以下材料：

- `实验(综合-物流管理平台设计)-面向对象程序设计与实践-2026春 发布版v3.docx`
- `logistics_v2/README.md`
- `logistics_v2/docs/V2_REQUIREMENTS_ANALYSIS.md`
- `logistics_v2/docs/V2_ARCHITECTURE_PLAN.md`
- `logistics_v2/docs/V2_DEVELOPMENT_ROADMAP.md`
- `logistics_v2/docs/V2_ACCEPTANCE_AND_BONUS.md`
- `logistics_v2/tests/V2_TEST_CASES.md`
- `logistics_v2/LOG/v2_LOG00.md`
- `logistics_v2/LOG/v2_LOG01.md`
- `logistics_v2/LOG/v2_LOG02.md`
- `logistics_v2/LOG/v2_LOG_SUM.md`
- `logistics_v2/src/main.cpp`

实验要求确认：V3 必须是独立网络版程序，采用传统 C/S 架构，客户端和服务器端为不同进程，使用 socket 通信，不能使用 RPC 框架。V3 的功能要求与单机版一致。

## 2. V2 当前可继承成果

V2 已经完成题目二要求，并具备多项加分能力：

- 三身份权限隔离：用户、快递员、管理员。
- 快递员管理：新增、冻结、停用、冲突清单。
- 三类快递：普通、易碎品、图书。
- `ExpressItem::getPrice()` 多态计费。
- 三状态：待揽收、待签收、已签收。
- 管理员手动分配和自动分配快递员。
- 快递员揽收与 50% 提成。
- 快递员任务查询与个人绩效。
- 管理员统计看板。
- 密码 salt + SHA-256。
- 密码隐藏输入。
- 登录失败三次冻结。
- 原子化保存。
- 日志哈希链。
- 保存失败回滚。

V3 不应推倒重来，应尽量把这些能力迁移到 server 端业务核心。

## 3. V3 架构主线

```text
common:
  保存 V2 可复用的实体、仓储、业务、安全、协议代码。

server:
  socket 监听、协议解析、Session 鉴权、权限路由、业务调用。

client:
  控制台菜单、隐藏密码输入、请求发送、响应展示。
```

第一目标是跑通最小闭环：

```text
client 连接 server
登录获取 token
用户寄件
管理员分配
快递员揽收
收件用户签收
管理员查看统计
```

## 4. V3 必须新增的核心模块

- `ProtocolCodec`
- `Request`
- `Response`
- `SocketServer`
- `SocketClient`
- `SessionManager`
- `ServerController`
- `ClientApp`
- `ConsoleClientUI`

## 5. 服务端红线

服务端不能保留：

- `ConsoleUI`
- `App`
- `std::cin`
- 菜单交互
- 密码隐藏输入
- 表格打印

服务端只能处理请求和返回响应。所有展示逻辑属于客户端。

## 6. 安全红线

- 服务端不能信任客户端传来的当前用户名。
- 登录成功后必须由 token 绑定身份。
- 修改型业务必须加锁。
- 明文密码不能入日志。
- 完整 token 不能入日志。
- 客户端不能直接操作数据文件。

## 7. 加分方向

V3 在满足基础要求后，建议重点做：

1. 文本 socket 协议设计清晰。
2. 换行分帧，正确处理半包和粘包。
3. token Session 安全模型。
4. 多客户端并发冲突处理。
5. 继承 V2 的自动分配、绩效、统计、日志哈希链能力。
6. 网络操作日志。
7. 完整测试清单和固定验收脚本。

## 8. 下一步开发建议

优先顺序：

```text
1. 从 V2 拆 common 业务核心。
2. 写 ProtocolCodec 单元式测试。
3. 写 server/client 的 PING 最小 socket 闭环。
4. 接入登录和 Session。
5. 接入用户寄件。
6. 接入管理员分配。
7. 接入快递员揽收。
8. 接入用户签收。
9. 补并发、日志、统计和演示脚本。
```

本阶段已生成 V3 规划文档，后续可按路线图进入编码实现。

