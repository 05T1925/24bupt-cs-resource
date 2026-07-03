# Logistics V3 网络代码零基础讲解

## 1. 先记住五个词

### IP

IP 用于定位一台主机。项目使用 `127.0.0.1`，表示“本机”，所以客户端和服务端运行在同一台电脑。

### 端口

端口用于定位主机中的某个网络程序。服务端监听 `9000` 端口，客户端也必须连接 `127.0.0.1:9000`。

### Socket

Socket 是程序操作网络连接的句柄。可以把它理解为网络通信对象：

- 服务端监听 Socket：等待别人连接。
- 服务端客户端 Socket：与某一个客户端通信。
- 客户端 Socket：与服务端通信。

### TCP

TCP 保证字节按顺序、可靠地到达，但不保存消息边界。它不知道哪里是一条 Request 的结束。

### 协议

协议是客户端和服务端对字节含义的共同约定。本项目规定每条消息以换行结束，字段用 `|` 分隔。

## 2. 为什么有两个服务端 Socket

`listenSocket_` 只负责监听：

```text
socket -> bind -> listen
```

当客户端连接时：

```text
clientSocket = accept(listenSocket_)
```

`accept` 不会把监听 Socket 变成客户端 Socket，而是返回一个新的 Socket。之后：

- `listenSocket_` 继续接受其他客户端。
- `clientSocket` 专门与当前客户端执行 `send/recv`。

这就是服务端能同时接待多个客户端的基础。

## 3. 客户端如何建立连接

`SocketClient::connectToServer` 的顺序：

1. `WSAStartup`：初始化 Windows 网络库。
2. `socket`：创建 TCP Socket。
3. `setsockopt`：设置接收超时。
4. 填写 `sockaddr_in`：IP、端口和 IPv4 类型。
5. `connect`：主动连接服务端。

其中：

- `AF_INET` 表示 IPv4。
- `SOCK_STREAM` 表示字节流。
- `IPPROTO_TCP` 表示 TCP。
- `htons` 把端口转换为网络字节序。

## 4. 服务端如何启动

`SocketServer::start` 的顺序：

1. `WSAStartup`
2. `socket`
3. `bind`
4. `listen`

`bind` 的含义是把 Socket 与本地地址绑定。若 9000 端口已被其他程序占用，通常会在这里失败。

`listen` 之后，Socket 才成为监听 Socket。真正等待客户端发生在 `accept`。

## 5. 一次请求的完整过程

以查询余额为例：

```text
ClientApp
  构造 Request{QUERY_BALANCE, token, []}
        |
        v
ProtocolCodec::encodeRequest
  REQ|QUERY_BALANCE|token|0\n
        |
        v
SocketClient::sendAll
        |
        v
TCP 字节流
        |
        v
SocketServer::recv + buffer
        |
        v
按 \n 取出一条完整帧
        |
        v
ProtocolCodec::decodeRequest
        |
        v
ServerController::handle
        |
        v
SessionManager 校验 token
        |
        v
LogisticsSystem::queryBalance
        |
        v
Response 原路返回客户端
```

## 6. `send` 为什么要循环

假设要发送 1000 字节，`send` 成功返回 400，只代表本次发送了前 400 字节，不代表剩余 600 字节自动完成。

所以 `sendAll` 维护：

```text
sentTotal = 已发送总数
剩余长度 = payload.size() - sentTotal
```

循环到 `sentTotal == payload.size()` 才表示完整帧已经交给 TCP。

## 7. `recv` 返回值是什么意思

```text
received > 0
```

本次收到 `received` 个有效字节。

```text
received == 0
```

对端正常关闭连接。

```text
received == SOCKET_ERROR
```

接收失败，应调用 `WSAGetLastError` 查看原因。

临时数组不一定以 `\0` 结尾，所以代码使用：

```cpp
buffer.append(recvBuffer, recvBuffer + received);
```

