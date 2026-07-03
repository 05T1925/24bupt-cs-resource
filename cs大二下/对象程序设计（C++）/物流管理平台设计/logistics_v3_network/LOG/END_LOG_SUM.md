# END_LOG_SUM：Logistics V3 Network 最终收口总结

## 0. 文档定位

本文件是 `logistics_v3_network` 在全部开发阶段完成后的最终收口总结，涵盖各阶段的修改优化内容，用作答辩、验收、报告撰写的完整上下文。

当前工程路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network
```

收口日期：2026-06-10

---

## 1. 最终修改文件清单

### common/（纯业务层，无新增修改）

| 文件 | 状态 | 说明 |
|------|------|------|
| `common/models/Entities.h` | 已修改 | 新增 Notification 实体类（8字段） |
| `common/models/Entities.cpp` | 已修改 | 新增 Notification serialize/deserialize |
| `common/protocol/ProtocolCodec.h` | 稳定 | 自定义文本协议编解码 |
| `common/protocol/ProtocolCodec.cpp` | 稳定 | REQ/RES 帧、字段转义 |
| `common/security/InputValidator.h` | 稳定 | 输入校验（用户名/密码/手机号/地址） |
| `common/security/InputValidator.cpp` | 稳定 | 密码强度、格式校验 |
| `common/security/StringUtil.h` | 稳定 | 字符串工具（转义/解析/格式化） |
| `common/security/StringUtil.cpp` | 稳定 | 辅助函数实现 |
| `common/security/HashUtil.h` | 稳定 | SHA-256 哈希 |
| `common/security/HashUtil.cpp` | 稳定 | 哈希实现 |
| `common/security/PasswordHasher.h` | 稳定 | 密码加盐哈希 |
| `common/security/PasswordHasher.cpp` | 稳定 | salt 生成与密码哈希 |
| `common/models/ExpressItem.h` | 稳定 | 多态计费模型 |
| `common/models/ExpressItem.cpp` | 稳定 | NormalItem/FragileItem/BookItem |
| `common/service/ServiceResult.h` | 稳定 | 统一业务返回结构 |
| `common/service/LogisticsSystem.h` | 已修改 | 新增 notification 方法声明、个人信息修改、改派、备注、评分 |
| `common/service/LogisticsSystem.cpp` | 已修改 | 完整实现所有新增业务方法 |
| `common/storage/StorageManager.h` | 稳定 | 原子持久化工具 |
| `common/storage/StorageManager.cpp` | 稳定 | .tmp→.bak→正式 三阶段写入 |
| `common/storage/Repositories.h` | 已修改 | 新增 NotificationRepository 声明 |
| `common/storage/Repositories.cpp` | 已修改 | 新增 NotificationRepository 实现 |
| `common/storage/Logger.h` | 稳定 | 日志哈希链 |
| `common/storage/Logger.cpp` | 稳定 | 9字段链式哈希日志 |

### server/（服务端）

| 文件 | 状态 | 说明 |
|------|------|------|
| `server/main.cpp` | 稳定 | 服务端入口，数据目录 data/server |
| `server/SocketServer.h` | 已修改 | 新增 logRequestSummary、连接统计 |
| `server/SocketServer.cpp` | 已修改 | 完整流式处理、会话清理、安全日志 |
| `server/SessionManager.h` | 已修改 | 新增 hasActiveSession、getSessionSummary |
| `server/SessionManager.cpp` | 已修改 | 重复登录拦截、安全摘要 |
| `server/ServerController.h` | 已修改 | 新增所有命令路由声明 |
| `server/ServerController.cpp` | 已修改 | 完整三身份命令路由 + 通知中心 |

### client/（客户端）

| 文件 | 状态 | 说明 |
|------|------|------|
| `client/main.cpp` | 已修改 | 完整交互菜单、所有业务流程、自测脚本 |
| `client/SocketClient.h` | 稳定 | Socket 客户端封装 |
| `client/SocketClient.cpp` | 稳定 | TCP 收发、缓冲处理 |

### data/（数据文件）

| 文件 | 说明 |
|------|------|
| `data/server/users.txt` | 用户数据（含 salt/hash/balance/address/frozen） |
| `data/server/admin.txt` | 管理员数据 |
| `data/server/couriers.txt` | 快递员数据 |
| `data/server/expresses.txt` | 快递数据（12-15字段向后兼容） |
| `data/server/auth_state.txt` | 认证失败计数 |
| `data/server/notifications.txt` | 通知中心数据（缺失不阻断启动） |
| `data/server/operations.log` | 链式哈希操作日志 |

### docs/（文档）

| 文件 | 说明 |
|------|------|
| `docs/V3_REQUIREMENTS_ANALYSIS.md` | 需求分析 |
| `docs/V3_ARCHITECTURE_PLAN.md` | 架构设计 |
| `docs/V3_DEVELOPMENT_ROADMAP.md` | 开发路线图 |
| `docs/V3_ACCEPTANCE_AND_BONUS.md` | 验收与加分 |
| `docs/V3_PROTOCOL_DESIGN.md` | 协议设计 |
| `docs/V3_DEMO_SCRIPT.md` | 演示脚本 |
| `docs/V3_ACCEPTANCE_DEMO_GUIDE.md` | 验收演示指南 |

---

## 2. 各阶段修改优化总结

### 阶段 1：独立工程初始化

**完成内容：**
- 创建 `logistics_v3_network` 工程目录，与 V1/V2 物理隔离
- 建立 `common/server/client/data/docs/LOG/tests/bin` 目录结构
- 编写 `init_phase0.bat` 初始化脚本
- 确立 C++17 + Winsock 技术栈

**关键决策：**
- 使用 `std::atomic_flag` 自旋锁替代 `std::mutex`（MinGW 兼容性）
- 使用 `-finput-charset=UTF-8 -fexec-charset=GBK` 解决中文编码

### 阶段 2：V2 业务核心迁移 + 自定义协议

**完成内容：**
- 从 V2 迁移 User/Admin/Courier/Express/ExpressItem 实体到 `common/models/`
- 迁移 InputValidator/PasswordHasher/HashUtil/StringUtil 到 `common/security/`
- 迁移 StorageManager/Repositories/Logger 到 `common/storage/`
- 迁移 LogisticsSystem 到 `common/service/`
- 设计并实现 `ProtocolCodec` 自定义文本协议（REQ/RES 帧格式）
- 字段转义（%→%25, |→%7C, \n→%0A）
- 3 个协议安全常量（MaxMessageLength=8192, MaxFieldLength=1024, MaxVectorItems=256）

**红线验证：**
- `common/` 无 std::cin/std::cout/ConsoleUI
- 所有业务输入来自函数参数，输出通过 ServiceResult 返回

### 阶段 3：Winsock 双进程 + Token 会话 + 零信任

**完成内容：**
- 实现 Winsock TCP 服务端（监听 127.0.0.1:9000）
- 实现 SocketClient TCP 客户端
- 实现 SessionManager（token 生成/验证/刷新/销毁）
- Token 生成：SHA-256(username|role|clock|randomA|randomB|this)
- 实现三身份权限路由（requireUserSession/requireAdminSession/requireCourierSession）
- 密码连续 3 次错误自动冻结（USER/COURIER）
- 安全审计日志（越权记录 ADMIN_PERMISSION_DENIED/COURIER_PERMISSION_DENIED）
- 服务端日志不含明文密码和完整 token

**流式处理：**
- 服务端和客户端均维护接收缓冲区
- 按 `\n` 分帧，正确处理半包和粘包
- sendAll() 处理 partial send

### 阶段 4：普通用户 + 管理员网络业务

**完成内容：**
- 用户注册（REGISTER_USER）含服务端实时查重
- 用户登录（LOGIN_USER）含冻结检查
- 余额查询/充值
- 寄件（SEND_EXPRESS）服务端计费
- 查询我的快递/待签收
- 签收/批量签收
- 管理员创建快递员/冻结解冻/停用
- 分配快递员（手动 + 自动单条 + 一键全部）
- 统计看板/快递员绩效
- 日志查询/哈希链校验
- `--selftest-user` 和 `--selftest-admin` 自测脚本

**关键实现：**
- 停用快递员返回冲突任务清单（toRecord 格式）
- 自动分配使用 selectBestCourierIndex() 负载均衡算法
- 所有业务在 CoreMutex 保护内完成 + saveAll 失败回滚

### 阶段 5：站内通知中心

**完成内容：**
- Notification 实体（id/time/targetRole/targetUsername/type/title/content/readFlag）
- NotificationRepository 持久化到 notifications.txt
- 7 个业务触点自动写入通知：
  1. setUserFrozen → USER, ACCOUNT_STATUS_CHANGED
  2. setCourierFrozen → COURIER, ACCOUNT_STATUS_CHANGED
  3. assignCourier → COURIER, TASK_ASSIGNED
  4. pickupExpress → USER(receiver), EXPRESS_PICKED_UP
  5. signExpress → USER(sender)+COURIER, EXPRESS_SIGNED+EXPRESS_COMPLETED
  6. reassignCourier → COURIER(new+old), TASK_REASSIGNED+TASK_REASSIGNED_AWAY
  7. rateExpress → COURIER, RATING_RECEIVED
- 3 个查询/操作命令：QUERY_MY_NOTIFICATIONS / MARK_NOTIFICATION_READ / QUERY_UNREAD_NOTIFICATION_COUNT
- 登录后自动显示未读通知数量
- 通知写入为 best-effort：失败不回滚核心业务
- notifications.txt 缺失时 server 正常启动

**设计决策：**
- 不做服务端实时 push，纯 request/response 拉取模式
- 避免异步消息、菜单打断、并发 send 等复杂问题

### 阶段 6：快递员业务 + 改派/备注/评分

**完成内容：**
- 快递员业务闭环（待揽收查询/单个揽收/批量揽收/任务查询/绩效）
- 快递改派（REASSIGN_COURIER）仅 WaitingPickup
- 快递备注修改（UPDATE_EXPRESS_NOTE）发件人或管理员，仅 WaitingPickup
- 评分（RATE_EXPRESS）收件人 1-5 分，不可重复，仅已签收
- 余额不足即时充值重试流程
- `--selftest-courier` 自测脚本

**资金一致性保障：**
- 揽收事务：校验→扣款→加收入→改状态→保存→写日志
- 任何一步失败即回滚 oldExpress/oldCourier/oldAdmin
- 并发保护：CoreMutex 全局锁 + 状态冲突返回 STATE_CONFLICT

### 阶段 7：个人信息管理 + 即时查重

**完成内容：**
- 用户自助修改个人信息（UPDATE_MY_PROFILE）
- 快递员自助修改个人信息（UPDATE_COURIER_PROFILE）
- 管理员强制修改用户/快递员信息（ADMIN_UPDATE_USER/ADMIN_UPDATE_COURIER）
- 用户名即时查重（CHECK_USERNAME_AVAILABLE）跨角色检查
- 手机号格式校验（CHECK_PHONE_AVAILABLE）
- 重复登录拦截（ALREADY_LOGGED_IN）
- 所有交互流程支持 q/Q 取消

### 阶段 8：并发安全自测

**完成内容：**
- `--selftest-concurrency` 自动复现并发冲突
- 双客户端同时揽收同一快递 → 一个 OK|SUCCESS，一个 ERR|STATE_CONFLICT
- CoreMutex/CoreLock 自旋锁贯穿所有写操作
- 并发提成只执行一次（Mutex 保护资金流转）

---

## 3. 最终新增命令清单（完整版）

### 通用（11 个）

```text
PING                         心跳检测
LOGIN_USER username password 用户登录
LOGIN_COURIER username pwd   快递员登录
LOGIN_ADMIN username pwd     管理员登录
LOGOUT                       登出
REGISTER_USER u n p pwd a   用户注册
CHECK_USERNAME_AVAILABLE u   用户名查重
CHECK_PHONE_AVAILABLE phone  手机号校验
CHANGE_PASSWORD old new      修改密码（三身份通用）
QUERY_MY_NOTIFICATIONS [0/1] 查询通知
QUERY_UNREAD_NOTIFICATION_COUNT 未读通知数
MARK_NOTIFICATION_READ id    标记已读
```

### 用户（8 个）

```text
QUERY_BALANCE                查余额
RECHARGE amount              充值
SEND_EXPRESS r desc type amt 寄件
QUERY_MY_EXPRESS             查我的快递
QUERY_WAITING_SIGN           查待签收
SIGN_EXPRESS id              签收
SIGN_BATCH id1 id2 ...       批量签收
UPDATE_MY_PROFILE n p a      修改个人信息
UPDATE_EXPRESS_NOTE id note  修改备注
RATE_EXPRESS id score cmt    评分
```

### 快递员（6 个）

```text
QUERY_MY_PICKUP_TASKS        查待揽收
PICKUP_EXPRESS id            揽收
PICKUP_BATCH id1 id2 ...     批量揽收
QUERY_MY_TASKS               查所有任务
VIEW_MY_PERFORMANCE          个人绩效
UPDATE_COURIER_PROFILE n p   修改个人信息
```

### 管理员（19 个）

```text
CREATE_COURIER u n p pwd     新增快递员
QUERY_USERS                  查用户
QUERY_COURIERS               查快递员
SET_USER_FROZEN u 0/1        冻结/解冻用户
SET_COURIER_FROZEN u 0/1     冻结/解冻快递员
REMOVE_COURIER u             停用快递员
ASSIGN_COURIER id courier    分配快递员
AUTO_ASSIGN_COURIER id       自动分配单条
AUTO_ASSIGN_ALL              一键自动分配
QUERY_ALL_EXPRESS            查全部快递
VIEW_DASHBOARD               统计看板
VIEW_COURIER_PERFORMANCE     快递员绩效
QUERY_LOGS [filter...]       查日志
VERIFY_LOG_CHAIN             校验日志哈希链
ADMIN_UPDATE_USER t n p a f  修改用户信息
ADMIN_UPDATE_COURIER t n p f 修改快递员信息
REASSIGN_COURIER id c reason 改派
UPDATE_EXPRESS_NOTE id note  修改备注（管理员）
```

**总计：46 个网络命令**

---

## 4. 最终新增数据文件说明

| 文件 | 字段数 | 格式 | 缺失行为 |
|------|--------|------|----------|
| `users.txt` | 8 | username\|name\|phone\|salt\|passwordHash\|balance\|address\|frozen | 启动失败 |
| `admin.txt` | 5 | username\|name\|salt\|passwordHash\|balance | 启动失败 |
| `couriers.txt` | 8 | username\|name\|phone\|salt\|passwordHash\|income\|frozen\|removed | 启动失败 |
| `expresses.txt` | 12-15 | id\|sender\|receiver\|courier\|sendTime\|pickupTime\|receiveTime\|status\|itemType\|itemAmount\|description\|fee[\|note][\|ratingScore][\|ratingComment] | 启动失败 |
| `auth_state.txt` | 4 | role\|username\|failedCount\|lastFailedTime | 启动失败 |
| `notifications.txt` | 8 | id\|time\|targetRole\|targetUsername\|type\|title\|content\|readFlag | 正常启动 |
| `operations.log` | 9 | seq\|time\|actorType\|actor\|action\|result\|detail\|prevHash\|currentHash | 正常启动 |

所有文件均采用原子写入：`.tmp → .bak → 正式文件`。

---

## 5. 所有构建和测试结果

### 构建验证

```text
build_all.bat  → EXIT 0
  build_server.bat → 编译 16 个 .cpp 文件 → 成功
  build_client.bat → 编译 5 个 .cpp 文件  → 成功

