// =============================================================================
// server/main.cpp - 服务端组合根与程序入口
// =============================================================================
// 本文件不实现具体业务，而是按依赖顺序组装整个服务端：
//   LogisticsSystem 负责业务与持久化
//       -> SessionManager 负责登录会话
//       -> ServerController 负责协议命令路由和鉴权
//       -> SocketServer 负责 TCP 监听、分帧和并发连接
//
// 一条请求的服务端调用链：
//   SocketServer::handleClient
//       -> ProtocolCodec::decodeRequest
//       -> ServerController::handle
//       -> LogisticsSystem 对应业务方法
//       -> Repository/Logger 持久化
//       -> ProtocolCodec::encodeResponse
// =============================================================================

#include "ServerController.h"
#include "SessionManager.h"
#include "SocketServer.h"

#include <iostream>
#include <string>

class ServerApp {
public:
    int run(int argc, char* argv[]) const {
#ifdef _WIN32
        SetConsoleOutputCP(936);
#endif
        const bool acceptOnce = argc > 1 && std::string(argv[1]) == "--once";

        std::cout << "============================================================" << '\n';
        std::cout << "  Logistics V3 Network Server" << '\n';
        std::cout << "  Phase 9: acceptance-ready with enhanced logging" << '\n';
        std::cout << "  Platform: Windows / Winsock TCP" << '\n';
        std::cout << "============================================================" << '\n';
        std::cout << "[Server] Mode: " << (acceptOnce ? "--once (single client self-test)" : "multi-client worker-thread") << '\n';
        std::cout << "[Server] Listen: 127.0.0.1:9000" << '\n';
        std::cout << "[Server] Data dir: data/server/" << '\n';

        // 业务核心必须先加载数据；后续路由和网络对象均引用该实例。
        LogisticsSystem system("data/server");
        const ServiceResult initResult = system.initialize();
        if (!initResult.ok) {
            std::cerr << "[Server] initialize failed: " << initResult.code << " | " << initResult.message << '\n';
            return 1;
        }
        std::cout << "[Server] Business core initialized." << '\n';

        // 会话表与业务数据使用不同的锁，避免会话查询直接耦合业务仓库。
        SessionManager sessions;
        ServerController controller(system, sessions);
        controller.ensureDemoAccounts();
        std::cout << "[Server] Demo accounts ensured." << '\n';

        // SocketServer 不拥有 controller/sessions，只在 ServerApp 生命周期内引用它们。
        SocketServer server("127.0.0.1", 9000, controller, sessions);
        if (!server.start()) {
            return 2;
        }
        server.run(acceptOnce);

        // Final statistics on graceful exit
        std::cout << "[Server] shutting down." << '\n';
        std::cout << "[Server] total requests handled: " << server.totalRequests() << '\n';
        std::cout << "[Server] final active sessions: " << sessions.totalSessionCount() << '\n';
        std::cout << "[Server] goodbye." << '\n';
        return 0;
    }
};

int main(int argc, char* argv[]) {
    const ServerApp app;
    return app.run(argc, argv);
}
