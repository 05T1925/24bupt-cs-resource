// =============================================================================
// SocketClient.h - 客户端 TCP 通信适配层
// =============================================================================
// 上层 ClientApp 只构造 Request/读取 Response，不直接操作 Winsock。
// 下层 ProtocolCodec 负责文本协议格式，SocketClient 负责连接和完整帧收发。
//
// 依赖方向：
//   ClientApp -> SocketClient -> ProtocolCodec -> Winsock
// SocketClient 不保存登录身份；token 由 ClientApp 放入每个 Request。
//
// Socket 可以理解为应用程序访问网络连接的“句柄”：
//   connect 成功后，socket_ 代表客户端与服务端之间的一条 TCP 连接。
//   对 socket_ 调用 send/recv，就是向这条连接写入/读取字节。
// =============================================================================

#ifndef LOGISTICS_V3_CLIENT_SOCKET_CLIENT_H
#define LOGISTICS_V3_CLIENT_SOCKET_CLIENT_H

#include "../common/protocol/ProtocolCodec.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <string>

class SocketClient {
public:
    // host/port 只保存目标地址，构造函数本身不发起网络连接。
    SocketClient(const std::string& host, unsigned short port);
    ~SocketClient();

    // 初始化 Winsock、创建 TCP socket、设置超时并连接服务端。
    bool connectToServer();
    // 当前协议采用严格的一问一答：发送一个请求后读取一条换行结束的响应帧。
    Response sendCommand(const Request& request);
    // close 可重复调用；析构函数也会调用它完成 RAII 资源释放。
    void close();

private:
    std::string host_;
    unsigned short port_;
#ifdef _WIN32
    // INVALID_SOCKET 表示当前没有可用连接。
    SOCKET socket_ = INVALID_SOCKET;
#endif
    // WSAStartup 成功后置 true，析构时据此调用 WSACleanup。
    bool wsaStarted_ = false;
    // 保留一次 recv 中多读到的下一帧字节，用于处理 TCP 粘包。
    std::string receiveBuffer_;

#ifdef _WIN32
    // 循环处理 partial send，保证请求帧完整写入 TCP 流。
    bool sendAll(const std::string& payload) const;
    // 持续接收直至找到 '\n'；多余字节留在 receiveBuffer_ 供下一次调用。
    std::string receiveFrame();
#endif
};

#endif