编译器：g++ (MinGW)
标准：C++17
参数：-Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK
链接：-lws2_32
```

### 自测结果（2026-06-10 验证通过）

```text
--selftest-user         EXIT 0  Phase 5 普通用户链路
--selftest-admin        EXIT 0  Phase 6 管理员链路
--selftest-courier      EXIT 0  Phase 7 快递员链路
--selftest-concurrency  EXIT 0  Phase 8 并发冲突链路
```

### 合规检查清单（全部通过）

| # | 检查项 | 结果 |
|---|--------|------|
| 1 | build_all.bat 编译成功 | ✅ |
| 2 | 4个 selftest 全部通过 | ✅ |
| 3 | common/ 无 cin/cout/ConsoleUI | ✅ |
| 4 | client 无直接读写 data/server | ✅ |
| 5 | server 日志无明文密码和完整 token | ✅ |
| 6 | notifications.txt 缺失时正常启动 | ✅ |
| 7 | expresses.txt 12-15字段向后兼容 | ✅ |
| 8 | VERIFY_LOG_CHAIN 哈希链可用 | ✅ |
| 9 | 并发揽收一个成功、一个 STATE_CONFLICT | ✅ |
| 10 | 重复登录返回 ALREADY_LOGGED_IN | ✅ |
| 11 | 新增命令全部经过 token 权限校验 | ✅ |
| 12 | 资金流水一致（寄件100%→平台，揽收50%→快递员） | ✅ |
| 13 | 评分不可重复提交（DUPLICATE_RATING） | ✅ |
| 14 | 改派仅允许 WaitingPickup | ✅ |
| 15 | q/Q 取消不发送业务请求 | ✅ |

---

## 6. 已知限制

1. **自旋锁替代标准锁**：当前 MinGW 环境对 `std::mutex`/`std::shared_mutex` 支持不稳定，使用 `std::atomic_flag` 自旋锁。如迁移到现代工具链，建议替换为标准锁。

2. **通知写入非事务性**：`addNotification()` 在 `saveAll()` 之前独立保存，如果后续 `saveAll()` 失败回滚，通知已持久化但业务数据已回滚。这是设计取舍（best-effort），不影响核心业务。

3. **改派不支持 WaitingSign**：已揽收状态的改派需要回滚提成，当前版本未实现此复杂逻辑。

4. **无心跳保活**：session 不会因长时间空闲而过期，仅在客户端断线时清理。

5. **无断线重连**：客户端断线后不会自动重连，需要重新启动客户端。

6. **单服务端实例**：不支持多个 server 进程共享数据目录，并发写文件可能导致数据损坏。

7. **数据文件无加密**：所有持久化数据以明文存储（密码除外，使用 salted SHA-256）。

8. **Express 序列化向后兼容上限**：最大支持 15 字段，未来新增字段需要继续维护兼容性。

---

## 7. 答辩演示建议

### 快速自动化演示（3 分钟）

```bat
build_all.bat
start "Server" cmd /k "bin\logistics_v3_server.exe"
bin\logistics_v3_client.exe --selftest-user
bin\logistics_v3_client.exe --selftest-admin
bin\logistics_v3_client.exe --selftest-courier
bin\logistics_v3_client.exe --selftest-concurrency
```

### 交互式演示流程（5 分钟）

1. **架构展示**：两个 cmd 窗口分别运行 server/client
2. **注册查重**：尝试注册已存在的用户名 → 被拦截
3. **越权演示**：用户登录后尝试访问管理员命令 → PERMISSION_DENIED
4. **业务闭环**：寄件 → 分配 → 揽收 → 签收
5. **余额不足流程**：低余额寄件 → 即时充值 → 重新提交
6. **并发高光**：`--selftest-concurrency` 双客户端抢单
7. **通知中心**：查询通知、标记已读
8. **日志哈希链**：`VERIFY_LOG_CHAIN` 校验完整性

### 答辩话术要点

- **架构亮点**：传统 C/S、自定义协议、零信任、多线程
- **安全亮点**：token 鉴权、越权拦截、审计日志、哈希链防篡改
- **工程亮点**：事务回滚、原子持久化、并发保护、向后兼容
- **体验亮点**：即时查重、余额不足流程、通知中心、q/Q 取消

---

## 8. 最终结论

Logistics V3 Network 已完成从 V2 单机控制台程序到 C/S 网络版平台的完整转型。工程具备：

```text
✅ 双进程 C/S 架构（Winsock TCP）
✅ 自定义 REQ/RES 文本协议（半包/粘包处理）
✅ Token 零信任会话管理
✅ 三身份权限隔离（46 个网络命令）
✅ 事务式资金流转（用户→平台→快递员）
✅ 并发状态保护（CoreMutex + 冲突检测）
✅ 原子化持久化（.tmp→.bak→正式）
✅ 日志哈希链（SHA-256 链式防篡改）
✅ 站内通知中心（7 触点自动生成）
✅ 4 个自动联调脚本（全部通过）
✅ 15 项合规检查（全部通过）
```

**项目状态：验收就绪。**

---

*END_LOG_SUM 生成时间：2026-06-10*
*工程路径：C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network*
