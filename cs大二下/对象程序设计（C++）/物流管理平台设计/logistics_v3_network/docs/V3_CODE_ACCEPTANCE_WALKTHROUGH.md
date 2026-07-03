# Logistics V3 代码验收讲解速查

## 1. 总体架构

项目采用 C/S 分层结构，核心依赖方向如下：

```text
client/main.cpp
  -> client/SocketClient
  -> common/protocol/ProtocolCodec
  -> TCP
  -> server/SocketServer
  -> server/ServerController
  -> server/SessionManager
  -> common/service/LogisticsSystem
  -> common/models + common/security
  -> common/storage/Repositories + Logger
  -> data/server/*.txt
```

设计原则是：界面不直接操作数据文件，网络层不直接实现业务规则，业务层不直接处理 Socket。

## 2. 一次请求如何执行

以快递员揽收为例：

1. `client/main.cpp` 从菜单读取快递单号。
2. `ClientApp::sendRequest` 生成 `Request`，放入命令、Token 和参数。
3. `SocketClient::sendCommand` 调用 `ProtocolCodec::encodeRequest`。
4. TCP 把字节发送到服务端。
5. `SocketServer::handleClient` 从流中按换行提取完整帧。
6. `ProtocolCodec::decodeRequest` 恢复 `Request` 并检查长度、转义和字段数。
7. `ServerController::handle` 把命令交给快递员路由。
8. `requireCourierSession` 用 `SessionManager` 校验 Token 和角色。
9. `LogisticsSystem::pickupExpress` 检查订单归属和状态，完成提成、状态、通知和保存。
10. `ServerController` 把 `ServiceResult` 转成 `Response`。
11. 服务端编码响应并完整发送。
12. 客户端解码后显示结果。

## 3. 各目录作用

### `client/`

- `main.cpp`：控制台界面、角色菜单、输入采集、结果展示和四组自测。
- `SocketClient.h/.cpp`：封装 Winsock 客户端，只处理连接和请求响应帧。

### `server/`

- `main.cpp`：服务端组合根，按顺序创建业务、会话、路由和网络对象。
- `SocketServer.h/.cpp`：监听端口、多连接线程、TCP 半包粘包和断线清理。
- `ServerController.h/.cpp`：命令路由、参数数量检查、角色鉴权和返回值转换。
- `SessionManager.h/.cpp`：Token 会话表、重复登录拦截、活跃时间和会话销毁。

### `common/protocol/`

- `ProtocolCodec.h/.cpp`：定义 `Request`、`Response` 和文本协议编解码。
- 协议层只检查帧是否合法，不判断用户余额、订单归属等业务条件。

### `common/service/`

- `ServiceResult.h`：业务层统一返回结构。
- `LogisticsSystem.h/.cpp`：核心业务入口，管理用户、快递员、订单、资金、通知和日志。
- 所有关键共享业务数据由 `CoreMutex` 保护。

### `common/models/`

- `Entities.h/.cpp`：持久化实体及状态变化方法。
- `ExpressItem.h/.cpp`：多态计费模型，普通件、易碎品、图书各自实现价格公式。

### `common/security/`

- `InputValidator`：用户名、手机号、密码、姓名、地址和正数规则。
- `PasswordHasher`：组织盐值和密码摘要输入。
- `HashUtil`：SHA-256 基础实现。
- `StringUtil`：数据字段转义、拆分、金额格式化和严格数值解析。

### `common/storage/`

- `StorageManager`：文本行读取和 `.tmp/.bak` 原子替换。
- `Repositories`：实体对象与数据文件之间的转换。
- `Logger`：追加式九字段哈希链审计日志。

## 4. 三种关键数据结构

### `Request/Response`

只用于网络层传输。`Request` 包含命令、Token 和参数；`Response` 包含成功标记、结果码、消息和记录。

### `ServiceResult`

只用于业务层返回。`ServerController::fromServiceResult` 把它映射为网络 `Response`，使业务层不依赖网络类型。

### Entity

`User/Admin/Courier/Express/AuthState/Notification` 是内存业务实体，通过 Repository 持久化。

