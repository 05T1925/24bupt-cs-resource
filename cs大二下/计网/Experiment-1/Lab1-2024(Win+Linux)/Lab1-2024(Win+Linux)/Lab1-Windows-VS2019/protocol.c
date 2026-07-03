/*
 * protocol.c — 协议库：物理层模拟、定时器管理、网络层接口、事件调度
 *
 * 本模块是数据链路层协议的底层支撑库，为上层（datalink.c）提供：
 *
 *   1. 物理层模拟：
 *      - 通过 TCP 在两台机器间传输数据（模拟物理信道）
 *      - 模拟信道传播延迟（CHAN_DELAY = 270ms）
 *      - 模拟信道带宽限制（CHAN_BPS = 8000 bps）
 *      - 模拟信道误码（Bit Error Rate，可配置 BER）
 *
 *   2. 定时器管理：
 *      - 支持多个数据帧定时器（0~127），用于超时重传
 *      - 支持 ACK/NAK 定时器（ID=128），用于延迟确认和 NAK 重发
 *
 *   3. 网络层接口：
 *      - get_packet()：生成测试数据包（模拟上层应用数据）
 *      - put_packet()：接收并校验数据包（交付给上层）
 *
 *   4. 事件调度器：
 *      - wait_for_event()：统一的事件等待与分发
 *      - 协调网络层、物理层、定时器三类事件源
 *
 * 运行模式：
 *   - 站 A（发送方）：启动 TCP 服务器，等待站 B 连接
 *   - 站 B（接收方）：主动连接站 A 的 TCP 端口
 *   - 两者通过 localhost TCP 通信，协议库在此之上模拟信道损伤
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <time.h>

/*
 * epoch：纪元时间戳（站 A 和站 B 使用相同的 epoch 基准）
 *
 * 用于计算相对时间（get_ms() 返回自 epoch 起的毫秒数）。
 * 站 B 在连接建立后发送自己的 epoch 给站 A，确保两端时间同步。
 */
static time_t epoch; /* epoch timestamp (be same for Station A & B) */

/* ==================================================================
 *                    Windows 平台实现
 * ================================================================== */
#ifdef _WIN32 /* for Windows Visual Studio */

#include <winsock.h>
#include <io.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "getopt.h"

#define getopt_long getopt_int
#define stricmp _stricmp

/*
 * socket_init()：Windows 平台初始化 Winsock 库
 *
 * Winsock 是 Windows 下的套接字 API 实现，使用前必须调用 WSAStartup 初始化。
 * MAKEWORD(1,1) 请求 Winsock 1.1 版本。
 * 若初始化失败，打印错误信息并退出程序。
 */
static void socket_init(void)
{
    WORD wVersionRequested;
    WSADATA WSAData;
    int status;

    wVersionRequested = MAKEWORD(1,1);  /* 请求 Winsock 1.1 版本 */
    status = WSAStartup(wVersionRequested, &WSAData);
    if (status != 0) {
        printf("Windows Socket DLL Error\n");
        exit(0);
    }
}

/*
 * get_ms()：获取自 epoch 起的相对毫秒时间戳（Windows 实现）
 *
 * 使用 _ftime() 获取当前系统时间，计算与 epoch 的差值。
 * 若 epoch 尚未设置（为 0），返回 0。
 *
 * 返回值：
 *   (当前时间 - epoch) * 1000 + 毫秒部分
 *
 * 这是整个协议库的时间基准，所有定时器和延迟都基于此函数。
 */
unsigned int get_ms(void)
{
    struct _timeb tm;

    _ftime(&tm);

    return (unsigned int)(epoch ? (tm.time - epoch) * 1000 + tm.millitm : 0);
}

#pragma comment(lib,"wsock32.lib")

/* ==================================================================
 *                    Linux 平台实现
 * ================================================================== */
#else /* for Linux */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#define stricmp strcasecmp
#define Sleep(ms) usleep((ms) * 1000)    /* Linux 下用 usleep 模拟 Sleep */
#define socket_init()                      /* Linux 无需显式初始化套接字 */
#define SOCKET int                         /* Linux 下 SOCKET 就是 int */

/*
 * get_ms()：获取自 epoch 起的相对毫秒时间戳（Linux 实现）
 *
 * 使用 gettimeofday() 获取当前时间，计算与 epoch 的差值。
 * 若 epoch 尚未设置（为 0），返回 0。
 */
unsigned int get_ms(void)
{
    struct timeval tm;
    struct timezone tz;

    gettimeofday(&tm, &tz);

    return (unsigned int)(epoch ? (tm.tv_sec - epoch) * 1000 + tm.tv_usec / 1000 : 0);
}

#endif

#include <math.h>

#include "protocol.h"

/* ======================== 信道参数定义 ======================== */

/*
 * CHAN_DELAY = 270ms：单向信道传播延迟
 * CHAN_BPS   = 8000：信道带宽（每秒比特数）
 *
 * 这两个参数决定了信道的传输特性：
 *   - 传播延迟影响 RTT（往返时间 = 2*270ms + 传输时间）
 *   - 带宽限制影响发送速率，通过 send_bytes_allowed 机制控制
 */
#define CHAN_DELAY 270       /* ms */
#define CHAN_BPS   8000      /* bits per second */

/* ABORT(s)：致命错误退出宏，打印错误信息并退出程序 */
#define ABORT(s) do { lprintf("\nFATAL: %s\nAbort.\n", s); exit(0); } while(0)

/* 默认参数 */
#define DEFAULT_TICK 15           /* 主循环的节拍间隔（毫秒） */
#define DEFAULT_CHAN_BER   1.0E-5 /* 默认误码率 */
#define DEFAULT_PORT  59144       /* 默认 TCP 端口号 */

/*
 * 内存越界保护机制：
 *
 * 在函数栈帧两端放置 magic number（魔数），定期检查是否被破坏。
 * 如果 magic number 被修改，说明程序发生了内存越界写操作。
 *
 * NMAGIC = 32         ：魔数数组长度（32 个元素）
 * HEAD_MAGIC          ：栈帧头部魔数值
 * FOOT_MAGIC          ：栈帧尾部魔数值
 */
#define NMAGIC     32
#define HEAD_MAGIC 0xa5a5e41b
#define FOOT_MAGIC 0xf5125a5a

static void magic_init(void);
static void magic_check(void);

/* 栈帧头部魔数数组 */
static unsigned int head_magic[NMAGIC];

/* ======================== 全局配置参数 ======================== */

/*
 * station    ：当前站点标识（'a' = 站 A / 发送方，'b' = 站 B / 接收方）
 * ber        ：误码率（Bit Error Rate），0.0 表示无差错信道（乌托邦模式）
 * mode_ibib  ：站 B 的网络层发送模式
 *              0 = BUSY-IDLE-BUSY-...（忙-闲交替）
 *              1 = IDLE-BUSY-BUSY-...（闲-忙-忙交替）
 * mode_flood ：洪泛模式（持续发送数据，不等待）
 * mode_cycle ：站 B 的发送周期（秒）
 * mode_life  ：程序运行的生命周期（毫秒），超时自动退出
 * mode_tick  ：主循环节拍间隔（毫秒）
 * mode_seed  ：随机数种子
 * debug_mask ：调试输出掩码（bit0:事件, bit1:帧, bit2:警告）
 * port       ：TCP 端口号
 */
