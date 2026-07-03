# v3_LOG_SUM01：Logistics V3 Network 阶段性全局状态序列化

## 0. 文档定位

本文件是 `logistics_v3_network` 在 Phase 0-6 后的高浓度状态快照，用作后续新对话框继续开发、验收、修复和报告撰写的唯一初始上下文。

当前工程路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network
```

当前阶段事实：

```text
Phase 0：独立工程初始化，已完成。
Phase 1：V2 业务核心迁移到 common，已完成。
Phase 2：自定义纯文本协议编解码，已完成。
Phase 3：Winsock PING/PONG 双进程闭环，已完成。
Phase 4：登录与 Session token 会话管理，已完成。
Phase 5：普通用户网络业务，已完成。
Phase 6：管理员网络业务，已完成。
Phase 7：快递员网络业务在源码中已有初步落点，下一阶段必须重点验收、加固和补并发资金一致性测试。
```

构建入口：

```text
build_all.bat
build_server.bat
build_client.bat
run_server.bat
run_client.bat
```

可执行文件：

```text
bin/logistics_v3_server.exe
bin/logistics_v3_client.exe
```

默认监听：

```text
127.0.0.1:9000
```

服务端支持：

```text
bin\logistics_v3_server.exe
bin\logistics_v3_server.exe --once
```

其中 `--once` 是单连接自动联调模式，接收一个客户端后退出；默认模式阻塞监听。

---

## 1. V3 C/S 架构基线与解耦状态

### 1.1 总体架构

V3 已经从 V2 单机控制台程序转型为传统 C/S 架构：

```text
client 进程
  -> Socket TCP
  -> 自定义文本协议 ProtocolCodec
  -> server 进程
  -> ServerController / SessionManager
  -> common 业务核心 LogisticsSystem
  -> data 文本持久化
```

server 与 client 是两个独立 exe：

```text
logistics_v3_server.exe
logistics_v3_client.exe
```

通信使用 Winsock TCP，不使用 RPC 框架，不使用 HTTP/B/S，不使用第三方网络库。

### 1.2 目录职责边界

```text
common/
  models/
    Entities.h/.cpp
    ExpressItem.h/.cpp
  protocol/
    ProtocolCodec.h/.cpp
  security/
    HashUtil.h/.cpp
    InputValidator.h/.cpp
    PasswordHasher.h/.cpp
    StringUtil.h/.cpp
  service/
    LogisticsSystem.h/.cpp
    ServiceResult.h
  storage/
    Logger.h/.cpp
    Repositories.h/.cpp
    StorageManager.h/.cpp

server/
  main.cpp
  SocketServer.h/.cpp
  ServerController.h/.cpp
  SessionManager.h/.cpp

client/
  main.cpp
  SocketClient.h/.cpp

data/
  server/
    users.txt
    admin.txt
    couriers.txt
    expresses.txt
    auth_state.txt
    operations.log

LOG/
  v3_LOG00.md ... v3_LOG07.md
```

职责：

```text
common：
  纯业务模型、仓储、安全工具、协议编解码、业务服务。
  不能有 UI、菜单、交互式输入输出。

server：
  Winsock 监听、连接收发、Session token、权限路由、业务调用。
  可以输出少量运维日志，如监听、连接、断开。

client：
  交互式菜单、隐藏密码输入、请求组包、响应展示、表格渲染。
  不能直接读写 data，不可绕过 server 调用 LogisticsSystem。
```

### 1.3 绝对红线声明

`common/` 已完成 UI 解耦，经过多轮扫描：

```text
common/**/*.h
common/**/*.cpp

