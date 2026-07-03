// =============================================================================
// SocketServer.cpp — Winsock TCP 服务端实现
// =============================================================================
// 文件用途：实现 TCP 监听、多线程 accept、流式分帧、partial send 处理。
// 所属模块：server（服务端网络层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 多线程模型：
//   主线程：accept → CreateThread(worker) → CloseHandle → 继续 accept
//   worker 线程：handleClient → recv 循环 → 分帧 → decode → handle → encode → send
//
// Winsock 服务端启动顺序：
//   WSAStartup -> socket -> bind -> listen -> accept
// 各步骤含义：
//   WSAStartup：启用 Windows Socket 库
//   socket：创建一个尚未绑定地址的 Socket
//   bind：把 Socket 绑定到本机 IP 和端口
//   listen：把它变为监听 Socket
//   accept：从等待队列取出一个客户端连接，返回新的通信 Socket
//
// TCP 流式处理（handleClient）：
//   1. 维护 per-connection std::string buffer
//   2. recv(1024) + buffer.append
//   3. buffer.size() > MaxMessageLength*2 → PROTOCOL_ERROR + 断开连接
//   4. while buffer.contains('\n') → 提取完整帧 + buffer.erase
//   5. decodeRequest → controller_.handle → encodeResponse → sendAll
//
// 断线处理：
//   recv==0 → 正常断开 → cleanupClientSessions
//   recv==SOCKET_ERROR → 异常断开 → cleanupClientSessions
//   cleanupClientSessions：清理该连接持有的全部 token
// =============================================================================

#include "SocketServer.h"

#include <algorithm>
#include <iostream>
#include <sstream>

SocketServer::SocketServer(const std::string& host, unsigned short port, ServerController& controller,
                           SessionManager& sessions)
    : host_(host), port_(port), controller_(controller), sessions_(sessions) {}

SocketServer::~SocketServer() {
    // 析构时停止 accept 并释放 Winsock；已连接 Socket 由各工作线程关闭。
    stop();
#ifdef _WIN32
    if (wsaStarted_) {
        WSACleanup();
        wsaStarted_ = false;
    }
#endif
}

bool SocketServer::start() {
#ifndef _WIN32
    std::cerr << "[Server] this Phase 3 implementation targets Winsock on Windows." << '\n';
    return false;
#else
    WSADATA data{};
    // Winsock 生命周期由 start/destructor 配对管理，初始化成功后才允许创建套接字。
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &data);
    if (startupResult != 0) {
        std::cerr << "[Server] WSAStartup failed: " << startupResult << '\n';
        return false;
    }
    wsaStarted_ = true;

    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    // 此处创建的是监听 Socket，后续不直接用它收发业务数据。
    if (listenSocket_ == INVALID_SOCKET) {
        std::cerr << "[Server] socket failed: " << WSAGetLastError() << '\n';
        return false;
    }

    sockaddr_in address{};
    // sockaddr_in 描述 IPv4 地址；端口必须转换为统一的网络字节序。
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    // 0.0.0.0 监听所有本地网卡，其余值按 IPv4 文本地址解析。
    address.sin_addr.s_addr = host_ == "0.0.0.0" ? htonl(INADDR_ANY) : inet_addr(host_.c_str());
    if (address.sin_addr.s_addr == INADDR_NONE && host_ != "255.255.255.255") {
        std::cerr << "[Server] invalid bind host: " << host_ << '\n';
        return false;
    }

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        // bind 失败常见原因：端口已被占用、地址不可用或权限不足。
        std::cerr << "[Server] bind failed: " << WSAGetLastError() << '\n';
        return false;
    }

    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        // listen 不接收业务数据，只允许系统把新连接排入等待队列。
        std::cerr << "[Server] listen failed: " << WSAGetLastError() << '\n';
        return false;
    }

    running_.store(true);
    std::cout << "[Server] listening on " << host_ << ':' << port_ << '\n';
    return true;
#endif
}

void SocketServer::run(bool acceptOnce) {
#ifdef _WIN32
    if (!running_.load()) {
        return;
    }
    acceptLoop(acceptOnce);
#endif
}

void SocketServer::stop() {
#ifdef _WIN32
    // 关闭监听 Socket 可使阻塞中的 accept 返回，从而让主循环退出。
    running_.store(false);
    closeListenSocket();
#endif
}

