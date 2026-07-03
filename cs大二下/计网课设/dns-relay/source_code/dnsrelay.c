/**
 * ============================================================
 * dnsrelay.c — DNS 中继器主程序
 * ============================================================
 *
 * 【程序概述】
 *   本程序实现了一个 DNS 中继服务器 (DNS Relay)，运行在 Windows 平台。
 *   它在客户端和上游 DNS 服务器之间充当中间人，工作模式如下:
 *
 *   客户端 ←→ [DNS中继器: 127.0.0.1:53] ←→ [上游DNS: 如 202.106.0.20:53]
 *                       ↓
 *               本地数据库 (dnsrelay.txt)
 *               内存缓存 (DNSCache, TTL)
 *
 * 【核心功能】
 *   1. 监听本地 53 端口 (DNS 标准端口)
 *   2. 收到客户端 DNS 查询后:
 *      a. 先查本地数据库 → 命中则直接回复 (实现拦截/屏蔽)
 *      b. 未命中则转发给上游 DNS 服务器
 *   3. 收到上游 DNS 回复后:
 *      a. 缓存结果到内存 (加速后续查询)
 *      b. 转发回复给原始客户端
 *
 * 【主循环架构】
 *   while (没有按下 Esc)
 *     ├─ WaitForEvent()  ← 使用 select() 等待事件
 *     │    ├─ UpdateCache()      更新 TTL 缓存
 *     │    ├─ 清理已回复/超时记录
 *     │    └─ select(1秒超时)     等待 UDP 数据到达
 *     ├─ dgram_arrival → 收到报文
 *     │    ├─ recvfrom()          接收报文
 *     │    ├─ 判断类型 (Query/Response)
 *     │    ├─ ResolveQuery()  或  ResolveResponse()
 *     │    └─ sendto()            发送处理后的报文
 *     └─ nothing → 超时，继续循环
 *
 * 【select() I/O 多路复用简介】
 *   select() 允许同时监听多个 socket 的状态变化。
 *   这里虽然只监听一个 socket，但利用了其超时机制:
 *     - 有数据到达: 立即返回，处理报文
 *     - 1 秒超时: 返回后可以执行维护任务 (清理队列、更新缓存)
 *   这避免了纯阻塞 recvfrom() 无法做维护工作的问题。
 *
 * 【编译和运行】
 *   编译: 需要链接 ws2_32.lib (Windows Socket 库)
 *   运行: dnsrelay [-d|-dd] [dns-server-ip] [filename.txt]
 *
 *   示例:
 *     dnsrelay                         使用默认配置
 *     dnsrelay -d 8.8.8.8 dnsrelay.txt  指定调试等级和上游 DNS
 *     dnsrelay -dd                      最详细调试输出
 *
 * 【退出方式】
 *   按 Esc 键退出程序（_kbhit() + _getch() 检测按键）
 */

#include <stdio.h>
#include <winsock2.h>    /* Windows Socket API v2 — socket/bind/sendto/recvfrom */
#include <WS2tcpip.h>    /* 扩展: inet_pton, inet_ntop */
#include <conio.h>       /* 控制台 I/O: _kbhit(), _getch() — 检测键盘输入 */
#include <time.h>        /* time() — 获取 Unix 时间戳 */
#include "control.h"     /* 公共控制模块 */
#include "database.h"    /* 数据管理模块 */
#include "resolve.h"     /* DNS 解析模块 */

/* 链接 Windows Socket 库 (ws2_32.lib)
 * #pragma comment 是 MSVC 特有的指令，用于自动链接指定库 */
#pragma comment(lib, "ws2_32.lib")

