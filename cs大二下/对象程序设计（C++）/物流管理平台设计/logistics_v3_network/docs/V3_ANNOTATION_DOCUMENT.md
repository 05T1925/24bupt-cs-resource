# Logistics V3 Network 源码注释文档

> 版本：Phase 9 终态验收版
> 日期：2026-06-10
> 用途：按模块整理全部源码注释，便于答辩展示和后续维护参考
>
> 文档结构对应项目模块划分：
> - 第一章：common/models（实体层）
> - 第二章：common/protocol（协议层）
> - 第三章：common/security（安全工具层）
> - 第四章：common/service（业务服务层）
> - 第五章：common/storage（持久化层）
> - 第六章：server（服务端网络层）
> - 第七章：client（客户端）

---

## 第一章：common/models — 实体层

### 1.1 Entities.h — 核心实体类定义

**文件用途：** 定义物流系统的 6 个核心实体类和快递状态枚举。

**包含类：**

| 类 | 持久化字段 | 文件 | 说明 |
|---|-----------|------|------|
| `ExpressStatus` | — | — | 三态枚举：WaitingPickup(0)→WaitingSign(1)→Signed(2) |
| `User` | 8字段 | users.txt | 普通用户：含 salt/hash/balance/frozen |
| `Admin` | 5字段 | admin.txt | 管理员单例：平台资金池 |
| `Courier` | 8字段 | couriers.txt | 快递员：含 frozen/removed 双重禁用 |
| `Express` | 12-15字段 | expresses.txt | 快递：向后兼容旧格式 |
| `AuthState` | 4字段 | auth_state.txt | 认证失败状态：记录连续错误次数 |
| `Notification` | 8字段 | notifications.txt | 站内通知：best-effort 持久化 |

**ExpressStatus 状态流转：**
```
WaitingPickup(0) → WaitingSign(1) → Signed(2)   // 单向不可逆
```

**User 序列化格式：**
```
username|name|phone|salt|passwordHash|balance|address|frozen
```

**Express 向后兼容：** deserialize 接受 12-15 字段，缺失字段使用默认值。

### 1.2 Entities.cpp — 序列化/反序列化实现

**关键方法：**

| 方法 | 功能 | 校验 |
|------|------|------|
| `User::serialize()` | 8字段 pipe 分隔 + StringUtil::escape | — |
| `User::deserialize()` | 反序列化 | 字段数==8 + parseDoubleStrict(balance) + frozen∈{0,1} |
| `User::deduct()` | 扣款 | balance+0.000001 ≥ amount |
| `Express::serialize()` | 15字段输出 | 所有字段经 escape 转义 |
| `Express::deserialize()` | 12-15字段兼容 | status∈[0,2] + parseDoubleStrict(amount,fee) |
| `Express::toRecord()` | 可读格式 | key=value;key=value 格式 |
| `Express::pickup()` | 揽收状态变更 | status=WaitingSign, pickupTime=now |
| `Express::sign()` | 签收状态变更 | status=Signed, receiveTime=now |

### 1.3 ExpressItem.h — 多态计费模型

**计费规则：**

| 类型 | typeCode | 计费公式 | 实现类 |
|------|----------|----------|--------|
| 普通快递 | "Normal" | 5元/kg × weightKg | NormalItem |
| 易碎品 | "Fragile" | 8元/kg × weightKg | FragileItem |
| 图书 | "Book" | 2元/本 × count | BookItem |

**工厂方法：** `ExpressItemFactory::createItem(type, amount)` → unique_ptr\<ExpressItem\>
无效类型或 amount≤0 返回 nullptr

---

## 第二章：common/protocol — 协议层

### 2.1 ProtocolCodec.h — 协议编解码器

**协议帧格式：**
```
请求帧：REQ|command|token|argCount|arg1|arg2|...\n
响应帧：RES|1/0|code|message|recordCount|record1|record2|...\n
```

**字段转义规则：**
```
%  → %25
|  → %7C
\n → %0A
\r → %0D
```

**协议安全常量：**

| 常量 | 值 | 用途 |
|------|-----|------|
| MaxMessageLength | 8192 | 单帧最大字节数 |
| MaxFieldLength | 1024 | 单字段最大字节数 |
| MaxVectorItems | 256 | 参数/记录数量上限 |

