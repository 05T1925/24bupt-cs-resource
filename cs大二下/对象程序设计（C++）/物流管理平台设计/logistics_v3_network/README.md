# Logistics V3 Network 物流管理平台网络版

本工程是《物流管理平台设计》题目三的独立网络版程序。它建立在 `logistics_v2` 已完成的单机版业务能力之上，但工程目录、可执行文件、数据目录和运行方式均与 V1/V2 物理隔离。

当前阶段：Phase 9 验收材料与报告支撑完成。V3 网络版已具备完整 C/S 业务链路、token 零信任鉴权、多客户端并发处理和验收演示脚本。

## 1. V3 硬性红线

- 必须采用传统 C/S 架构。
- `server` 与 `client` 必须编译为两个不同的进程。
- 客户端与服务端之间必须使用 socket 通信。
- 禁止使用 RPC 框架替代 socket 协议。
- 客户端不能直接读写 `data/`。
- 客户端不能绕过服务端直接调用 `LogisticsSystem`。
- 服务端不能残留控制台菜单、密码输入、表格渲染等 UI 流程。
- 服务端不能信任客户端传来的当前用户名，后续必须通过 token/session 绑定身份。

## 2. 目录职责

```text
logistics_v3_network/
  common/
    models/      实体和值对象，未来迁移 User/Admin/Courier/Express 等
    protocol/    Request/Response、协议编解码、字段转义
    security/    密码哈希、token、输入校验、安全工具
    service/     LogisticsSystem 业务服务
    storage/     Repository、StorageManager、Logger
  server/        服务端进程入口，Socket 监听、Session、权限路由
  client/        客户端进程入口，菜单、密码隐藏、请求封装、表格展示
  data/          服务端专属数据文件
  docs/          设计文档
  tests/         测试清单和后续自动化测试
  LOG/           开发日志
  bin/           编译产物
```

`common/` 是纯业务与基础库目录，不允许放入 `std::cin`、菜单流程或控制台 UI。`client/` 可以包含交互式输入输出；`server/` 只允许输出必要的启动和运维日志。

## 3. 构建与联调方式

初始化目录：

```bat
init_phase0.bat
```

分别构建两个进程：

```bat
build_server.bat
build_client.bat
```

一次构建全部：

```bat
build_all.bat
```

运行服务端和客户端：

```bat
run_server.bat
run_client.bat
```

当前产物：

```text
bin/logistics_v3_server.exe
bin/logistics_v3_client.exe
```

构建脚本使用 C++17，并保留 V2 验证过的中文编码参数：

```bat
-finput-charset=UTF-8 -fexec-charset=GBK
```

同时链接 Winsock：

```bat
-lws2_32
```

## 4. 最终运行说明

推荐在两个或多个终端中运行，所有命令都在项目根目录执行：

```bat
cd /d "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

启动服务端：

```bat
bin\logistics_v3_server.exe
```

服务端正常输出类似：

```text
Logistics V3 Server started.
Phase 8: multi-client concurrency + SessionManager.
Mode: blocking listen
[Server] listening on 127.0.0.1:9000
```

启动交互式客户端：

```bat
bin\logistics_v3_client.exe
```

默认演示账号：

```text
管理员：admin / Admin0219
普通用户：login_user / User1234
快递员：demo_courier / Courier1234
```

自动联调建议：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-admin

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-courier
```

并发冲突自测需要服务端保持默认持续监听：

```bat
bin\logistics_v3_server.exe
bin\logistics_v3_client.exe --selftest-concurrency
```

说明：自动联调会向 `data/server` 追加演示数据和操作日志，这是为了保留验收证据。

## 5. 架构特性

