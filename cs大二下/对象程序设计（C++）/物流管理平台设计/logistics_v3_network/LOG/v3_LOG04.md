# v3_LOG04：Phase 3 Server 最小网络闭环完成记录

## 1. 本次目标

在 Phase 2 协议编解码基础上引入真实 Winsock 通信，完成 server/client 双进程 `PING -> PONG` 联调。

## 2. 新增文件

```text
server/SocketServer.h
server/SocketServer.cpp
client/SocketClient.h
client/SocketClient.cpp
```

入口文件已切换为网络模式：

```text
server/main.cpp
client/main.cpp
```

构建脚本已更新：

```text
build_server.bat
build_client.bat
```

## 3. SocketServer 核心职责

`SocketServer` 负责：

- `WSAStartup`
- `socket`
- `bind`
- `listen`
- `accept`
- per-connection `recv`
- 按 `\n` 从缓冲区拆出完整协议帧
- 调用 `ProtocolCodec::decodeRequest`
- PING 路由返回 PONG
- `ProtocolCodec::encodeResponse`
- `sendAll`
- 析构时 `closesocket` 和 `WSACleanup`

## 4. TCP 流式缓冲红线落实

服务端没有假设一次 `recv()` 等于一条完整消息，而是使用：

```text
std::string buffer
recv -> append
while buffer contains '\n':
  frame = buffer.substr(...)
  buffer.erase(...)
  ProtocolCodec::decodeRequest(frame)
```

客户端等待响应时也使用同样思想维护 `receiveBuffer_`，直到读到 `\n` 再解码。

## 5. 当前路由

请求：

```text
REQ|PING||0\n
```

响应：

```text
RES|1|PONG|Server is alive|0\n
```

客户端展示：

```text
OK|PONG|Server is alive
```

## 6. 验证结果

已执行：

```text
build_all.bat
bin/logistics_v3_server.exe --once
bin/logistics_v3_client.exe
```

客户端输出：

```text
Logistics V3 Client started.
Phase 3: send PING and wait for PONG.
[Client] received: OK|PONG|Server is alive
```

服务端输出：

```text
Logistics V3 Server started.
Phase 3: Winsock PING/PONG server.
Mode: single client self-test
[Server] listening on 127.0.0.1:9000
[Server] client connected: 127.0.0.1:xxxxx
[Server] client disconnected: 127.0.0.1:xxxxx
```

说明 V3 已经完成真实双进程 socket 最小闭环。

## 7. 后续 Phase 4 入口

下一阶段建议新增：

```text
server/SessionManager
server/ServerController
LOGIN_USER / LOGIN_COURIER / LOGIN_ADMIN
token 生成与校验
```

此后 PING 路由可从 `SocketServer` 下沉到 `ServerController`。

