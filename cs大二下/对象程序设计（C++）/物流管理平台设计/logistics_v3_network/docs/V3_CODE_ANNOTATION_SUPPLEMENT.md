# Logistics V3 Network 代码注释补充文档

> 生成日期：2026-06-10
> 用途：补充和完善源码注释，便于验收评审和后续维护
> 原则：只标注分析和建议，不改动源码

---

## 一、模块架构总览

```
┌─────────────────────────────────────────────────────────────┐
│                      client/main.cpp                        │
│  ClientApp: 交互菜单、4个selftest、TablePrinter、密码隐藏    │
│  SocketClient: Winsock connect/sendAll/receiveFrame         │
├─────────────────────────────────────────────────────────────┤
│                    common/protocol/                         │
│  ProtocolCodec: REQ/RES 编解码、字段转义、长度限制           │
├─────────────────────────────────────────────────────────────┤
│                    server/main.cpp                          │
│  ServerApp: 初始化→ensureDemoAccounts→监听                  │
│  SocketServer: accept多线程、handleClient流式处理            │
│  ServerController: 46命令路由、三身份权限校验                │
│  SessionManager: Token 生命周期、重复登录拦截                │
├─────────────────────────────────────────────────────────────┤
│                  common/service/                            │
│  LogisticsSystem: 46个业务方法、CoreMutex全局锁、事务回滚    │
├─────────────────────────────────────────────────────────────┤
│  common/models/    │ common/security/  │ common/storage/     │
│  Entities (6)      │ InputValidator    │ StorageManager      │
│  ExpressItem (4)   │ PasswordHasher    │ Repositories (6)    │
│                    │ HashUtil (SHA256) │ Logger (哈希链)     │
│                    │ StringUtil         │                     │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、分文件功能摘要与注释建议

### 2.1 `common/models/Entities.h` / `Entities.cpp`

**包含类：** User、Admin、Courier、Express、AuthState、Notification

**已实现状态：** 全部完成

**建议补充注释：**

```
User (8字段 serialization)
  - serialize(): 8字段 pipe 分隔，含 StringUtil::escape 转义
  - deserialize(): 严格校验 parts.size()==8，parseDoubleStrict 解析 balance
  - deduct(): 使用 0.000001 容差比较 double 金额
  - frozen_ 字段: "0"=正常/"1"=冻结

Admin (5字段 serialization)
  - 单例实体（内存中仅一个 admin_ 实例）
  - 默认账号 admin/Admin0219，在 LogisticsSystem::initialize() 自动创建

Courier (8字段 serialization)
  - frozen_ 和 removed_ 双重标记
  - markRemoved() 不删除数据，仅设置 removed_=true

Express (12-15字段 serialization，向后兼容)
  - serialize() 输出 15 字段（含 note、ratingScore、ratingComment）
  - deserialize() 接受 12-15 字段
  - toRecord(): 生成可读的 key=value;key=value 格式
  - status 流转: WaitingPickup(0)→WaitingSign(1)→Signed(2)
  - belongsToUser(): sender==user || receiver==user
  - belongsToCourier(): courier==user

AuthState (4字段)
  - 记录认证失败次数，不存储明文密码
  - recordFailure(): failedCount++ 并更新时间
  - clearFailures(): 登录成功后清零

Notification (8字段)
  - id 格式: NT000001 递增
  - targetRole: USER/COURIER/ADMIN
  - readFlag: 0=未读/1=已读
```

---

### 2.2 `common/protocol/ProtocolCodec.h` / `ProtocolCodec.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
协议帧格式:
  REQ|command|token|argCount|arg1|arg2|...\n   (请求)
  RES|1/0|code|message|recordCount|r1|...\n    (响应)

字段转义规则 (escapeField/unescapeField):
  % → %25, | → %7C, \n → %0A, \r → %0D
  未知或不完整转义序列 → ProtocolError

安全常量:
  MaxMessageLength = 8192   (单帧上限)
  MaxFieldLength   = 1024   (单字段上限)
  MaxVectorItems   = 256    (参数/记录数量上限)

帧校验 (decodeRequest):
  1. normalizeFrame: 去尾 \n/\r，检查未转义换行
  2. splitRawFields: 按 | 切分（不处理转义，由 unescapeField 处理）
  3. 校验 fields[0]=="REQ"，fields.size() >= 4
  4. 校验 fields.size() == 4 + argCount
  5. 每个 arg 单独 unescape + 长度校验