- 双进程传统 C/S：`server` 和 `client` 是两个独立 exe，通信只通过 Winsock TCP socket。
- 自定义文本协议：`REQ/RES` 帧、`\n` 分帧、字段转义、字段数量校验、8KB 消息长度限制。
- TCP 流式处理：服务端和客户端都维护接收缓冲区，能够处理半包和粘包。
- token 零信任鉴权：登录后服务端生成 token，业务身份只从 `SessionManager` 解析，客户端不能伪造当前用户名。
- 三身份权限隔离：普通用户、快递员、管理员命令分别进入专属路由，越权返回 `PERMISSION_DENIED`。
- 多线程连接模型：服务端默认模式每个 client 连接一个 worker thread，主线程继续 accept 新连接。
- 并发状态保护：`LogisticsSystem` 使用全局 `CoreMutex/CoreLock` 保护状态机、资金流、持久化和回滚。
- 事务式揽收：快递状态变更、管理员扣款、快递员收入增加、保存文件在同一临界区内完成。
- 原子化持久化：`.tmp -> .bak -> 正式文件`，保存失败时业务层恢复旧对象。
- 日志哈希链：`operations.log` 记录 9 字段链式哈希，可用 `VERIFY_LOG_CHAIN` 校验篡改。

## 6. V2 迁移边界

已迁移到 `common/`：

- 实体：`User`、`Admin`、`Courier`、`Express`、`ExpressStatus`
- 多态计费：`ExpressItem`、`NormalItem`、`FragileItem`、`BookItem`
- 查询与统计：`ExpressQueryCondition`、`SystemStatistics`、`CourierPerformance`
- 安全工具：`InputValidator`、`SHA256`、`PasswordHasher`
- 持久化：`StorageManager`、各 Repository、`Logger`
- 业务服务：`LogisticsSystem`

必须留在客户端或重写：

- `ConsoleUI`
- `App`
- 密码隐藏输入
- 菜单流程
- 表格渲染
- 暂停和人工输入逻辑

## 7. 当前验证标准

- `server/main.cpp` 和 `client/main.cpp` 能分别编译。
- `bin/` 下生成两个独立 exe。
- client/server 通过 Winsock TCP 和自定义 `REQ/RES` 协议通信。
- 登录后所有业务身份均由 `SessionManager` token 解析。
- 普通用户、管理员、快递员三类网络业务均可通过客户端触发。
- 快递员揽收只能使用 token 绑定身份，越权揽收和重复揽收会被服务端拒绝并写入审计日志。
- 两个客户端同时揽收同一快递时，只允许一个成功，另一个返回 `STATE_CONFLICT`。
- 客户端断开或异常退出时，服务端关闭该 socket 并清理本连接关联 token，不影响其他客户端。

自动联调：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-admin

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-courier