无 #include <iostream>
无 std::cin
无 std::cout
无 ConsoleUI
```

业务核心的所有输入均来自函数参数，所有输出均通过返回值传递。当前统一服务返回结构为：

```cpp
struct ServiceResult {
    bool ok;
    std::string code;
    std::string message;
    std::vector<std::string> data;
};
```

后续任何新增 common 代码都必须继续遵守：

```text
common 不出现控制台菜单。
common 不读取用户输入。
common 不打印业务输出。
common 不依赖 client/server UI 细节。
```

### 1.4 当前构建策略

`build_server.bat` 编译：

```text
server/main.cpp
server/SocketServer.cpp
server/SessionManager.cpp
server/ServerController.cpp
common/models/Entities.cpp
common/models/ExpressItem.cpp
common/protocol/ProtocolCodec.cpp
common/security/StringUtil.cpp
common/security/HashUtil.cpp
common/security/PasswordHasher.cpp
common/security/InputValidator.cpp
common/storage/StorageManager.cpp
common/storage/Repositories.cpp
common/storage/Logger.cpp
common/service/LogisticsSystem.cpp
```

`build_client.bat` 编译：

```text
client/main.cpp
client/SocketClient.cpp
common/protocol/ProtocolCodec.cpp
common/security/InputValidator.cpp
common/security/StringUtil.cpp
```

共同编译参数：

```text
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK
```

Winsock 链接：

```text
-lws2_32
```

注意：当前 MinGW 环境对 `std::mutex/std::shared_mutex/std::filesystem` 支持不稳定，开发过程中已实际触发编译失败。因此工程使用：

```text
CoreMutex / CoreLock
SessionMutex / SessionLock
```

它们基于 `std::atomic_flag` 自旋锁封装，作为当前可编译环境下的并发保护点。未来如换现代工具链，可以替换为 `std::mutex` 或 `std::shared_mutex`。

---

## 2. 通信协议与会话安全防线

### 2.1 自定义纯文本协议

协议文件：

```text
common/protocol/ProtocolCodec.h
common/protocol/ProtocolCodec.cpp
```

核心结构：

```cpp
struct Request {
    std::string command;
    std::string token;
    std::vector<std::string> args;
};

struct Response {
    bool ok;
    std::string code;
    std::string message;
    std::vector<std::string> records;
};
```

协议异常：

```cpp
class ProtocolError : public std::runtime_error {
public:
    const std::string& code() const;
};
```

异常码：

```text
PROTOCOL_ERROR
UNKNOWN_COMMAND
INVALID_ARGUMENT
```

### 2.2 帧格式

请求帧：

```text
REQ|command|token|argCount|arg1|arg2|...\n
```

响应帧：

```text
RES|1/0|code|message|recordCount|record1|record2|...\n
```

说明：

```text
REQ / RES 用于区分帧类型。
token 固定为第二业务字段，游客命令可为空。
argCount / recordCount 用于校验字段数量，防止少参、多参、畸形帧。
\n 是唯一消息边界。
```

### 2.3 转义规则

用户输入、地址、描述、记录内容可能包含协议控制字符。协议层透明转义：

编码时：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

解码时反向还原：

```text
%25 -> %
%7C -> |
%0A -> \n
%0D -> \r
```

重要原则：

```text
上层业务不需要手动转义。
所有字段都由 ProtocolCodec 统一 escape/unescape。
未知或不完整转义序列会抛出 ProtocolError。
```

### 2.4 协议安全限制

当前常量：

```text
MaxMessageLength = 8192
MaxFieldLength   = 1024
MaxVectorItems   = 256
```

作用：

```text
限制单帧大小，防止超长发包撑爆内存。
限制字段大小，防止恶意大字段。
限制参数/记录数量，防止畸形帧或响应过载。
```

### 2.5 TCP 半包/粘包处理

Phase 3 已落实 TCP 流式缓冲红线。

服务端在 `SocketServer::handleClient` 中维护：

```cpp
std::string buffer;
```

接收循环逻辑：

```text
recv bytes
buffer.append(bytes)
while buffer contains '\n':
  frame = buffer.substr(0, newline + 1)
  buffer.erase(0, newline + 1)
  ProtocolCodec::decodeRequest(frame)
  ServerController::handle(request)
  ProtocolCodec::encodeResponse(response)
  sendAll(responseFrame)