响应帧校验 (decodeResponse):
  1. 同上结构，fields[0]=="RES"
  2. fields[1] 必须是 "0" 或 "1"
  3. 校验 fields.size() == 5 + recordCount

命令字校验 (validateCommand):
  仅允许 [A-Z][0-9]_ (大写字母、数字、下划线)

parseCount:
  逐字符解析，非数字→PROTOCOL_ERROR，超过 MaxVectorItems→INVALID_ARGUMENT
```

**建议补充注释：** 每个校验函数前增加注释说明校验条件和异常码

---

### 2.3 `common/service/LogisticsSystem.h` / `LogisticsSystem.cpp`

**已实现状态：** 全部完成（46 个业务方法）

**核心设计要点：**

```
CoreMutex/CoreLock (基于 std::atomic_flag 自旋锁):
  - MinGW 兼容性方案（std::mutex 不稳定）
  - 所有 LogisticsSystem 公共方法入口加 CoreLock lock(mutex_)
  - 读操作和写操作均加锁（保守策略保证一致性）

LogisticsSystem 构造:
  - 初始化 6 个 Repository（指定 data/ 路径）
  - NotificationRepository 文件缺失不阻断

initialize():
  1. ensureDirectory + load 5 个必须文件
  2. notificationRepository_.load 为可选
  3. admin 无密码时创建默认管理员 admin/Admin0219
  4. 写 INIT 日志

登录三部曲 (loginUser/loginCourier/loginAdmin):
  1. 查找实体索引
  2. 检查 frozen/removed
  3. 密码哈希校验
  4. 失败→handleAuthFailure (记录失败次数, >=3 自动冻结 USER/COURIER)
  5. 成功→clearAuthFailure + saveAll + 写 SUCCESS 日志
  6. ADMIN 不自动冻结

揽收事务 (pickupExpress) - 最危险的状态变更点:
  1. 校验 expressId 存在
  2. 校验 courier 存在且可用
  3. 校验 express.courier == courierUsername (权限)
  4. 校验 express.status == WaitingPickup (状态)
  5. snapshot: oldExpress, oldCourier, oldAdmin
  6. admin.deduct(commission = fee*0.5)
  7. courier.addIncome(commission)
  8. express.pickup(now)
  9. addNotification(receiver, EXPRESS_PICKED_UP)
  10. saveAll → 失败回滚 old 对象
  11. write log

批量揽收 (pickupBatchExpresses):
  - 逐单调用 pickupExpress，每次独立加锁
  - 返回逐单结果: expressId;SUCCESS/FAILED;code;message
  - summary.code = PARTIAL_SUCCESS/FAILED

自动分配算法 (selectBestCourierIndex):
  - 过滤 frozen/removed
  - 排序: 未完成任务少>收入低>username 字典序

资金流转:
  sendExpress:  user.balance -= fee, admin.balance += fee
  pickupExpress: admin.balance -= fee*0.5, courier.income += fee*0.5

原子保存 (saveAll):
  5个 Repository 串行 save（user/admin/courier/express/auth_state）
  通知单独保存（best-effort，失败不影响核心业务）

通知生成 (addNotification):
  - 调用者必须持有 CoreLock（private 方法注释说明）
  - 通知 save 失败 → pop_back + 写日志，不回滚核心业务
```

**建议补充注释：**
- 每个业务方法建议增加 `// [事务] CoreLock 保护` 或 `// [只读] CoreLock 保护`
- 涉及资金的方法建议增加资金流向注释
- `pickupExpress` 建议增加事务步骤编号注释（1-12步）

---

### 2.4 `common/storage/StorageManager.h` / `StorageManager.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
saveLinesAtomically 三阶段写入:
  阶段1: 写 .tmp 文件（新建，覆盖）
  阶段2: 旧文件 → .bak（备份）
  阶段3: .tmp → 正式文件（原子 rename）
  失败恢复: .bak → 正式文件（如果 bak 存在）

loadLines:
  - 跳过空行
  - 文件不存在返回 true（不报错）

ensureDirectory:
  - 逐级创建目录（_mkdir）
  - 平台适配: _WIN32 使用 _mkdir，否则 mkdir