/**
 * WaitForEvent() — 等待网络事件（使用 select() I/O 多路复用）
 *
 * 【功能说明】
 *   在主循环的每次迭代中调用，负责:
 *   1. 更新 DNS 缓存 TTL
 *   2. 清理客户端查询表中已回复或超时的记录
 *   3. 使用 select() 等待 socket 可读事件（最多等待 1 秒）
 *   4. 返回事件类型
 *
 * 【清理逻辑】
 *   在进入 select() 等待之前，先清理队列中无需再关注的记录:
 *
 *   while (队列非空) {
 *     ① 如果队首记录已回复 (r=1) → 弹出 (已完成)
 *     ② 如果队首记录已超时 → 弹出 (放弃等待)
 *     ③ 如果队首记录正常等待中 → 停止检查 (后续记录更新)
 *   }
 *
 *   为什么要停止检查？
 *     因为队列是按时间顺序排列的，如果队首记录尚未超时，
 *     后面的记录入队时间更晚，更不可能超时，无需检查。
 *
 * 【关于 select() 超时时间】
 *   select 超时设为 1 秒，因为:
 *   1. nslookup 等客户端的默认超时约 2 秒
 *   2. 我们的记录超时是 3 秒 (TIMEOUT)
 *   3. 1 秒的检查间隔足够在超时前发现并清理记录
 *   4. 太短的超时会增加 CPU 开销
 *
 * @param fd 监听的 UDP socket 描述符
 * @return dgram_arrival — 有数据到达, nothing — 超时或没有事件
 */
event_type WaitForEvent(SOCKET fd) {
    /* ---- 第一步: 更新 DNS 缓存 TTL ---- */
    UpdateCache();

    /* ---- 第二步: 清理客户端查询表 ---- */
    while (1) {
        CRecord record = { 0 };

        /* 情况①: 队首记录已回复 → 弹出 */
        if (CTableUsage()
            && FindCRecord(GetCTableFrontIndex(), &record)
            && record.r) {
            /* 该查询已成功回复客户端，从队列中移除 */
            PopCRecord();
            continue; /* 继续检查下一个 */
        }

        /* 情况②: 队首记录已超时 → 弹出 */
        if (CTableUsage()
            && FindCRecord(GetCTableFrontIndex(), &record)
            && record.expireTime
            && time(NULL) > record.expireTime) {
            /*
             * 超时处理说明:
             *   当前实现直接丢弃超时的查询请求。
             *
             *   为什么不重新发送?
             *     select() 超时设为 1 秒，而 nslookup 客户端的重发间隔
             *     通常为 2 秒。如果我们重新发送，客户端可能已重试，
             *     这会导致重复请求，意义不大。
             *
             *   实际效果:
             *     如果客户端需要重试，它会重新发送一个 query，
             *     届时中继器会当作新请求处理。
             */
            debugPrintf("超时删除\n");
            PopCRecord();
            continue; /* 继续检查下一个 */
        }

        /* 情况③: 队首记录仍在等待，无需继续检查 */
        else
            break;
    }

    /* ---- 第三步: 使用 select() 监听 socket ---- */

    fd_set readfds;                 /* 可读文件描述符集合 */
    FD_ZERO(&readfds);              /* 清空集合 */
    FD_SET(fd, &readfds);           /* 将我们的 socket 加入集合 */

    /* 设置超时时间: 1 秒
     * struct timeval { long tv_sec; long tv_usec; } */
    struct timeval t = { 1, 0 };    /* 1 秒 + 0 微秒 */

    /* select() 参数说明:
     *   第1个参数: nfds (Windows 中忽略，设为 0)
     *   第2个参数: 可读集合
     *   第3个参数: 可写集合 (不需要，设为 0)
     *   第4个参数: 异常集合 (不需要，设为 0)
     *   第5个参数: 超时时间
     *
     *   返回值: >0 有事件, 0 超时, <0 错误 */
    select(0, &readfds, 0, 0, &t);

    /* 检查我们的 socket 是否可读 */
    if (FD_ISSET(fd, &readfds)) {
        return dgram_arrival;   /* 有 UDP 数据报到达 */
    }

    return nothing;              /* 超时，没有数据到达 */
}

