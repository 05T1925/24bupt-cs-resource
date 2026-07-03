// =============================================================================
// SocketClient.cpp - Winsock 客户端实现
// =============================================================================
// sendCommand 是本类的业务边界：Request 在此编码成字节流，服务端响应在此
// 还原为 Response。网络错误以异常上抛，由 ClientApp 的菜单层统一提示。
// =============================================================================

#include "SocketClient.h"

#include <stdexcept>

SocketClient::SocketClient(const std::string& host, unsigned short port)
    : host_(host), port_(port) {}

SocketClient::~SocketClient() {
    // 先关闭 socket，再清理 Winsock；顺序与 connectToServer 的建立顺序相反。
    close();
#ifdef _WIN32
    if (wsaStarted_) {
        WSACleanup();
        wsaStarted_ = false;
    }
#endif
}

bool SocketClient::connectToServer() {
#ifndef _WIN32
    return false;
#else
    WSADATA data{};
    // 每个客户端对象独立管理一次 Winsock 初始化与清理。
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0) {
        return false;
    }
    wsaStarted_ = true;

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // AF_INET=IPv4，SOCK_STREAM=面向字节流，IPPROTO_TCP=使用 TCP 协议。
    if (socket_ == INVALID_SOCKET) {
        return false;
    }

    // 超时只约束 recv 等待时间，连接建立和发送失败仍由各自返回值处理。
    const DWORD timeoutMs = 10000;  // 10 秒超时
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in address{};
    // htons 将主机字节序端口转换为网络字节序；inet_addr 解析 IPv4 文本。
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = inet_addr(host_.c_str());
    if (address.sin_addr.s_addr == INADDR_NONE && host_ != "255.255.255.255") {
        return false;
    }

    if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        // connect 会发起 TCP 建连；失败时关闭刚创建的 socket，避免资源泄漏。
        close();
        return false;
    }
    return true;
#endif
}

Response SocketClient::sendCommand(const Request& request) {
#ifndef _WIN32
    throw std::runtime_error("SocketClient currently targets Winsock on Windows.");
#else
    if (socket_ == INVALID_SOCKET) {
        throw std::runtime_error("client socket is not connected.");
    }
    // 编解码器统一执行转义与长度校验，SocketClient 只负责可靠收发字节流。
    const std::string payload = ProtocolCodec::encodeRequest(request);
    // 本项目按“一次请求后等待一次响应”工作，同一 SocketClient 不并发发送多个请求。
    if (!sendAll(payload)) {
        throw std::runtime_error("failed to send request.");
    }
    return ProtocolCodec::decodeResponse(receiveFrame());
#endif
}

void SocketClient::close() {
#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
#endif
}

#ifdef _WIN32
bool SocketClient::sendAll(const std::string& payload) const {
    std::size_t sentTotal = 0;
    // send 返回本轮实际写入长度，不能假设一次调用发送完整 payload。
    while (sentTotal < payload.size()) {
        const int sent = send(socket_, payload.data() + sentTotal,
                              static_cast<int>(payload.size() - sentTotal), 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return false;
        }
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

std::string SocketClient::receiveFrame() {
    // recvBuffer 是单次系统调用的临时空间，receiveBuffer_ 才是跨 recv 保留的数据。
    char recvBuffer[1024];
    while (true) {
        // 优先消费历史缓冲，避免为已经收齐的帧再次调用 recv。
        const std::size_t newline = receiveBuffer_.find('\n');
        if (newline != std::string::npos) {
            const std::string frame = receiveBuffer_.substr(0, newline + 1U);
            receiveBuffer_.erase(0, newline + 1U);
            return frame;
        }

        const int received = recv(socket_, recvBuffer, sizeof(recvBuffer), 0);
        if (received == 0) {
            // TCP 中 recv 返回 0 表示对端已按正常流程关闭连接。
            throw std::runtime_error("服务端已关闭连接，请检查服务端是否仍在运行。");
        }
        if (received == SOCKET_ERROR) {
            // SOCKET_ERROR 表示系统调用失败，需要通过 WSAGetLastError 获取原因。
            const int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                throw std::runtime_error("等待服务端响应超时（10秒），请检查服务端是否正常运行。");
            }
            throw std::runtime_error("接收服务端响应失败，错误码：" + std::to_string(err));
        }
        // recvBuffer 不是以 '\0' 结尾的 C 字符串，只能按 received 个有效字节追加。
        receiveBuffer_.append(recvBuffer, recvBuffer + received);
        // 与服务端采用同一数量级的缓冲限制，防止无换行响应无限增长。
        if (receiveBuffer_.size() > ProtocolCodec::MaxMessageLength * 2U) {
            throw ProtocolError("PROTOCOL_ERROR", "client receive buffer exceeded protocol limit.");
        }
    }
}
#endif