```

客户端在 `SocketClient::receiveFrame()` 中维护：

```cpp
std::string receiveBuffer_;
```

客户端同样按 `\n` 截断完整响应帧，然后调用：

```cpp
ProtocolCodec::decodeResponse(frame)
```

绝对不要假设：

```text
一次 recv() == 一条完整消息
一次 send() == 完整发出全部数据
```

当前 `sendAll()` 已处理 partial send。

### 2.6 SessionManager 与 token 会话

文件：

```text
server/SessionManager.h
server/SessionManager.cpp
```

Session 结构：

```cpp
struct Session {
    std::string token;
    std::string username;
    std::string role;
    std::string loginTime;
    std::string lastActiveTime;
};
```

Session 表：

```cpp
std::unordered_map<std::string, Session> sessions_;
```

接口：

```text
createSession(username, role) -> token
getSession(token, session) -> bool
touch(token) -> bool
removeSession(token)
```

Token 生成：

```text
SHA-256(username | role | high_resolution_clock ticks | random_device随机数A | random_device随机数B | this指针)
```

当前 token 是 64 位十六进制 SHA-256 字符串。

并发保护：

```text
SessionMutex + SessionLock
```

基于 `atomic_flag` 自旋锁，原因同上：当前 MinGW 标准锁实现不稳定。

### 2.7 零信任机制

服务端权限判断入口：

```text
server/ServerController.cpp
```

核心红线：

```text
服务端业务必须从 token 中提取 username 和 role。
服务端绝不信任客户端明文传来的“当前用户名”。
客户端传来的 username 只能用于登录、注册、目标对象或查询条件，不能作为操作者身份。
```

用户业务：

```text
requireUserSession(request, session, response)
  SessionManager::getSession(request.token, session)
  session.role == USER
  LogisticsSystem 使用 session.username
```

管理员业务：

```text
requireAdminSession(request, session, response)
  SessionManager::getSession(request.token, session)
  session.role == ADMIN
```

快递员业务：

```text
requireCourierSession(request, session, response)
  SessionManager::getSession(request.token, session)
  session.role == COURIER
```

越权时返回：

```text
PERMISSION_DENIED
AUTH_REQUIRED
```

并写入安全审计：

```text
ADMIN_PERMISSION_DENIED
COURIER_PERMISSION_DENIED
```

### 2.8 登录与冻结安全

`LogisticsSystem` 已新增：

```text
loginUser(username, password)
loginCourier(username, password)
loginAdmin(username, password)
```

认证状态持久化：

```text
data/server/auth_state.txt
role|username|failedCount|lastFailedTime
```

规则：

```text
USER / COURIER：
  密码错误 -> failedCount + 1
  failedCount >= 3 -> 自动冻结实体 frozen=1
  登录成功 -> failedCount 清零

ADMIN：
  密码错误也记录失败次数
  不自动冻结，避免系统锁死
```

日志红线：

```text
不记录明文密码。
不记录完整 token 到业务日志。
```

---

## 3. 已落地的业务流与后台能力

### 3.1 V2 业务核心迁移

`common/models/ExpressItem.h/.cpp` 保留多态计费体系：

```cpp
class ExpressItem {
public:
    virtual double getPrice() const = 0;
};

class NormalItem;
class FragileItem;
class BookItem;
```

计费规则：

```text
NormalItem   普通快递：5 元/kg
FragileItem  易碎品：8 元/kg
BookItem     图书：2 元/本
```

寄件时服务端调用：

```text
ExpressItemFactory::createItem(itemType, itemAmount)
item->getPrice()
```

客户端从不提交最终费用。

### 3.2 数据文件格式

用户：

```text
users.txt
username|name|phone|salt|passwordHash|balance|address|frozen
```

管理员：

```text
admin.txt
username|name|salt|passwordHash|balance
```

快递员：

```text
couriers.txt
username|name|phone|salt|passwordHash|income|frozen|removed
```

快递 12 字段格式：

```text
expresses.txt
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee
```

状态：

```text
0 -> WaitingPickup / 待揽收
1 -> WaitingSign   / 待签收
2 -> Signed        / 已签收
```

认证状态：

```text
auth_state.txt
role|username|failedCount|lastFailedTime
```

日志 9 字段哈希链：

```text
operations.log
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