bin\logistics_v3_server.exe
bin\logistics_v3_client.exe --selftest-concurrency
```

`--selftest-concurrency` 需要服务端使用默认持续监听模式，因为它会打开多个客户端连接并同时抢同一快递单。

## 8. 验收脚本

详细 5 分钟演示脚本见：

```text
docs/V3_DEMO_SCRIPT.md
```

建议演示顺序：

```text
1. 构建并启动 server。
2. 双开 client 证明多进程 C/S。
3. 跑 --selftest-admin 展示 token 越权拦截。
4. 跑 --selftest-courier 展示快递员业务闭环。
5. 跑 --selftest-concurrency 展示并发抢单只成功一次。
6. 关闭一个交互式 client，观察 server 继续运行并打印断开日志。
```

## 9. 各阶段新增功能清单

### 阶段 1-2：基础 C/S 架构与协议

- TCP Winsock socket 通信（127.0.0.1:9000）
- 自定义文本协议：REQ/RES 帧、`\n` 分帧、字段转义、8KB 消息上限
- 流式缓冲区处理半包/粘包
- 三身份登录（USER/COURIER/ADMIN）

### 阶段 3：身份认证与零信任

- Token 生成与 SessionManager 生命周期管理
- 所有业务命令强制 token 校验（`AUTH_REQUIRED`）
- 三身份权限隔离（`PERMISSION_DENIED`）
- 密码连续错误 3 次自动冻结
- 安全审计日志（含越权记录）
- 重复登录拦截（`ALREADY_LOGGED_IN`）
- 断线自动清理会话

### 阶段 4：用户端与快递员端完整业务

- 用户注册、登录、充值、寄件、签收、批量签收
- 快递员揽收、批量揽收、任务查询、绩效查询
- 管理员创建快递员、冻结/解冻、停用、分配
- 自动分配（单条 + 一键全部）
- 资金流转：用户付费 → 平台收款 → 快递员 50% 提成
- 事务式回滚保护

### 阶段 5：站内通知中心

- 7 个业务触点自动生成通知（冻结、分配、揽收、签收、改派、评分）
- 查询全部/仅未读通知
- 标记单条已读
- 未读数量统计
- 登录时自动提示未读数量
- 通知持久化到 `notifications.txt`

### 阶段 6：改派、备注、评分与余额不足流程

- 快递改派（仅 WaitingPickup，含新旧快递员通知）
- 快递备注修改（发件人或管理员，仅 WaitingPickup）
- 评分（收件人，1-5 分，不可重复，仅已签收快递）
- 余额不足时可即时充值并重新提交寄件

### 阶段 7：个人信息管理

- 用户自助修改个人信息（name/phone/address）
- 快递员自助修改个人信息（name/phone）
- 管理员强制修改用户/快递员信息（含冻结状态）
- 用户名即时查重（`CHECK_USERNAME_AVAILABLE`）
- 手机号格式校验（`CHECK_PHONE_AVAILABLE`）
- 所有交互流程支持 q/Q 取消

### 阶段 8：并发安全自测

- 双客户端并发揽收同一快递 → 一个成功、一个 `STATE_CONFLICT`
- `CoreMutex/CoreLock` 自旋锁保护
- 自测客户端 `--selftest-concurrency` 自动复现

## 10. 新增命令清单

### 通用命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `PING` | 无 | 心跳检测 |
| `LOGIN_USER` | username, password | 用户登录 |
| `LOGIN_COURIER` | username, password | 快递员登录 |
| `LOGIN_ADMIN` | username, password | 管理员登录 |
| `LOGOUT` | 无 | 登出 |
| `REGISTER_USER` | username, name, phone, password, address | 用户注册 |
| `CHECK_USERNAME_AVAILABLE` | username | 用户名查重 |
| `CHECK_PHONE_AVAILABLE` | phone | 手机号格式校验 |
| `CHANGE_PASSWORD` | oldPassword, newPassword | 修改密码 |

### 用户命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `QUERY_BALANCE` | 无 | 查询余额 |
| `RECHARGE` | amount | 充值 |
| `SEND_EXPRESS` | receiver, description, itemType, itemAmount [, note] | 寄件 |
| `QUERY_MY_EXPRESS` | 无 | 查询我的快递 |
| `QUERY_WAITING_SIGN` | 无 | 查询待签收 |
| `SIGN_EXPRESS` | expressId | 签收单个 |
| `SIGN_BATCH` | expressId1 expressId2 ... | 批量签收 |
| `UPDATE_MY_PROFILE` | name, phone, address | 修改个人信息 |
| `UPDATE_EXPRESS_NOTE` | expressId, note | 修改备注 |
| `RATE_EXPRESS` | expressId, score, comment | 评分 |

### 快递员命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `QUERY_MY_PICKUP_TASKS` | 无 | 查待揽收 |
| `PICKUP_EXPRESS` | expressId | 揽收单个 |
| `PICKUP_BATCH` | expressId1 expressId2 ... | 批量揽收 |
| `QUERY_MY_TASKS` | 无 | 查询所有任务 |
| `VIEW_MY_PERFORMANCE` | 无 | 个人绩效 |
| `UPDATE_COURIER_PROFILE` | name, phone | 修改个人信息 |

### 管理员命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `CREATE_COURIER` | username, name, phone, password | 新增快递员 |
| `QUERY_USERS` | 无 | 查询用户 |
| `QUERY_COURIERS` | 无 | 查询快递员 |
| `SET_USER_FROZEN` | username, 0/1 | 冻结/解冻用户 |
| `SET_COURIER_FROZEN` | username, 0/1 | 冻结/解冻快递员 |
| `REMOVE_COURIER` | courierUsername | 停用快递员 |
| `ASSIGN_COURIER` | expressId, courierUsername | 分配快递员 |
| `AUTO_ASSIGN_COURIER` | expressId | 自动分配单条 |
| `AUTO_ASSIGN_ALL` | 无 | 一键自动分配 |
| `QUERY_ALL_EXPRESS` | 无 | 查询全部快递 |
| `VIEW_DASHBOARD` | 无 | 统计看板 |
| `VIEW_COURIER_PERFORMANCE` | 无 | 快递员绩效 |
| `QUERY_LOGS` | [actorType] [actor] [action] [result] | 查询日志 |
| `VERIFY_LOG_CHAIN` | 无 | 校验日志哈希链 |
| `ADMIN_UPDATE_USER` | target, name, phone, address, frozen | 修改用户 |
| `ADMIN_UPDATE_COURIER` | target, name, phone, frozen | 修改快递员 |
| `REASSIGN_COURIER` | expressId, newCourier, reason | 改派 |
| `UPDATE_EXPRESS_NOTE` | expressId, note | 修改备注 |

### 通知中心命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `QUERY_MY_NOTIFICATIONS` | [unreadOnly 0/1] | 查询通知 |
| `MARK_NOTIFICATION_READ` | notificationId | 标记已读 |
| `QUERY_UNREAD_NOTIFICATION_COUNT` | 无 | 未读数量 |

## 11. 新增数据文件说明

| 文件 | 格式 | 说明 |
|------|------|------|
| `data/server/users.txt` | 8 字段 pipe 分隔 | 用户数据（含 salt/hash/balance） |
| `data/server/admin.txt` | 5 字段 pipe 分隔 | 管理员数据 |
| `data/server/couriers.txt` | 8 字段 pipe 分隔 | 快递员数据 |
| `data/server/expresses.txt` | 12-15 字段 pipe 分隔 | 快递数据（向后兼容旧格式） |
| `data/server/auth_state.txt` | 4 字段 pipe 分隔 | 认证失败计数 |
| `data/server/notifications.txt` | 8 字段 pipe 分隔 | 通知数据（文件缺失不阻断启动） |
| `data/server/operations.log` | 9 字段 pipe 分隔 | 链式哈希操作日志 |

所有持久化文件采用 `.tmp → .bak → 正式文件` 原子写入策略。

## 12. 演示建议

快速全套自动化验证：

```bat
build_all.bat

