// =============================================================================
// SocketServer.h - 服务端 TCP 接入层
// =============================================================================
// 本类负责网络连接与协议帧边界，不实现身份权限和物流规则：
//   字节流 <-> ProtocolCodec <-> Request/Response
//   Request -> ServerController
// SessionManager 仅用于请求摘要统计和断线会话清理。
//
// 两类 Socket 的区别：
//   listenSocket_：只负责监听端口和接受新连接，相当于“总机”。
//   clientSocket：accept 为每个客户端返回的通信 Socket，相当于“一条具体通话”。
//   业务收发只能使用 clientSocket，不能使用 listenSocket_。
// =============================================================================

#ifndef LOGISTICS_V3_SERVER_SOCKET_SERVER_H
#define LOGISTICS_V3_SERVER_SOCKET_SERVER_H

#include "../common/protocol/ProtocolCodec.h"
#include "ServerController.h"
#include "SessionManager.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <atomic>
#include <string>
#include <vector>

class SocketServer {
public:
    SocketServer(const std::string& host, unsigned short port, ServerController& controller,
                 SessionManager& sessions);
    ~SocketServer();

    bool start();
    // acceptOnce 用于单连接测试；常规运行会持续接受并发连接。
    void run(bool acceptOnce);
    void stop();

    // 统计值被多个工作线程更新，使用 atomic 避免额外业务锁。
    long long totalRequests() const { return totalRequests_.load(); }
    int activeConnections() const { return activeConnections_.load(); }

private:
    // host_/port_ 是服务端要绑定的本地监听地址。
    std::string host_;
    unsigned short port_;
    ServerController& controller_;
    SessionManager& sessions_;
#ifdef _WIN32
    // 监听 Socket 生命周期覆盖整个服务端运行期。
    SOCKET listenSocket_ = INVALID_SOCKET;
    // 堆上上下文把连接所有权从 accept 线程移交给工作线程入口。
    struct ClientThreadContext {
        SocketServer* server;
        SOCKET clientSocket;
        std::string clientAddress;
    };
#endif
    bool wsaStarted_ = false;
    std::atomic<bool> running_{false};
    std::atomic<long long> totalRequests_{0};
    std::atomic<int> activeConnections_{0};

#ifdef _WIN32
    // 主线程接受连接；默认每个连接进入独立工作线程处理。
    void acceptLoop(bool acceptOnce);
    // 每个连接维护自己的 TCP 缓冲区和 Token 清单。
    void handleClient(SOCKET clientSocket, const std::string& clientAddress);
    // 正常登出之外的连接关闭路径也需要清理会话。
    void cleanupClientSessions(const std::vector<std::string>& tokens, const std::string& clientAddress);
    static DWORD WINAPI clientThreadEntry(LPVOID context);
    // TCP send 可能只发送部分字节，因此必须循环直至完整帧发送完成。
    bool sendAll(SOCKET socketHandle, const std::string& payload) const;
    void closeListenSocket();

    // Log a security summary for each request (no token, no password)
    void logRequestSummary(const std::string& clientAddress, const std::string& command,
                           const std::string& token, const std::string& resultCode);
#endif
};

#endif
