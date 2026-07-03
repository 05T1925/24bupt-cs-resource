# Logistics V3 Network 答辩演示手册

> 答辩人：北京邮电大学计算机学院 2024 级 刘文涛
> 日期：2026-06-10
> 建议时长：8-10 分钟（5分钟演示 + 3-5分钟代码与协议讲解）

---

## 一、演示准备（开场前 30 秒）

### 1.1 打开三个终端窗口

```
┌─────────────────┬─────────────────┬─────────────────┐
│   终端1:Server   │  终端2:Client-A  │  终端3:Client-B  │
│  (持续监听模式)   │  (交互式演示)     │  (并发演示备用)   │
└─────────────────┴─────────────────┴─────────────────┘
```

### 1.2 预构建

```bat
cd "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

**话术：** "我先构建一下项目。可以看到 server 和 client 是两个独立编译产物，不是同一个进程的不同入口。"

---

## 二、演示流程（5 分钟）

### 第一幕：双进程 C/S 架构证明（0:00-0:40）

**操作：**
```
终端1: bin\logistics_v3_server.exe
终端2: bin\logistics_v3_client.exe
```

**展示点：**
- 终端1 显示 "listening on 127.0.0.1:9000"
- 终端2 显示交互菜单
- 如果有终端3，再跑一个 client 证明多客户端

**话术：**
> "助教您看，server 和 client 是两个独立的进程，通过 Winsock TCP socket 通信。client 不能直接读写 data 目录，所有业务请求都必须经过 socket 发到服务端。这是题目要求的传统 C/S 架构。"

**可以顺手打开的源码文件：**
- `server/main.cpp` — 展示 server 入口
- `client/main.cpp` — 展示 client 入口

---

### 第二幕：零信任权限拦截（0:40-1:30）

**操作：**
```bat
# 终端1 先 Ctrl+C 停掉，然后：
bin\logistics_v3_server.exe --once

# 终端2：
bin\logistics_v3_client.exe --selftest-admin
```

**展示点：**
- 屏幕上首先出现 `[Client] user token calls admin dashboard: ERR|PERMISSION_DENIED`
- 然后 admin 登录后才能正常进入管理员业务

**话术：**
> "这里先用普通用户登录拿到 USER token，然后故意调用管理员命令 VIEW_DASHBOARD。服务端没有相信客户端菜单，而是从 SessionManager 根据 token 解析 role，发现不是 ADMIN 就直接返回 PERMISSION_DENIED，并且写了一条 ADMIN_PERMISSION_DENIED 审计日志。这就是零信任——服务端不信任客户端传来的任何身份声明。"

**可以顺手打开的源码文件：**
- `server/ServerController.cpp` → `requireAdminSession` 方法（约第566行）
- 指出 `system_.auditSecurityEvent("ADMIN_PERMISSION_DENIED")` 审计日志写入

---

### 第三幕：快递员揽收与资金闭环（1:30-2:40）

**操作：**
```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-courier
```

**展示点（屏幕逐行出现）：**
1. 管理员创建 courierA / courierB
2. 发件人寄三单（Fragile 16元 + Normal 20元 + Book 10元）
3. 管理员分配：2单→A，1单→B
4. courierA 登录，查待揽收 → 2单
5. **`courier A pickup courier B task: ERR|PERMISSION_DENIED`** ← 越权拦截
6. **`pickup batch own tasks: OK|PARTIAL_SUCCESS`** ← 批量揽收成功
7. **`repeat pickup after batch: ERR|STATE_CONFLICT`** ← 重复揽收拦截
8. 绩效验证：income 增加 18.00（= 16×0.5 + 20×0.5）

**话术：**
> "这一步展示物流核心链路。快递员 A 不能揽收分配给快递员 B 的任务——服务端在 pickupExpress 中校验 express.courier 必须等于 token 解析出的 session.username。
>
> 揽收时不只是改状态，还同步完成资金分账：管理员账户扣 50% 运费给快递员作为提成。这三件事——状态变更、管理员扣款、快递员收入增加——在 CoreMutex 同一把锁保护下完成，是一个不可分割的逻辑事务。"

**可以顺手打开的源码文件：**
- `common/service/LogisticsSystem.cpp` → `pickupExpress` 方法（约第504行）
- 指出 12 步原子事务的注释

---

### 第四幕：并发抢单高光时刻（2:40-3:50）

**操作：**
```bat
# 终端1：默认持续监听
bin\logistics_v3_server.exe