/**
 * main() — DNS 中继器主函数
 *
 * 【程序启动流程】
 *
 *   1. 解析命令行参数 (debug等级、DNS服务器、数据库文件)
 *   2. 构建 DNS 数据库 (打开 TXT 文件)
 *   3. 初始化 Windows Socket (WSAStartup)
 *   4. 创建 UDP socket
 *   5. 绑定 53 端口
 *   6. 初始化客户端查询表
 *   7. 进入主循环:
 *        WaitForEvent → 收到报文 → 处理 → 发送 → 重复
 *   8. 退出时清理 (closesocket, WSACleanup)
 *
 * 【Windows Socket 编程基础】
 *
 *   WSAStartup()
 *     在使用任何 socket 函数前必须调用，初始化 Winsock DLL。
 *     MAKEWORD(2, 2) 表示请求 Winsock 2.2 版本。
 *
 *   socket()
 *     创建一个 socket。参数:
 *       AF_INET      : IPv4 地址族
 *       SOCK_DGRAM   : 数据报套接字 (UDP)
 *       IPPROTO_UDP  : UDP 协议
 *
 *   bind()
 *     将 socket 绑定到本地地址和端口。
 *     htonl(INADDR_ANY) 表示监听所有网络接口（0.0.0.0）。
 *
 *   recvfrom()
 *     从 UDP socket 接收数据报，同时获取发送方的地址。
 *     用于后续回复时知道发给谁。
 *
 *   sendto()
 *     向指定地址发送 UDP 数据报。
 *
 *   closesocket()
 *     关闭 socket，释放系统资源。
 *
 *   WSACleanup()
 *     终止 Winsock DLL 的使用，释放资源。
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 0 = 正常退出, 非0 (通过 return 0) 提前退出
 */
