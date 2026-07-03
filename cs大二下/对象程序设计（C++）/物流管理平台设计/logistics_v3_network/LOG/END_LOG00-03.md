# END_LOG00-03：Phase 0-3 全阶段交接日志

## 0. 文档定位

本文件是 `logistics_v3_network` 项目自 2026-06-10 接手后，经 Phase 0（审计）、Phase 1（服务端展示+注册/登录即时校验+CHANGE_PASSWORD）、Phase 2（余额不足充值+重复登录+LOGOUT）、Phase 3（属性修改+快递改派+Express note）四轮迭代后的终态快照。

工程路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network
```

---

## 1. Phase 0：项目审计（2026-06-10 接手）

### 1.1 审计结论

项目处于 **Phase 9 终态**，全部 9 个开发阶段均已完成编码并通过自测回归。核心架构完整且自洽：

```text
client 进程 (logistics_v3_client.exe)
  ├── main.cpp: ClientApp（交互式菜单、隐藏密码输入、TablePrinter 表格渲染）
  ├── SocketClient（sendAll + receiveBuffer_ 按 \n 分帧）
  └── Winsock TCP → 127.0.0.1:9000
        │
        ▼ REQ|command|token|argCount|arg1|...|\n
        ▼ RES|1/0|code|message|recordCount|r1|...|\n
        │
server 进程 (logistics_v3_server.exe)
  ├── main.cpp: ServerApp（启动、初始化、ensureDemoAccounts）
  ├── SocketServer（acceptLoop → CreateThread → handleClient worker thread）
  ├── ServerController（handle → 命令分流：handleLogin / handleUserCommand / handleAdminCommand / handleCourierCommand）
  ├── SessionManager（createSession / getSession / touch / remove，SessionMutex 保护）
  └── LogisticsSystem（CoreMutex 保护全部业务入口）
        ├── UserRepository / AdminRepository / CourierRepository / ExpressRepository / AuthStateRepository
        ├── StorageManager::saveLinesAtomically（.tmp → .bak → 正式文件）
        └── Logger（9 字段哈希链，append-only）
