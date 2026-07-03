# Logistics V3 终版验收展示与问答手册

> 日期：2026-06-18  
> 建议展示时长：6-8 分钟  
> 原则：先证明基础要求，再展示业务闭环，最后集中展示创新得分点。

## 一、验收前准备

### 1. 进入工程并编译

```bat
cd /d "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

预期看到：

```text
[Build] server build success: bin\logistics_v3_server.exe
[Build] client build success: bin\logistics_v3_client.exe
[Build] all targets built successfully.
```

开场话术：

> 项目使用 C++17 和 Winsock，实现了两个独立可执行文件。客户端负责交互，服务端负责身份、业务和持久化，二者只通过 TCP Socket 通信。

### 2. 准备三个终端

```text
终端 1：持续运行服务端
终端 2：运行交互客户端或自动自测
终端 3：备用客户端，用于证明多客户端或重新登录
```

若端口被占用：

```bat
netstat -ano | findstr :9000
```

## 二、推荐现场流程

## 第 1 步：证明传统 C/S 双进程

终端 1：

```bat
bin\logistics_v3_server.exe
```

终端 2：

```bat
bin\logistics_v3_client.exe
```

可在终端 3 再启动一个客户端：

```bat
bin\logistics_v3_client.exe
```

展示内容：

- 服务端监听 `127.0.0.1:9000`。
- 两个客户端能同时连接。
- `bin` 中存在两个独立 EXE。
- 客户端源码不直接操作 `data/server`。

话术：

> 这是传统 C/S 双进程结构。`SocketServer` 负责监听和接收连接，`SocketClient` 主动连接。客户端不能直接访问业务仓库，所有操作必须发到服务端。

基础要求得分点：

- 独立 server/client。
- Socket 通信。
- 非 HTTP、非 RPC 框架。
- 客户端和服务端职责分离。

## 第 2 步：快速证明用户基础业务

保持终端 1 的服务端运行，终端 2 执行：

```bat
bin\logistics_v3_client.exe --selftest-user
```

重点观察：

```text
register sender: OK
register receiver: OK
recharge 100: OK
send fragile 2kg: OK
malicious sign: ERR|PERMISSION_DENIED
```

关键断言：

- 易碎品 `2 kg` 由服务端算出 `16.00`。
- 余额由 `100.00` 变为 `84.00`。
- 发件人不能冒充收件人签收。

话术：

> 客户端只提交物品类型和数量，运费由服务端的多态计费模型计算。当前用户身份也不是客户端传用户名决定，而是由 Token 对应的 Session 决定。

## 第 3 步：证明管理员权限和调度能力

终端 2：

```bat
bin\logistics_v3_client.exe --selftest-admin
```

重点观察：

```text
user token calls admin dashboard: ERR|PERMISSION_DENIED
query all express: OK
remove courier conflict: ERR|COURIER_HAS_UNFINISHED_TASKS
auto assign all: OK
view dashboard: OK
```

话术：

> 自测先故意使用普通用户 Token 调管理员看板，服务端按 Session 角色拒绝，并记录越权审计。管理员还可以自动分配、查看看板和绩效；停用有未完成任务的快递员时会返回冲突任务清单。

创新得分点：

- Token 零信任鉴权。
- 越权安全审计。
- 自动分配算法。
- 停用冲突清单。
- 管理看板与绩效统计。

## 第 4 步：证明揽收、资金和状态一致性

终端 2：

```bat
bin\logistics_v3_client.exe --selftest-courier
```

重点观察：

```text
courier A pickup courier B task: ERR|PERMISSION_DENIED
pickup batch own tasks: OK
repeat pickup after batch: ERR|STATE_CONFLICT
Phase 7 courier network business passed
```

关键结果：

- 快递员 A 无权揽收 B 的任务。
- 两单批量揽收成功。
- 重复揽收被状态机拒绝。
- 收入增加 `18.00`。

资金解释：

```text
寄件：用户余额 -= fee，平台余额 += fee
揽收：平台余额 -= fee * 0.5，快递员收入 += fee * 0.5
```

话术：

> 揽收同时涉及订单状态、平台余额和快递员收入。三项变化在 `CoreMutex` 保护的同一临界区中完成，保存失败时恢复对象快照。

## 第 5 步：展示最高优先级创新点，并发抢单

服务端必须使用默认持续监听模式。终端 2：

```bat
bin\logistics_v3_client.exe --selftest-concurrency
```

预期结果顺序可能互换，但必须满足：

```text
一个 OK|SUCCESS
一个 ERR|STATE_CONFLICT
Phase 8 concurrent pickup conflict passed
```

话术：

> 自测创建两个独立 Socket 连接，并用启动标志让两个线程几乎同时揽收同一订单。网络线程可以并发，但核心状态检查和修改由 `CoreMutex` 串行化，因此只支付一次提成、只发生一次状态变化。

这是最值得主动展示的得分点。

## 第 6 步：展示网络可靠性

启动交互客户端，登录后直接关闭客户端窗口。观察服务端：

```text
client disconnected
cleaned 1 session(s)
active sessions: ...
```

话术：

> `recv` 返回 0 代表对端正常关闭，返回 `SOCKET_ERROR` 代表异常。两种路径都会清理该连接关联的 Session，不影响服务端和其他客户端。

随后可重新启动客户端并登录，证明残留会话已清理。

## 第 7 步：展示日志哈希链

交互客户端中登录管理员：

```text
主菜单：2 登录
身份：3 管理员
管理员菜单：14 校验日志哈希链
```

预期：

```text
OK|SUCCESS|日志哈希链完整。
```

话术：

> `operations.log` 每条记录包含上一条哈希。校验时同时检查序号、`prevHash` 和重算的 `currentHash`，篡改或删除历史记录都能被发现。

## 三、备用一键演示

不熟悉手工菜单时，优先使用：

```bat
demo_launcher.bat
```

建议选择：

```text
2. 按步交互讲解模式
```

它会依次运行用户、管理员、快递员和并发自测，每一步暂停，适合边运行边讲解。

若时间非常紧，可选择：

```text
1. 一键全自动演示模式
```

## 四、代码展示顺序

现场不要随机翻文件，按这条链路展示：

1. `server/main.cpp`：对象组装和服务启动。
2. `client/SocketClient.cpp`：`connect/send/recv`。
3. `server/SocketServer.cpp`：`bind/listen/accept`、工作线程、分帧。
4. `common/protocol/ProtocolCodec.h/.cpp`：REQ/RES、转义、长度限制。
5. `server/SessionManager.cpp`：Token 到 Session 的映射。
6. `server/ServerController.cpp`：命令路由和角色鉴权。
7. `common/service/LogisticsSystem.cpp`：业务、资金、状态机和锁。
8. `common/storage/StorageManager.cpp`：原子保存。
9. `common/storage/Logger.cpp`：日志哈希链。

一句话调用链：

```text
ClientApp -> SocketClient -> TCP -> SocketServer
-> ProtocolCodec -> ServerController -> SessionManager
-> LogisticsSystem -> Repository/Logger -> Response 原路返回
```

## 五、助教高频问题与答案

### 1. 为什么说这是 C/S，而不是菜单拆成两个程序？

因为 server 和 client 是两个独立进程、两个 EXE。客户端通过 TCP 连接服务端，不能直接读取 `data/server`，也不能直接调用 `LogisticsSystem`。

### 2. `socket/bind/listen/accept` 分别做什么？

- `socket`：创建网络句柄。
- `bind`：把服务端 Socket 绑定到本地 IP 和端口。
- `listen`：把它变成监听 Socket。
- `accept`：取出一个客户端连接并返回新的通信 Socket。

监听 Socket 继续接收新连接，通信 Socket 才用于当前客户端的 `send/recv`。

### 3. 为什么不能认为一次 `recv` 


TCP 是字节流，不保留消息边界，可能发生半包和粘包。代码把收到的字节放入缓冲区，再按 `\n` 提取完整协议帧。

### 4. `sendAll` 为什么要循环？

一次 `send` 可能只写入部分字节。代码根据返回值累计已发送长度，直到完整帧全部发送。

### 5. 为什么使用自定义协议？

课程要求使用 Socket 且不使用 RPC 框架。自定义文本协议便于展示和调试，同时实现了字段转义、数量校验和长度限制。

### 6. 字段中有 `|` 或换行怎么办？

编码时转换为 `%7C`、`%0A` 等序列，接收后还原。先按原始分隔符切分，再做反转义，避免字段内容破坏帧结构。

### 7. Token 和 TCP 连接有什么区别？

TCP 解决字节传输通道，Token 解决应用层身份。每个需要鉴权的请求都显式带 Token，服务端从 Session 中恢复用户名和角色。

### 8. 如何防止客户端伪造用户名？

操作者身份不取自业务参数，而取自 Token 对应的 `Session.username`。客户端参数只能表示收件人、目标用户等业务对象。

### 9. 为什么权限要校验两层？

`ServerController` 判断请求者角色；`LogisticsSystem` 判断具体订单是否属于该用户、状态是否允许、余额是否充足。

### 10. 如何防止并发重复揽收？

`pickupExpress` 在同一把 `CoreMutex` 中执行状态检查和状态修改。第一个请求完成后，第二个请求看到订单不再是待揽收，返回 `STATE_CONFLICT`。

### 11. 为什么使用两把锁？

- `CoreMutex` 保护业务集合和资金状态。
- `SessionMutex` 保护 Token 会话表。

分开可以避免会话查询和业务修改不必要地共用同一锁。

### 12. 自旋锁有什么优缺点？

优点是实现简单、兼容当前 MinGW 环境、短临界区延迟低。缺点是竞争时会消耗 CPU，不适合高并发或长临界区。该项目是课程级低并发服务端，因此采用保守方案。

### 13. 保存失败如何回滚？

修改前复制旧对象，修改后调用 `saveAll`。若失败则恢复内存快照。单个文件使用 `.tmp -> .bak -> 正式文件` 替换。

### 14. 这是真正的数据库事务吗？

不是。单个文件具备原子替换保护，但多个文件间不是数据库级事务。内存可以回滚，已经成功替换的其他文件不能自动跨文件回滚。这是当前实现的明确边界。

### 15. 为什么通知失败不回滚核心业务？

通知是附加能力，采用 best-effort。不能因为通知文件写入失败，让已经完成的揽收或签收业务对用户显示失败。

### 16. 日志哈希链如何发现篡改？

`currentHash` 包含本条内容和上一条 `prevHash`。修改一条会使该条摘要不匹配，并破坏后续链；删除一条还会造成序号断裂。

### 17. 密码如何保存？

实体只保存盐值和 SHA-256 摘要，不保存明文。登录时用保存的盐重新计算摘要并比较。

谨慎补充：

> 当前是课程项目的摘要方案；生产系统应使用 bcrypt、scrypt 或 Argon2 等专用慢哈希。

### 18. 自动分配算法是什么？

过滤冻结或停用快递员后，依次比较：

1. 未完成任务更少。
2. 收入更低。
3. 用户名词典序更小。

这样兼顾负载均衡、收入公平和结果确定性。

### 19. 状态机是什么？

```text
WaitingPickup -> WaitingSign -> Signed
```

状态只能向前。分配不改变状态，揽收进入待签收，收件人签收进入已签收。

### 20. 为什么已揽收订单不允许改派？

揽收时已经发生平台向快递员支付提成。若再改派，需要额外设计资金冲正和审计规则，当前版本为了保证一致性明确拒绝。

### 21. 为什么批量操作不是一次总事务？

批量揽收和签收按订单逐笔调用单项事务，每笔独立成功或失败，并返回逐单结果。这样某一条错误不会阻塞其他合法订单。

### 22. 如何处理重复登录？

登录业务验证成功后，`SessionManager::hasActiveSession` 检查相同用户名和角色是否已有会话，有则返回 `ALREADY_LOGGED_IN`。

### 23. 服务端异常会不会直接崩溃？

连接线程捕获协议异常和标准异常，并尽量转换成错误响应。单个客户端断线或请求失败不会终止整个服务端。

### 24. 为什么没有独立资金流水文件？

当前余额和收入保存在实体文件，资金相关操作写入 `operations.log` 供审计。项目没有虚构独立 `FinanceLog`；若继续扩展，可增加专门的不可变资金流水实体。

## 六、基础要求之外的创新亮点

按推荐展示优先级排列：

### S 级：务必主动展示

1. **双客户端并发抢同一单**  
   一个成功、一个状态冲突，且提成只支付一次。

2. **Token 零信任权限隔离**  
   USER Token 调管理员命令被拒绝并留下审计记录。

3. **TCP 半包、粘包处理**  
   客户端和服务端都有持久接收缓冲区，不把一次 `recv` 当一条消息。

### A 级：时间允许必须讲

4. **揽收资金与状态逻辑事务**  
   状态、平台余额、快递员收入在同一临界区修改并有快照回滚。

5. **日志 SHA-256 哈希链**  
   支持管理员在线校验日志完整性。

6. **三阶段原子文件保存**  
   `.tmp/.bak/正式文件`降低半写入风险。

7. **多线程服务端与断线会话清理**  
   主线程持续 `accept`，每个连接单独处理。

### B 级：作为业务完整性加分

8. 多态服务端计费。
9. 智能自动分配。
10. 批量揽收、批量签收及逐单结果。
11. 通知中心与未读计数。
12. 快递改派及新旧快递员双向通知。
13. 快递评分与快递员绩效。
14. 余额不足后即时充值并重试。
15. 登录失败三次自动冻结普通用户和快递员。

## 七、不要声称已经实现的内容

以下能力当前不应作为已完成亮点：

- Token 自动过期。
- 定时心跳保活。
- 自动断线重连。
- 数据库级跨文件事务。
- TLS 加密通信。
- 查询分页。
- 独立资金流水实体。
- 生产级密码哈希。

可回答：

> 这些属于下一步工程化方向，当前版本优先保证课程要求下的 C/S 架构、权限、并发和业务一致性。

## 八、现场展示优先级

若只有 3 分钟：

```text
双进程 -> USER 越权拦截 -> 并发抢单 -> 哈希链
```

若有 5 分钟：

```text
双进程 -> 用户自测 -> 管理员越权 -> 快递员资金闭环
-> 并发抢单 -> 哈希链
```

若有 8 分钟：

```text
增加人工业务闭环、通知中心、自动分配和断线清理
```

## 九、30 秒总结话术

> 我的项目首先满足了传统 C/S、独立双进程和 Socket 通信等基础要求。在此基础上，我实现了自定义 REQ/RES 协议及半包粘包处理、Token 零信任鉴权、多线程连接处理和并发状态保护。业务上保留了多态计费、自动分配、资金分账、通知、评分和绩效；可靠性上增加了对象快照回滚、单文件原子替换和日志哈希链。最核心的演示是双客户端同时揽收同一单时只成功一次，证明网络并发下状态和资金不会重复变化。