```

---

### 2.5 `common/storage/Repositories.h` / `Repositories.cpp`

**已实现状态：** 全部完成

**6 个 Repository:**

| Repository | 实体类型 | load 方式 | save 方式 | 特殊处理 |
|------------|----------|-----------|-----------|----------|
| UserRepository | vector\<User\> | loadLines→deserialize | serialize→saveLinesAtomically | 8字段严格校验 |
| AdminRepository | Admin (单例) | loadLines[0]→deserialize | serialize→saveLinesAtomically | 文件空→保留默认 |
| CourierRepository | vector\<Courier\> | loadLines→deserialize | serialize→saveLinesAtomically | 8字段严格校验 |
| ExpressRepository | vector\<Express\> | loadLines→deserialize | serialize→saveLinesAtomically | 12-15字段兼容 |
| AuthStateRepository | vector\<AuthState\> | loadLines→deserialize | serialize→saveLinesAtomically | 4字段校验 |
| NotificationRepository | vector\<Notification\> | loadLines→deserialize | serialize→saveLinesAtomically | 文件缺失不报错 |

**建议补充注释：**
- Repository 的 load 方法标注 "文件不存在返回 true"
- save 方法标注调用了 `StorageManager::saveLinesAtomically`

---

### 2.6 `common/storage/Logger.h` / `Logger.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
write():
  1. readLastChainState 获取 nextSeq 和 prevHash
  2. currentHash = SHA256(seq|time|actorType|actor|action|result|detail|prevHash)
  3. 追加写入 9 字段: seq|time|actorType|actor|action|result|detail|prevHash|currentHash
  4. 字段含特殊字符时使用 StringUtil::escape

verifyHashChain():
  1. 加载全部日志行
  2. 逐行校验: 字段数==9、seq 连续、prevHash 连续、currentHash 正确
  3. 初始 prevHash = "GENESIS"

queryLogs():
  - 4 个可选筛选条件（actorType/actor/action/result）
  - 返回格式: seq|time|actorType|actor|action|result|detail (7字段)
```

---

### 2.7 `common/security/InputValidator.h` / `InputValidator.cpp`

**已实现状态：** 全部完成

**校验规则：**

```
checkUsername():
  - 长度 3-32 字符
  - 字母开头，仅含字母数字下划线

checkPhone():
  - 11 位数字
  - 以 '1' 开头

checkPasswordStrength():
  - 长度 8-64
  - 必须同时含字母和数字
  - 不能全为空格

checkName():
  - 长度 1-64
  - 仅含字母、中文、空格

checkAddress():
  - 长度 1-256
  - 仅含字母、中文、数字、空格、常用符号

passwordUnchanged():
  - oldPassword == newPassword → 返回 true（拒绝修改）
```

---

### 2.8 `server/SessionManager.h` / `SessionManager.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
SessionMutex/SessionLock:
  - 同 CoreMutex，基于 atomic_flag 自旋锁
  - 保护 sessions_ map（unordered_map<string, Session>）

createSession():
  1. generateToken: SHA-256(username|role|clock|randomA|randomB|this)
  2. 创建 Session{token, username, role, loginTime, lastActiveTime}
  3. sessions_[token] = session

hasActiveSession():
  - 遍历所有 session，检查 username+role 是否已有活跃会话
  - 重复登录拦截的前置检查

getSessionSummary():
  - 返回 "role|username" 或 "GUEST|"（token 空/无效）
  - 用于安全日志，不暴露完整 token

Token 熵源:
  - username + role (确定性)
  - high_resolution_clock::now() ticks (纳秒级时间戳)
  - random_device 两个独立 64-bit 随机数
  - this 指针地址
```

**建议补充注释：**
- `generateToken` 的参数说明和熵源说明
- `hasActiveSession` 的 O(n) 遍历性能说明

---

### 2.9 `server/ServerController.h` / `ServerController.cpp`

**已实现状态：** 全部完成（46 个命令路由）

**核心设计要点：**

```
handle() 主路由:
  PING → 直接返回 PONG
  LOGIN_USER/COURIER/ADMIN → handleLogin(role)
  REGISTER_USER → handleRegisterUser
  CHECK_USERNAME_AVAILABLE/CHECK_PHONE_AVAILABLE → 直接处理
  LOGOUT → handleLogout
  QUERY_MY_NOTIFICATIONS/MARK_NOTIFICATION_READ/... → handleNotificationCommand
  QUERY_BALANCE/RECHARGE/SEND_EXPRESS/... → handleUserCommand
  管理员命令 → isAdminCommand → handleAdminCommand
  快递员命令 → isCourierCommand → handleCourierCommand
  其他 → UNKNOWN_COMMAND

登录处理 (handleLogin):
  1. 校验 args.size()==2
  2. 调用 system_.loginUser/loginCourier/loginAdmin
  3. 业务失败→返回错误
  4. sessions_.hasActiveSession → 重复登录→ALREADY_LOGGED_IN
  5. sessions_.createSession → 返回 token+role+username