**协议异常：** `ProtocolError(code, message)` — code ∈ {PROTOCOL_ERROR, UNKNOWN_COMMAND, INVALID_ARGUMENT}

**编解码方法：**

| 方法 | 功能 | 输入 | 输出 |
|------|------|------|------|
| `encodeRequest` | Request→REQ帧 | Request结构 | REQ帧文本 |
| `encodeResponse` | Response→RES帧 | Response结构 | RES帧文本 |
| `decodeRequest` | REQ帧→Request | REQ帧文本 | Request结构（抛ProtocolError） |
| `decodeResponse` | RES帧→Response | RES帧文本 | Response结构（抛ProtocolError） |

### 2.2 ProtocolCodec.cpp — 编解码实现

**编码流程：**
1. validateCommand — 校验命令字字符集（仅[A-Z][0-9]_）
2. validateFieldLength — 校验字段长度
3. 校验 args.size() <= MaxVectorItems
4. 输出 "REQ|command|token|argCount|arg1|...|\n"
5. validateMessageLength — 校验整帧长度

**解码流程：**
1. normalizeFrame — 去尾\n/\r + 拒绝未转义换行
2. splitRawFields — 按|切分（不处理转义，关键设计）
3. 校验 fields[0]=="REQ" + fields.size()>=4
4. parseCount(argCount) — 逐字符验证数字
5. 校验 fields.size()==4+argCount
6. 逐个 arg unescape + 长度校验

**转义算法详解：**
- escapeField：逐字符扫描 → %→%25 |→%7C \n→%0A \r→%0D
- unescapeField：逐字符扫描 → 遇%读取后2字符 → 匹配4种序列 → 不匹配抛异常
- splitRawFields 在 unescape 之前执行：避免 %7C 被当作分隔符

---

## 第三章：common/security — 安全工具层

### 3.1 InputValidator — 输入校验

| 校验方法 | 规则 | 返回 |
|----------|------|------|
| `checkUsername` | 长度3-32，字母开头，仅字母数字下划线 | 空串=合法 |
| `checkPhone` | 11位数字，以1开头 | 空串=合法 |
| `checkPasswordStrength` | 长度8-64，含字母+数字，不能全空格 | 空串=合法 |
| `checkName` | 长度1-64，字母/中文/空格 | 空串=合法 |
| `checkAddress` | 长度1-256 | 空串=合法 |
| `passwordUnchanged` | oldPassword==newPassword | true=拒绝 |

**两种接口风格：**
- `check*()` — 返回错误消息字符串（空=合法），用于服务端校验
- `validate*()` — 返回 bool，用于客户端快速判断

### 3.2 PasswordHasher — 密码加盐哈希

```
makeSalt(username+phone)  → 随机32位十六进制盐值
hashPassword(salt, password)  → SHA-256(salt + password)
```

**安全特性：** 每用户独立随机 salt，相同密码不同哈希值

### 3.3 HashUtil — SHA-256 实现

手写 SHA-256 哈希函数，用于：
- 密码哈希（PasswordHasher）
- Token 生成（SessionManager）
- 日志哈希链（Logger）

### 3.4 StringUtil — 字符串工具

| 方法 | 功能 |
|------|------|
| `formatMoney(double)` | 保留2位小数 |
| `formatAmount(double)` | 保留2位小数 |
| `parseDoubleStrict(string, double&)` | 严格解析（拒绝非法字符） |
| `split(string, char)` | 按分隔符切分 |
| `escape(string)` | 转义（%→%25 等） |
| `unescape(string)` | 反转义 |

---

## 第四章：common/service — 业务服务层

### 4.1 ServiceResult.h — 统一返回结构

```cpp
struct ServiceResult {
    bool ok;                    // 操作成功/失败
    std::string code;           // 标准结果码
    std::string message;        // 可读消息
    std::vector<std::string> data;  // 附加数据
};
```

**工厂方法：** `success()` / `successWithData()` / `failure()`

### 4.2 LogisticsSystem.h — 业务核心接口