static int station;
static double ber = DEFAULT_CHAN_BER;  /* Bit Error Rate */
static int mode_ibib = 0;    /* 0: BUSY-IDLE-BUSY-..., 1: IDLE-BUSY-BUSY-... */
static int mode_flood = 0;   /* flood mode */
static int mode_cycle = 100;  /* seconds */
static int mode_life = 0x7fffff00;
static int mode_tick = DEFAULT_TICK;
static int mode_seed = 0x098bcde1;
static int debug_mask = 0; /* debug mask */
static unsigned short port = DEFAULT_PORT;

static SOCKET sock;                 /* TCP 通信套接字 */
static int now;                     /* 当前时间戳（毫秒） */
static int noise = 0;              /* 累计引入的比特错误数 */

/*
 * station_name()：返回当前站点的名称字符串
 *
 * 返回值：
 *   站 A 返回 "A"，站 B 返回 "B"，未知返回 "XXX"
 */
char *station_name(void)
{
    return (char *)(station == 'a' ? "A" : station == 'b' ? "B" : "XXX");
}

/* ======================== 命令行选项定义 ======================== */

static struct option intopts[] = {
    { "help",   no_argument, NULL, '?' },
    { "utopia", no_argument, NULL, 'u' },    /* 无差错信道 */
    { "flood",  no_argument, NULL, 'f' },    /* 洪泛模式 */
    { "ibib",   no_argument, NULL, 'i' },    /* IDLE-BUSY 模式 */
    { "nolog",  no_argument, NULL, 'n' },    /* 不创建日志文件 */
    { "debug",  required_argument, NULL, 'd' }, /* 调试级别 */
    { "port",   required_argument, NULL, 'p' }, /* 端口号 */
    { "ber",    required_argument, NULL, 'b' }, /* 误码率 */
    { "log",    required_argument, NULL, 'l' }, /* 日志文件名 */
    { "ttl",    required_argument, NULL, 't' }, /* 生存时间 */
    { 0, 0, 0, 0 },
};

#define OPT_SHORT "?ufind:p:b:l:t:"

/*
 * config()：解析命令行参数并配置运行参数
 *
 * 支持的命令行选项：
 *   -?, --help          打印帮助信息
 *   -u, --utopia        启用乌托邦模式（ber=0，无差错信道）
 *   -f, --flood         启用洪泛模式（持续发送数据）
 *   -i, --ibib          站 B 使用 IDLE-BUSY-IDLE-BUSY 发送模式
 *   -n, --nolog         不创建日志文件
 *   -d, --debug=<0-7>   设置调试掩码
 *   -p, --port=<port>   指定 TCP 端口号
 *   -b, --ber=<ber>     设置误码率
 *   -l, --log=<filename> 指定日志文件名
 *   -t, --ttl=<seconds> 设置程序运行时间上限
 *
 * 最后必须指定站点名称（A 或 B）。
 *
 * 参数：
 *   argc, argv：main 函数传入的命令行参数
 */
static void config(int argc, char **argv)
{
    char fname[1024];
    int opt;

    /* 未提供足够参数时打印用法并退出 */
    if (argc < 2) {
    usage:
        printf("\nUsage:\n  %s <options> <station-name>\n", argv[0]);
        printf(
            "\nOptions : \n"
            "    -?, --help : print this\n"
            "    -u, --utopia : utopia channel (an error-free channel)\n"
            "    -f, --flood : flood traffic\n"
            "    -i, --ibib  : set station B layer 3 sender mode as IDLE-BUSY-IDLE-BUSY-...\n"
            "    -n, --nolog : do not create log file\n"
            "    -d, --debug=<0-7>: debug mask (bit0:event, bit1:frame, bit2:warning)\n"
            "    -p, --port=<port#> : TCP port number (default: %u)\n"
            "    -b, --ber=<ber> : Bit Error Rate (received data only)\n"
            "    -l, --log=<filename> : using assigned file as log file\n"
            "    -t, --ttl=<seconds> : set time-to-live\n"
            "\n"
            "i.e.\n"
            "    %s -fd3 -b 1e-4 A\n"
            "    %s --flood --debug=3 --ber=1e-4 A\n"
            "\n",
            DEFAULT_PORT, argv[0], argv[0]);
        exit(0);
    }

    /* 日志文件名默认为空 */
    strcpy(fname, "");

    /* 循环解析命令行选项 */
    while ((opt = getopt_long(argc, argv, OPT_SHORT, intopts, NULL)) != -1) {
        switch (opt) {
        case '?':
            goto usage;

        case 'u':
            ber = 0.0;          /* 乌托邦模式：无误码 */
            break;

        case 'f':
            mode_flood = 1;     /* 洪泛模式 */
            break;

        case 'i':
            mode_ibib = 1;      /* IDLE-BUSY-IDLE-BUSY 模式 */
            break;

        case 'n':
            strcpy(fname, "nul");  /* 不写日志 */
            break;

        case 'd':
            debug_mask = atoi(optarg);  /* 调试掩码 */
            break;

        case 'p':
            port = (unsigned short)atoi(optarg);  /* 端口号 */
            break;

        case 'b':
            ber = strtod(optarg, 0);  /* 误码率 */
            if (ber >= 1.0) {
                printf("Bad BER %.3f\n", ber);
                goto usage;
            }
            break;

        case 'l':
            strcpy(fname, optarg);  /* 日志文件名 */
            break;

        case 't':
            mode_life = atoi(optarg) * 1000; /* 生存时间（转换为毫秒） */
            break;

        default:
            printf("ERROR: Unsupported option\n");
            goto usage;
        }
    }

    /* 必须指定站点名称 */
    if (optind == argc)
        goto usage;

    /* 站点名称必须是 'A' 或 'B' */
    station = tolower(argv[optind++][0]);
    if (station != 'a' && station != 'b')
        ABORT("Station name must be 'A' or 'B'");

    /* 根据可执行文件名和站点名生成默认日志文件名 */
    if (fname[0] == 0) {
        strcpy(fname, argv[0]);
        if (stricmp(fname + strlen(fname) - 4, ".exe") == 0)
            *(fname + strlen(fname) - 4) = 0;  /* 去掉 .exe 后缀 */
        strcat(fname, station == 'a' ? "-A.log" : "-B.log");
    }

    /* 打开日志文件 */
    if (stricmp(fname, "nul") == 0)
        log_file = NULL;  /* 不写日志 */
    else if ((log_file = fopen(fname, "w")) == NULL)
        printf("WARNING: Failed to create log file \"%s\": %s\n", fname, strerror(errno));

    /* 打印配置信息到日志 */
    lprintf(
        "=============================================================\n"
        "                    Station %s                               \n"
        "-------------------------------------------------------------\n",
        station_name());

    lprintf("Protocol.lib, version %s, jiangyanjun0718@bupt.edu.cn\n", VERSION, __DATE__);
    lprintf("Channel: %d bps, %d ms propagation delay, bit error rate ", CHAN_BPS, CHAN_DELAY);
    if (ber > 0.0)
        lprintf("%.1E\n", ber);
    else
        lprintf("0\n");
    lprintf("Log file \"%s\", TCP port %d, debug mask 0x%02x\n", fname, port, debug_mask);
}