三身份权限校验 (requireUserSession/requireAdminSession/requireCourierSession):
  1. token 空 → AUTH_REQUIRED
  2. sessions_.getSession → 失败→AUTH_REQUIRED
  3. session.role != 要求角色 → PERMISSION_DENIED
  4. requireAdminSession/requireCourierSession → 写审计日志
  5. sessions_.touch(token) → 刷新活跃时间

ensureDemoAccounts():
  - 注册 demo_user/login_user/freeze_user/phase6_sender/phase6_receiver
  - 创建 demo_courier/phase6_courier
  - phase6_sender 充值 100 并寄一单 Fragile 1kg（seed data）

管理员命令列表 (isAdminCommand):
  19 个命令，包含 CHANGE_PASSWORD 和 UPDATE_EXPRESS_NOTE (管理员也可用)

快递员命令列表 (isCourierCommand):
  7 个命令，包含 CHANGE_PASSWORD 和 UPDATE_COURIER_PROFILE
```

**建议补充注释：**
- 每个 handler 入口标注权限要求（GUEST/USER/COURIER/ADMIN）
- 参数数量校验的注释说明每个参数的语义

---

### 2.10 `server/SocketServer.h` / `SocketServer.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
start():
  1. WSAStartup
  2. socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
  3. bind + listen(SOMAXCONN)
  4. running_ = true

acceptLoop:
  --once 模式: accept 一个连接 → 同步 handleClient → 退出
  默认模式: accept → CreateThread(clientThreadEntry) → 主线程继续 accept
  CreateThread 失败 → 降级为同步处理

handleClient (每个连接的处理循环):
  1. 维护 std::string buffer
  2. recv(1024) → buffer.append
  3. buffer.size() > MaxMessageLength*2 → PROTOCOL_ERROR + 断开
  4. while buffer contains '\n':
     a. frame = buffer.substr(0, newline+1)
     b. buffer.erase(0, newline+1)
     c. ProtocolCodec::decodeRequest(frame)
     d. controller_.handle(request)
     e. 记录 connectionTokens (用于断线清理)
     f. ProtocolCodec::encodeResponse(response)
     g. sendAll(responseFrame)
  5. recv==0 → 正常断开 → cleanupClientSessions
  6. recv==SOCKET_ERROR → 异常断开 → cleanupClientSessions

cleanupClientSessions:
  - 遍历 connectionTokens → controller_.closeSession(token)
  - 打印 session 统计: USER/ADMIN/COURIER 各多少

sendAll:
  - 循环 send，处理 partial send
  - send==SOCKET_ERROR 或 ==0 → 返回 false

logRequestSummary:
  - 格式: [Server] clientAddress cmd=COMMAND session=role|username => CODE
  - 不输出完整 token 和密码

线程安全统计:
  - totalRequests_: std::atomic<long long>
  - activeConnections_: std::atomic<int>
```

**建议补充注释：**
- `handleClient` 的流式处理循环说明半包/粘包处理逻辑
- 缓冲区上限 `MaxMessageLength * 2` 的设计意图

---

### 2.11 `client/SocketClient.h` / `SocketClient.cpp`

**已实现状态：** 全部完成

**核心设计要点：**

```
connectToServer():
  1. WSAStartup
  2. socket + connect
  3. 地址解析: inet_addr

sendCommand():
  1. ProtocolCodec::encodeRequest(request)
  2. sendAll(payload)
  3. receiveFrame() → ProtocolCodec::decodeResponse

sendAll:
  - 循环 send，处理 partial send（同服务端逻辑）

receiveFrame (客户端流式缓冲):
  1. 检查 receiveBuffer_ 是否已有完整帧（含\n）
  2. 有 → 取出返回
  3. 无 → recv(1024) → receiveBuffer_.append
  4. 超上限 → ProtocolError
  5. recv==0 → "server closed connection"
  6. recv==SOCKET_ERROR → "recv response failed"
```

---

### 2.12 `client/main.cpp` (ClientApp)

**已实现状态：** 全部完成（约 1380 行）

**核心组件：**

```
TablePrinter:
  - printRecords(): 表格渲染，处理中英文字符宽度
  - displayWidth(): 中文字符按 2 个宽度计算
  - trimForCell(): 超长截断 + "..."