### 3.3 原子保存与哈希链

`StorageManager::saveLinesAtomically` 保留：

```text
写 file.tmp
旧 file -> file.bak
file.tmp -> file
失败时尽量恢复 bak
```

`Logger` 保留哈希链完整性：

```text
currentHash = SHA256(seq|time|actorType|actor|action|result|detail|prevHash)
```

管理员网络命令：

```text
VERIFY_LOG_CHAIN
```

调用：

```text
LogisticsSystem::verifyLogHashChain()
```

### 3.4 普通用户网络业务

已落地命令：

```text
REGISTER_USER
LOGIN_USER
QUERY_BALANCE
RECHARGE
SEND_EXPRESS
QUERY_MY_EXPRESS
QUERY_WAITING_SIGN
SIGN_EXPRESS
```

游客命令：

```text
REGISTER_USER
LOGIN_USER
```

需要 USER token：

```text
QUERY_BALANCE
RECHARGE
SEND_EXPRESS
QUERY_MY_EXPRESS
QUERY_WAITING_SIGN
SIGN_EXPRESS
```

`SEND_EXPRESS` 参数：

```text
receiver
description
itemType
itemAmount
```

服务端实际调用：

```cpp
system_.sendExpress(session.username, receiver, description, itemType, itemAmount);
```

已验证：

```text
注册新用户
登录
充值 100
寄 Fragile 2kg
服务端计费 16.00
剩余余额 84.00
发件人恶意签收自己寄给收件人的快递 -> PERMISSION_DENIED
```

自动联调：

```text
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user
```

### 3.5 管理员网络业务

已落地命令：

```text
LOGIN_ADMIN
QUERY_USERS
CREATE_COURIER
QUERY_COURIERS
REMOVE_COURIER
SET_COURIER_FROZEN
QUERY_ALL_EXPRESS
ASSIGN_COURIER
AUTO_ASSIGN_COURIER
AUTO_ASSIGN_ALL
VIEW_DASHBOARD
VIEW_COURIER_PERFORMANCE
VERIFY_LOG_CHAIN
```

所有管理员命令都必须：

```text
SessionManager::getSession(token)
session.role == ADMIN
```

普通用户或快递员 token 调管理员命令：

```text
ERR|PERMISSION_DENIED|当前身份无权执行管理员业务。
```

并写日志：

```text
ADMIN_PERMISSION_DENIED
```

### 3.6 管理员自动分配调度算法

V2 的自动分配策略已接入网络版：

```text
AUTO_ASSIGN_COURIER
AUTO_ASSIGN_ALL
```

核心选择函数：

```text
LogisticsSystem::selectBestCourierIndex()
```

候选过滤：

```text
跳过 frozen == true 的快递员
跳过 removed == true 的快递员
```

排序优先级：

```text
1. 未完成任务数少者优先
   未完成任务 = WaitingPickup + WaitingSign
2. 收入 income 低者优先
3. username 字典序升序
```

单个自动分配：

```text
AUTO_ASSIGN_COURIER|token|expressId
```

批量自动分配：

```text
AUTO_ASSIGN_ALL|token
```

批量返回 records：

```text
expressId;SUCCESS;courierUsername
expressId;FAILED;reason
```

注意：当前网络版自动分配实现使用 `selectBestCourierIndex()`，每次在当前内存状态上选择最优快递员。后续如要更接近 V2 最终批量快照增量算法，可继续优化批量分配的负载快照缓存。

### 3.7 停用快递员冲突任务清单

管理员命令：

```text
REMOVE_COURIER|token|courierUsername
```

服务层：

```text
LogisticsSystem::removeCourier(username)
```

规则：

