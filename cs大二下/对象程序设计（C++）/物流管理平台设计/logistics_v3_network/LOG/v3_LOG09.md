# v3_LOG09：Phase 8 多客户端并发与异常防线完成记录

## 1. 本次目标

在 Phase 0-7 网络业务全部跑通的基础上，将服务端从单连接顺序处理升级为多客户端并发连接模型，并验证并发状态下重复揽收只能成功一次。

## 2. 多客户端连接模型

`SocketServer::acceptLoop` 默认监听模式已升级：

```text
accept 一个 client socket
为该连接创建 Win32 worker thread
worker thread 执行 recv / 协议解析 / ServerController 路由
主线程立即回到 accept，继续接收新客户端
```

当前工具链缺少可用的 `std::thread` 和 `std::mutex`，实际采用 Win32 `CreateThread`。线程创建成功后立即 `CloseHandle`，由系统回收线程对象，避免主线程阻塞等待。

`--once` 模式仍保持单连接同步处理，保证 Phase 3-7 自动联调脚本稳定。

## 3. 连接断开与 Session 清理

每个连接线程维护本连接出现过的 token：

```text
登录成功响应中的 token
后续请求携带的 token
```

当 `recv == 0` 或 `recv == SOCKET_ERROR` 时：

```text
退出该客户端线程循环
清理本连接关联 token
closesocket(clientSocket)
打印断开日志
```

协议异常、业务异常和未知异常不会抛出到进程外，而是转换为结构化响应：

```text
ERR|PROTOCOL_ERROR|...
ERR|SERVER_ERROR|...
```

## 4. 半包/粘包防线复核

服务端仍使用 per-connection buffer：

```text
recv bytes
append buffer
while buffer contains '\n':
  extract one frame
  decode request
  route and respond
```

缓冲区超过协议限制时返回 `PROTOCOL_ERROR` 并关闭该连接，避免恶意半包长期撑大内存。

## 5. 业务并发锁

`LogisticsSystem` 保留全局 `CoreMutex + CoreLock` 保护业务入口。当前 MinGW 工具链不支持标准 `std::mutex`，因此仍使用 `std::atomic_flag` 自旋锁实现 RAII 互斥。

修改型业务包括：

```text
REGISTER_USER
RECHARGE
SEND_EXPRESS
ASSIGN_COURIER
AUTO_ASSIGN_COURIER
AUTO_ASSIGN_ALL
PICKUP_EXPRESS
SIGN_EXPRESS
CREATE_COURIER
REMOVE_COURIER
SET_COURIER_FROZEN
```

均在服务层锁内完成状态检查、资金变更、持久化和回滚。

## 6. 并发抢单自测

新增客户端自动联调：

```text
bin\logistics_v3_server.exe
bin\logistics_v3_client.exe --selftest-concurrency
```

流程：

```text
管理员创建快递员
注册发件/收件用户
发件用户充值并寄出 Fragile 2kg 快递
管理员分配给该快递员
两个独立 SocketClient 连接分别登录同一快递员
两个 worker 线程同时发送 PICKUP_EXPRESS 同一单号
```

验收结果：

```text
一个请求 OK|SUCCESS
另一个请求 ERR|STATE_CONFLICT
server 不崩溃
```

## 7. Phase 0-7 回归

已回归：

```text
build_all.bat
--selftest-user
--selftest-admin
--selftest-courier
--selftest-concurrency
```

结果均通过。下一阶段进入 Phase 9，重点整理验收脚本、报告材料和演示说明。