# 终端2：
bin\logistics_v3_client.exe --selftest-concurrency
```

**展示点（关键输出）：**
```
[Client] concurrent pickup A: OK|SUCCESS|揽收成功，提成：8.00
[Client] concurrent pickup B: ERR|STATE_CONFLICT|该快递当前状态不可揽收。
[SelfTest] Phase 8 concurrent pickup conflict passed.
```

**话术：**
> "这个脚本会打开两个独立 socket 连接，两个连接同时登录同一个快递员账号，在同一时刻对同一个快递单发起 PICKUP_EXPRESS。
>
> 服务端的网络层是多线程的——主线程只负责 accept，每个客户端连接由独立 worker thread 处理。虽然两个请求几乎同时到达，但进入业务状态机时被 CoreMutex 串行化。第一个请求把状态从待揽收改成待签收，第二个请求进来时已经看到状态变化，所以返回 STATE_CONFLICT。
>
> 这证明了系统不会因为并发导致重复揽收、重复发提成，也不会把文件数据写坏。"

**可以顺手打开的源码文件：**
- `server/SocketServer.cpp` → `acceptLoop` 方法（`CreateThread` per client）
- `common/service/LogisticsSystem.h` → `CoreMutex/CoreLock` 定义
- `client/main.cpp` → `runConcurrencySelfTest` 方法（`WaitForMultipleObjects`）

---

### 第五幕：异常断线兜底 + 哈希链收尾（3:50-5:00）

**操作：**
1. 终端2 运行交互式 client，登录任意账号
2. 直接关闭终端2 窗口
3. 观察终端1 server 输出

**展示点：**
```
[Server] client disconnected: 127.0.0.1:xxxxx
[Server] cleaned 1 session(s) for 127.0.0.1:xxxxx
[Server] active sessions: X (USER=X ADMIN=X COURIER=X)
```

**话术：**
> "每个客户端连接都有独立的 recv 循环。recv 返回 0 表示正常断开，SOCKET_ERROR 表示异常断开，服务端都会退出该连接线程、清理本连接关联的 token、关闭 socket。只影响当前连接，不会让 server 崩溃。"

**可选收尾展示（管理员登录后）：**
```bat
# 交互式登录 admin
VERIFY_LOG_CHAIN
```

**话术：**
> "最后展示一下日志哈希链。每条操作日志都包含 prevHash 和 currentHash，SHA-256 链式校验，任何人篡改日志都会被立刻检测出来。这说明业务层不只是完成网络化，还保留了原子保存、回滚和日志完整性校验这些工程可靠性设计。"

---

## 三、代码架构讲解（3-5 分钟）

### 3.1 总体架构图

```
┌──────────────┐     TCP Socket      ┌──────────────┐
│  client.exe  │ ◄──────────────────► │  server.exe  │
│              │   REQ/RES 文本协议    │              │
│  main.cpp    │                      │  main.cpp    │
│  SocketClient│                      │  SocketServer│
│  TablePrinter│                      │  ServerCtrl  │
│  交互菜单     │                      │  SessionMgr  │
└──────────────┘                      └──────┬───────┘
                                             │
                                      ┌──────▼───────┐
                                      │  common/     │
                                      │  LogisticsSystem
                                      │  Entities    │
                                      │  ProtocolCodec
                                      │  Storage     │
                                      └──────────────┘