ClientApp:
  主入口 run():
    --selftest-user    → runUserSelfTest
    --selftest-admin   → runAdminSelfTest
    --selftest-courier → runCourierSelfTest
    --selftest-concurrency → runConcurrencySelfTest
    COMMAND user pass  → loginOnce (CLI 单次登录)
    无参数             → mainMenu (交互菜单)

  交互菜单层次:
    mainMenu → registerUserFlow / interactiveLogin
      userMenu (12项) → 余额/充值/寄件/查询/签收/批量/密码/信息/备注/评分/通知
      adminMenu (20项) → 用户管理/快递员管理/分配/统计/日志/改派/通知
      courierMenu (8项) → 揽收/批量/任务/绩效/密码/信息/通知
      notificationMenu (4项) → 全部/未读/标记/计数

  四个 SelfTest 脚本:
    runUserSelfTest:
      - 注册 sender+receiver
      - sender 充值 100
      - 寄 Fragile 2kg → 验证 fee=16.00, balance=84.00
      - 发件人恶意签收 → 验证 PERMISSION_DENIED

    runAdminSelfTest:
      - 用 USER token 调 VIEW_DASHBOARD → 验证 PERMISSION_DENIED
      - 用 ADMIN token 登录
      - 分配 + 停用冲突清单 + 一键分配 + 看板 + 绩效

    runCourierSelfTest:
      - 创建 courierA/courierB, sender, receiver
      - sender 寄 3 单: Fragile*2, Normal*4, Book*5
      - 分配: 2单→A, 1单→B
      - courierA 越权揽收 B 的任务 → PERMISSION_DENIED
      - courierA 批量揽收自己的 2 单 → SUCCESS
      - 重复揽收 → STATE_CONFLICT
      - 验证 income delta = 18.00 (16*0.5 + 20*0.5)

    runConcurrencySelfTest:
      - 创建 courier+sender+receiver
      - sender 寄 Fragile 2kg (fee=16.00)
      - 两个独立 SocketClient 连接
      - 共享同一 token（绕过重复登录限制）
      - CreateThread 两个线程同时 PICKUP_EXPRESS
      - atomic<bool> start 同步启动
      - 验证: 一个 OK, 一个 STATE_CONFLICT

  辅助方法:
    loginOnce(): 登录+保存 token/role/username + 显示未读通知数
    sendLogout(): 登出（best-effort）
    sendRequest(): 组装 Request 并发送
    readPasswordHidden(): _getch() 实现密码隐藏输入（Win32）
    uniqueSuffix(): 毫秒时间戳生成唯一后缀
    splitExpressIds(): 逗号/空格分隔的 ID 列表解析
    extractMoneyValue()/extractIntValue(): 从 record 字符串解析数值