## 5. 两把锁为什么分开

- `CoreMutex`：保护用户、快递员、快递、余额、认证状态和通知等业务集合。
- `SessionMutex`：只保护 Token 到 Session 的映射。

分开后，查询会话不会直接占用业务锁；同时两类数据的职责更清晰。

## 6. TCP 半包和粘包

TCP 是字节流，没有“一次发送对应一次接收”的保证。

- 半包：一次 `recv` 只收到一帧的一部分。
- 粘包：一次 `recv` 同时收到多帧。

客户端和服务端都维护字符串缓冲区，以 `\n` 作为帧结束标志。没有完整帧就继续接收；有多帧就循环逐帧处理，多余部分留给下一次。

## 7. 业务状态机

```text
WaitingPickup -> WaitingSign -> Signed
待揽收          待签收         已签收
```

- 分配快递员不改变订单状态。
- 揽收时状态从待揽收变为待签收，并支付快递员提成。
- 只有收件人能把待签收订单变为已签收。
- 状态只向前变化，重复揽收和重复签收会返回 `STATE_CONFLICT`。

## 8. 资金流向

寄件：

```text
用户余额 -= 运费
平台余额 += 运费
```

揽收：

```text
平台余额 -= 运费的 50%
快递员收入 += 运费的 50%
```

资金变化、订单状态变化和对象保存位于同一业务临界区。保存失败时恢复内存快照。

## 9. 持久化与“原子”的含义

单个数据文件保存流程：

1. 新内容写入 `.tmp`。
2. 原文件移动为 `.bak`。
3. `.tmp` 重命名为正式文件。
4. 第三步失败时尝试恢复 `.bak`。

这保证单个文件尽量不会出现半写入，但多个数据文件之间不是数据库事务。`saveAll` 失败时会恢复调用方的内存对象，已经成功替换的其他文件不能自动跨文件回滚。

## 10. 日志哈希链

每条日志包含：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

`currentHash` 由本条前八个逻辑字段计算，并包含 `prevHash`。修改历史记录会导致本条或后续记录校验失败。

## 11. 权限为什么校验两次

第一层在 `ServerController`：

- Token 是否存在。
- Session 是否有效。
- Session 角色是否匹配命令。

第二层在 `LogisticsSystem`：

- 订单是否属于当前用户或快递员。
- 订单状态是否允许操作。
- 账号是否冻结、快递员是否停用。
- 余额是否充足。

路由鉴权解决“你是什么身份”，业务校验解决“这个具体对象是否允许你操作”。

## 12. 常见验收问题

**为什么客户端不能自己计算运费？**  
客户端数据不可信。服务端通过 `ExpressItemFactory` 创建多态物品对象并计算费用，可防止篡改运费。

**为什么使用 Repository？**  
隔离业务对象和文件格式。`LogisticsSystem` 不需要知道文件如何打开、字段如何拼接。

**为什么有 `ServiceResult` 和 `Response` 两套结构？**  
前者属于业务层，后者属于协议层。分离后业务核心可以脱离 Socket 单独测试和复用。

**如何防止两个快递员同时揽收一单？**  
`pickupExpress` 在 `CoreMutex` 内先检查状态再修改。第一个线程完成后，第二个线程看到状态已改变并返回冲突。

**通知保存失败会怎样？**  
通知是附加能力，采用 best-effort。通知失败会记录日志，但不撤销已经成功的核心物流业务。

**停用和冻结有什么区别？**  
冻结可由管理员恢复；停用使用 `removed` 标记并保留历史数据。存在未完成任务时不允许停用。

**为什么日志不等于资金流水表？**  
当前项目没有独立 `FinanceLog` 实体。余额和收入是业务事实，`operations.log` 是操作审计证据。

**为什么登录后还要每次带 Token？**  
TCP 连接本身不代表业务身份。每次请求携带 Token，服务端才能恢复角色和用户名并执行零信任校验。

**为什么断线时清理 Token？**  
防止会话残留导致账号被误判为重复登录，也减少失效 Token 的存活时间。