```

### 1.2 架构红线核查

全部 12 项红线均通过：

| 红线 | 状态 |
|------|:---:|
| server/client 是两个独立 exe | ✅ |
| Winsock TCP socket 通信 | ✅ |
| 未使用 RPC/HTTP/Web/B/S | ✅ |
| client 不直接读写 data/ | ✅ |
| client 不绕过 server 调用 LogisticsSystem | ✅ |
| common/ 无 UI/menu/std::cin/std::cout | ✅ |
| 业务身份从 token 解析 | ✅ |
| 不信任客户端传来的 username | ✅ |
| 明文密码不入日志 | ✅ |
| 完整 token 不入日志 | ✅ |
| CoreMutex/CoreLock 未替换 | ✅ |
| 构建脚本可编译 | ✅ |

### 1.3 初始命令清单

**审计时 31/31 命令已全部实现**：游客 4 个、普通用户 8 个、管理员 14 个、快递员 5 个。

全部 4 个 selftest 均通过（user/admin/courier/concurrency）。

### 1.4 初始数据问题

- `users.txt` 第 10 行 `zqq` 中文编码损坏
- 自测数据持续追加导致数据文件膨胀
- admin.balance 偏离初始值（累计寄件收入＞揽收支出）

---

## 2. Phase 1：服务端信息展示 + 注册/登录即时校验 + CHANGE_PASSWORD

### 2.1 修改文件清单

| # | 文件 | 变更类型 |
|---|------|----------|
| 1 | `common/security/InputValidator.h` | 增强 |
| 2 | `common/security/InputValidator.cpp` | 增强 |
| 3 | `common/models/Entities.h` | 新增 setter |
| 4 | `common/models/Entities.cpp` | 新增实现 |
| 5 | `common/service/LogisticsSystem.h` | 新增接口 |
| 6 | `common/service/LogisticsSystem.cpp` | 增强/新增 |
| 7 | `server/SessionManager.h` | 新增接口 |
| 8 | `server/SessionManager.cpp` | 新增实现 |
| 9 | `server/SocketServer.h` | 增强（SessionManager& 参数、原子计数器、logRequestSummary） |
| 10 | `server/SocketServer.cpp` | 增强（连接/断开/请求安全日志） |
| 11 | `server/ServerController.h` | 新增 handler 声明 |
| 12 | `server/ServerController.cpp` | 新增命令路由+handlers |
| 13 | `server/main.cpp` | 增强启动横幅+退出统计 |
| 14 | `client/main.cpp` | 增强注册流程+菜单 |

### 2.2 新增命令

| 命令 | 权限 | 参数 | 返回 |
|------|------|------|------|
| `CHECK_USERNAME_AVAILABLE` | GUEST | `username` | `SUCCESS`/`DUPLICATE`/`INVALID_ARGUMENT` |
| `CHECK_PHONE_AVAILABLE` | GUEST | `phone` | `SUCCESS`/`INVALID_ARGUMENT`（允许多用户共用手机号） |

### 2.3 补强校验规则

| 校验项 | 规则 |
|--------|------|
| **用户名** | 3-32 字符、仅字母数字下划线、服务端查重（跨 user/courier/admin） |
| **手机号** | 11 位数字、以 1 开头 |
| **密码强度** | ≥8 位、≤64 位、必须同时含字母+数字、禁止全空格 |
| **姓名** | 非空、≤64 字符 |
| **地址** | 非空、≤256 字符 |
| **新旧密码** | 不能相同（`PASSWORD_UNCHANGED`） |
| **改密后 salt** | 每次修改密码重新生成 salt |

### 2.4 CHANGE_PASSWORD 增强

- 已扩展至 **USER / COURIER / ADMIN** 三种角色
- 每次修改重新生成 salt + passwordHash
- 错误码：`AUTH_FAILED` / `INVALID_ARGUMENT` / `PASSWORD_UNCHANGED` / `SUCCESS`

### 2.5 服务端新增展示

**启动横幅**：程序名 / 阶段描述 / 平台 / 模式 / 监听地址 / data 目录

**每条请求安全摘要**（不含 token、不含密码）：
```
[Server] 127.0.0.1:57844 cmd=LOGIN_USER session=GUEST| => LOGIN_SUCCESS
[Server] 127.0.0.1:57844 cmd=SEND_EXPRESS session=USER|u61942094 => SUCCESS
```

**连接/断开统计**：active connections / cleaned sessions / session 分级统计

**退出统计**：total requests handled / final active sessions

### 2.6 客户端注册流程优化

1. 输入用户名 → 立即调用 `CHECK_USERNAME_AVAILABLE`
2. 如重复 → 提示重新输入
3. 输入手机号 → 本地格式校验 + `CHECK_PHONE_AVAILABLE`
4. 输入密码 → 本地强度校验
5. 所有通过后 → 发送 `REGISTER_USER`（服务端再次校验）

---

## 3. Phase 2：余额不足充值 + 重复登录检查 + LOGOUT

### 3.1 修改文件清单

| # | 文件 | 变更摘要 |
|---|------|----------|
| 1 | `server/SessionManager.h` | 新增 `hasActiveSession(username, role)`、`countActiveSessions()` |
| 2 | `server/SessionManager.cpp` | 实现上述方法 |
| 3 | `common/service/LogisticsSystem.cpp` | `BALANCE_NOT_ENOUGH` 响应增强：records 含 `balance=`/`fee=`/`shortage=` |
| 4 | `server/ServerController.h` | 新增 `handleLogout` 声明 |
| 5 | `server/ServerController.cpp` | `LOGOUT` 路由+`handleLogin` 重复登录检查 |
| 6 | `client/main.cpp` | 余额不足充值重试流程、LOGOUT 调用、ALREADY_LOGGED_IN 提示、并发测试共享 token |

### 3.2 新增命令

| 命令 | 权限 | 参数 | 说明 |
|------|------|------|------|
| `LOGOUT` | 任何有效 token | 无 | 删除当前 token 对应 session |

### 3.3 新增错误码

| 错误码 | 触发条件 |
|--------|----------|
| `ALREADY_LOGGED_IN` | 同一 username+role 已有活跃 session |

### 3.4 余额不足充值重试流程

```
SEND_EXPRESS → BALANCE_NOT_ENOUGH?
  ├─ 是 → 显示: 当前余额 / 预计费用 / 缺口金额
  │       询问: "是否立即充值？(y/n)"
  │       ├─ y → 输入充值金额 → RECHARGE
  │       │       ├─ 成功 → "是否使用刚才的寄件信息重新提交？"
  │       │       │         ├─ y → 重新 SEND_EXPRESS（保存的 receiver/description/itemType/amountText）
  │       │       │         └─ n → 结束
  │       │       └─ 失败 → "充值失败，寄件已取消。"
  │       └─ n → 结束
  └─ 否 → 正常展示寄件结果