#ifdef _WIN32
void SocketServer::acceptLoop(bool acceptOnce) {
    while (running_.load()) {
        sockaddr_in clientAddress{};
        int addressLength = sizeof(clientAddress);
        // accept 通常会阻塞等待；成功后 clientSocket 专属于这一条客户端连接。
        SOCKET clientSocket = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddress), &addressLength);
        if (clientSocket == INVALID_SOCKET) {
            if (running_.load()) {
                std::cerr << "[Server] accept failed: " << WSAGetLastError() << '\n';
            }
            break;
        }

        std::ostringstream clientText;
        // 客户端端口由操作系统临时分配，仅用于日志区分连接。
        clientText << inet_ntoa(clientAddress.sin_addr) << ':' << ntohs(clientAddress.sin_port);

        const int currentActive = activeConnections_.fetch_add(1) + 1;
        std::cout << "[Server] client connected: " << clientText.str()
                  << " (active connections: " << currentActive << ")" << '\n';

        if (acceptOnce) {
            // 单次模式在当前线程同步服务一个连接，便于自动测试确定退出时机。
            handleClient(clientSocket, clientText.str());
            closesocket(clientSocket);
            const int remaining = activeConnections_.fetch_sub(1) - 1;
            std::cout << "[Server] client disconnected: " << clientText.str()
                      << " (active connections: " << remaining << ")" << '\n';
            running_.store(false);
            break;
        }

        // 常规模式下每条连接拥有独立接收缓冲区和连接级 Token 清单。
        ClientThreadContext* context = new ClientThreadContext{this, clientSocket, clientText.str()};
        HANDLE threadHandle = CreateThread(nullptr, 0, &SocketServer::clientThreadEntry, context, 0, nullptr);
        if (threadHandle == nullptr) {
            std::cerr << "[Server] CreateThread failed: " << GetLastError() << '\n';
            delete context;
            // 创建线程失败时同步处理当前连接，避免已接受的客户端被直接丢弃。
            handleClient(clientSocket, clientText.str());
            closesocket(clientSocket);
            const int remaining = activeConnections_.fetch_sub(1) - 1;
            std::cout << "[Server] client disconnected: " << clientText.str()
                      << " (active connections: " << remaining << ")" << '\n';
        } else {
            // 关闭线程句柄不终止线程，工作线程仍负责 socket 的最终关闭。
            CloseHandle(threadHandle);
        }
    }
}

DWORD WINAPI SocketServer::clientThreadEntry(LPVOID rawContext) {
    // Windows 线程入口只能接收一个 void*，因此先还原为自定义上下文结构。
    ClientThreadContext* context = static_cast<ClientThreadContext*>(rawContext);
    SocketServer* server = context->server;
    const SOCKET clientSocket = context->clientSocket;
    const std::string clientAddress = context->clientAddress;
    // 参数复制完成后立即释放一次性上下文，后续资源归工作线程管理。
    delete context;

    try {
        server->handleClient(clientSocket, clientAddress);
    } catch (const std::exception& error) {
        std::cerr << "[Server] client thread failed for " << clientAddress << ": "
                  << error.what() << '\n';
    }
    closesocket(clientSocket);
    const int remaining = server->activeConnections_.fetch_sub(1) - 1;
    std::cout << "[Server] client disconnected: " << clientAddress
              << " (active connections: " << remaining << ")" << '\n';
    return 0;
}

void SocketServer::cleanupClientSessions(const std::vector<std::string>& tokens,
                                         const std::string& clientAddress) {
    // 连接异常结束也必须撤销该连接见过的会话，避免重复登录检查留下幽灵会话。
    for (const std::string& token : tokens) {
        controller_.closeSession(token);
    }
    if (!tokens.empty()) {
        std::cout << "[Server] cleaned " << tokens.size()
                  << " session(s) for " << clientAddress << '\n';
        // Print session breakdown after cleanup
        const int userCount = sessions_.sessionCountByRole("USER");
        const int adminCount = sessions_.sessionCountByRole("ADMIN");
        const int courierCount = sessions_.sessionCountByRole("COURIER");
        std::cout << "[Server] active sessions: " << sessions_.totalSessionCount()
                  << " (USER=" << userCount
                  << " ADMIN=" << adminCount
                  << " COURIER=" << courierCount << ")" << '\n';
    }
}

void SocketServer::logRequestSummary(const std::string& clientAddress, const std::string& command,
                                     const std::string& token, const std::string& resultCode) {
    const std::string summary = sessions_.getSessionSummary(token);
    // summary format: "role|username" or "GUEST|"
    std::cout << "[Server] " << clientAddress
              << " cmd=" << command
              << " session=" << summary
              << " => " << resultCode << '\n';
}