int main(int argc, char *argv[]) {

    /* ============================================================
     * 阶段 1: 解析命令行参数
     * ============================================================ */
    if (dealOpts(argc, argv)) {
        /* 参数格式正确，打印当前配置 */
        debugPrintf("Debug level %d\n", gDebugLevel);
        debugPrintf("Name server %s:53\n", addrDNSserv);
        debugPrintf("Database using %s\n", gDBtxt);
    } else {
        /* 参数格式错误，打印使用帮助 */
        debugPrintf("Please use the following format:\n"
            "dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]");
        return 0;
    }

    /* ============================================================
     * 阶段 2: 构建 DNS 数据库
     * ============================================================ */
    if (!BuildDNSDatabase()) {
        debugPrintf("Failed to build the database.\n");
        return 0;
    }

    /* ============================================================
     * 阶段 3: 变量声明
     * ============================================================ */
    WSADATA wsaData;                                /* Winsock 版本信息结构体 */
    SOCKADDR_IN addrSrv;                            /* 服务器 (dnsrelay) 的本地地址 */
    SOCKADDR_IN addrCli;                            /* 客户端地址（或上游 DNS 地址）
                                                       注意: 在 ResolveQuery 中可能被修改为
                                                       上游 DNS 服务器地址 */
    int addrCliSize = sizeof(addrCli);              /* 地址结构体大小，recvfrom 需要 */
    int Port = 53;                                  /* 绑定端口: 53 (DNS 标准端口) */
    char ipStrBuf[20] = { '\0' };                   /* IP 地址字符串临时缓冲区（调试用）*/
    event_type event;                               /* 事件类型变量 */

    unsigned char recvBuf[MAX_BUFSIZE] = { '\0' };  /* UDP 接收缓冲区 */
    unsigned char sendBuf[MAX_BUFSIZE] = { '\0' };  /* UDP 发送缓冲区 */
    int recvByte = 0;                                /* 接收到的字节数 */
    int sendByte = 0;                                /* 要发送的字节数 */

    /* ============================================================
     * 阶段 4: 初始化 Windows Socket (Winsock)
     * ============================================================ */
    /* WSAStartup: 加载 Winsock DLL
     *   MAKEWORD(2,2) → 请求版本 2.2
     *   返回 0 表示成功 */
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        debugPrintf("Server: WSAStartup failed with error %ld\n", WSAGetLastError());
        return 0;
    }

    /* ============================================================
     * 阶段 5: 创建 UDP Socket
     * ============================================================ */
    /* socket() 参数:
     *   AF_INET      — IPv4 协议族
     *   SOCK_DGRAM   — 数据报套接字 (UDP)
     *   IPPROTO_UDP  — UDP 协议
     *
     * DNS 协议通常使用 UDP (对于超过 512 字节的响应会使用 TCP，
     * 但本程序仅处理 UDP 的基本情况) */
    SOCKET sockSrv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockSrv == INVALID_SOCKET) {
        debugPrintf("Invalid socket error %ld\n", WSAGetLastError());
        WSACleanup();       /* 清理 Winsock */
        return 0;
    } else {
        debugPrintf("Socket() is OK!\n");
    }

    /* ============================================================
     * 阶段 6: 绑定本地地址
     * ============================================================ */
    /* 配置服务器地址结构:
     *   sin_family       = AF_INET (IPv4)
     *   sin_port         = htons(53) (DNS 端口，主机字节序→网络字节序)
     *   sin_addr.s_addr  = htonl(INADDR_ANY) (0.0.0.0 — 监听所有网卡)
     *
     * INADDR_ANY 表示接受来自任何网络接口的连接/数据。
     * 这对于监听本机地址并转发给其他地址的中继器很重要。 */
    addrSrv.sin_family = AF_INET;
    addrSrv.sin_port = htons(Port);
    addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    /* bind() 将 socket 与本地地址绑定
     * 绑定失败通常是因为端口被占用（如已有其他 DNS 服务在运行）
     * 或没有管理员权限（Windows 上绑定 <1024 端口需要管理员权限） */
    if (bind(sockSrv, (SOCKADDR *)&addrSrv, sizeof(addrSrv)) == SOCKET_ERROR) {
        debugPrintf("Failed to bind() with error %ld\n", WSAGetLastError());
        closesocket(sockSrv);   /* 关闭 socket */
        WSACleanup();           /* 清理 Winsock */
        return 0;
    } else {
        debugPrintf("Bind() is OK!\n");
    }

    /* ============================================================
     * 阶段 7: 初始化客户端查询表
     * ============================================================ */
    /* 将 ClientTable 的 front 和 rear 都设为 0 */
    InitCTable();

    /* ============================================================
     * 阶段 8: 主循环
     * ============================================================ */
    /* 循环条件: _kbhit() 检测是否有键盘输入
     *           _getch() 读取按下的键
     *           == 27 检测是否为 Esc 键 (ASCII 27)
     * 按 Esc 键退出程序 */
    while (!(_kbhit() && _getch() == 27)) {

        /* ---- 等待事件 (select 复用) ---- */
        event = WaitForEvent(sockSrv);

        switch (event) {
        case timeout:
            /* select() 超时，这一秒内没有数据到达
             * 这是正常的，主循环的下一次迭代会继续清理和维护 */
            debugPrintf("Timeout.\n");
            break;

        case dgram_arrival:
            /* ---- 有 UDP 数据报到达 ---- */

            /* 清空接收和发送缓冲区，避免旧数据干扰 */
            ClearBuffer(recvBuf, recvByte);
            ClearBuffer(sendBuf, sendByte);

            /* ---- 接收数据报 ---- */
            debugPrintf("datagram arrived. Recieving...\n");
            /* recvfrom() 参数:
             *   sockSrv      : 监听的 socket
             *   recvBuf      : 接收缓冲区
             *   MAX_BUFSIZE  : 缓冲区最大容量
             *   0            : flags (无特殊选项)
             *   &addrCli     : 输出参数，发送方的地址
             *   &addrCliSize : 输入输出参数，地址结构大小 */
            recvByte = recvfrom(sockSrv, (char *)recvBuf, MAX_BUFSIZE, 0,
                (SOCKADDR *)&addrCli, &addrCliSize);

            if (recvByte <= 0) {
                /* recvfrom 失败（返回值 <= 0 表示出错） */
                debugPrintf("recvfrom() failed with error %ld\n", WSAGetLastError());
                recvByte = 0;   /* 重置 recvByte */
                break;
            } else {
                debugPrintf("datagram received. %d Bytes in all.\n", recvByte);
            }

            /* ---- 判断报文类型: Query 还是 Response ---- */
            /* DNS Header 第 3 字节 (FLAGS 高字节) 的 bit 7 = QR 标志
             *   recvBuf[2] & 0x80 提取 QR bit
             *   >> 7 把它移到最低位
             *   0 = Query (查询), 1 = Response (响应) */
            if ((recvBuf[2] & 0x80) >> 7 == 0) {
                /* ============================================
                 * 收到 DNS 查询 (Query)
                 * 来源: 客户端 (如 nslookup, 浏览器等)
                 * ============================================ */
                debugPrintf("收到一个请求报文 内容如下:\n");
                DebugBuffer(recvBuf, recvByte);

                /* 调用 ResolveQuery 处理:
                 *   - 先在本地数据库/缓存查找
                 *   - 命中则构造 Answer 回复
                 *   - 未命中则准备转发给上游 DNS
                 *   - 返回值 < 0 表示处理失败 */
                sendByte = ResolveQuery(recvBuf, sendBuf, recvByte, &addrCli);

                if (sendByte < 0) {
                    /* 处理失败 (如 ClientTable 已满) */
                    debugPrintf("failed to solved query.\n");
                    break;
                }
            } else {
                /* ============================================
                 * 收到 DNS 响应 (Response)
                 * 来源: 上游 DNS 服务器
                 * ============================================ */
                debugPrintf("收到一个响应报文 内容如下:\n");
                DebugBuffer(recvBuf, recvByte);

                /* 调用 ResolveResponse 处理:
                 *   - 提取 IP 和 TTL 并缓存
                 *   - 在 ClientTable 中匹配原始客户端
                 *   - 还原原始 ID，准备发给客户端
                 *   - 返回值 < 0 表示处理失败 (如 ID 不匹配) */
                sendByte = ResolveResponse(recvBuf, sendBuf, recvByte, &addrCli);

                if (sendByte < 0) {
                    debugPrintf("failed to solved response.\n");
                    break;
                }
            }

            /* ---- 发送处理后的报文 ---- */
            /* 打印发送目标 */;
            debugPrintf("datagram sending to %s:%d\n",
                inet_ntop(AF_INET, (void *)&addrCli.sin_addr, ipStrBuf, 16),
                htons(addrCli.sin_port)
            );

            addrCliSize = sizeof(addrCli);
            /* sendto() 参数:
             *   sockSrv    : socket
             *   sendBuf    : 要发送的数据
             *   sendByte   : 数据长度
             *   0          : flags
             *   &addrCli   : 目标地址（客户端 或 上游DNS服务器）
             *   addrCliSize: 地址结构大小 */
            sendto(sockSrv, (char *)sendBuf, sendByte, 0,
                (SOCKADDR *)&addrCli, addrCliSize);
            debugPrintf("datagram sending succeed %d Bytes in all.\n", sendByte);

            /* ---- 调试: 打印发送的报文 ---- */
            if ((sendBuf[2] & 0x80) >> 7 == 0) {
                debugPrintf("发送了一个请求报文 内容如下:\n");
                DebugBuffer(sendBuf, sendByte);
            } else {
                debugPrintf("发送了一个响应报文 内容如下:\n");
                DebugBuffer(sendBuf, sendByte);
            }

            break;

        default:
            /* 未预期的事件类型，忽略 */
            break;
        }
    }

    /* ============================================================
     * 阶段 9: 清理资源
     * ============================================================ */

    /* 关闭 socket */
    if (closesocket(sockSrv) != 0)
        debugPrintf("close socket failed with error %ld\n", GetLastError());
    else
        debugPrintf("socket closed.\n");

    /* 清理 Winsock */
    if (WSACleanup() != 0)
        debugPrintf("WSA clean up failed with error %ld\n", GetLastError());
    else
        debugPrintf("clean up is OK.\n");

    return 0;
}