```

### 3.5 重复登录检查

- **检查位置**：`ServerController::handleLogin`，密码验证成功后、createSession 之前
- **检查逻辑**：`sessions_.hasActiveSession(username, role)` → 遍历 sessions_ 匹配 username+role
- **不同 role 不冲突**：同一用户可同时以 USER/COURIER/ADMIN 身份登录（不同 role）
- **断线兜底**：`SocketServer::cleanupClientSessions` 在连接断开时自动删除该连接关联的所有 token
- **LOGOUT 兜底**：客户端主动退出时可调用 LOGOUT 释放 session

### 3.6 并发测试兼容

`--selftest-concurrency` 改为共享 token 策略：登录一次得到 token，两个连接复用同一 token 并发 PICKUP_EXPRESS。既不触发重复登录拦截，又保证并发冲突测试有效性。

---

## 4. Phase 3：属性修改 + 快递改派 + Express note

### 4.1 修改文件清单

| # | 文件 | 变更摘要 |
|---|------|----------|
| 1 | `common/models/Entities.h` | User/Courier 新增 `setName`/`setPhone`/`setAddress`；Express 新增 `note_` 字段 |
| 2 | `common/models/Entities.cpp` | 实现 setter；Express serialize→13 字段；deserialize 兼容 12/13 字段 |
| 3 | `common/service/LogisticsSystem.h` | 新增 5 个接口 |
| 4 | `common/service/LogisticsSystem.cpp` | 实现 5 个方法(均 CoreLock + 回滚 + 校验 + 日志) |
| 5 | `server/ServerController.cpp` | 路由 5 个命令 + handler 实现 |
| 6 | `client/main.cpp` | 用户/快递员/管理员菜单新增对应入口(支持 q/Q 取消) |

### 4.2 新增命令

| 命令 | 权限 | 参数 | 说明 |
|------|------|------|------|
| `UPDATE_MY_PROFILE` | USER | `name` `phone` `address` | 用户改自己信息(username/balance/frozen 不可改) |
| `UPDATE_COURIER_PROFILE` | COURIER | `name` `phone` | 快递员改自己信息(username/income/frozen 不可改) |
| `ADMIN_UPDATE_USER` | ADMIN | `targetUsername` `name` `phone` `address` `frozen(0/1)` | 管理员修改用户属性，frozen 变更写审计日志 |
| `ADMIN_UPDATE_COURIER` | ADMIN | `targetUsername` `name` `phone` `frozen(0/1)` | 管理员修改快递员属性(income 不可改) |
| `REASSIGN_COURIER` | ADMIN | `expressId` `newCourierUsername` `reason` | 改派 WaitingPickup 快递 |

### 4.3 Express note 字段（第 13 字段）

**向后兼容设计**：

- **旧格式（12 字段）**：`id|sender|...|fee`
- **新格式（13 字段）**：`id|sender|...|fee|note`
- `deserialize()` 同时接受 12 和 13 字段 → 旧数据自动 note=""
- `serialize()` 始终输出 13 字段
- `toRecord()` 仅在 note 非空时追加 `;note=...`

### 4.4 改派业务规则

| 快递状态 | REASSIGN_COURIER 行为 |
|----------|----------------------|
| `WaitingPickup (0)` | ✅ 允许改派 |
| `WaitingSign (1)` | ❌ `STATE_CONFLICT`："快递已揽收，当前版本不支持涉及提成回滚的改派。" |
| `Signed (2)` | ❌ `STATE_CONFLICT`："已签收快递不允许改派。" |

**并发安全**：`REASSIGN_COURIER` 与 `PICKUP_EXPRESS` 均在 CoreLock 内，保证：
- 先揽收 → 改派因状态≠WaitingPickup 失败
- 先改派 → 旧快递员揽收因 courier 不匹配失败(PERMISSION_DENIED)

---

## 5. 当前完整命令清单（Phase 0-3 终态）

### 5.1 游客命令（6 个）

| 命令 | 来源 |
|------|------|
| `PING` | Phase 0 |
| `REGISTER_USER` | Phase 0 |
| `LOGIN_USER` | Phase 0 |
| `LOGIN_COURIER` | Phase 0 |
| `LOGIN_ADMIN` | Phase 0 |
| `CHECK_USERNAME_AVAILABLE` | Phase 1 ✨ |
| `CHECK_PHONE_AVAILABLE` | Phase 1 ✨ |

### 5.2 普通用户命令（10 个）

| 命令 | 来源 |
|------|------|
| `QUERY_BALANCE` | Phase 0 |
| `RECHARGE` | Phase 0 |
| `SEND_EXPRESS` | Phase 0 |
| `QUERY_MY_EXPRESS` | Phase 0 |
| `QUERY_WAITING_SIGN` | Phase 0 |
| `SIGN_EXPRESS` | Phase 0 |
| `SIGN_BATCH` | Phase 0 |
| `CHANGE_PASSWORD` | Phase 0 → Phase 1 增强 ✨ |
| `UPDATE_MY_PROFILE` | Phase 3 ✨ |
| `LOGOUT` | Phase 2 ✨ |

### 5.3 管理员命令（19 个）

| 命令 | 来源 |
|------|------|
| `QUERY_USERS` | Phase 0 |
| `SET_USER_FROZEN` | Phase 0 |
| `CREATE_COURIER` | Phase 0 |
| `QUERY_COURIERS` | Phase 0 |
| `REMOVE_COURIER` | Phase 0 |
| `SET_COURIER_FROZEN` | Phase 0 |
| `QUERY_ALL_EXPRESS` | Phase 0 |
| `ASSIGN_COURIER` | Phase 0 |
| `AUTO_ASSIGN_COURIER` | Phase 0 |
| `AUTO_ASSIGN_ALL` | Phase 0 |
| `VIEW_DASHBOARD` | Phase 0 |
| `VIEW_COURIER_PERFORMANCE` | Phase 0 |
| `QUERY_LOGS` | Phase 0 |
| `VERIFY_LOG_CHAIN` | Phase 0 |
| `CHANGE_PASSWORD` | Phase 0 → Phase 1 增强 ✨ |
| `ADMIN_UPDATE_USER` | Phase 3 ✨ |
| `ADMIN_UPDATE_COURIER` | Phase 3 ✨ |
| `REASSIGN_COURIER` | Phase 3 ✨ |
| `LOGOUT` | Phase 2 ✨ |

### 5.4 快递员命令（8 个）

| 命令 | 来源 |
|------|------|
| `QUERY_MY_PICKUP_TASKS` | Phase 0 |
| `PICKUP_EXPRESS` | Phase 0 |
| `PICKUP_BATCH` | Phase 0 |
| `QUERY_MY_TASKS` | Phase 0 |
| `VIEW_MY_PERFORMANCE` | Phase 0 |
| `CHANGE_PASSWORD` | Phase 0 → Phase 1 增强 ✨ |
| `UPDATE_COURIER_PROFILE` | Phase 3 ✨ |
| `LOGOUT` | Phase 2 ✨ |

**总计：42 个命令（原始 31 + Phase 1 新增 2 + Phase 2 新增 1 + Phase 3 新增 5 + LOGOUT 通用于 3 个角色 = 31+2+1+5+3=42）**

实际上 LOGOUT 只是一个命令，但为三种角色各提供菜单入口。**实际独立命令数：31 + 2 + 1 + 5 = 39 个**。

---

## 6. 当前数据文件格式

```
users.txt       → username|name|phone|salt|passwordHash|balance|address|frozen          (8 字段)
admin.txt       → username|name|salt|passwordHash|balance                               (5 字段)
couriers.txt    → username|name|phone|salt|passwordHash|income|frozen|removed            (8 字段)
expresses.txt   → id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee|note  (12/13 字段，兼容)
auth_state.txt  → role|username|failedCount|lastFailedTime                               (4 字段)
operations.log  → seq|time|actorType|actor|action|result|detail|prevHash|currentHash     (9 字段，哈希链)
```

---

## 7. 关键设计确认

### 7.1 协议帧格式

请求：`REQ|command|token|argCount|arg1|arg2|...\n`
响应：`RES|1/0|code|message|recordCount|record1|record2|...\n`

### 7.2 Token 零信任

- 登录成功：`sessions_.createSession(username, role)` → SHA-256 token
- 后续请求：`sessions_.getSession(request.token, session)` → `session.username` / `session.role`
- 所有业务命令使用 `session.username`，不使用客户端参数中的 username

### 7.3 并发保护

```text
CoreMutex / CoreLock     → LogisticsSystem 全局业务锁 (atomic_flag 自旋)
SessionMutex / SessionLock → SessionManager sessions_ 保护
```

### 7.4 多线程连接模型

```text
主线程: accept → CreateThread → CloseHandle → 继续 accept
worker thread: recv 循环 → 协议解析 → ServerController → sendAll → 断开时清理 session + closesocket
```

### 7.5 揽收事务一致性

```text
CoreLock 内：
  1. 校验 expressId 存在
  2. 校验 courier 可用
  3. 校验 express.courier == session.username
  4. 校验 status == WaitingPickup
  5. commission = fee * 0.5
  6. admin.balance -= commission
  7. courier.income += commission
  8. express.status = WaitingSign
  9. express.pickupTime = now
  10. saveAll() → 失败则恢复旧对象快照
  11. logger_.write(PICKUP_EXPRESS SUCCESS)