```

**建议补充注释：**
- 每个 selftest 方法增加步骤编号和验证点注释
- `sendExpressFlow` 的余额不足重试流程增加分支说明
- `runConcurrencySelfTest` 的线程同步机制说明

---

## 三、关键方法实现状态与依赖表

### 3.1 LogisticsSystem 方法清单

| 方法 | 状态 | 锁保护 | 事务回滚 | 写日志 | 写通知 | 依赖 |
|------|------|--------|----------|--------|--------|------|
| `initialize` | ✅ | CoreLock | N/A | INIT | - | StorageManager, 6 Repositories |
| `loginUser` | ✅ | CoreLock | saveAll | LOGIN_USER | - | findUserIndex, handleAuthFailure |
| `loginCourier` | ✅ | CoreLock | saveAll | LOGIN_COURIER | - | findCourierIndex, handleAuthFailure |
| `loginAdmin` | ✅ | CoreLock | saveAll | LOGIN_ADMIN | - | handleAuthFailure |
| `registerUser` | ✅ | CoreLock | pop_back | REGISTER_USER | - | InputValidator, findUserIndex |
| `checkUsernameAvailable` | ✅ | CoreLock | N/A | - | - | findUserIndex, findCourierIndex |
| `checkPhoneAvailable` | ✅ | CoreLock | N/A | - | - | InputValidator |
| `queryUsers` | ✅ | CoreLock | N/A | - | - | - |
| `recharge` | ✅ | CoreLock | oldUser | RECHARGE | - | findUserIndex |
| `queryBalance` | ✅ | CoreLock | N/A | - | - | findUserIndex |
| `createCourier` | ✅ | CoreLock | pop_back | CREATE_COURIER | - | InputValidator, findCourierIndex |
| `queryCouriers` | ✅ | CoreLock | N/A | - | - | unfinishedCountForCourier |
| `removeCourier` | ✅ | CoreLock | oldCourier | REMOVE_COURIER | - | findCourierIndex, 冲突清单 |
| `setUserFrozen` | ✅ | CoreLock | oldUser | SET_USER_FROZEN | ACCOUNT_STATUS_CHANGED | findUserIndex |
| `setCourierFrozen` | ✅ | CoreLock | oldCourier | SET_COURIER_FROZEN | ACCOUNT_STATUS_CHANGED | findCourierIndex |
| `sendExpress` | ✅ | CoreLock | oldSender+oldAdmin+pop | SEND_EXPRESS | - | ExpressItemFactory, generateExpressId |
| `assignCourier` | ✅ | CoreLock | oldExpress | ASSIGN_COURIER | TASK_ASSIGNED | findExpressIndex, findCourierIndex |
| `autoAssignCourier` | ✅ | CoreLock | oldExpress | AUTO_ASSIGN | - | findExpressIndex, selectBestCourierIndex |
| `autoAssignAllWaitingPickup` | ✅ | CoreLock | saveAll | AUTO_ASSIGN_ALL | - | selectBestCourierIndex |
| `pickupExpress` | ✅ | CoreLock | oldExp+oldCour+oldAdmin | PICKUP_EXPRESS | EXPRESS_PICKED_UP | 多重校验, commission 计算 |
| `pickupBatchExpresses` | ✅ | 逐单锁 | 逐单回滚 | - | - | 调用 pickupExpress |
| `signExpress` | ✅ | CoreLock | oldExpress | SIGN_EXPRESS | SIGNED+COMPLETED | findExpressIndex |
| `signBatchExpresses` | ✅ | 逐单锁 | 逐单回滚 | - | - | 调用 signExpress |
| `changePassword` | ✅ | CoreLock | oldUser | CHANGE_PASSWORD | - | findUserIndex, PasswordHasher |
| `changeCourierPassword` | ✅ | CoreLock | oldCourier | CHANGE_PASSWORD | - | findCourierIndex, PasswordHasher |
| `changeAdminPassword` | ✅ | CoreLock | oldAdmin | CHANGE_PASSWORD | - | PasswordHasher |
| `updateMyProfile` | ✅ | CoreLock | oldUser | UPDATE_MY_PROFILE | - | findUserIndex, InputValidator |
| `updateCourierProfile` | ✅ | CoreLock | oldCourier | UPDATE_COURIER_PROFILE | - | findCourierIndex, InputValidator |
| `adminUpdateUser` | ✅ | CoreLock | oldUser | ADMIN_UPDATE_USER | - | findUserIndex, InputValidator |
| `adminUpdateCourier` | ✅ | CoreLock | oldCourier | ADMIN_UPDATE_COURIER | - | findCourierIndex, InputValidator |
| `reassignCourier` | ✅ | CoreLock | oldExpress | REASSIGN_COURIER | TASK_REASSIGNED+AWAY | findExpressIndex, findCourierIndex |
| `updateExpressNote` | ✅ | CoreLock | oldExpress | UPDATE_EXPRESS_NOTE | - | findExpressIndex, role 判断 |
| `rateExpress` | ✅ | CoreLock | oldExpress | RATE_EXPRESS | RATING_RECEIVED | findExpressIndex, 重复评分检测 |
| `queryUserExpresses` | ✅ | CoreLock | N/A | - | - | belongsToUser |
| `queryWaitingSignExpresses` | ✅ | CoreLock | N/A | - | - | receiver+isWaitingSign |
| `queryCourierPickupTasks` | ✅ | CoreLock | N/A | - | - | belongsToCourier+isWaitingPickup |
| `queryCourierExpresses` | ✅ | CoreLock | N/A | - | - | belongsToCourier |
| `queryAllExpresses` | ✅ | CoreLock | N/A | - | - | - |
| `viewDashboard` | ✅ | CoreLock | N/A | - | - | 统计计算 |
| `viewMyCourierPerformance` | ✅ | CoreLock | N/A | - | - | courierPerformanceRecord |
| `viewCourierPerformance` | ✅ | CoreLock | N/A | - | - | courierPerformanceRecord |
| `queryLogs` | ✅ | CoreLock | N/A | - | - | logger_.queryLogs |
| `verifyLogHashChain` | ✅ | 无锁 | N/A | - | - | logger_.verifyHashChain |
| `auditSecurityEvent` | ✅ | 无锁 | N/A | 审计日志 | - | logger_.write |
| `addNotification` | ✅ | 调用者持锁 | 独立保存 | 失败日志 | - | notificationRepository_.save |
| `queryMyNotifications` | ✅ | CoreLock | N/A | - | - | 角色+用户过滤 |
| `markNotificationRead` | ✅ | CoreLock | saveAll | - | - | 角色+用户+id 匹配 |
| `countUnreadNotifications` | ✅ | CoreLock | N/A | - | - | 角色+用户+readFlag 过滤 |

### 3.2 错误码汇总

| 错误码 | 含义 | 触发位置 |
|--------|------|----------|
| `SUCCESS` | 操作成功 | 各业务方法 |
| `PROTOCOL_ERROR` | 协议格式错误 | ProtocolCodec, ServerController |
| `UNKNOWN_COMMAND` | 未知命令 | ServerController::handle |
| `INVALID_ARGUMENT` | 参数不合法 | InputValidator, 各 handler |
| `AUTH_REQUIRED` | 需要登录 | require*Session |
| `AUTH_FAILED` | 认证失败 | handleAuthFailure |
| `TOKEN_EXPIRED` | Token 过期 | SessionManager (预留) |
| `PERMISSION_DENIED` | 权限不足 | require*Session, pickupExpress, signExpress |
| `ACCOUNT_FROZEN` | 账号已冻结 | loginUser, loginCourier |
| `NOT_FOUND` | 实体不存在 | find*Index 失败 |
| `DUPLICATE` | 实体重复 | registerUser, createCourier, checkUsernameAvailable |
| `DUPLICATE_RATING` | 重复评分 | rateExpress |
| `BALANCE_NOT_ENOUGH` | 余额不足 | sendExpress, pickupExpress |
| `PASSWORD_UNCHANGED` | 新旧密码相同 | changePassword |
| `STATE_CONFLICT` | 状态冲突 | pickupExpress, signExpress, reassignCourier |
| `COURIER_HAS_UNFINISHED_TASKS` | 快递员有未完成任务 | removeCourier |
| `STORAGE_FAILED` | 存储失败 | saveAll, saveLinesAtomically |
| `SERVER_ERROR` | 服务端异常 | SocketServer try-catch |
| `LOG_CHAIN_BROKEN` | 日志哈希链断裂 | verifyHashChain |
| `ALREADY_LOGGED_IN` | 重复登录 | handleLogin |
| `LOGIN_SUCCESS` | 登录成功（非错误码） | handleLogin |
| `PARTIAL_SUCCESS` | 部分成功 | pickupBatch, signBatch |
| `PONG` | PING 响应 | handle |

---

## 四、模块间依赖关系

```
client/main.cpp
  └── SocketClient
        └── ProtocolCodec
              └── StringUtil (仅客户端用到 split/parseDouble)

