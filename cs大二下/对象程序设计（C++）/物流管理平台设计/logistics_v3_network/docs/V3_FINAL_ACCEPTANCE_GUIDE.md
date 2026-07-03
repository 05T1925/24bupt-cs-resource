# Logistics V3 Network 最终验收指南

> 版本：Phase 9 终态验收版
> 日期：2026-06-10
> 工程：`C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network`
> 目标受众：助教验收 / 课程答辩

---

## 0. 项目概览

| 项目 | 内容 |
|------|------|
| 架构 | 传统 C/S 双进程（server.exe + client.exe），Winsock TCP socket 通信 |
| 协议 | 自定义纯文本 REQ/RES 帧 + `\n` 分帧 + 4 字符转义 + 3 安全常量 |
| 语言/标准 | C++17，g++ (MinGW)，编译参数 `-Wall -Wextra -pedantic -O2` |
| 业务层 | 46 个网络命令，覆盖 USER/COURIER/ADMIN 三类角色 |
| 安全 | Token(SHA-256) 零信任 + 三身份权限隔离 + 审计日志 + 密码哈希 |
| 并发 | CoreMutex(atomic_flag 自旋锁) + SessionMutex 双锁保护 |
| 持久化 | .tmp→.bak→正式 三阶段原子写入 + 9字段链式哈希日志 |
| 自测 | 4 个 selftest 脚本全部通过（user/admin/courier/concurrency） |

### 已完成功能 vs 规划项 vs 待完善

| 状态 | 内容 |
|------|------|
| ✅ 已实现 | 全部 46 个网络命令、token 零信任鉴权、多线程并发、CoreMutex 保护、原子持久化、日志哈希链、通知中心、改派/备注/评分、即时查重、余额不足充值流程、q/Q 取消 |
| 📋 规划项 | 心跳保活(PING/PONG 自动机制)、服务端优雅退出、客户端断线重连、Token 过期机制、查询分页 |
| ⚠️ 待完善 | saveAll() 非数据库事务（5个文件逐个保存）、Express ID 基于 size()+1（清空后可能重用）、admin.balance 因自测数据漂移需定期重置 |

---

## 一、构建与运行验证

### 1.1 构建命令

