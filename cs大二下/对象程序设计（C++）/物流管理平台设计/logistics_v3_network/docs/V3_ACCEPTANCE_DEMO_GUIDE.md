# V3 网络版全景演示与架构剖析指南

答辩人：北京邮电大学计算机学院 2024 级 刘文涛

本指南用于《物流管理平台设计》V3 网络版助实验收演示。建议演示时先证明 C/S 双进程架构，再走一遍人工业务链路，最后用自动脚本展示并发和异常防线。

## 0. 演示前准备

> 【操作动作】打开 PowerShell 或 cmd，进入项目目录并构建：

```bat
cd  "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

> 【预期输出】看到：

```text
[Build] server build success: bin\logistics_v3_server.exe
[Build] client build success: bin\logistics_v3_client.exe
[Build] all targets built successfully.
```

**【口语解说】**  
助教您好，我是 2024 级刘文涛。这里先构建 V3 网络版，可以看到最终生成的是两个独立可执行文件：`logistics_v3_server.exe` 和 `logistics_v3_client.exe`。这一步先证明本项目不是把菜单简单拆开，而是确实按题目要求做成了传统 C/S 双进程架构。

**【代码解释】**  
构建脚本分别编译 `server/`、`client/` 和共享的 `common/`。其中 `server` 负责 socket 监听、Session、权限路由；`client` 负责菜单输入和响应展示；`common` 只保留实体、协议、安全、仓储和业务服务。

## 1. 双进程物理隔离证明

> 【操作动作】方式一：双击项目根目录下的：

```text
run_server.bat
run_client.bat
```

> 【操作动作】方式二：用两个终端手动运行：

终端 1：

```bat
bin\logistics_v3_server.exe
```

终端 2：

```bat
bin\logistics_v3_client.exe
```

> 【预期输出】服务端显示监听：

```text
Logistics V3 Server started.
Phase 8: multi-client concurrency + SessionManager.
Mode: blocking listen
[Server] listening on 127.0.0.1:9000
```

> 【预期输出】客户端显示交互菜单：

```text
Logistics V3 Client started.
1. 注册普通用户
2. 登录
0. 退出
请选择:
```

**【口语解说】**  
助教您看，现在 server 和 client 是两个完全独立的进程。client 不能直接读写 `data/`，也不能绕过 server 调用业务函数，所有请求都必须通过 Winsock socket 发到服务端。  
我还特意把 `common` 做成纯业务模块，里面不放菜单、不读 `std::cin`、不做表格打印。这样业务核心可以被 server 调用，但不会混入客户端 UI 逻辑，职责比较干净。

**【代码解释】**  
- `server/SocketServer.cpp`：负责 `bind/listen/accept/recv/send`。
- `client/SocketClient.cpp`：负责连接服务端、发送请求、等待响应。
- `common/service/LogisticsSystem.cpp`：只处理注册、寄件、分配、揽收、签收等业务规则。
- `common/protocol/ProtocolCodec.cpp`：负责自定义协议编码和解码。

## 2. 人工交互完整业务链路

这一段建议使用 `demo_launcher.bat` 的人工演示模式，让脚本提示你每一步输入什么。

> 【操作动作】运行 launcher：

```bat
demo_launcher.bat
```

> 【操作动作】选择：

```text
[3] 人工交互完整业务演示模式
```

launcher 会给出本轮建议账号，例如：

```text
发件用户 Sender   : mans12345 / User1234
收件用户 Receiver : manr12345 / User1234
管理员 Admin      : admin / Admin0219
快递员 Courier    : demo_courier / Courier1234
```

### 2.1 用户注册、登录、充值、寄件

> 【操作动作】在客户端中依次操作：

```text
1. 注册普通用户
2. 注册收件用户
3. 登录发件用户
4. 充值 100
5. 寄件
   收件用户名：本轮 receiver
   物品描述：Manual fragile package
   类型：2 易碎品
   重量：2