**CoreMutex/CoreLock：** 基于 `std::atomic_flag` 的自旋锁（MinGW 兼容性方案）

- `CoreMutex::lock()` — test_and_set(memory_order_acquire) 自旋
- `CoreMutex::unlock()` — clear(memory_order_release)
- `CoreLock` — RAII 守卫（构造lock/析构unlock）

**46 个业务方法（按身份分类）：**

| 身份 | 方法数 | 锁保护 | 事务回滚 |
|------|:---:|:---:|:---:|
| 初始化 | 1 | CoreLock | N/A |
| GUEST | 4（登录+注册） | CoreLock | saveAll失败回滚 |
| USER | 12 | CoreLock | saveAll失败回滚 |
| COURIER | 6 | CoreLock | 揽收12步回滚 |
| ADMIN | 18 | CoreLock | saveAll失败回滚 |
| 通知 | 4 | CoreLock | best-effort |
| 审计 | 1 | 无锁 | 日志直接写入 |

### 4.3 LogisticsSystem.cpp — 核心实现

#### [已实现] 登录方法

`loginUser / loginCourier / loginAdmin`：
1. 查找用户索引 → NOT_FOUND
2. 检查 frozen/removed → ACCOUNT_FROZEN
3. 密码哈希校验 → handleAuthFailure(failedCount+1, >=3自动冻结)
4. clearAuthFailure + saveAll + write log

#### [已实现] 揽收事务（pickupExpress）— 最危险的状态变更

**12步原子事务（CoreLock保护）：**
1. 校验 expressId 存在
2. 校验 courier 可用（非 frozen/removed）
3. 校验 express.courier == session.username（权限红线）
4. 校验 express.status == WaitingPickup（状态红线）
5. commission = fee × 0.5
6. admin.deduct(commission)
7. courier.addIncome(commission)
8. express.pickup(now) → status=WaitingSign
9. addNotification → receiver
10. saveAll 5文件
11. 失败 → 恢复 oldExpress/oldCourier/oldAdmin
12. write log

#### [已实现] 寄件方法（sendExpress）

- 服务端多态计费：ExpressItemFactory::createItem → getPrice()
- 资金：user.deduct(fee) + admin.addBalance(fee)
- 余额不足：返回 BALANCE_NOT_ENOUGH + data[balance=, fee=, shortage=]
- 回滚：saveAll失败 → pop_back + 恢复 oldSender + oldAdmin

#### [已实现] 签收方法（signExpress）

- 收件人匹配校验 → PERMISSION_DENIED
- 状态校验（非WaitingPickup + 非重复签收）
- 通知 sender + courier

#### [已实现] 自动分配算法（selectBestCourierIndex）

三优先级排序：
1. 未完成任务数少者优先
2. 收入低者优先
3. username 字典序升序

#### [⚠️ 待完善] 持久化方法（saveAll）

串行保存5文件：user→admin→courier→express→auth_state
短路机制：&& 运算符
已知限制：每个文件独立原子保存但5个文件间非数据库事务

#### [📋 规划项] 通知方法（addNotification）

best-effort 策略：通知独立保存，失败不回滚核心业务

---

## 第五章：common/storage — 持久化层

### 5.1 StorageManager — 原子持久化

**三阶段原子写入（saveLinesAtomically）：**
1. 写 `.tmp` 文件（新建/覆盖）
2. 旧文件 → `.bak`（备份）
3. `.tmp` → 正式文件（rename）
4. 失败恢复：`.bak` → 正式文件

**其他方法：** `ensureDirectory`（逐级_mkdir）、`loadLines`（文件不存在返回true）、`fileExists`

### 5.2 Repositories — 6个实体仓库

| Repository | 实体 | load/save | 特殊处理 |
|------------|------|-----------|----------|
| UserRepository | vector\<User\> | loadLines→deserialize / serialize→saveLinesAtomically | 8字段校验 |
| AdminRepository | Admin单例 | loadLines[0]→deserialize / serialize→saveLinesAtomically | 空→保留默认 |
| CourierRepository | vector\<Courier\> | 同上 | 8字段校验 |
| ExpressRepository | vector\<Express\> | 同上 | 12-15字段兼容 |
| AuthStateRepository | vector\<AuthState\> | 同上 | failedCount非负校验 |
| NotificationRepository | vector\<Notification\> | 同上 | 文件缺失不报错 |