start "Server" cmd /k "bin\logistics_v3_server.exe"

bin\logistics_v3_client.exe --selftest-user
bin\logistics_v3_client.exe --selftest-admin
bin\logistics_v3_client.exe --selftest-courier
bin\logistics_v3_client.exe --selftest-concurrency
```

交互式演示建议顺序：

1. 构建 + 启动 server
2. 用户注册 → 查重拦截 → 登录 → 充值 → 寄件 → 余额不足流程
3. 管理员分配快递员 → 越权拦截 → 改派
4. 快递员揽收 → 批量揽收 → 越权揽收拦截
5. 收件人签收 → 评分 → 重复评分拦截
6. 通知中心：查看通知、标记已读
7. 并发测试：双客户端抢单

## 13. 构建与测试命令

```bat
REM 构建
build_all.bat

REM 单测（需先启动 server）
bin\logistics_v3_client.exe --selftest-user
bin\logistics_v3_client.exe --selftest-admin
bin\logistics_v3_client.exe --selftest-courier

REM 并发测（需服务端持续监听）
bin\logistics_v3_client.exe --selftest-concurrency

REM 手工交互
bin\logistics_v3_client.exe
```

## 14. 创新加分方向

1. 心跳保活机制 Heartbeat  
   client 定时发送 `PING`，server 返回 `PONG` 并刷新 session 活跃时间。亮点是能演示网络版对断线、长时间空闲连接和会话过期的处理能力。

2. 优雅的服务端安全退出  
   server 捕获控制台退出信号，停止 accept 新连接，等待正在处理的请求完成，flush 日志并保存数据后退出。亮点是体现服务端工程可靠性，不是直接关闭进程。

3. 客户端断线重连  
   client 检测 `send/recv` 失败后提示重连，重连成功后要求重新登录或尝试恢复 session。亮点是覆盖真实 C/S 系统常见网络抖动场景。

4. 统一错误码和异常保护边界  
   `ServerController` 将协议错误、权限错误、业务错误和未知异常统一转换为 `ERR|code|message`。亮点是避免服务端崩溃，并让助教能清楚看到错误处理体系。