6. 记下快递单号，例如 EX000123
```

> 【预期输出】寄件成功：

```text
OK|SUCCESS|寄件成功，快递单号：EXxxxxxx，费用：16.00，剩余余额：84.00
```

**【口语解说】**  
这里展示的是普通用户的核心功能：登录、充值和发送快递。费用不是客户端算的，客户端只提交物品类型和数量。服务端根据多态计费类 `ExpressItem` 计算费用，比如易碎品 2kg 是 `8 * 2 = 16` 元。  
这点可以证明客户端不是可信业务端，它只是输入入口，真正的计费和余额扣除都在服务端完成。

**【代码解释】**  
`ServerController::handleUserCommand` 收到 `SEND_EXPRESS` 后，不使用客户端传来的当前用户名，而是先从 token 中取出 `session.username`，再调用：

```cpp
system_.sendExpress(session.username, receiver, description, itemType, itemAmount);
```

`LogisticsSystem::sendExpress` 内部完成：

```text
校验发件人/收件人
创建 ExpressItem 多态对象
计算费用
扣用户余额
增加管理员余额
保存快递记录
```

### 2.2 管理员分配快递员

> 【操作动作】打开管理员客户端，登录：

```text
身份选择：管理员
账号：admin
密码：Admin0219
```

> 【操作动作】选择：

```text
5. 分配快递员
快递单号：刚才记下的 EXxxxxxx
快递员用户名：demo_courier
```

> 【预期输出】分配成功：

```text
OK|SUCCESS|分配成功。
```

**【口语解说】**  
这里展示物流管理功能。管理员通过网络客户端给待揽收快递分配快递员。服务端会校验当前 token 必须是 ADMIN，普通用户 token 或快递员 token 调这个命令都会被拒绝。

**【代码解释】**  
管理员命令进入 `ServerController::handleAdminCommand`。它先执行：

```text
SessionManager::getSession(token)
检查 session.role == ADMIN
```

如果角色不对，返回：

```text
ERR|PERMISSION_DENIED
```

并写入安全审计日志 `ADMIN_PERMISSION_DENIED`。

### 2.3 快递员揽收

> 【操作动作】打开快递员客户端，登录：

```text
身份选择：快递员
账号：demo_courier
密码：Courier1234
```

> 【操作动作】选择：

```text
1. 查看待揽收任务
2. 揽收单个快递
   快递单号：刚才的 EXxxxxxx
```

> 【预期输出】揽收成功：

```text
OK|SUCCESS|揽收成功，提成：8.00
```

**【口语解说】**  
助教您看，揽收是整个系统里最关键的状态流转点。这里不只是把状态改成待签收，还要同步完成资金分账：管理员账户扣掉 50% 运费，快递员收入增加 50%。  
为了防止并发下重复揽收或者重复发提成，我把这个过程放在服务端的同一把业务锁保护下，状态检查、资金变化和文件保存是一个逻辑事务。

**【代码解释】**  
`LogisticsSystem::pickupExpress` 的核心顺序是：

```text
1. 校验 expressId 存在
2. 校验 courier 存在且可用
3. 校验 express.courier == session.username
4. 校验状态必须是 WaitingPickup
5. commission = fee * 0.5
6. admin.balance -= commission
7. courier.income += commission
8. express.status = WaitingSign
9. 保存数据
10. 写日志
```

这里的 `session.username` 来自 token，不来自客户端参数，所以快递员不能伪造“我是别人”。

### 2.4 收件用户签收

> 【操作动作】打开收件用户客户端，登录：

```text
身份选择：普通用户
账号：本轮 receiver
密码：User1234
```

> 【操作动作】选择：

```text
5. 查询待签收
6. 签收快递
   快递单号：EXxxxxxx