server/main.cpp
  └── SocketServer
        ├── ServerController
        │     ├── SessionManager
        │     │     └── HashUtil (SHA-256)
        │     └── LogisticsSystem
        │           ├── Entities (User/Admin/Courier/Express/AuthState/Notification)
        │           │     └── StringUtil (escape/unescape/split)
        │           ├── ExpressItem (NormalItem/FragileItem/BookItem)
        │           ├── InputValidator
        │           ├── PasswordHasher
        │           │     └── HashUtil (SHA-256)
        │           ├── Logger
        │           │     ├── HashUtil
        │           │     └── StorageManager
        │           └── Repositories (6个)
        │                 └── StorageManager (saveLinesAtomically)
        └── ProtocolCodec (服务端编解码)
```

---

## 五、数据流完整路径（以寄件为例）

```
Client                                Server
───────────────────────────────────────────────────────────
1. 用户输入寄件信息                     
2. ClientApp::sendExpressFlow         
   → 组装参数 [receiver, desc, type, amount, note]
3. SocketClient::sendCommand          
   → ProtocolCodec::encodeRequest     
   → "REQ|SEND_EXPRESS|token|4/5|...\n"
   → sendAll()                        
                                     4. SocketServer::handleClient
                                        → recv → buffer → 找到 \n
                                     5. ProtocolCodec::decodeRequest
                                        → 校验帧格式 → Request{command, token, args}
                                     6. ServerController::handle
                                        → handleUserCommand
                                     7. requireUserSession
                                        → SessionManager::getSession(token)
                                        → session.role=="USER" ✓
                                     8. 校验 args.size()==4或5
                                        → parseDoubleStrict(amount)
                                     9. LogisticsSystem::sendExpress
                                        → CoreLock lock(mutex_)
                                        → 校验 sender/receiver 存在、非冻结、非同一人
                                        → ExpressItemFactory::createItem → getPrice()
                                        → 余额检查（不足→BALANCE_NOT_ENOUGH+缺口信息）
                                        → sender.deduct(fee), admin.addBalance(fee)
                                        → generateExpressId, Express 入 vector
                                        → saveAll (5个 Repository)
                                        → 失败→恢复 oldSender + oldAdmin + pop
                                        → logger_.write("SEND_EXPRESS", "SUCCESS")