### 5.3 Logger — 哈希链日志

**9字段格式：**
```
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

**哈希链公式：**
```
currentHash = SHA-256(seq|time|actorType|actor|action|result|detail|prevHash)
初始 prevHash = "GENESIS"
```

**方法：**

| 方法 | 功能 | 锁 |
|------|------|:---:|
| `write()` | 追加一条日志（readLastChainState + 计算currentHash + 追加写入） | 无 |
| `verifyHashChain()` | 逐条校验seq/prevHash/currentHash | 无 |
| `queryLogs()` | 4字段筛选查询 | 无 |

---

## 第六章：server — 服务端网络层

### 6.1 SessionManager — 会话管理

**Token 生成算法：**
```
SHA-256(username | role | high_resolution_clock ticks |
        random_device随机数A | random_device随机数B | this指针)
→ 64位十六进制字符串
```

**并发保护：** SessionMutex/SessionLock（atomic_flag自旋锁，独立于CoreMutex）

**关键方法：**

| 方法 | 功能 | 权限 |
|------|------|:---:|
| `createSession` | 生成token+创建Session | SessionLock |
| `getSession` | 根据token查找会话 | SessionLock |
| `touch` | 刷新lastActiveTime | SessionLock |
| `removeSession` | 销毁会话 | SessionLock |
| `hasActiveSession` | 重复登录检查（遍历所有session） | SessionLock |
| `getSessionSummary` | 安全日志用"role\|username"摘要 | SessionLock |

### 6.2 ServerController — 命令路由

**主路由 handle()：**

```
PING → 直接返回PONG
LOGIN_USER/COURIER/ADMIN → handleLogin
REGISTER_USER → handleRegisterUser
CHECK_USERNAME_AVAILABLE/CHECK_PHONE_AVAILABLE → 独立处理
LOGOUT → handleLogout
通知命令 → handleNotificationCommand
用户命令 → handleUserCommand (requireUserSession)
管理员命令 → handleAdminCommand (requireAdminSession + 审计)
快递员命令 → handleCourierCommand (requireCourierSession + 审计)
其他 → UNKNOWN_COMMAND
```

**权限校验（零信任）：**

```
requireAdminSession:
  1. token空 → AUTH_REQUIRED
  2. getSession失败 → AUTH_REQUIRED
  3. role != "ADMIN" → PERMISSION_DENIED + auditSecurityEvent(ADMIN_PERMISSION_DENIED)
  4. touch(token)

requireCourierSession:
  1. token空 → AUTH_REQUIRED
  2. getSession失败 → AUTH_REQUIRED
  3. role != "COURIER" → PERMISSION_DENIED + auditSecurityEvent(COURIER_PERMISSION_DENIED)
  4. touch(token)
```

**重复登录拦截（handleLogin）：**
```
业务登录成功 → hasActiveSession(username, role)
  → true → ALREADY_LOGGED_IN
  → false → createSession → 返回 token+role+username