```text
快递员不存在 -> NOT_FOUND
快递员已停用 -> STATE_CONFLICT
存在 WaitingPickup / WaitingSign 未完成任务 -> COURIER_HAS_UNFINISHED_TASKS
无未完成任务 -> markRemoved()
```

冲突时返回：

```text
ok=false
code=COURIER_HAS_UNFINISHED_TASKS
message=快递员存在未完成任务，不能停用。
records=冲突任务清单
```

records 内容来自 `Express::toRecord()`，包含：

```text
id
sender
receiver
courier
status
itemType
amount
fee
```

该点是验收亮点：不是简单拒绝，而是明确告诉管理员哪些任务阻塞停用。

### 3.8 管理员统计与绩效

管理员命令：

```text
VIEW_DASHBOARD
VIEW_COURIER_PERFORMANCE
```

`VIEW_DASHBOARD` 返回 records：

```text
平台余额
预估快递员支出
待揽收
待签收
已签收
普通快递数量/费用
易碎品数量/费用
图书数量/费用
```

`VIEW_COURIER_PERFORMANCE` 返回每个快递员：

```text
username
name
waitingPickup
waitingSign
signed
unfinished
total
completionRate
income
frozen
removed
```

### 3.9 快递员网络业务当前落点

虽然本总结标题是 Phase 0-6，但当前源码中已经存在 Phase 7 快递员业务的初步落点。

已出现命令：

```text
LOGIN_COURIER
QUERY_MY_PICKUP_TASKS
PICKUP_EXPRESS
PICKUP_BATCH
QUERY_MY_TASKS
VIEW_MY_PERFORMANCE
```

路由：

```text
ServerController::handleCourierCommand
ServerController::requireCourierSession
```

权限：

```text
SessionManager::getSession(token)
session.role == COURIER
system 使用 session.username
```

服务层：

```text
queryCourierPickupTasks(courierUsername)
pickupExpress(expressId, courierUsername)
pickupBatchExpresses(expressIds, courierUsername)
queryCourierExpresses(courierUsername)
viewMyCourierPerformance(courierUsername)
```

注意：下一阶段仍需把 Phase 7 作为首要任务进行系统性验收、加固和并发测试，不能只因为已有入口就默认“完全验收完成”。

---

## 4. Phase 7 待办交接与并发预警

### 4.1 下阶段首要任务

下一阶段首要任务：

```text
Phase 7：快递员网络业务验收、加固与演示闭环
```

核心命令：

```text
QUERY_MY_PICKUP_TASKS
PICKUP_EXPRESS
PICKUP_BATCH
QUERY_MY_TASKS
VIEW_MY_PERFORMANCE
```

必须完成的验收链路：

```text
1. 管理员创建或确认快递员存在。
2. 用户寄件，状态 WaitingPickup。
3. 管理员手动分配给快递员 A。
4. 快递员 A 登录。
5. 快递员 A 查询本人待揽收任务。
6. 快递员 A 单个揽收。
7. 验证状态 WaitingPickup -> WaitingSign。
8. 验证管理员余额扣除 fee * 50%。
9. 验证快递员 income 增加 fee * 50%。
10. 快递员 A 批量揽收多单。
11. 快递员 A 尝试揽收快递员 B 的任务，必须 PERMISSION_DENIED。
12. 重复揽收必须 STATE_CONFLICT。
13. 快递员查询本人全部任务和个人绩效。
```

### 4.2 当前建议新增的自动联调

建议在 client 中固定或完善：

```text
--selftest-courier
```

自测流程建议：

```text
管理员登录
创建 courier_A / courier_B
注册 sender / receiver
sender 充值
sender 寄三单
管理员分配两单给 courier_A，一单给 courier_B
courier_A 登录
courier_A 查询待揽收
courier_A 恶意揽收 courier_B 的单 -> PERMISSION_DENIED
courier_A 批量揽收自己的两单 -> SUCCESS
查询 courier_A 绩效
```