/* ==================================================================
 *              通信套接字创建与协议初始化
 * ================================================================== */

/*
 * protocol_init()：初始化整个协议栈
 *
 * 这是协议栈的入口初始化函数，完成以下工作：
 *
 *   1. 初始化套接字库（Windows 下需要）
 *   2. 初始化内存保护魔数
 *   3. 解析命令行参数
 *   4. 建立 TCP 连接（站 A 作为服务端，站 B 作为客户端）
 *   5. 时间同步（站 B 发送 epoch 给站 A）
 *   6. 配置套接字选项（超时、缓冲区大小、Nagle 算法）
 *
 * 连接建立流程：
 *   站 A（服务端）：
 *     socket() → bind() → listen() → accept() → 接收 epoch
 *   站 B（客户端）：
 *     socket() → connect() → 发送 epoch
 *
 * 参数：
 *   argc, argv：main 函数传入的命令行参数
 */
void protocol_init(int argc, char **argv)
{
    SOCKET admin_sock;
    int i;
    struct sockaddr_in name;

    socket_init();
    magic_init();

    config(argc, argv);

    /* ============ 站 A（服务端）============ */
    if (station == 'a') {

        srand(mode_seed ^ 97209);  /* 初始化随机数生成器 */

        /* 绑定地址：INADDR_ANY 表示监听所有网络接口 */
        name.sin_family = AF_INET;
        name.sin_addr.s_addr = INADDR_ANY;
        name.sin_port = htons(port);

        /* 创建监听套接字 */
        admin_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (admin_sock < 0)
            ABORT("Create TCP socket");
        if (bind(admin_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
            lprintf("Station A: Failed to bind TCP port %u", port);
            ABORT("Station A failed to bind TCP port");
        }

        listen(admin_sock, 5);  /* 开始监听，最大等待队列长度 5 */

        lprintf("Station A is waiting for station B on TCP port %u ... ", port);
        fflush(stdout);

        /* 阻塞等待站 B 连接 */
        sock = accept(admin_sock, 0, 0);
        if (sock < 0)
            ABORT("Station A failed to communicate with station B");
        lprintf("Done.\n");

        /* 接收站 B 的 epoch，实现时间同步 */
        recv(sock, (char *)&epoch, sizeof(epoch), 0);
    }

    /* ============ 站 B（客户端）============ */
    if (station == 'b') {

        srand(mode_seed ^ 18231);  /* 初始化随机数生成器 */

        /* 创建客户端套接字 */
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0)
            ABORT("Create TCP socket");

        /* 连接目标：127.0.0.1（本机） */
        name.sin_family = AF_INET;
        name.sin_addr.s_addr = inet_addr("127.0.0.1");
        name.sin_port = htons((short)port);

        /* 重试连接（最多 60 次，每次间隔 2 秒） */
        for (i = 0; i < 60; i++) {
            lprintf("Station B is connecting station A (TCP port %u) ... ", port);
            fflush(stdout);

            if (connect(sock, (struct sockaddr *)&name, sizeof(struct sockaddr_in)) < 0) {
                lprintf("Failed!\n");
                Sleep(2000);
            } else {
                lprintf("Done.\n");
                break;
            }
        }
        if (i == 6)
            ABORT("Station B failed to connect station A");

        /* 发送本机 epoch 给站 A，实现时间同步 */
        time(&epoch);
        send(sock, (char *)&epoch, sizeof(epoch), 0);
    }

    /* 打印 epoch 信息 */
    {
        struct tm *newtime;
        newtime = localtime(&epoch);
        lprintf("New epoch: %s", asctime(newtime));
        lprintf("=================================================================\n\n");
    }

    /* 配置套接字选项 */
    {
        int timeout_ms = 10;       /* 收发超时 10ms（非阻塞轮询） */
        int buf_size = 1024 * 64;  /* 64KB 缓冲区 */
        int on = 1;

        /* 设置收发超时 */
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout_ms, sizeof(int));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout_ms, sizeof(int));

        /* 增大收发缓冲区 */
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&buf_size, sizeof(int));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&buf_size, sizeof(int));

        /* 禁用 Nagle 算法（TCP_NODELAY），减少延迟 */
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
    }

    get_ms();  /* 初始化时间基准 */
}

/* ==================================================================
 *                    物理层：发送端
 * ================================================================== */

/*
 * 发送队列结构：
 *
 * SQ_SIZE = 128KB：发送循环队列的总容量
 *
 * 数据的流动路径：
 *   上层（datalink.c）→ send_frame() → send_byte() → sq[] 队列
 *                   → socket_send() → TCP 发送
 *
 * 带宽控制：send_bytes_allowed 变量记录当前时刻允许发送的字节数，
 * 基于信道带宽 (CHAN_BPS) 和时间流逝计算，实现速率限制。
 *
 * 帧编码：每个原始字节被拆分为两个 4 位半字节（nibble）发送，
 * 帧边界用 0xFF 标记。这是为了模拟物理层的比特传输。
 */

#define SQ_SIZE (128 * 1024)

static unsigned char sq[SQ_SIZE];   /* 发送循环队列 */
static int sq_head, sq_tail;        /* 队列头尾指针 */
static int inform_phl_ready = 1;    /* 是否通知物理层就绪 */

#define sq_inc(p, n) (p = (p + n) % SQ_SIZE)  /* 队列指针递增（模 SQ_SIZE） */

static int send_bytes_allowed = 0;  /* 当前时刻允许发送的字节数（带宽控制） */

/*
 * sq_len()：返回发送队列中当前待发送的字节数
 *
 * 计算循环队列中 sq_head 到 sq_tail 之间的数据量。
 *
 * 返回值：
 *   队列中的数据字节数
 */
static int sq_len(void)
{
    return (sq_tail + SQ_SIZE - sq_head) % SQ_SIZE;
}

/*
 * phl_sq_len()：获取物理层发送队列长度（供 datalink.c 调用）
 *
 * 返回值：
 *   发送队列中的字节数
 *
 * 用途：datalink.c 的 update_network_layer() 通过此函数判断
 * 物理层是否有足够空间，从而控制网络层的发送速率。
 */
int phl_sq_len(void)
{
    return sq_len();
}