void SocketServer::handleClient(SOCKET clientSocket, const std::string& clientAddress) {
    std::string buffer;
    // 同一 TCP 连接可能先登录再发业务请求，需记录其 Token 以便断线统一回收。
    std::vector<std::string> connectionTokens;
    char recvBuffer[1024];

    while (running_.load()) {
        // recv 最多读取 sizeof(recvBuffer) 个字节，返回值才是本次真实收到的长度。
        const int received = recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);
        if (received == 0) {
            cleanupClientSessions(connectionTokens, clientAddress);
            return;
        }
        if (received == SOCKET_ERROR) {
            std::cerr << "[Server] recv failed from " << clientAddress << ": " << WSAGetLastError() << '\n';
            cleanupClientSessions(connectionTokens, clientAddress);
            return;
        }

        // 不把 recvBuffer 当 C 字符串使用，因为网络数据不保证自带 '\0' 结尾。
        buffer.append(recvBuffer, recvBuffer + received);
        // 双倍帧上限允许暂存一个完整帧和下一帧片段，同时限制无换行攻击占用内存。
        if (buffer.size() > ProtocolCodec::MaxMessageLength * 2U) {
            Response response;
            response.ok = false;
            response.code = "PROTOCOL_ERROR";
            response.message = "接收缓冲区超过协议限制。";
            sendAll(clientSocket, ProtocolCodec::encodeResponse(response));
            cleanupClientSessions(connectionTokens, clientAddress);
            return;
        }

        std::size_t newline = buffer.find('\n');
        // TCP 不保留消息边界：一次 recv 可能含半帧、多帧或帧尾与下一帧开头。
        while (newline != std::string::npos) {
            // frame 包含结尾换行；buffer.erase 后剩余字节可能是下一帧或半帧。
            const std::string frame = buffer.substr(0, newline + 1U);
            buffer.erase(0, newline + 1U);

            Response response;
            std::string responseFrame;
            std::string requestCommand;
            std::string requestToken;
            try {
                // 网络层到业务层的边界：文本帧先转成 Request，Controller 才处理命令。
                const Request request = ProtocolCodec::decodeRequest(frame);
                requestCommand = request.command;
                requestToken = request.token;
                // 记录请求携带的已有 Token，也记录本连接刚登录获得的新 Token。
                if (!request.token.empty() &&
                    std::find(connectionTokens.begin(), connectionTokens.end(), request.token) == connectionTokens.end()) {
                    connectionTokens.push_back(request.token);
                }
                response = controller_.handle(request);
                if (response.ok && response.code == "LOGIN_SUCCESS" && !response.records.empty() &&
                    std::find(connectionTokens.begin(), connectionTokens.end(), response.records[0]) == connectionTokens.end()) {
                    connectionTokens.push_back(response.records[0]);
                }
                // Controller 返回结构化 Response，再编码成客户端能够解析的文本帧。
                responseFrame = ProtocolCodec::encodeResponse(response);
            } catch (const ProtocolError& error) {
                response.ok = false;
                response.code = error.code();
                response.message = error.what();
                // 嵌套 try-catch：确保错误响应编码也失败时，仍能发送一个兜底响应
                try {
                    responseFrame = ProtocolCodec::encodeResponse(response);
                } catch (...) {
                    responseFrame = "RES|0|SERVER_ERROR|内部错误\\n";  // 硬编码最小响应帧
                }
            } catch (const std::exception& error) {
                response.ok = false;
                response.code = "SERVER_ERROR";
                response.message = error.what();
                try {
                    responseFrame = ProtocolCodec::encodeResponse(response);
                } catch (...) {
                    responseFrame = "RES|0|SERVER_ERROR|内部错误\\n";
                }
            }

            // Log security summary: command, session role/username, result code
            // (no full token, no password fields)
            logRequestSummary(clientAddress, requestCommand, requestToken, response.code);

            // Increment total requests counter
            totalRequests_.fetch_add(1);

            if (!sendAll(clientSocket, responseFrame)) {
                cleanupClientSessions(connectionTokens, clientAddress);
                return;
            }
            newline = buffer.find('\n');
        }
    }
    cleanupClientSessions(connectionTokens, clientAddress);
}

bool SocketServer::sendAll(SOCKET socketHandle, const std::string& payload) const {
    std::size_t sentTotal = 0;
    // send 的成功返回值仅代表本次写入字节数，不保证等于 payload 总长度。
    while (sentTotal < payload.size()) {
        const int sent = send(socketHandle, payload.data() + sentTotal,
                              static_cast<int>(payload.size() - sentTotal), 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            std::cerr << "[Server] send failed: " << WSAGetLastError() << '\n';
            return false;
        }
        // 下一轮从尚未发送的位置继续，直至 sentTotal 等于总长度。
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

void SocketServer::closeListenSocket() {
    if (listenSocket_ != INVALID_SOCKET) {
        // INVALID_SOCKET 哨兵可防止 stop 和析构重复关闭同一个句柄。
        closesocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET;
    }
}
#endif