```bat
cd /d "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

**预期输出：**
```
[Build] server build success: bin\logistics_v3_server.exe
[Build] client build success: bin\logistics_v3_client.exe
[Build] all targets built successfully.
```

**验证点：**
- [ ] server 编译 16 个 .cpp 文件（`common/` 全量 + `server/` 全量）
- [ ] client 编译 5 个 .cpp 文件（不链接 `LogisticsSystem.o` 和各 Repository）
- [ ] C++17 + Winsock `-lws2_32` + 中文编码参数
- [ ] 两个独立 exe 文件生成

### 1.2 运行命令

| 命令 | 说明 |
|------|------|
| `bin\logistics_v3_server.exe` | 服务端默认模式（持续监听，多线程 accept） |
| `bin\logistics_v3_server.exe --once` | 服务端单次模式（接受一个客户端后退出） |
| `bin\logistics_v3_client.exe` | 交互式客户端 |
| `bin\logistics_v3_client.exe --selftest-user` | 用户业务自动自测 |
| `bin\logistics_v3_client.exe --selftest-admin` | 管理员业务自动自测 |
| `bin\logistics_v3_client.exe --selftest-courier` | 快递员业务自动自测 |
| `bin\logistics_v3_client.exe --selftest-concurrency` | 并发冲突自动自测（需 server 持续监听） |

### 1.3 双进程隔离证明

1. 终端1：`bin\logistics_v3_server.exe` → 显示监听 127.0.0.1:9000
2. 终端2：`bin\logistics_v3_client.exe` → 显示交互菜单
3. 终端3：再启动一个 client → 仍能连接（多客户端支持）
4. 红线验证：client 不直接读写 `data/`，server 日志不含 token/密码明文

---

## 二、通信协议验证

### 2.1 帧格式

```
请求帧：REQ|command|token|argCount|arg1|arg2|...\n
响应帧：RES|1/0|code|message|recordCount|record1|record2|...\n
```

### 2.2 协议测试清单（10项）

| # | 测试 | 预期 |
|---|------|------|
| 1 | `PING` | `OK|PONG` |
| 2 | 空请求 | `ERR|PROTOCOL_ERROR` |
| 3 | 未知命令 | `ERR|UNKNOWN_COMMAND` |
| 4 | 参数不足 | `ERR|PROTOCOL_ERROR` |
| 5 | 超长字段(>1024) | `ERR|INVALID_ARGUMENT` |
| 6 | 超长消息(>8192) | `ERR|INVALID_ARGUMENT` |
| 7 | 字段含 `\|`/`\n`/`%` | 正确转义还原（%→%25, \|→%7C, \n→%0A） |
| 8 | 连续发两条命令 | 拆成两条分别处理 |
| 9 | 一条命令拆两次 send | 合并为一条处理（buffer 拼接） |
| 10 | 半条帧后断开 | 释放连接不影响其他 client |

---

## 三、认证与权限验证

### 3.1 Token 机制

| 特性 | 实现 |
|------|------|
| Token 生成 | SHA-256(username\|role\|clock ticks\|randomA\|randomB\|this) |
| Token 格式 | 64 位十六进制字符串 |
| 身份解析 | SessionManager::getSession(token) → session.username/role |
| 重复登录 | hasActiveSession → ALREADY_LOGGED_IN |
| 断线清理 | recv==0/SOCKET_ERROR → cleanupClientSessions |
| 日志安全 | logRequestSummary 只输出 "role\|username"，不输出完整 token |

### 3.2 权限矩阵

| 身份 | 可执行命令数 | 越权处理 |
|------|:---:|------|
| GUEST（无 token） | 5（PING/REGISTER/LOGIN_USER/LOGIN_COURIER/LOGIN_ADMIN） | — |
| USER | 14（含 CHANGE_PASSWORD/SIGN_BATCH/UPDATE_PROFILE 等） | — |
| COURIER | 7（含 PICKUP_BATCH/VIEW_MY_PERFORMANCE 等） | PERMISSION_DENIED + 审计日志 |
| ADMIN | 19（含 REASSIGN_COURIER/VERIFY_LOG_CHAIN 等） | PERMISSION_DENIED + 审计日志 |

### 3.3 登录安全验证

| # | 测试 | 预期 |
|---|------|------|
| 1 | 密码错误 | `ERR|AUTH_FAILED` |
| 2 | USER 连续3次错误 | `ERR|ACCOUNT_FROZEN`，users.txt frozen=1 |
| 3 | COURIER 连续3次错误 | `ERR|ACCOUNT_FROZEN`，couriers.txt frozen=1 |
| 4 | ADMIN 连续错误 | 不自动冻结（避免系统锁死） |
| 5 | 无 token 调业务命令 | `ERR|AUTH_REQUIRED` |
| 6 | USER token 调管理员命令 | `ERR|PERMISSION_DENIED` + ADMIN_PERMISSION_DENIED 审计 |
| 7 | COURIER token 调用户命令 | `ERR|PERMISSION_DENIED` |
| 8 | 重复登录 | `ERR|ALREADY_LOGGED_IN` |
| 9 | LOGOUT 后重登 | 允许 |
| 10 | 断线后重登 | 允许（session 已自动清理） |

---

## 四、业务功能验证（46 命令清单）

### 4.1 用户业务（14 个命令）

| 命令 | 关键验证 | 预期 |
|------|----------|------|
| `REGISTER_USER` | 服务端格式校验+查重 | SUCCESS / DUPLICATE |
| `CHECK_USERNAME_AVAILABLE` | 跨 USER/COURIER/ADMIN 查重 | SUCCESS / DUPLICATE |
| `CHECK_PHONE_AVAILABLE` | 11位1开头纯数字 | SUCCESS / INVALID_ARGUMENT |
| `LOGIN_USER` | 密码验证+冻结检查 | LOGIN_SUCCESS / AUTH_FAILED |
| `QUERY_BALANCE` | 需 USER token | 余额 |
| `RECHARGE` | amount>0，回滚保护 | SUCCESS |
| `SEND_EXPRESS` | 服务端计费，余额不足即时充值 | SUCCESS / BALANCE_NOT_ENOUGH |
| `QUERY_MY_EXPRESS` | 仅本人相关 | records |
| `QUERY_WAITING_SIGN` | 仅待签收 | records |
| `SIGN_EXPRESS` | 仅收件人，发件人拦截 | SUCCESS / PERMISSION_DENIED |
| `SIGN_BATCH` | 批量签收，逐单结果 | PARTIAL_SUCCESS |
| `CHANGE_PASSWORD` | 原密码+新密码强度 | SUCCESS / PASSWORD_UNCHANGED |
| `UPDATE_MY_PROFILE` | name/phone/address | SUCCESS |
| `RATE_EXPRESS` | 收件人+已签收+不重复 | SUCCESS / DUPLICATE_RATING |
| `UPDATE_EXPRESS_NOTE` | 发件人+WaitingPickup | SUCCESS / PERMISSION_DENIED |

#### 寄件计费验证（服务端多态计费）

| 类型 | 参数 | 计费公式 | 预期费用 |
|------|------|----------|----------|
| Normal（普通） | 3 kg | 5元×3 | 15.00 |
| Fragile（易碎品） | 2 kg | 8元×2 | 16.00 |
| Book（图书） | 4 本 | 2元×4 | 8.00 |

#### 余额不足即时充值流程

1. 余额<费用 → `ERR|BALANCE_NOT_ENOUGH` + balance/fee/shortage
2. 客户端提示"是否立即充值？" → 选 y
3. 充值成功 → 提示"是否使用原寄件信息重新提交？" → 选 y
4. 重新提交成功 → `OK|SUCCESS` + 新余额

### 4.2 快递员业务（7 个命令）

| 命令 | 关键验证 | 预期 |
|------|----------|------|
| `LOGIN_COURIER` | 密码+冻结+停用检查 | LOGIN_SUCCESS |
| `QUERY_MY_PICKUP_TASKS` | 仅本人+WaitingPickup | records |
| `PICKUP_EXPRESS` | 12步原子事务+越权拦截+状态冲突 | SUCCESS / PERMISSION_DENIED / STATE_CONFLICT |
| `PICKUP_BATCH` | 批量揽收，逐单结果 | PARTIAL_SUCCESS |
| `QUERY_MY_TASKS` | 本人全部任务 | records |
| `VIEW_MY_PERFORMANCE` | 完成率+收入+评分 | performance record |
| `UPDATE_COURIER_PROFILE` | name/phone | SUCCESS |

#### 揽收12步原子事务

```
CoreLock 保护下：
1. 校验 expressId 存在 → NOT_FOUND
2. 校验 courier 可用（非 frozen/removed）→ ACCOUNT_FROZEN
3. 校验 express.courier == session.username → PERMISSION_DENIED + 审计日志
4. 校验 express.status == WaitingPickup → STATE_CONFLICT + 审计日志
5. commission = fee × 0.5
6. admin.deduct(commission)  → BALANCE_NOT_ENOUGH（兜底保护）
7. courier.addIncome(commission)
8. express.pickup(now)  → status = WaitingSign
9. addNotification → receiver（EXPRESS_PICKED_UP）
10. saveAll 5文件
11. 失败 → 恢复 oldExpress/oldCourier/oldAdmin 快照
12. logger_.write(PICKUP_EXPRESS SUCCESS)
```

### 4.3 管理员业务（19 个命令）

| 命令 | 关键验证 | 预期 |
|------|----------|------|
| `CREATE_COURIER` | 格式校验+查重 | SUCCESS / DUPLICATE |
| `QUERY_USERS` | 全部用户列表 | records |
| `QUERY_COURIERS` | 含未完成任务计数 | records |
| `SET_USER_FROZEN` | 发 ACCOUNT_STATUS_CHANGED 通知 | SUCCESS |
| `SET_COURIER_FROZEN` | 发 ACCOUNT_STATUS_CHANGED 通知 | SUCCESS |
| `REMOVE_COURIER` | 有未完成任务→冲突清单 | SUCCESS / COURIER_HAS_UNFINISHED_TASKS |
| `ASSIGN_COURIER` | 手动分配（仅 WaitingPickup） | SUCCESS |
| `AUTO_ASSIGN_COURIER` | 自动分配（三优先级算法） | SUCCESS |
| `AUTO_ASSIGN_ALL` | 一键全部，逐单结果 | SUCCESS + results |
| `QUERY_ALL_EXPRESS` | 全部快递 | records |
| `VIEW_DASHBOARD` | 8项指标（余额/预估支出/各状态/各类型） | dashboard |
| `VIEW_COURIER_PERFORMANCE` | 全部快递员绩效排行 | records |
| `QUERY_LOGS` | 4可选筛选条件 | records |
| `VERIFY_LOG_CHAIN` | 哈希链完整性 | SUCCESS / LOG_CHAIN_BROKEN |
| `ADMIN_UPDATE_USER` | 含冻结状态修改 | SUCCESS |
| `ADMIN_UPDATE_COURIER` | 含冻结状态修改 | SUCCESS |
| `REASSIGN_COURIER` | 仅 WaitingPickup，双向通知 | SUCCESS / STATE_CONFLICT |
| `UPDATE_EXPRESS_NOTE` | 管理员版 | SUCCESS |
| `CHANGE_PASSWORD` | 管理员密码修改 | SUCCESS |

#### 自动分配算法（selectBestCourierIndex）

```
三优先级排序：
1. 未完成任务数少者优先（= WaitingPickup + WaitingSign）
2. 收入(income)低者优先
3. username 字典序升序
排除：frozen=true 或 removed=true 的快递员
```

---

## 五、通知中心验证

### 5.1 7 个业务触点

| 业务动作 | 通知目标 | 通知类型 |
|----------|----------|----------|
| 冻结/解冻用户 | USER | ACCOUNT_STATUS_CHANGED |
| 冻结/解冻快递员 | COURIER | ACCOUNT_STATUS_CHANGED |
| 分配快递员 | COURIER | TASK_ASSIGNED |
| 快递员揽收 | USER(receiver) | EXPRESS_PICKED_UP |
| 用户签收 | USER(sender)+COURIER | EXPRESS_SIGNED + EXPRESS_COMPLETED |
| 改派快递 | COURIER(new+old) | TASK_REASSIGNED + TASK_REASSIGNED_AWAY |
| 评分 | COURIER | RATING_RECEIVED |

### 5.2 通知命令

| 命令 | 功能 | 权限 |
|------|------|------|
| `QUERY_MY_NOTIFICATIONS [0/1]` | 查询通知（可选 unreadOnly） | 三身份通用 |
| `MARK_NOTIFICATION_READ id` | 标记已读 | 三身份通用 |
| `QUERY_UNREAD_NOTIFICATION_COUNT` | 未读数量 | 三身份通用 |

**验证点：**
- [ ] 通知按 targetRole+targetUsername 隔离
- [ ] notifications.txt 缺失时 server 正常启动
- [ ] 通知写入失败不回滚核心业务（best-effort）
- [ ] 登录后自动显示未读通知数

---

## 六、并发安全验证

### 6.1 并发测试

| # | 测试 | 命令 | 预期 |
|---|------|------|------|
| 1 | 双客户端并发揽收 | `--selftest-concurrency` | 一个OK\|SUCCESS + 一个ERR\|STATE_CONFLICT |
| 2 | 并发揽收资金不重复 | 验证 admin balance + courier income | 提成只执行一次 |
| 3 | 并发审计日志 | QUERY_LOGS | SUCCESS + COURIER_PICKUP_STATE_CONFLICT |
| 4 | 双客户端并发签收 | 同上逻辑 | 一个成功一个STATE_CONFLICT |
| 5 | 管理员分配 vs 快递员揽收并发 | 数据不损坏 |
| 6 | 多用户同时寄件 | 快递单号不重复 |
| 7 | 多线程同时写日志 | 哈希链不破坏 |
| 8 | 多客户端同时查询 | server 不阻塞 accept |

### 6.2 CoreMutex 保护范围

所有 LogisticsSystem 公共方法入口均有 `CoreLock lock(mutex_)`，读操作和写操作均加锁。

### 6.3 并发自测命令

```bat
REM 终端1：启动服务端（持续监听）
bin\logistics_v3_server.exe