/*
 * send_byte()：向物理层发送队列写入一个字节
 *
 * 数据优先直接发送（如果允许且队列为空），否则进入循环队列排队。
 *
 * 带宽控制逻辑：
 *   - 如果 send_bytes_allowed > 0 且队列为空：直接发送（不排队）
 *   - 否则：将字节加入队列，等待 socket_send() 定时发送
 *   - 队列满时触发 ABORT（防止无限积压）
 *
 * 参数：
 *   byte：要发送的一个字节
 */
static void send_byte(unsigned char byte)
{
    inform_phl_ready = 1;

    /* 如果带宽允许且队列为空，直接发送（省去排队延迟） */
    if (send_bytes_allowed && sq_head == sq_tail) {
        send(sock, (char *)&byte, 1, 0);
        send_bytes_allowed--;
        return;
    }

    /* 队列满时触发致命错误 */
    if (sq_len() == SQ_SIZE - 1)
        ABORT("Physical Layer Sending Queue overflow");

    /* 将字节加入循环队列 */
    sq[sq_tail] = byte;
    sq_inc(sq_tail, 1);
}

/*
 * send_frame()：向物理层发送一个完整的帧
 *
 * 帧编码格式：
 *   起始标记 0xFF
 *   + 对每个字节 frame[i]：
 *       发送低 4 位（frame[i] & 0x0f）
 *       发送高 4 位（(frame[i] & 0xf0) >> 4）
 *   + 结束标记 0xFF
 *
 * 这种编码方式称为"半字节编码"（nibble encoding）：
 *   - 每个原始字节被拆分为两个 4 位半字节
 *   - 每个半字节作为一个独立的字节发送（高 4 位为 0）
 *   - 0xFF 作为帧边界标记（因为它不是合法的半字节编码值）
 *
 * 参数：
 *   frame：帧数据缓冲区
 *   len：帧长度（字节数）
 */
void send_frame(unsigned char *frame, int len)
{
    int i;

    send_byte(0xff);  /* 帧起始标记 */

    for (i = 0; i < len; i++) {
        send_byte(frame[i] & 0x0f);           /* 低 4 位 */
        send_byte((frame[i] & 0xf0) >> 4);    /* 高 4 位 */
    }
    send_byte(0xff);  /* 帧结束标记 */
}

/*
 * send_sq_data()：将发送队列中 [start, end1) 区间的数据通过 TCP 发出
 *
 * 参数：
 *   start：起始索引
 *   end1：结束索引（不含）
 *
 * 返回值：
 *   实际发送的字节数；若 TCP 断开则退出程序
 */
static int send_sq_data(unsigned int start, unsigned int end1)
{
    int ret;

    if (start >= end1)
        return 0;

    ret = send(sock, (char *)&sq[start], end1 - start, 0);
    if (ret <= 0) {
        lprintf("TCP Disconnected.\n");
        exit(0);
    }

    return ret;
}

/*
 * socket_send()：定时从发送队列中取出数据并通过 TCP 发送
 *
 * 带宽控制核心：
 *   send_bytes_allowed = (时间差) * 带宽(bps) / 8(字节) / 1000(毫秒) * 2
 *
 *   乘以 2 是因为每个原始字节经过半字节编码后变为 2 个字节，
 *   所以物理层发送速率是应用数据速率的两倍。
 *
 * 工作流程：
 *   1. 根据时间流逝计算本周期允许发送的字节数
 *   2. 从队列中取出不超过允许量的数据
 *   3. 通过 TCP send 发出
 *   4. 更新队列头指针和剩余允许量
 */
static void socket_send(void)
{
    static int last_ts = 0;  /* 上次发送的时间戳 */
    int n, send_tail = sq_head, send_bytes;

    if (last_ts == 0)
        last_ts = now;

    if (now <= last_ts)  /* 时间未前进，无需发送 */
        return;

    /* 根据时间流逝和信道带宽计算本周期允许发送的字节数 */
    /* * 2 是因为半字节编码：1 个原始字节 → 2 个线上字节 */
    send_bytes_allowed = (now - last_ts) * CHAN_BPS / 8 / 1000 * 2;
    n = sq_len();
    if (n > send_bytes_allowed)
        n = send_bytes_allowed;
    sq_inc(send_tail, n);  /* 计算本次发送的尾部位置 */

    /* 处理循环队列的两种分片情况 */
    if (send_tail >= sq_head)
        send_bytes = send_sq_data(sq_head, send_tail);     /* 单段发送 */
    else {
        send_bytes = send_sq_data(sq_head, SQ_SIZE);        /* 先发送尾部段 */
        send_bytes += send_sq_data(0, send_tail);            /* 再发送头部段 */
    }

    sq_inc(sq_head, send_bytes);          /* 更新队列头指针 */
    send_bytes_allowed -= send_bytes;     /* 扣除已发送的字节 */
    last_ts = now;
}

/* ==================================================================
 *                    物理层：接收端
 * ================================================================== */

/*
 * BLKSIZE：接收块大小
 * 公式：16 * 带宽(bps) / 8(字节) / (1000/tick) = 每次 tick 可接收的字节数 * 16
 */
#define BLKSIZE (16 * CHAN_BPS / 8 / (1000 / DEFAULT_TICK))

/*
 * struct BLK：接收数据块（链表节点）
 *
 * 接收到的数据按块组织成链表，每个块包含：
 *   commit_ts：交付时间戳（模拟传播延迟后何时可用）
 *   rptr/wptr：读/写指针（类似循环队列）
 *   link：指向下一个块的指针
 *   data[]：实际数据
 */
struct BLK {
    int commit_ts;                      /* 数据可交付的时间戳（当前时间 + 传播延迟） */
    int rptr, wptr;                     /* 读指针和写指针 */
    struct BLK *link;                   /* 链表指针 */
    unsigned char data[BLKSIZE];        /* 数据缓冲区 */
};

static struct BLK *rblk_head, *rblk_tail;  /* 接收块链表头尾 */
static unsigned int nbits;                  /* 累计接收的总比特数（用于 BER 统计） */

/*
 * socket_recv()：从 TCP 套接字接收数据，创建接收块加入链表
 *
 * 工作流程：
 *   1. 分配一个新的 BLK 结构体
 *   2. 通过 recv() 读取数据到块中
 *   3. 模拟信道误码（按 BER 概率翻转随机比特）
 *   4. 设置 commit_ts = now + 传播延迟（模拟信道延迟）
 *   5. 将块加入接收链表尾部
 *
 * 误码模拟：
 *   - 根据当前实际误码率与目标 BER 的比较，动态调整噪声注入概率
 *   - fact 参数：实际误码率高于目标时取 3.5（适当降低），低于时取 6.0（积极注入）
 *   - 噪声注入：随机选一个字节，翻转其中随机一个比特
 *
 * 注意：
 *   - 误码只在接收端模拟（"received data only"）
 *   - 如果字节的低 4 位为 0，不注入噪声（保护帧边界标记 0xFF？）
 */