而不是把它直接当普通 C 字符串追加。

## 8. 什么是半包和粘包

发送方连续发送：

```text
REQ-A\n
REQ-B\n
```

接收方可能得到：

```text
第一次 recv: REQ-
第二次 recv: A\nREQ-B\n
```

这同时出现了半包和粘包。

解决方法不是猜测每次 `recv` 对应几条消息，而是：

1. 把收到的字节追加到连接缓冲区。
2. 查找 `\n`。
3. 找到就取出一条完整帧。
4. 剩余内容继续保留。
5. 找不到就继续 `recv`。

## 9. 为什么字段需要转义

协议使用 `|` 分隔字段，使用 `\n` 结束帧。如果用户备注本身包含这些字符，就可能破坏结构。

所以发送前转换：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

接收后再还原。先按原始 `|` 切分，再做反转义，这样 `%7C` 不会被误认为真正的分隔符。

## 10. Token 不是 TCP 连接

TCP 连接只表示客户端和服务端之间有一条字节通道，并不能证明当前请求是谁发送的。

登录成功后，服务端创建 Token：

```text
token -> Session{username, role, loginTime, lastActiveTime}
```

后续每个业务请求都带 Token。服务端通过 Token 找到身份，再检查角色权限。

因此可以这样回答助教：

> TCP 负责可靠传输，Token 负责应用层身份认证，两者属于不同层次。

## 11. 服务端为什么使用工作线程

`acceptLoop` 是主线程。如果它直接长期服务一个客户端，就无法及时接受其他连接。

默认模式下：

1. 主线程执行 `accept`。
2. 为新连接创建工作线程。
3. 工作线程执行该连接的 `recv/处理/send` 循环。
4. 主线程立即回到 `accept`，继续等待其他客户端。

每条连接拥有独立的：

- `clientSocket`
- TCP 接收缓冲区
- 连接见过的 Token 列表

业务数据由所有线程共享，因此由 `CoreMutex` 防止并发冲突。

## 12. 三种常见“边界”

### 网络层边界

`SocketServer` 负责拿到完整帧，但不判断订单是否属于当前用户。

### 协议层边界

`ProtocolCodec` 判断字段数量、长度和转义是否合法，但不判断余额是否充足。

### 业务层边界

`LogisticsSystem` 判断账号、余额、订单状态、订单归属和资金变化。

## 13. 助教常问问题

**为什么不用一次 `recv` 直接当一条消息？**  
因为 TCP 没有消息边界，可能发生半包和粘包。

**为什么用换行分帧？**  
纯文本协议实现简单、便于调试；字段内部换行会先转义，因此不会与帧结束符混淆。

**`listenSocket_` 能否直接接收客户端业务数据？**  
不能。它只负责监听，业务数据通过 `accept` 返回的 `clientSocket` 收发。

**为什么端口要调用 `htons`？**  
不同 CPU 的字节序可能不同，网络协议统一使用网络字节序。

**为什么关闭线程句柄后线程还在运行？**  
`CloseHandle` 只是释放主线程持有的 Windows 句柄，不等于终止工作线程。

**为什么要设置接收超时？**  
防止客户端在服务端无响应时永久阻塞。

**为什么连接断开还要清理 Session？**  
避免失效 Token 残留，并防止重复登录检查一直认为账号在线。

**并发揽收为什么只有一个成功？**  
网络线程可以并发到达，但核心业务用同一把 `CoreMutex` 串行执行状态检查和修改。

## 14. 建议阅读顺序

网络初学者按以下顺序阅读最省力：

1. `common/protocol/ProtocolCodec.h`
2. `client/SocketClient.h`
3. `client/SocketClient.cpp`
4. `server/main.cpp`
5. `server/SocketServer.h`
6. `server/SocketServer.cpp`
7. `server/SessionManager.h`
8. `server/ServerController.h`
9. `common/service/LogisticsSystem.cpp`