```

**话术：**
> "这是整体架构。client 和 server 是两个独立 exe，通信只走 Winsock TCP socket。common 是纯业务模块——server 调用它来完成注册、寄件、分配、揽收、签收这些业务操作，client 不能直接调用 common 的业务层。"

### 3.2 协议层讲解

**打开 `common/protocol/ProtocolCodec.h`**

**话术：**
> "协议是我自己设计的纯文本协议。请求帧以 REQ 开头，响应帧以 RES 开头，用竖线分隔字段，换行符 \n 做消息边界。
>
> 为什么不用 JSON 或 HTTP？因为课程要求不能使用 RPC 框架，手写协议可以更精确地控制安全边界——比如字段转义、长度限制、参数数量校验这些都在协议层统一处理。
>
> 转义规则很简单：百分号、竖线、换行、回车这四个特殊字符用百分号编码。关键设计是——先按原始竖线切分字段，再做转义还原。这样避免用户输入里的 %7C 被误当成字段分隔符。"

**可以展示的关键代码行：**
- `ProtocolCodec.h:32-34` — 三个安全常量（8KB消息限制、1KB字段限制、256条目限制）
- `ProtocolCodec.cpp:167-180` — `splitRawFields`（先切分再还原的关键设计）
- `ProtocolCodec.cpp:237-248` — `validateCommand`（命令字字符集校验，防止注入）

### 3.3 Session 与 Token 讲解

**打开 `server/SessionManager.cpp`**

**话术：**
> "登录成功后，服务端生成一个 SHA-256 token 返回给客户端。token 的熵源包括用户名、角色、纳秒级时间戳、两个硬件随机数和服务端对象地址——保证不可预测。
>
> 后续所有业务请求，服务端都从 token 解析出当前用户身份，不信任客户端传来的任何用户名参数。这就是零信任的核心。"信客户端传来的任何用户名参数。这就是零信任的核心。"

**可以展示的关键代码行：**
- `SessionManager.cpp:109-122` — `generateToken` 方法（SHA-256 熵源）
- `ServerController.cpp:566-583` — `requireAdminSession`（权限校验+审计日志）

### 3.4 揽收事务讲解

**打开 `common/service/LogisticsSystem.cpp` → `pickupExpress` 方法**

**话术：**
> "揽收是整个系统最危险的操作——同时涉及状态变更和资金流转。我把它设计成一个 12 步的原子事务，全部在 CoreMutex 同一把锁里完成。
>
> 关键步骤是：先校验快递员身份和快递状态，然后管理员扣 50% 运费给快递员，同时把状态改成待签收，最后保存 5 个数据文件。每一步失败都会恢复旧对象快照。
>
> 并发场景下，第二个请求进入时状态已经不是待揽收了，直接返回 STATE_CONFLICT——这样就避免了重复揽收、重复发提成。"

**可以展示的关键代码行：**
- `LogisticsSystem.cpp` 揽收方法注释中的12步列表
- 旧对象快照备份：`const Express oldExpress = expresses_[expressIndex];`
- 失败回滚：`expresses_[expressIndex] = oldExpress;`

### 3.5 并发保护讲解

**打开 `common/service/LogisticsSystem.h`**

**话术：**
> "并发保护用的是基于 atomic_flag 的自旋锁。为什么不用标准库的 std::mutex？因为当前 MinGW 环境对标准锁的支持不稳定，实际编译会报错。所以用了 atomic_flag 自旋锁作为兼容性替代方案。
>
> 两个独立的锁：CoreMutex 保护业务状态机，SessionMutex 保护 session 表。这样 session 操作不会阻塞业务，业务操作也不会阻塞 session 查询。"

**可以展示的关键代码行：**
- `LogisticsSystem.h:14-33` — `CoreMutex/CoreLock` 定义
- `LogisticsSystem.h:127` — `mutable CoreMutex mutex_;`

### 3.6 持久化与日志讲解

**打开 `common/storage/StorageManager.cpp`**

**话术：**
> "数据持久化采用三阶段原子写入：先写临时文件，再备份旧文件，最后用 rename 替换。这样即使在写入过程中断电，也不会丢失全部数据。
>
> 操作日志用 SHA-256 哈希链保护完整性。每条日志包含前一条的哈希值，形成一条不可篡改的链条。管理员可以随时用 VERIFY_LOG_CHAIN 命令校验。"

**可以展示的关键代码行：**
- `StorageManager.cpp:49-88` — `saveLinesAtomically`（三阶段写入）
- `Logger.cpp:45-87` — `verifyHashChain`（哈希链校验）

---

## 四、常见问题准备

### Q1：为什么不用 HTTP/JSON？

**答：** 课程要求不能使用 RPC 框架，且传统 C/S 架构用纯文本协议更轻量。手写协议可以精确控制安全边界——字段转义、长度限制、参数校验都在协议层统一处理，不会依赖第三方库的漏洞。

### Q2：并发安全怎么保证的？

**答：** 两层锁保护。CoreMutex 保护业务状态机——所有 LogisticsSystem 方法入口都加锁，包括读操作。SessionMutex 保护 session 表。两把锁独立，互不阻塞。虽然用的是 atomic_flag 自旋锁（因为 MinGW 标准库兼容性问题），但机制和标准 mutex 完全一样。

### Q3：资金安全怎么保证的？

**答：** 揽收操作——状态变更、管理员扣款、快递员收入增加——在同一个 CoreMutex 临界区内完成，是一个不可分割的逻辑事务。任何一个子步骤失败，都会通过保存旧对象快照来回滚。并发下第二个请求因为状态已变会直接返回 STATE_CONFLICT，不会重复执行资金操作。

### Q4：怎么防止客户端伪造身份？

**答：** 零信任架构。登录后服务端生成 SHA-256 token，后续所有业务请求的身份都从 token 解析，不信任客户端传来的任何用户名。客户端传来的 username 只能作为操作目标（比如"我要寄给谁"），不能作为操作者身份（"我是谁"）。

### Q5：TCP 粘包怎么处理的？

**答：** 服务端和客户端各维护一个接收缓冲区。收到数据后追加到缓冲区，然后循环查找换行符 \n。找到完整帧就取出来处理，剩下的留在缓冲区等下一次 recv。不会假设一次 recv 就是一条完整消息。

### Q6：日志被篡改了能发现吗？

**答：** 能。每条日志的 currentHash = SHA-256(本条内容 + 前一条的哈希值)，形成链式结构。VERIFY_LOG_CHAIN 命令逐条校验 seq 连续性 + prevHash 连续性 + currentHash 正确性，任何篡改都会导致校验失败。

---

## 五、答辩节奏建议

```
0:00-0:30  开场准备：三个终端就位，构建完成
0:30-1:10  第一幕：双进程 C/S + 构建产物展示
1:10-2:00  第二幕：零信任权限拦截（selftest-admin）
2:00-3:10  第三幕：快递员揽收与资金闭环（selftest-courier）
3:10-4:20  第四幕：并发抢单冲突（selftest-concurrency）★ 高光
4:20-5:00  第五幕：异常断线 + 哈希链收尾
5:00-5:30  过渡：打开 IDE/编辑器，准备讲代码
5:30-6:30  协议层：ProtocolCodec 帧格式 + 转义 + 安全常量
6:30-7:30  业务层：pickupExpress 12步事务 + CoreMutex 锁
7:30-8:30  网络层：SocketServer 多线程 + Session 零信任
8:30-9:00  持久化：原子保存 + 日志哈希链
9:00-10:00 自由提问
```

---

*答辩手册生成时间：2026-06-10*