static void socket_recv(void)
{
    struct BLK *blk;
    unsigned char *p;

    /* 分配新的接收块 */
    blk = (struct BLK *)malloc(sizeof(struct BLK));
    if (blk == NULL)
        ABORT("No enough memory");

    blk->rptr = 0;
    blk->wptr = recv(sock, (char *)blk->data, BLKSIZE, 0);
    if (blk->wptr <= 0) {
        lprintf("TCP disconnected.\n");
        exit(0);
    }
    nbits += blk->wptr * 4;  /* 累计接收比特数（*4 因为半字节编码？） */

    /* 模拟信道误码 */
    if (ber != 0.0) {
        int a;
        double rate, fact;

        rate = (double)noise / nbits;  /* 当前实际误码率 */
        /* 动态调整因子：实际误码率超目标时减小噪声，反之加大 */
        fact = rate > ber ? 3.5 : 6.0;
        /* 计算本块注入噪声的概率 */
        a = (int)((1.0 - pow(1.0 - ber, fact * blk->wptr)) * (RAND_MAX + 1.0) + 0.5);
        if (rand() <= a) {
            /* 随机选择块中一个字节，翻转其随机一个比特 */
            p = &blk->data[rand() % blk->wptr];
            if (*p & 0x0f) {  /* 仅在低 4 位非零时注入噪声 */
                *p ^= 1 << (rand() % 8);  /* 翻转随机比特 */
                noise++;
                dbg_warning("Impose noise on received data, %u/%u=%.1E\n", noise, nbits, (double)noise / nbits);
            }
        }
    }

    /* 设置交付时间 = 当前时间 + 传播延迟 - 10ms（提前量） */
    blk->commit_ts = now + CHAN_DELAY - 10;
    blk->link = NULL;

    /* 加入接收链表 */
    if (rblk_head == NULL)
        rblk_head = rblk_tail = blk;
    else {
        rblk_tail->link = blk;
        rblk_tail = blk;
    }
}

/*
 * recv_byte()：从接收队列中读取一个字节
 *
 * 从接收链表的头部块中读取一个字节。当块中所有数据被读完时，
 * 释放该块并将链表头移到下一个块。
 *
 * 前提条件：接收队列不能为空，且当前时间已超过块的 commit_ts。
 * 否则触发 ABORT（说明调用时机不对）。
 *
 * 返回值：
 *   读取到的字节
 */
static unsigned char recv_byte(void)
{
    unsigned char ch;
    struct BLK *blk = rblk_head;

    if (blk == NULL || blk->commit_ts > now)
        ABORT("recv_byte(): Receiving Queue is empty");

    ch = blk->data[blk->rptr++];  /* 读取一个字节，读指针后移 */
    if (blk->rptr == blk->wptr) {  /* 当前块读完了 */
        rblk_head = blk->link;     /* 链表头移到下一块 */
        free(blk);                 /* 释放已读完的块 */
    }

    return ch;
}

/* ==================================================================
 *                    定时器管理
 * ================================================================== */

/*
 * 定时器体系说明：
 *
 * NTIMER = 129：共 129 个定时器
 *   - timer[0] ~ timer[127]：数据帧定时器，每个窗口中的帧一个
 *     （实际上总共只用了 NR_BUFS 个，但按序号索引）
 *   - timer[ACK_TIMER_ID] = timer[128]：ACK/NAK 定时器
 *
 * 定时器原理：
 *   - 每个定时器存储一个绝对时间戳（ms），表示到期时刻
 *   - 定时器值为 0 表示未启动
 *   - scan_timer() 扫描所有定时器，找到第一个已到期的
 */

#define NTIMER 129
static int timer[NTIMER];
#define ACK_TIMER_ID (NTIMER - 1)  /* ACK 定时器 ID = 128 */

/*
 * start_timer()：启动指定序号的数据帧定时器
 *
 * 定时器到期时间 = 当前时间 + 队列中数据的发送时间 + 指定延迟
 * phl_sq_len() * 8000 / CHAN_BPS 估算发送队列中已有数据的传输时间。
 *
 * 参数：
 *   nr：定时器序号（0~127）
 *   ms：超时时间（毫秒）
 */
void start_timer(unsigned int nr, unsigned int ms)
{
    if (nr >= ACK_TIMER_ID)
        ABORT("start_timer(): timer No. must be 0~128");
    timer[nr] = now + phl_sq_len() * 8000 / CHAN_BPS + ms;
}

/*
 * stop_timer()：停止指定序号的数据帧定时器
 *
 * 将定时器值设为 0 表示停止。仅对数据定时器有效（0~127），
 * ACK 定时器有专门的控制函数。
 *
 * 参数：
 *   nr：定时器序号
 */
void stop_timer(unsigned int nr)
{
    if (nr < ACK_TIMER_ID)
        timer[nr] = 0;
}

/*
 * get_timer()：获取指定定时器的剩余时间
 *
 * 返回值：
 *   0          ：定时器未启动或已到期，或 ID 无效
 *   timer[nr] - now：定时器到期的剩余毫秒数
 */
int get_timer(unsigned int nr)
{
    if (nr >= ACK_TIMER_ID || timer[nr] == 0)
        return 0;
    return timer[nr] > now ? timer[nr] - now : 0;
}

/*
 * start_ack_timer()：启动 ACK/NAK 定时器
 *
 * ACK 定时器用于两种用途：
 *   1. ACK 累积确认延迟：收到 DATA 帧后不立即 ACK，等定时器到期再发送
 *   2. NAK 重发间隔：NAK 可能丢失，定时重复发送
 *
 * 仅在定时器未启动时设置（避免覆盖正在运行的定时器）。
 *
 * 参数：
 *   ms：定时器延迟（毫秒）
 */
void start_ack_timer(unsigned int ms)
{
    if (timer[ACK_TIMER_ID] == 0)
        timer[ACK_TIMER_ID] = now + ms;
}

/*
 * stop_ack_timer()：停止 ACK/NAK 定时器
 */
void stop_ack_timer(void)
{
    timer[ACK_TIMER_ID] = 0;
}

/*
 * scan_timer()：扫描所有定时器，找到第一个到期的
 *
 * 遍历 timer[0] 到 timer[NTIMER-1]：
 *   - 检查每个非零且已到期的定时器（timer[i] <= now）
 *   - 将到期定时器清零
 *   - 返回对应的事件类型：
 *       ACK_TIMEOUT：ACK 定时器到期
 *       DATA_TIMEOUT：数据帧定时器到期（*nr 设置为定时器序号）
 *
 * 参数：
 *   nr：输出参数，存放到期定时器的序号（数据定时器时有效）
 *
 * 返回值：
 *   0          ：无定时器到期
 *   ACK_TIMEOUT：ACK 定时器到期
 *   DATA_TIMEOUT：数据帧定时器到期
 */