```

**管理员命令列表（19个）：** CREATE_COURIER, SET_USER_FROZEN, QUERY_USERS, QUERY_COURIERS, REMOVE_COURIER, SET_COURIER_FROZEN, QUERY_ALL_EXPRESS, ASSIGN_COURIER, AUTO_ASSIGN_COURIER, AUTO_ASSIGN_ALL, VIEW_DASHBOARD, VIEW_COURIER_PERFORMANCE, VERIFY_LOG_CHAIN, QUERY_LOGS, CHANGE_PASSWORD, ADMIN_UPDATE_USER, ADMIN_UPDATE_COURIER, REASSIGN_COURIER, UPDATE_EXPRESS_NOTE

**快递员命令列表（7个）：** QUERY_MY_PICKUP_TASKS, PICKUP_EXPRESS, PICKUP_BATCH, QUERY_MY_TASKS, VIEW_MY_PERFORMANCE, CHANGE_PASSWORD, UPDATE_COURIER_PROFILE

**演示账号创建（ensureDemoAccounts）：** demo_user/login_user/freeze_user/phase6_sender/phase6_receiver + demo_courier/phase6_courier + 种子快递

### 6.3 SocketServer — TCP 服务端

**启动流程（start）：**
1. WSAStartup(2.2)
2. socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)
3. bind(host:port) + listen(SOMAXCONN)
4. running_ = true

**多线程模型：**
```
主线程：accept → CreateThread(worker) → CloseHandle → 继续accept
worker：handleClient → recv循环 → 分帧 → decode → handle → encode → send
```

**TCP流式处理（handleClient）：**
```
1. 维护 std::string buffer
2. recv(1024) → buffer.append
3. buffer.size() > MaxMessageLength*2 → PROTOCOL_ERROR + 断开
4. while buffer.contains('\n') → 提取帧 → decodeRequest → controller_.handle → encodeResponse → sendAll
5. recv==0 → 正常断开 → cleanupClientSessions
6. recv==SOCKET_ERROR → 异常断开 → cleanupClientSessions
```

**断线处理（cleanupClientSessions）：**
- 遍历 connectionTokens → controller_.closeSession(token)
- 打印 session 统计：USER/ADMIN/COURIER 人数

**sendAll：** 循环 send，处理 partial send

**安全日志（logRequestSummary）：** 格式 `[Server] clientAddress cmd=COMMAND session=role|username => CODE`

---

## 第七章：client — 客户端

### 7.1 SocketClient — TCP 客户端

**连接流程（connectToServer）：** WSAStartup → socket → connect → 返回成功/失败

**请求发送（sendCommand）：** encodeRequest → sendAll → receiveFrame → decodeResponse

**TCP流式处理（receiveFrame）：**
```
1. 检查 receiveBuffer_ 是否已有完整帧（含\n）
2. 有 → 取出返回
3. 无 → recv(1024) → append
4. 超限 → ProtocolError
5. recv==0 → "server closed connection"
6. recv==SOCKET_ERROR → "recv response failed"
```

### 7.2 ClientApp（client/main.cpp）— 交互客户端

**TablePrinter：** 表格渲染，CJK字符宽度自适应（中文字符按2宽度），超长截断

**4个 Selftest 脚本：**

| Selftest | 验证流程 | 关键断言 |
|----------|----------|----------|
| `runUserSelfTest` | 注册sender+receiver→登录→充值100→寄Fragile 2kg→恶意签收拦截 | fee=16.00, balance=84.00, PERMISSION_DENIED |
| `runAdminSelfTest` | USER token调VIEW_DASHBOARD→ADMIN登录→分配→停用冲突清单→一键分配 | PERMISSION_DENIED, COURIER_HAS_UNFINISHED_TASKS |
| `runCourierSelfTest` | 创建courierA/B→寄3单→分配→越权揽收→批量→重复→绩效 | PERMISSION_DENIED, STATE_CONFLICT, income delta=18.00 |
| `runConcurrencySelfTest` | 双SocketClient+CreateThread+atomic同步→同时PICKUP_EXPRESS同一单 | 1 SUCCESS + 1 STATE_CONFLICT |

**交互菜单层次：**
- mainMenu → registerUserFlow / interactiveLogin
- userMenu（12项）：余额/充值/寄件/查询/签收/批量/密码/信息/备注/评分/通知
- adminMenu（20项）：全部管理员操作
- courierMenu（8项）：揽收/任务/绩效/通知
- notificationMenu（4项）：全部/仅未读/标记已读/未读计数

**关键交互流程：**
- registerUserFlow：用户名→本地校验→CHECK_USERNAME_AVAILABLE服务端查重→姓名→手机号→密码强度→地址→REGISTER_USER
- sendExpressFlow：收件人→描述→备注→类型→数量→SEND_EXPRESS→余额不足→即时充值→重试
- signExpressWithRatingPrompt：签收成功后提示是否立即评分
- readPasswordHidden：_getch()实现密码隐藏输入（Win32）

---

## 附录A：错误码汇总

| 错误码 | 含义 | 触发位置 |
|--------|------|----------|
| `SUCCESS` | 操作成功 | 各业务方法 |
| `PROTOCOL_ERROR` | 协议格式错误 | ProtocolCodec, ServerController |
| `UNKNOWN_COMMAND` | 未知命令 | ServerController::handle |
| `INVALID_ARGUMENT` | 参数不合法 | InputValidator, 各handler |
| `AUTH_REQUIRED` | 需要登录 | require*Session |
| `AUTH_FAILED` | 认证失败 | handleAuthFailure |
| `PERMISSION_DENIED` | 权限不足 | require*Session, pickupExpress, signExpress |
| `ACCOUNT_FROZEN` | 账号已冻结 | loginUser, loginCourier |
| `NOT_FOUND` | 实体不存在 | find*Index失败 |
| `DUPLICATE` | 实体重复 | registerUser, createCourier |
| `DUPLICATE_RATING` | 重复评分 | rateExpress |
| `BALANCE_NOT_ENOUGH` | 余额不足 | sendExpress, pickupExpress |
| `PASSWORD_UNCHANGED` | 新旧密码相同 | changePassword |
| `STATE_CONFLICT` | 状态冲突 | pickupExpress, signExpress, reassignCourier |
| `COURIER_HAS_UNFINISHED_TASKS` | 快递员有未完成任务 | removeCourier |
| `STORAGE_FAILED` | 存储失败 | saveAll, saveLinesAtomically |
| `SERVER_ERROR` | 服务端异常 | SocketServer try-catch |
| `LOG_CHAIN_BROKEN` | 日志哈希链断裂 | verifyHashChain |
| `ALREADY_LOGGED_IN` | 重复登录 | handleLogin |
| `LOGIN_SUCCESS` | 登录成功 | handleLogin |
| `PARTIAL_SUCCESS` | 部分成功 | pickupBatch, signBatch |
| `PONG` | PING响应 | handle |

## 附录B：数据文件字段映射

### users.txt
```
username | name | phone | salt | passwordHash | balance | address | frozen
   0        1      2       3         4            5         6        7