```

> 【预期输出】签收成功：

```text
OK|SUCCESS|签收成功。
```

**【口语解说】**  
这里完成题目要求的完整物流链路：用户发送快递，管理员分配快递员，快递员揽收，收件用户签收。签收时服务端同样从 token 确定当前用户，只有真正的收件人能签收这单。

**【代码解释】**  
`signExpress` 会校验：

```text
express.receiver == session.username
状态不能是 WaitingPickup
不能重复签收 Signed 状态
```

不满足条件就返回 `PERMISSION_DENIED` 或 `STATE_CONFLICT`。

## 3. 协议分帧与 token 安全说明

> 【操作动作】可以向助教展示 `docs/V3_PROTOCOL_DESIGN.md` 或源码 `ProtocolCodec.cpp`。

**【口语解说】**  
助教您看，TCP 是字节流，不保证一次 `recv()` 就是一条完整命令。所以我没有偷懒把一次 `recv` 当完整请求，而是设计了 `REQ/RES` 文本协议，用 `\n` 做帧边界。服务端每个连接维护一个 buffer，收到数据后不断查找换行符，能处理半包和粘包。  
同时字段里如果有 `|`、换行或 `%`，协议层会转义，避免用户输入破坏协议结构。

**【代码解释】**  
请求帧格式：

```text
REQ|command|token|argCount|arg1|arg2|...\n
```

响应帧格式：

```text
RES|1/0|code|message|recordCount|record1|...\n
```

关键实现点：

```text
SocketServer::handleClient 维护 receive buffer
ProtocolCodec::decodeRequest 校验字段数量
ProtocolCodec::escapeField / unescapeField 处理特殊字符
```

## 4. 极限防线与高并发自动展示

> 【操作动作】运行：

```bat
demo_launcher.bat
```

> 【操作动作】选择：

```text
[1] 一键全自动演示模式
```

或只展示并发时，手动运行：

终端 1：

```bat
bin\logistics_v3_server.exe
```

终端 2：

```bat
bin\logistics_v3_client.exe --selftest-concurrency
```

> 【预期输出】并发抢单测试中看到：

```text
[Client] concurrent pickup A: OK|SUCCESS|...
[Client] concurrent pickup B: ERR|STATE_CONFLICT|...
[SelfTest] Phase 8 concurrent pickup conflict passed.
```

顺序可能反过来，但必须是一个成功，一个 `STATE_CONFLICT`。

**【口语解说】**  
这里是我认为 V3 最能体现网络版价值的地方。脚本会创建两个独立 socket 客户端，同时登录同一个快递员，并在同一时刻对同一个快递单发起揽收。  
服务端的网络层是多客户端并发处理的，主线程只负责 `accept`，每个连接由独立 worker thread 处理。虽然两个请求几乎同时到达，但进入业务状态机时会被 `CoreMutex/CoreLock` 串行化，所以只有第一个请求能把状态从待揽收改成待签收，第二个请求看到状态已经变化，就返回 `STATE_CONFLICT`。  
这可以证明系统不会因为并发导致重复揽收、重复发提成，也不会把文件数据写坏。

**【代码解释】**  
多线程网络模型：

```text
SocketServer::acceptLoop
accept client socket
CreateThread 派发连接处理线程
主线程继续 accept
```

异常断线处理：

```text
recv == 0        -> 客户端正常断开
recv == SOCKET_ERROR -> 客户端异常断开
清理该连接 token
关闭 socket
不影响 server 进程和其他 client
```

业务锁保护：

```cpp
CoreLock lock(mutex_);
```

所有修改型业务都在临界区中完成状态检查、资金变化、保存和回滚。

## 5. 最后总结话术

**【口语解说】**  
总结一下，我这个 V3 网络版主要不是只把原来的菜单搬到两个窗口，而是做了完整的 C/S 架构升级：  
第一，server/client 物理隔离，client 只能通过 socket 通信；  
第二，自定义协议解决 TCP 半包、粘包和特殊字符问题；  
第三，token Session 做零信任鉴权，服务端不相信客户端传来的当前用户名；  
第四，多线程 server 支持多个客户端同时连接；  
第五，关键状态流转用互斥锁保护，保证并发下资金和物流状态一致；  
第六，底层还有原子保存和日志哈希链，方便追踪和校验操作完整性。  
所以这个版本既满足题目三网络版的基本要求，也保留了 V2 的业务完整性和一些工程化加分点。