当前 `client/main.cpp` 中已经出现类似 `--selftest-courier` 的逻辑痕迹，需要下阶段实际构建运行确认。

### 4.3 最高级别并发与资金预警

给下一任架构师的备忘录：

```text
快递员揽收是 V3 最危险的资金状态变更点。
实现和修改 PICKUP_EXPRESS / PICKUP_BATCH 时，必须死守事务一致性。
```

单次揽收必须作为一个不可分割的业务事务：

```text
1. 校验 expressId 存在。
2. 校验 courier 存在、未 frozen、未 removed。
3. 校验 express.courier == session.username。
4. 校验 express.status == WaitingPickup。
5. commission = express.fee * 0.5。
6. 管理员账户 admin.balance -= commission。
7. 快递员账户 courier.income += commission。
8. express.status = WaitingSign。
9. express.pickupTime = now。
10. 保存 users/admin/couriers/expresses/auth_state。
11. 写 PICKUP_EXPRESS SUCCESS 日志。
```

任何一步保存失败必须回滚：

```text
Express oldExpress
Courier oldCourier
Admin oldAdmin

失败后恢复 oldExpress / oldCourier / oldAdmin。
不能出现状态已变但钱没转。
不能出现钱已转但状态没变。
不能出现重复揽收导致重复发提成。
```

并发预警：

```text
两个客户端可能同时请求同一 expressId。
一个快递员可能重复点击揽收。
管理员可能正在重新分配或自动分配。
收件用户可能在状态变化边界尝试签收。
```

当前 `LogisticsSystem` 已有 `CoreMutex` 全局锁保护服务入口。下一阶段必须确认：

```text
pickupExpress 全流程在同一把锁内完成。
pickupBatchExpresses 不会绕过单单状态检查。
批量揽收中的部分成功/失败需要有清晰 records。
并发重复揽收只能成功一次，另一次必须 STATE_CONFLICT 或 PERMISSION_DENIED。
```

如果工具链升级并支持标准库锁，建议迁移为：

```text
std::mutex：先保守保护全部业务入口。
std::shared_mutex：后续区分读查询和写操作。
```

但不要在没有测试的情况下贸然细分锁粒度。

### 4.4 Phase 7 后建议继续补的能力

优先级从高到低：

```text
1. --selftest-courier 自动联调脚本跑通并记录日志。
2. 并发重复揽收测试。
3. PICKUP_BATCH 返回逐单结果表。
4. 快递员个人绩效表格展示优化。
5. 快递员 token 调管理员命令、用户命令的越权测试。
6. 日志哈希链在快递员揽收后仍完整。
7. 管理员 dashboard 在揽收后显示支出和状态变化。
```

---

## 5. 当前常用联调命令

构建：

```bat
build_all.bat
```

PING：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe PING unused unused
```

管理员登录：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe LOGIN_ADMIN admin Admin0219
```

普通用户登录：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe LOGIN_USER login_user User1234
```

快递员登录：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe LOGIN_COURIER demo_courier Courier1234
```

用户业务自测：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user
```

管理员业务自测：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-admin
```

快递员业务建议自测：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-courier
```

注意：`--selftest-courier` 是否已完全通过，需要下一阶段实际运行确认。

---

## 6. 最终交接结论

V3 当前已经具备网络版主体框架：

```text
双进程 C/S
Winsock TCP
自定义 REQ/RES 文本协议
按 \n 分帧处理半包/粘包
ProtocolCodec 转义与长度限制
Session token 零信任鉴权
V2 多态计费迁移
V2 三状态物流流转
V2 原子保存
V2 日志哈希链
普通用户网络业务
管理员网络业务
快递员网络业务初步落点
```

下一个对话框不要重做 Phase 0-6。应直接从：

```text
Phase 7：快递员网络业务验收、加固、并发抢单与资金一致性测试
```

开始。

最高优先级仍是：

```text
快递员揽收时的状态、资金、日志、持久化必须一致。
任何并发或失败路径都不能导致重复提成、状态错乱或账实不一致。
```