```

### admin.txt
```
username | name | salt | passwordHash | balance
   0        1      2         3            4
```

### couriers.txt
```
username | name | phone | salt | passwordHash | income | frozen | removed
   0        1      2       3         4            5        6        7
```

### expresses.txt (15字段，兼容12-15)
```
id | sender | receiver | courier | sendTime | pickupTime | receiveTime | status | itemType | itemAmount | description | fee | note | ratingScore | ratingComment
 0     1         2         3          4           5             6          7         8           9            10       11    12        13              14
```

### auth_state.txt
```
role | username | failedCount | lastFailedTime
  0       1           2              3
```

### notifications.txt
```
id | time | targetRole | targetUsername | type | title | content | readFlag
 0    1         2             3           4      5        6         7
```

### operations.log (9字段哈希链)
```
seq | time | actorType | actor | action | result | detail | prevHash | currentHash
 0     1        2         3       4        5        6         7           8
```

## 附录C：项目文件清单

```
common/models/     Entities.h/.cpp, ExpressItem.h/.cpp
common/protocol/   ProtocolCodec.h/.cpp
common/security/   HashUtil.h/.cpp, PasswordHasher.h/.cpp,
                   InputValidator.h/.cpp, StringUtil.h/.cpp
common/service/    ServiceResult.h, LogisticsSystem.h/.cpp
common/storage/    StorageManager.h/.cpp, Repositories.h/.cpp, Logger.h/.cpp
server/            main.cpp, SocketServer.h/.cpp, ServerController.h/.cpp,
                   SessionManager.h/.cpp
client/            main.cpp, SocketClient.h/.cpp
data/server/       users.txt, admin.txt, couriers.txt, expresses.txt,
                   auth_state.txt, notifications.txt, operations.log
docs/              V3_*.md (设计文档 + 验收指南)
LOG/               v3_LOG*.md, END_LOG*.md, 06_10_LOG*.md
tests/             V3_TEST_CASES.md
build_*.bat        build_all.bat, build_server.bat, build_client.bat
run_*.bat          run_server.bat, run_client.bat
```

---

*注释文档生成时间：2026-06-10*
*基于 Phase 9 全量源码阅读与分析*
*标注说明：✅已实现 / 📋规划项 / ⚠️待完善*