static int scan_timer(int *nr)
{
    int i;

    for (i = 0; i < NTIMER; i++) {
        if (timer[i] && timer[i] <= now) {
            *nr = i;
            timer[i] = 0;  /* 清除定时器 */
            return i == ACK_TIMER_ID ? ACK_TIMEOUT : DATA_TIMEOUT;
        }
    }
    return 0;
}

/* ==================================================================
 *                    网络层函数
 * ================================================================== */

/*
 * 网络层接口说明：
 *
 * 数据链路层（datalink.c）通过网络层接口与上层通信：
 *   - get_packet()：从网络层获取待发送的数据包
 *   - put_packet()：将接收到的数据包交付给网络层
 *   - enable/disable_network_layer()：流量控制
 *
 * 实现中，网络层生成模拟的测试数据包（而非真正的应用数据）。
 * 数据包内容为随机填充，前 2 字节为包序号（用于接收方校验）。
 */

static int network_layer_active = 0;  /* 网络层是否活跃 */
static int rpackets, rbytes;          /* 接收统计：包数量、总字节数 */

/*
 * enable_network_layer()：启用网络层
 *
 * 允许网络层提供新的数据包给数据链路层发送。
 */
void enable_network_layer(void)
{
    network_layer_active = 1;
}

/*
 * disable_network_layer()：禁用网络层
 *
 * 暂停网络层提供数据包，实现背压流量控制。
 */
void disable_network_layer(void)
{
    network_layer_active = 0;
}

/*
 * network_layer_ready()：检查网络层是否准备好发送新数据包
 *
 * 返回 1 的条件：
 *   1. 网络层处于活跃状态
 *   2. 满足发送间隔限制（基于信道带宽的速率控制）
 *   3. 站 B 的特殊发送模式（BUSY-IDLE 交替）
 *   4. 已过初始冷却期（CHAN_DELAY + 3 个数据包的传输时间）
 *
 * 洪泛模式下，只要网络层活跃就返回 1（忽略速率限制）。
 *
 * 返回值：
 *   1：可以发送新数据包
 *   0：暂不能发送
 */
static int network_layer_ready(void)
{
    static int last_ts = 0;

    if (!network_layer_active)
        return 0;

    /* 洪泛模式：无条件允许发送 */
    if (mode_flood)
        return 1;

    /* 速率控制：确保带宽利用率不超过 75% */
    if ((now - last_ts) * CHAN_BPS / 8 / 1000 < PKT_LEN * 3 / 4)
        return 0;

    /* 站 B 的特殊发送模式 */
    if (station == 'b') {
        /* BUSY-IDLE 交替模式 */
        if (now / 1000 / mode_cycle % 2 != mode_ibib) {
            if (now - last_ts < 4000 + rand() % 500)
                return 0;
        }
        /* 初始冷却期（等待管道填满） */
        if (now < CHAN_DELAY + 3 * PKT_LEN * 8000 / CHAN_BPS)
            return 0;
    }

    last_ts = now;

    return 1;
}

/*
 * randA()：站 A 的伪随机数生成器
 *
 * 使用线性同余生成器（LCG）：
 *   holdrand = holdrand * 214013 + 2531011
 *   返回 (holdrand >> 16) & 0x7fff
 *
 * 每个站使用独立的随机数生成器实例，确保双方数据可预测
 * （接收方需要生成相同的数据来校验包内容）。
 *
 * 返回值：
 *   15 位伪随机数（0~32767）
 */