```

### 7.6 原子持久化

```text
StorageManager::saveLinesAtomically:
  1. 写 file.tmp
  2. 旧 file → file.bak
  3. file.tmp → file
  4. 失败时恢复 bak
```

### 7.7 日志哈希链

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
currentHash = SHA-256(seq|time|actorType|actor|action|result|detail|prevHash)
GENESIS 为初始 prevHash
```

---

## 8. 当前仍暂缓的项目（加分项未落地）

| 项目 | 来源 | 优先级 |
|------|------|:---:|
| 心跳保活 (PING/PONG 自动) | README §9 | 高 |
| 优雅退出 (Graceful Shutdown) | README §9 | 中 |
| 客户端断线重连 | README §9 | 低 |
| Token 过期机制 | V3_PROTOCOL_DESIGN.md §7 | 低 |
| 查询分页 | V3_PROTOCOL_DESIGN.md §7 | 低 |
| 数据清理（自测残留） | 06_10_LOG01 | 低 |
| `saveAll()` 事务性加固 | 06_10_LOG00 | 低 |

---

## 9. 全量回归验证（Phase 0-3 终态）

| 项目 | 结果 |
|------|:---:|
| `build_server.bat` | ✅ 零警告零错误 |
| `build_client.bat` | ✅ 零警告零错误 |
| `--selftest-user` | ✅ PASS |
| `--selftest-admin` | ✅ PASS |
| `--selftest-courier` | ✅ PASS |
| `--selftest-concurrency` | ✅ PASS（共享 token，A=SUCCESS, B=STATE_CONFLICT） |
| 旧 expresses.txt (12字段) 兼容 | ✅ 自动兼容 |
| 改密码后旧密码失效 | ✅ 新 salt 生成 |
| 重复登录拦截 | ✅ ALREADY_LOGGED_IN |
| LOGOUT 释放 session | ✅ SUCCESS |
| 余额不足充值重试 | ✅ 客户端流程完整 |
| Express note 13 字段 | ✅ 向后兼容读写 |

---

## 10. 交接建议

项目当前处于 **Phase 9 + P1/P2/P3 增强** 状态，核心架构和全部必做功能稳定。后续工作建议：

1. **数据清理**：备份 `data/server/` → 重置干净种子数据（删除自测残留和 zqq 乱码行）
2. **心跳保活**：client 定时 PING → server 刷新 lastActiveTime → 超时清理过期 session
3. **优雅退出**：`SetConsoleCtrlHandler` 捕获退出信号 → 停止 accept → 等待活跃线程 → flush 退出
4. **报告撰写**：根据 `docs/V3_ACCEPTANCE_DEMO_GUIDE.md` 和 `docs/V3_DEMO_SCRIPT.md` 完成验收演示

**不可更改的红线**（永久有效）：
- server/client 双进程 C/S
- Winsock TCP socket 通信
- Token Session 零信任鉴权
- common/ 禁止 UI 污染
- client 禁止直接读写 data/
- CoreMutex/CoreLock 不轻易替换
- 明文密码/完整 token 不入日志
- 每次修改后 `build_all.bat` 必须通过
