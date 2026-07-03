# v3_LOG05：Phase 4 登录与 Session 会话管理完成记录

## 1. 本次目标

在 Phase 3 Socket 最小闭环基础上，引入 C/S 网络版必须具备的 token 会话机制，完成三种身份登录路由：

```text
LOGIN_USER
LOGIN_COURIER
LOGIN_ADMIN
```

## 2. 新增核心文件

```text
server/SessionManager.h
server/SessionManager.cpp
server/ServerController.h
server/ServerController.cpp
```

已修改：

```text
server/SocketServer.h
server/SocketServer.cpp
server/main.cpp
client/main.cpp
common/models/Entities.h/.cpp
common/storage/Repositories.h/.cpp
common/service/LogisticsSystem.h/.cpp
build_server.bat
```

## 3. SessionManager 设计

Session 字段：

```text
token
username
role
loginTime
lastActiveTime
```

Token 生成：

```text
SHA-256(username | role | high_resolution_clock ticks | random_device随机数 | this指针)
```

Session 表：

```text
unordered_map<string, Session>
```

并发保护：

```text
SessionMutex + SessionLock
```

说明：当前 MinGW 工具链对 `std::mutex/std::shared_mutex` 支持不稳定，工程使用 `atomic_flag` 自旋锁封装保持可编译和并发保护点。后续可替换为标准读写锁而不改上层接口。

## 4. 登录安全继承

`LogisticsSystem` 已新增：

```text
loginUser
loginCourier
loginAdmin
```

持久化认证状态：

```text
auth_state.txt
role|username|failedCount|lastFailedTime
```

规则：

- 用户和快递员密码错误会累计失败次数。
- 连续失败 3 次自动冻结并持久化。
- 管理员记录失败次数但不自动冻结，避免系统锁死。
- 日志不写明文密码。

## 5. 路由行为

登录成功响应：

```text
RES|1|LOGIN_SUCCESS|message|3|token|role|username
```

客户端收到后缓存：

```text
token_
role_
username_
```

登录失败响应：

```text
AUTH_FAILED
ACCOUNT_FROZEN
PROTOCOL_ERROR
```

## 6. 客户端联调方式

交互式：

```text
run_client.bat
```

命令行快速测试：

```text
bin/logistics_v3_client.exe LOGIN_ADMIN admin Admin0219
bin/logistics_v3_client.exe LOGIN_USER login_user User1234
bin/logistics_v3_client.exe LOGIN_USER freeze_user Wrong001
```

## 7. 已验证结果

管理员正确登录：

```text
[Client] received: OK|LOGIN_SUCCESS|管理员登录成功
[Client] token: 64位SHA-256 token
[Client] role: ADMIN, username: admin
```

三次错误冻结：

```text
1st -> AUTH_FAILED failedCount=1
2nd -> AUTH_FAILED failedCount=2
3rd -> ACCOUNT_FROZEN
```

持久化确认：

```text
auth_state.txt 中 failedCount=3
users.txt 中 frozen=1
```

PING 回归：

```text
[Client] received: OK|PONG|Server is alive
```

说明 Phase 4 没有破坏 Phase 3 的最小网络闭环。

## 8. Phase 0-3 回归检查

- Phase 0：server/client/common 目录与双进程构建仍存在。
- Phase 1：common 无 UI 污染扫描通过。
- Phase 2：ProtocolCodec 仍被 server/client 共用。
- Phase 3：PING/PONG 回归通过。