static int randA(void)
{
    static unsigned int holdrand = 0x65109bc4;
    return ((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff;
}

/*
 * randB()：站 B 的伪随机数生成器
 *
 * 与 randA 相同的算法，但使用不同的种子（0x1e459090），
 * 确保两站产生不同的随机序列。
 *
 * 返回值：
 *   15 位伪随机数（0~32767）
 */
static int randB(void)
{
    static unsigned int holdrand = 0x1e459090;
    return ((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff;
}

/*
 * next_char()：从随机数生成器中提取一个字节
 *
 * 取 my_rand() 的低 8 位作为一个随机字节。
 */
#define next_char() ((unsigned char)(my_rand() & 0xff))

static int layer3_ready = 0;  /* 网络层是否有准备好的数据包 */

/*
 * get_packet()：从网络层获取一个待发送的数据包
 *
 * 生成模拟的测试数据包：
 *   1. 包内容为伪随机数据（前 2 字节除外）
 *   2. 前 2 字节为包序号：站 A 从 10000 开始，站 B 从 20000 开始
 *      （station - 'a' + 1 = 1 for A, 2 for B）
 *
 * 注意：必须在 network_layer_ready() 返回 1 后调用，
 * 否则触发 ABORT。
 *
 * 参数：
 *   packet：输出缓冲区（至少 PKT_LEN 字节）
 *
 * 返回值：
 *   数据包长度（= PKT_LEN）
 */
int get_packet(unsigned char *packet)
{
    static int pkt_no = 0;
    int i, len;
    int (*my_rand)(void) = station == 'a' ? randA : randB;

    if (!layer3_ready)
        ABORT("get_packet(): Network layer is not ready for a new packet");

    len = PKT_LEN;
    /* 填充随机数据（从第 3 字节开始，前 2 字节为序号） */
    for (i = 2; i < len; i++)
        packet[i] = next_char();
    /* 前 2 字节存放包序号（用于接收方校验和调试） */
    *(unsigned short *)packet = (station - 'a' + 1) * 10000 + (pkt_no++ % 10000);

    layer3_ready = 0;  /* 消费数据包，标记为未就绪 */

    return len;
}

static int ts0;  /* 首次接收的时间戳（用于吞吐量计算） */

/*
 * put_packet()：将接收到的数据包交付给网络层
 *
 * 工作流程：
 *   1. 校验包长度（必须等于 PKT_LEN）
 *   2. 校验包内容（重新生成随机序列，比对是否一致）
 *   3. 更新接收统计（包数、字节数）
 *   4. 定期输出吞吐量统计信息
 *
 * 校验失败则 ABORT——说明数据链路层交付了错误的数据。
 *
 * 参数：
 *   packet：接收到的数据包
 *   len：包长度（必须等于 PKT_LEN）
 */
void put_packet(unsigned char *packet, int len)
{
    static int last_ts = 0;
    int i, (*my_rand)(void) = station == 'a' ? randB : randA;

    if (len != PKT_LEN)
        ABORT("Bad Packet length");

    /* 校验数据包内容：重新生成相同的随机序列并比对 */
    for (i = 2; i < PKT_LEN; i++) {
        if (packet[i] != next_char())
            ABORT("Network Layer received a bad packet from data link layer");
    }
    rpackets++;
    rbytes += len;

    /* 每 2 秒打印一次吞吐量统计 */
    if (now - last_ts > 2000 && now > ts0 + 2000) {
        double bps;
        bps = (double)rbytes * 8 * 1000 / (now - ts0);
        lprintf(".... %d packets received, %.0f bps, %.2f%%, Err %d (%.1e)\n",
            rpackets, bps, bps / CHAN_BPS * 100, noise, (double)noise/nbits);
        last_ts = now;
    }
}

/* ======================== 调试输出函数 ======================== */

#define DBG_EVENT    0x01   /* 事件调试 */
#define DBG_FRAME    0x02   /* 帧调试 */
#define DBG_WARNING  0x04   /* 警告调试 */

/*
 * dbg_event()：输出事件级别的调试信息
 *
 * 仅在 debug_mask 的 DBG_FRAME 位为 1 时输出。
 * （注意：函数名是 dbg_event，但实际检查的是 DBG_FRAME 位，
 *  且它与 dbg_frame 的逻辑完全一致，这可能是原始代码的一个小疏忽）
 *
 * 参数：
 *   fmt, ...：printf 风格的格式字符串和参数
 */
void dbg_event(char *fmt, ...)
{
    va_list arg_ptr;

    if (debug_mask & DBG_FRAME) {
        va_start(arg_ptr, fmt);
        __v_lprintf(fmt, arg_ptr);
        va_end(arg_ptr);
    }
}

/*
 * dbg_frame()：输出帧级别的调试信息
 *
 * 仅在 debug_mask 的 DBG_FRAME 位为 1 时输出。
 * 用于打印帧的发送/接收详情（SEQ、ACK 号等）。
 *
 * 参数：
 *   fmt, ...：printf 风格的格式字符串和参数
 */
void dbg_frame(char *fmt, ...)
{
    va_list arg_ptr;

    if (debug_mask & DBG_FRAME) {
        va_start(arg_ptr, fmt);
        __v_lprintf(fmt, arg_ptr);
        va_end(arg_ptr);
    }
}

/*
 * dbg_warning()：输出警告级别的调试信息
 *
 * 仅在 debug_mask 的 DBG_WARNING 位为 1 时输出。
 * 用于打印信道噪声注入等警告信息。
 *
 * 参数：
 *   fmt, ...：printf 风格的格式字符串和参数
 */
void dbg_warning(char *fmt, ...)
{
    va_list arg_ptr;

    if (debug_mask & DBG_WARNING) {
        va_start(arg_ptr, fmt);
        __v_lprintf(fmt, arg_ptr);
        va_end(arg_ptr);
    }
}

/* ==================================================================
 *                    事件生成器（核心调度）
 * ================================================================== */

/*
 * 事件系统是整个协议栈的核心调度机制。
 *
 * 五类事件：
 *   NETWORK_LAYER_READY  ：网络层有数据要发送
 *   PHYSICAL_LAYER_READY ：物理层发送队列有空间
 *   FRAME_RECEIVED       ：接收到一个完整帧
 *   DATA_TIMEOUT         ：数据帧定时器超时
 *   ACK_TIMEOUT          ：ACK/NAK 定时器超时
 *
 * wait_for_event() 在一个循环中检查所有事件源，
 * 返回最先发生的事件给 datalink.c 的主循环处理。
 */

#define PHL_SQ_LEVEL  50  /* 物理层队列低水位线（触发 PHYSICAL_LAYER_READY） */

static int sleep_cnt, start_ms, wakeup_ms, busy_cnt;  /* 性能监控变量 */
static int bias_cnt;  /* 时间偏差计数器 */

/*
 * struct RCV_FRAME：接收帧链表节点
 *
 * 接收到的帧在解码后放入此链表，等待 wait_for_event 取走。
 */
struct RCV_FRAME {
    int len;                           /* 帧数据长度 */
    int state;                         /* 解码状态（0：等待低 4 位，1：等待高 4 位） */
    unsigned char frame[2048];         /* 帧数据缓冲区 */
    struct RCV_FRAME *link;            /* 链表指针 */
};

static struct RCV_FRAME *rf_head, *rf_tail, *rf_buf;  /* 接收帧链表 */

/*
 * recv_frame()：从接收帧队列中取出一帧
 *
 * 将链表头部帧的数据复制到用户缓冲区，释放链表节点。
 *
 * 前提条件：接收队列不能为空。
 * 如果缓冲区太小装不下帧数据，触发 ABORT。
 *
 * 参数：
 *   buf：用户提供的缓冲区
 *   size：缓冲区大小
 *
 * 返回值：
 *   帧数据的实际长度（字节数）
 */
int recv_frame(unsigned char *buf, int size)
{
    int len;
    struct RCV_FRAME *next;
    char msg[256];

    if (rf_head == NULL)
        ABORT("recv_frame(): Receiving Queue is empty");

    len = rf_head->len;

    if (size < len) {
        sprintf(msg, "recv_frame(): %d-byte buffer is too small to save %d-byte received frame", size, len);
        ABORT(msg);
    }

    memcpy(buf, rf_head->frame, len);

    /* 从链表中移除头部节点 */
    next = rf_head->link;
    if (next == NULL)
        rf_tail = NULL;
    free(rf_head);
    rf_head = next;

    return len;
}

/*
 * wait_for_event()：等待并返回下一个事件
 *
 * 这是协议栈的"心脏"——统一的事件循环，每轮执行以下步骤：
 *
 *   1. 更新当前时间（now = get_ms()）
 *   2. 检查接收数据块是否到期（commit_ts <= now）
 *      → 解码半字节编码的字节流
 *      → 识别 0xFF 帧边界标记
 *      → 组装完整帧加入 rf 链表
 *      → 如有完整帧，返回 FRAME_RECEIVED
 *   3. 通过 select() 检查套接字状态
 *      → 可写：调用 socket_send() 发送数据
 *      → 可读：调用 socket_recv() 接收数据
 *   4. 检查网络层是否有新数据要发送
 *      → 返回 NETWORK_LAYER_READY
 *   5. 扫描所有定时器
 *      → 返回 DATA_TIMEOUT 或 ACK_TIMEOUT
 *   6. 检查物理层发送队列是否低于水位线
 *      → 返回 PHYSICAL_LAYER_READY
 *   7. 休眠一个 tick（mode_tick 毫秒）
 *   8. 检查是否超过生命周期（mode_life）
 *
 * 半字节解码状态机：
 *   rf_buf->state == 0：当前收到的是低 4 位
 *   rf_buf->state == 1：当前收到的是高 4 位，与之前的低 4 位拼接
 *
 * 参数：
 *   arg：输出参数，存放事件的附加信息（如超时的定时器 ID）
 *
 * 返回值：
 *   事件类型常量（NETWORK_LAYER_READY, FRAME_RECEIVED 等）
 *
 * 注意：本函数使用 Sleep() 来模拟实时运行，实际项目中使用时应替换为
 * 真正的实时调度器或硬件定时器。
 */
int wait_for_event(int *arg)
{
    fd_set rfd, wfd;
    struct timeval tm;
    int event, n, i, nfds;
    unsigned char ch;

    for (;;) {

        now = get_ms();  /* 更新全局时间戳 */

        /* ====== 阶段 2：处理已到期的接收数据块 ====== */
        if (rblk_head && rblk_head->commit_ts <= now) {
            /* 计算当前块中等待处理的字节数 */
            n = rblk_head->wptr - rblk_head->rptr;

            /* 记录首次接收时间（用于吞吐量统计） */
            if (ts0 == 0) {
                ts0 = now;
                if (ts0 >= n / 2)
                    ts0 -= n / 2;  /* 回退半个块的时间作为补偿 */
            }

            /* 处理块中的每个字节 */
            for (i = 0; i < n; i++) {
                ch = recv_byte();
                if (ch == 0xff) {
                    /* 0xFF 是帧边界标记 */
                    if (rf_buf == NULL)
                        /* 帧开始：分配新的帧缓冲区 */
                        rf_buf = (struct RCV_FRAME *)calloc(1, sizeof(struct RCV_FRAME));
                    else {
                        /* 帧结束：将完成的帧加入链表 */
                        if (rf_buf->len > 0) {
                            if (rf_head == NULL)
                                rf_head = rf_tail = rf_buf;
                            else {
                                rf_tail->link = rf_buf;
                                rf_tail = rf_buf;
                            }
                            rf_buf = NULL;  /* 准备接收下一帧 */
                        }
                    }
                } else if (rf_buf && rf_buf->len < sizeof(rf_buf->frame)) {
                    /* 半字节解码状态机 */
                    if (rf_buf->state == 0) {
                        /* 状态 0：收到低 4 位，保存 */
                        rf_buf->frame[rf_buf->len] = ch;
                        rf_buf->state = 1;  /* 切换到等待高 4 位状态 */
                    } else {
                        /* 状态 1：收到高 4 位，与低 4 位拼接成完整字节 */
                        rf_buf->frame[rf_buf->len] |= (ch << 4) ^ (ch & 0xf0);
                        rf_buf->len++;       /* 帧长度递增 */
                        rf_buf->state = 0;   /* 切换到等待低 4 位状态 */
                    }
                }
            }

            /* 若有完整的帧在队列中，立即返回 */
            if (rf_head)
                return FRAME_RECEIVED;
        }

        /* ====== 阶段 3：检查套接字状态 ====== */
        tm.tv_sec = tm.tv_usec = 0;
        FD_ZERO(&rfd);
        FD_ZERO(&wfd);
        FD_SET(sock, &rfd);
        FD_SET(sock, &wfd);

        nfds = (int)(sock + 1);
        if (select(nfds, &rfd, &wfd, 0, &tm) < 0)
            ABORT("system select()");

        /* 套接字可写：发送队列中的数据 */
        if (FD_ISSET(sock, &wfd))
            socket_send();

        /* 套接字可读：接收新数据 */
        if (FD_ISSET(sock, &rfd))
            socket_recv();

        /* ====== 阶段 4：检查网络层事件 ====== */
        if (network_layer_ready()) {
            layer3_ready = 1;
            return NETWORK_LAYER_READY;
        }

        /* ====== 阶段 5：检查定时器超时 ====== */
        if ((event = scan_timer(arg)) != 0)
            return event;

        /* ====== 阶段 6：检查物理层就绪 ====== */
        if (inform_phl_ready && phl_sq_len() < PHL_SQ_LEVEL) {
            inform_phl_ready = 0;
            return PHYSICAL_LAYER_READY;
        }

        /* ====== 阶段 7：休眠一个 tick ====== */
        if (1) {
            /* 正常休眠模式：等待 mode_tick 毫秒 */
            int ms0, t;
            static time_t last_warn;
            ms0 = get_ms();
            magic_check();        /* 内存越界检查 */
            Sleep(mode_tick);     /* 休眠 */
            t = get_ms() - ms0;   /* 实际休眠时间 */
            /* 若实际休眠时间远超预期，说明系统负载过高 */
            if (t > mode_tick + 50 && time(0) > last_warn + 1) {
                lprintf("** WARNING: System too busy, sleep %d ms, but be awakened %d ms later\n",
                    mode_tick, t);
                last_warn = time(0);
            }
        } else {
            /* 性能监控模式（默认不执行）：追踪休眠偏差 */
            int ticks, ms;

            sleep_cnt++;

            ms = get_ms();
            if (start_ms == 0)
                start_ms = ms;
            else if (ms - wakeup_ms > 1) {
                ticks = (ms - start_ms) / mode_tick;
                lprintf("====== CPU BUSY for %d ms (cnt %d)\n", ms - wakeup_ms, ++busy_cnt);
                lprintf("------ noSleep %d, sleep %d, Elapse %d ticks\n", ticks - sleep_cnt, sleep_cnt, ticks);
            }

            magic_check();
            ms = get_ms();
            Sleep(mode_tick);
            wakeup_ms = get_ms();

            ms = wakeup_ms - ms;
            if (ms > mode_tick + 1 || ms < mode_tick - 1)
                lprintf("++++++ Sleep(%d)=%d+%d (cnt %d)\n", mode_tick, mode_tick, ms - mode_tick, ++bias_cnt);
        }

        /* ====== 阶段 8：检查生命周期 ====== */
        if (now > mode_life) {
            lprintf("Quit.\n");
            exit(0);
        }
    }
}

/* ==================================================================
 *                    内存越界保护
 * ================================================================== */

/*
 * 栈帧底部魔数数组，用于检测内存越界写。
 */
static unsigned int foot_magic[NMAGIC];

/*
 * magic_init()：初始化内存保护魔数
 *
 * 在函数的栈帧两端各放置 32 个魔数值：
 *   - head_magic[] 在栈帧顶部（函数入口处）
 *   - foot_magic[] 在栈帧底部
 *
 * 如果程序发生缓冲区溢出等内存越界操作，这些魔数值可能被改写，
 * magic_check() 会检测到并报错。
 */
static void magic_init(void)
{
    int i;
    for (i = 0; i < NMAGIC; i++) {
        head_magic[i] = HEAD_MAGIC;
        foot_magic[i] = FOOT_MAGIC;;
    }
}

/*
 * magic_check()：检查内存保护魔数是否完好
 *
 * 逐一检查 head_magic[] 和 foot_magic[] 数组：
 *   - 若任何元素被修改，说明发生了内存越界
 *   - 打印错误信息并退出程序
 *
 * 这个检查在每轮主循环的 Sleep 前后调用，起到近似连续监控的效果。
 */
static void magic_check(void)
{
    int i;

    for (i = 0; i < NMAGIC; i++) {
        if (head_magic[i] != HEAD_MAGIC)
            goto exit;
    }

    for (i = 0; i < NMAGIC; i++) {
        if (foot_magic[i] != FOOT_MAGIC)
            goto exit;
    }
    return;

exit:
    ABORT("Memory used by 'protocol.lib' is corrupted by your program");

}
