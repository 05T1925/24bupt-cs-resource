# V3 助实验收 5 分钟通关脚本

## 0. 演示前准备

操作步骤：

```bat
cd /d "C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v3_network"
build_all.bat
```

预期输出：

```text
[Build] server build success: bin\logistics_v3_server.exe
[Build] client build success: bin\logistics_v3_client.exe
[Build] all targets built successfully.
```

讲解话术：

```text
这里先证明 server 和 client 是两个独立编译产物。工程使用 C++17 和 Winsock，client 不直接读写 data，所有业务都必须经 socket 到 server。
```

## 1. 基础 C/S 证明

操作步骤：

终端 1：

```bat
bin\logistics_v3_server.exe
```

终端 2：

```bat
bin\logistics_v3_client.exe
```

可以再打开终端 3，再运行一次 client。

预期输出：

server：

```text
Logistics V3 Server started.
Phase 8: multi-client concurrency + SessionManager.
Mode: blocking listen
[Server] listening on 127.0.0.1:9000
[Server] client connected: 127.0.0.1:xxxxx
```

client：

```text
Logistics V3 Client started.
Phase 8: concurrency self-test capable client.
1. 注册普通用户
2. 登录
0. 退出
```

讲解话术：

```text
这是传统 C/S 架构，不是单进程菜单拆分。server 独立监听 127.0.0.1:9000，多个 client 可以同时连接。默认模式下 server 每 accept 一个客户端，就创建一个 worker thread 处理该连接，主线程继续监听新连接。
```

## 2. 零信任权限拦截

操作步骤：

终端 1 启动单次 server：

```bat
bin\logistics_v3_server.exe --once
```

终端 2 执行管理员自测：

```bat
bin\logistics_v3_client.exe --selftest-admin
```

预期输出重点：

```text
[Client] user token calls admin dashboard: ERR|PERMISSION_DENIED|...
[Client] query all express: OK|SUCCESS|...
[SelfTest] Phase 6 admin network business passed.
```

讲解话术：

```text
这里先用普通用户登录拿到 USER token，然后故意调用管理员命令 VIEW_DASHBOARD。服务端没有相信客户端菜单，而是从 SessionManager 根据 token 解析 role，发现不是 ADMIN 后直接返回 PERMISSION_DENIED，并写 ADMIN_PERMISSION_DENIED 审计日志。后面再用 admin token 才能正常进入管理员业务。
```

## 3. 快递员业务闭环

操作步骤：

终端 1：

```bat
bin\logistics_v3_server.exe --once
```

终端 2：

```bat
bin\logistics_v3_client.exe --selftest-courier
```

预期输出重点：

```text
[Client] pickup tasks before: OK|SUCCESS|...
[Client] courier A pickup courier B task: ERR|PERMISSION_DENIED|...
[Client] pickup batch own tasks: OK|PARTIAL_SUCCESS|...
[Client] repeat pickup after batch: ERR|STATE_CONFLICT|...
[SelfTest] Phase 7 courier network business passed.
```

讲解话术：

```text
这一步展示物流核心链路：管理员创建快递员，用户寄件，管理员分配，快递员查询本人待揽收任务，再进行批量揽收。服务端在 pickupExpress 中校验 express.courier 必须等于 token 对应的 session.username，因此快递员 A 不能揽收快递员 B 的任务。揽收时状态变为待签收，同时管理员扣 50% 提成，快递员收入增加 50%，这三件事在同一把业务锁保护下完成。
```

## 4. 高并发冲突展示

操作步骤：

终端 1 启动默认持续监听 server：

```bat
bin\logistics_v3_server.exe
```

终端 2 执行并发自测：

```bat
bin\logistics_v3_client.exe --selftest-concurrency
```

预期输出重点：

```text
[Client] concurrent pickup A: OK|SUCCESS|...
[Client] concurrent pickup B: ERR|STATE_CONFLICT|...
[SelfTest] Phase 8 concurrent pickup conflict passed.
```

也可能 A/B 顺序互换，但必须满足：

```text
一个 OK|SUCCESS
一个 ERR|STATE_CONFLICT
```

讲解话术：

```text
这个脚本会打开两个独立 socket 连接，两个连接同时登录同一个快递员账号，并在同一时刻发送 PICKUP_EXPRESS 同一单号。服务端 worker thread 是并发的，但 LogisticsSystem 内部有全局 CoreMutex 保护状态机，所以只有第一个请求能把状态从待揽收改成待签收，第二个请求进来时已经看到状态变化，因此返回 STATE_CONFLICT。这样避免了重复发提成、重复状态流转和文件数据损坏。
```

## 5. 异常断线兜底

操作步骤：

1. 保持终端 1 的 server 默认模式运行。
2. 终端 2 运行 `bin\logistics_v3_client.exe`，登录任意账号。
3. 直接关闭终端 2 窗口，或在客户端菜单输入 `0` 退出。
4. 再打开终端 3 运行 `bin\logistics_v3_client.exe`，确认仍能连接。

预期输出：

server 侧出现类似：

```text
[Server] client disconnected: 127.0.0.1:xxxxx
[Server] cleaned 1 session(s) for 127.0.0.1:xxxxx
```

讲解话术：

```text
每个客户端连接都有独立的 recv 循环。recv 返回 0 表示正常断开，SOCKET_ERROR 表示异常断开，服务端都会退出该连接线程、清理本连接关联 token、关闭 socket。异常只影响当前连接，不会让 server 进程崩溃，也不会影响其他客户端。
```

## 6. 收尾展示点

可选操作：

管理员登录后执行日志校验：

```text
VERIFY_LOG_CHAIN
```

讲解话术：

```text
本项目还保留 V2 的操作日志哈希链，每条日志都包含 prevHash 和 currentHash。验收时可以说明：业务层不仅完成网络化，还保留了原子保存、回滚和日志完整性校验这些工程可靠性设计。
```

## 7. 5 分钟节奏建议

```text
0:00 - 0:40 构建和双进程 C/S
0:40 - 1:30 token 权限拦截
1:30 - 2:40 快递员揽收与资金闭环
2:40 - 3:50 并发抢单冲突
3:50 - 4:30 异常断线不崩溃
4:30 - 5:00 总结协议、Session、锁、原子持久化、日志哈希链
```