10. ServerController::fromServiceResult → Response{ok, code, message, data}
11. ProtocolCodec::encodeResponse → "RES|1/0|code|message|3|...\n"
12. sendAll() →
                                     13. SocketClient::receiveFrame
                                         → receiveBuffer_ → 找到 \n
                                     14. ProtocolCodec::decodeResponse → Response
15. ClientApp::printResponse → 显示结果
16. 如 BALANCE_NOT_ENOUGH:
    → 显示余额/费用/缺口
    → 询问充值→RECHARGE→询问重试→SEND_EXPRESS
```

---

## 六、并发流程详解（以并发揽收为例）

```
Client A (线程1)                    Server (CoreMutex)              Client B (线程2)
─────────────────────────────────────────────────────────────────────────────
1. send PICKUP_EXPRESS EX000006                                          
                                    2. worker thread A                
                                       handleClient → recv            
                                       decodeRequest                  
                                       controller.handle              
                                       requireCourierSession ✓        
                                    3. pickupExpress                  
                                       → CoreLock lock(mutex_)        
                                       → 检查状态=WaitingPickup ✓     
                                       → 改状态→WaitingSign            
                                       → admin.deduct(8.00)           
                                       → courier.addIncome(8.00)      
                                       → saveAll                      
                                       → 写日志                       
                                       → CoreLock unlock              
                                    4. RES|1|SUCCESS|... →            5. Client A 显示 SUCCESS
                                                                      6. send PICKUP_EXPRESS EX000006
                                                                      7. worker thread B
                                                                         → CoreLock lock
                                                                         → 检查状态=WaitingSign ✗
                                                                         → STATE_CONFLICT
                                                                         → CoreLock unlock
                                                                      8. RES|0|STATE_CONFLICT|...
9. (已完成)                                                           10. Client B 显示 STATE_CONFLICT
```

---

## 七、数据文件字段映射

### users.txt (8字段)
```
username | name | phone | salt | passwordHash | balance | address | frozen
  0         1      2       3         4            5         6        7
```

### admin.txt (5字段)
```
username | name | salt | passwordHash | balance
  0         1      2         3            4
```

### couriers.txt (8字段)
```
username | name | phone | salt | passwordHash | income | frozen | removed
  0         1      2       3         4            5        6        7
```

### expresses.txt (15字段，向后兼容12-15)
```
id | sender | receiver | courier | sendTime | pickupTime | receiveTime | status | itemType | itemAmount | description | fee | note | ratingScore | ratingComment
 0     1         2         3          4           5             6          7         8           9            10       11    12        13              14
```

### auth_state.txt (4字段)
```
role | username | failedCount | lastFailedTime
  0       1           2              3
```

### notifications.txt (8字段)
```
id | time | targetRole | targetUsername | type | title | content | readFlag
 0    1         2             3           4      5        6         7
```

### operations.log (9字段，链式哈希)
```
seq | time | actorType | actor | action | result | detail | prevHash | currentHash
 0     1        2         3       4        5        6         7           8
```

---

## 八、编码兼容性说明

1. **GBK/UTF-8 混编：** 编译参数 `-finput-charset=UTF-8 -fexec-charset=GBK`，源码 UTF-8，运行时 GBK
2. **数据文件编码：** 以 UTF-8 写入（`std::ofstream` 默认），可能混入 GBK 字符（通过 `_getch` 读取的 Windows 控制台输入）
3. **控制台：** `SetConsoleOutputCP(936)` 设置 GBK 代码页
4. **历史数据残留：** `data/server/users.txt` 第 12 行含 GBK 编码中文（"lwt" 用户），`deserialize` 能正常解析（`StringUtil::unescape` 保留原始字节）
5. **自测残留：** `data/phase1_selftest_*` 目录是早期自测备份，不影响当前运行

---

*注释补充文档生成时间：2026-06-10*
*基于 Phase 9 全量源码阅读与分析*