REM 终端2：运行并发自测
bin\logistics_v3_client.exe --selftest-concurrency
```

---

## 七、数据文件与日志验证

### 7.1 数据文件格式

| 文件 | 字段数 | 格式 | 原子写入 | 缺失行为 |
|------|--------|------|----------|----------|
| `users.txt` | 8 | username\|name\|phone\|salt\|hash\|balance\|address\|frozen | ✅ | 启动失败 |
| `admin.txt` | 5 | username\|name\|salt\|hash\|balance | ✅ | 启动失败→自动创建 |
| `couriers.txt` | 8 | username\|name\|phone\|salt\|hash\|income\|frozen\|removed | ✅ | 启动失败 |
| `expresses.txt` | 12-15 | 向后兼容旧格式 | ✅ | 启动失败 |
| `auth_state.txt` | 4 | role\|username\|failedCount\|lastFailedTime | ✅ | 启动失败 |
| `notifications.txt` | 8 | id\|time\|targetRole\|targetUsername\|type\|title\|content\|readFlag | ✅ | 正常启动 |
| `operations.log` | 9 | seq\|time\|actorType\|actor\|action\|result\|detail\|prevHash\|currentHash | append | 正常启动 |

### 7.2 日志哈希链验证

```
currentHash = SHA-256(seq|time|actorType|actor|action|result|detail|prevHash)
初始 prevHash = "GENESIS"
```

| 测试 | 预期 |
|------|------|
| `VERIFY_LOG_CHAIN` | `OK|SUCCESS|日志哈希链完整。` |
| 篡改一条记录 | `ERR|LOG_CHAIN_BROKEN|日志 currentHash 校验失败。` |
| 删除一行 | `ERR|LOG_CHAIN_BROKEN|日志序号断裂。` |

### 7.3 原子持久化验证

```
.tmp → .bak → 正式文件（rename）
失败恢复：.bak → 正式文件
```

### 7.4 finance_logs.txt

**当前状态**：工程中不存在此文件。资金流水通过 `operations.log` 中的 PICKUP_EXPRESS/RECHARGE/SEND_EXPRESS 记录追踪。每条揽收日志的 detail 字段包含 commission 金额，可用于资金流水交叉验证。

---

## 八、Selftest 覆盖范围

### 8.1 自动覆盖

| Selftest | 覆盖命令 | 关键断言 |
|----------|----------|----------|
| `--selftest-user` | REGISTER_USER, LOGIN_USER, RECHARGE, SEND_EXPRESS, SIGN_EXPRESS, QUERY_MY_EXPRESS | fee=16.00, balance=84.00, PERMISSION_DENIED |
| `--selftest-admin` | LOGIN_USER, VIEW_DASHBOARD, LOGIN_ADMIN, QUERY_ALL_EXPRESS, ASSIGN_COURIER, REMOVE_COURIER, AUTO_ASSIGN_ALL, VIEW_COURIER_PERFORMANCE | PERMISSION_DENIED, COURIER_HAS_UNFINISHED_TASKS |
| `--selftest-courier` | LOGIN_ADMIN, CREATE_COURIER, REGISTER_USER, LOGIN_USER, RECHARGE, SEND_EXPRESS, ASSIGN_COURIER, LOGIN_COURIER, QUERY_MY_PICKUP_TASKS, PICKUP_EXPRESS, PICKUP_BATCH, VIEW_MY_PERFORMANCE | PERMISSION_DENIED, STATE_CONFLICT, income delta=18.00 |
| `--selftest-concurrency` | LOGIN_ADMIN, CREATE_COURIER, REGISTER_USER, SEND_EXPRESS, ASSIGN_COURIER, PICKUP_EXPRESS×2 | 1 SUCCESS + 1 STATE_CONFLICT |

### 8.2 手动验证建议

| 场景 | 操作 |
|------|------|
| 交互式完整链路 | 注册→登录→充值→寄件→管理员分配→快递员揽收→收件人签收→评分 |
| 越权演示 | USER 登录后尝试 VIEW_DASHBOARD → PERMISSION_DENIED |
| 余额不足流程 | 低余额寄件→即时充值→重新提交 |
| 通知中心 | 查看通知→标记已读→刷新未读数 |
| 日志哈希链 | 管理员登录→VERIFY_LOG_CHAIN |
| q/Q 取消 | 任意交互流程输入 q 取消 |

---

## 九、资金流水一致性验证

| 验证项 | 公式 | 数据源 |
|--------|------|--------|
| 平台余额 | admin.balance = Σ(寄件fee) - Σ(揽收fee×0.5) | admin.txt |
| 快递员收入 | courier.income = Σ(揽收fee×0.5) | couriers.txt |
| 用户余额 | user.balance = 充值 - Σ(寄件fee) | users.txt |
| 并发揽收 | 提成只执行一次 | CoreMutex 保护 |

---

## 十、异常与边界验证

| # | 场景 | 预期 |
|---|------|------|
| 1 | server 未启动 | client 提示 "failed to connect" |
| 2 | client 异常退出 | server 打印 "client disconnected" + 清理 session |
| 3 | 向自己寄件 | `ERR|INVALID_INPUT` |
| 4 | 收件人不存在 | `ERR|NOT_FOUND` |
| 5 | 冻结用户登录 | `ERR|ACCOUNT_FROZEN` |
| 6 | 签收待揽收快递 | `ERR|STATE_CONFLICT` |
| 7 | 重复签收 | `ERR|STATE_CONFLICT` |
| 8 | 改派 WaitingSign 快递 | `ERR|STATE_CONFLICT` |
| 9 | 重复评分 | `ERR|DUPLICATE_RATING` |
| 10 | q 取消交互 | 不发送请求 |

---

## 十一、完整验收脚本

### 11.1 一键自动化演示（3分钟）

```bat
build_all.bat
start "Server" cmd /k "bin\logistics_v3_server.exe"
bin\logistics_v3_client.exe --selftest-user
bin\logistics_v3_client.exe --selftest-admin
bin\logistics_v3_client.exe --selftest-courier
bin\logistics_v3_client.exe --selftest-concurrency
```

### 11.2 交互式演示流程（5分钟）

```
1. 构建+启动server（证明双进程）
2. 注册查重→被拦截
3. 越权演示→USER token调管理员命令→PERMISSION_DENIED
4. 业务闭环→寄件→分配→揽收→签收
5. 余额不足→即时充值→重试寄件
6. 并发高光→双客户端抢单
7. 通知中心→查看/标记/计数
8. 日志哈希链→VERIFY_LOG_CHAIN
```

### 11.3 最终回归指令

```bat
build_all.bat
bin\logistics_v3_server.exe --once && bin\logistics_v3_client.exe --selftest-user
bin\logistics_v3_server.exe --once && bin\logistics_v3_client.exe --selftest-admin
bin\logistics_v3_server.exe --once && bin\logistics_v3_client.exe --selftest-courier
REM 并发需手动：先启动 server，再运行 selftest-concurrency
```

---

## 十二、合规清单

| # | 检查项 | 状态 |
|---|--------|:---:|
| 1 | build_all.bat 编译成功 | ✅ |
| 2 | 4 个 selftest 全部 EXIT 0 | ✅ |
| 3 | common/ 无 cin/cout/ConsoleUI | ✅ |
| 4 | client 无直接读写 data/server | ✅ |
| 5 | server 日志无明文密码/完整 token | ✅ |
| 6 | notifications.txt 缺失正常启动 | ✅ |
| 7 | expresses.txt 12-15字段兼容 | ✅ |
| 8 | VERIFY_LOG_CHAIN 哈希链可用 | ✅ |
| 9 | 并发揽收 1 SUCCESS + 1 STATE_CONFLICT | ✅ |
| 10 | 重复登录 ALREADY_LOGGED_IN | ✅ |
| 11 | 46命令全部经 token 校验 | ✅ |
| 12 | 资金流水一致 | ✅ |
| 13 | 评分不可重复 | ✅ |
| 14 | 改派仅 WaitingPickup | ✅ |
| 15 | q/Q 取消不发送请求 | ✅ |

---

*验收指南生成时间：2026-06-10*
*基于 Phase 9 全量源码阅读与分析*
