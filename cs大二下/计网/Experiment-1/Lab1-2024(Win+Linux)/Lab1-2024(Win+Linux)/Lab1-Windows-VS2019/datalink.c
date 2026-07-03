/*
 * datalink.c — 数据链路层：选择重传协议（Selective Repeat）实现
 *
 * 本模块实现了数据链路层的选择重传 ARQ 协议，负责在不可靠的物理链路上
 * 提供可靠的数据传输服务。
 *
 * ======================== 选择重传协议概述 ========================
 *
 * 选择重传（Selective Repeat）是滑动窗口协议的一种，与回退 N 帧（Go-Back-N）
 * 不同，选择重传只重传丢失或错误的帧，而不重传之后已正确接收的帧。
 *
 * 核心思想：
 *   1. 发送方维护一个发送窗口，可以连续发送多个帧而不必等待确认
 *   2. 接收方对每个正确收到的帧单独确认（或通过 NAK 请求重传）
 *   3. 接收方缓存乱序到达的帧，按序交付给网络层
 *   4. 发送方只重传超时或收到 NAK 的帧
 *
 * 本实现还支持捎带确认（Piggybacking）：
 *   - ACK 信息可附加在 DATA 帧中，减少单独 ACK 帧的数量
 *   - 序号空间为 0~MAX_SEQ（15），窗口大小为 (MAX_SEQ+1)/2 = 8
 *
 * ======================== 关键常量说明 ========================
 *
 * MAX_SEQ = 15          : 最大序号（序号范围 0~15，共 16 个序号）
 * NR_BUFS = 8           : 缓冲区数量 = 窗口大小 = (MAX_SEQ+1)/2
 *                         选择重传要求窗口大小 ≤ 序号空间的一半，防止序号混淆
 * DATA_TIMER = 2300ms   : 数据帧超时重传时间
 * ACK_TIMER = 300ms     : ACK 帧发送延迟（累积确认的等待时间）
 * NAK_REPEAT_INTERVAL   : NAK 重复发送间隔（800ms），用于确保发送方收到 NAK
 * FRAME_HEAD_LEN = 3    : 帧头长度（kind + seq + ack，共 3 字节）
 * FRAME_CRC_LEN = 4     : CRC 校验码长度（4 字节，CRC-32）
 * FRAME_DATA_LEN        : 数据帧长度 = 帧头 + 数据包
 * FRAME_MAX_LEN         : 最大帧长 = 帧头 + 数据包 + CRC
 * PHL_QUEUE_LIMIT       : 物理层发送队列限制，用于流量控制
 */

#include <stdio.h>
#include <string.h>

#include "protocol.h"
#include "datalink.h"

/* ======================== 协议常量定义 ======================== */

#define MAX_SEQ 15                          /* 最大帧序号（0~15，共 16 个） */
#define NR_BUFS ((MAX_SEQ + 1) / 2)         /* 窗口 / 缓冲区数 = 8 */
#define GBN_NR_BUFS NR_BUFS                 /* GBN 对照使用同样的窗口大小 */
#define OUT_BUFS (MAX_SEQ + 1)              /* 发送缓存覆盖完整序号空间 */
#define DATA_TIMER 2300                     /* 数据帧超时重传时间（毫秒） */
#define GBN_DATA_TIMER 1800                 /* GBN 整窗回退的重传等待时间 */
#define ACK_TIMER 300                       /* ACK 累积确认等待时间（毫秒） */
#define NAK_REPEAT_INTERVAL 800             /* NAK 重发间隔（毫秒），0 表示不重发 */
#define FRAME_HEAD_LEN 3                    /* 帧头长度：kind(1) + seq(1) + ack(1) */
#define FRAME_CRC_LEN 4                     /* CRC-32 校验字段长度 */
#define FRAME_DATA_LEN (FRAME_HEAD_LEN + PKT_LEN)   /* 含数据的帧长度 */
#define FRAME_MAX_LEN (FRAME_DATA_LEN + FRAME_CRC_LEN) /* 帧最大长度（含 CRC） */
#define PHL_QUEUE_LIMIT (FRAME_DATA_LEN * 2)        /* 物理层队列上限，触发流控 */

/*
 * struct FRAME：帧结构体
 *
 * 帧格式（在 send_data_frame 中序列化为字节流）：
 *   +------+------+------+-----------+-----------+
 *   | kind | seq  | ack  |  data[]   |   CRC32   |
 *   +------+------+------+-----------+-----------+
 *    kind: 帧类型（FRAME_DATA / FRAME_ACK / FRAME_NAK）
 *    seq:  发送帧序号（仅数据帧有效）
 *    ack:  捎带确认号（期望收到的下一帧序号，即累积确认）
 *    data: 数据载荷（仅数据帧携带）
 *    CRC:  CRC-32 校验值（4 字节，小端序）
 *
 * padding 字段用于保证结构体对齐，不参与实际传输。
 */
struct FRAME {
    unsigned char kind;                  /* 帧类型：DATA / ACK / NAK */
    unsigned char seq;                   /* 本帧序号（发送方序号） */
    unsigned char ack;                   /* 捎带确认号（期望接收的下一帧） */
    unsigned char data[PKT_LEN];         /* 数据载荷 */
    unsigned int padding;                /* 对齐填充（不传输） */
};

/* ======================== 协议全局状态变量 ======================== */

/*
 * 发送方状态变量：
 *
 *   ack_expected       : 发送窗口下界（最早未确认帧的序号）
 *                        收到 ACK 后向前滑动
 *   next_frame_to_send : 发送窗口上界（下一个可发送帧的序号）
 *                        窗口大小 = (next_frame_to_send - ack_expected) mod 16 ≤ NR_BUFS
 *   nbuffered          : 当前已缓冲但未确认的帧数 = 窗口中的帧数
 */
static unsigned char ack_expected = 0;       /* 最早未确认帧序号（窗口下界） */
static unsigned char next_frame_to_send = 0; /* 下一个待发送帧序号（窗口上界） */
static unsigned char frame_expected = 0;     /* 接收方期望的下一个按序帧序号 */
static unsigned char too_far = NR_BUFS;      /* 接收窗口上界 = (frame_expected + NR_BUFS) mod 16 */
static unsigned char nbuffered = 0;          /* 当前窗口中未确认的帧数 */
static unsigned char no_nak = 1;             /* 是否尚未发送 NAK（避免重复发送 NAK） */

/*
 * 发送方和接收方缓冲区：
 *
 *   out_buf[NR_BUFS][PKT_LEN] : 发送缓冲区，保存窗口中已发送但未确认的数据包
 *                               索引 = 帧序号 % NR_BUFS
 *   in_buf[NR_BUFS][PKT_LEN]  : 接收缓冲区，保存乱序到达的数据包
 *                               索引 = 帧序号 % NR_BUFS
 *   arrived[NR_BUFS]           : 接收标记数组，arrived[i]=1 表示序号为 i 的帧已正确接收
 */
static unsigned char out_buf[OUT_BUFS][PKT_LEN];
static unsigned char in_buf[NR_BUFS][PKT_LEN];
static unsigned char arrived[NR_BUFS];

enum PROTOCOL_MODE {
    PROTOCOL_SR,
    PROTOCOL_GBN
};

static enum PROTOCOL_MODE protocol_mode = PROTOCOL_SR;

static unsigned char send_window_size(void)
{
    return protocol_mode == PROTOCOL_GBN ? GBN_NR_BUFS : NR_BUFS;
}

/*
 * inc(k)：序号递增宏（模 MAX_SEQ+1，即 0~15 循环）
 *
 * 将序号 k 加 1，超过 MAX_SEQ 时回绕到 0。
 * 因为序号空间为 0~MAX_SEQ（16 个值），所以模 16 循环。
 */
#define inc(k)                 \
    do {                       \
        if ((k) < MAX_SEQ)     \
            (k)++;             \
        else                   \
            (k) = 0;           \
    } while (0)

/*
 * between(a, b, c)：判断序号 b 是否在区间 [a, c) 内（模 MAX_SEQ+1）
 *
 * 在滑动窗口协议中，序号是循环使用的（模 16），因此不能简单地用
 * a <= b < c 判断。本函数考虑了序号回绕的情况。
 *
 * 三种合法情况（在圆周上，a 到 c 的顺时针区间包含 b）：
 *   1. a <= b < c              正常递增（无回绕）
 *   2. c < a <= b              区间跨越 MAX_SEQ 回绕点，b 在回绕点之后
 *   3. b < c < a               区间跨越回绕点，b 在回绕点之前
 *
 * 参数：
 *   a：区间起点（含）
 *   b：待判断的序号
 *   c：区间终点（不含）
 *
 * 返回值：
 *   1 表示 b 在 [a, c) 内，0 表示不在
 *
 * 典型用法：
 *   - between(ack_expected, seq, next_frame_to_send)：判断 seq 是否在发送窗口内
 *   - between(frame_expected, seq, too_far)：判断 seq 是否在接收窗口内
 */
static int between(unsigned char a, unsigned char b, unsigned char c)
{
    return ((a <= b) && (b < c)) || ((c < a) && (a <= b)) ||
           ((b < c) && (c < a));
}

/*
 * update_network_layer()：根据缓冲区状态更新网络层就绪状态
 *
 * 当以下条件同时满足时，通知网络层可以继续提供数据包：
 *   1. 当前缓冲帧数 < 窗口大小（有空闲缓冲区）
 *   2. 物理层发送队列 < 上限（不会积压过多数据）
 *
 * 否则禁用网络层，实现背压（backpressure）流量控制。
 */
static void update_network_layer(void)
{
    if (nbuffered < send_window_size() && phl_sq_len() < PHL_QUEUE_LIMIT)
        enable_network_layer();
    else
        disable_network_layer();
}

static void strip_datalink_options(int *argc, char **argv)
{
    int i, j;

    for (i = 1; i < *argc;) {
        if (strcmp(argv[i], "--gbn") == 0) {
            protocol_mode = PROTOCOL_GBN;
        } else if (strcmp(argv[i], "--sr") == 0) {
            protocol_mode = PROTOCOL_SR;
        } else if (strcmp(argv[i], "--protocol=gbn") == 0) {
            protocol_mode = PROTOCOL_GBN;
        } else if (strcmp(argv[i], "--protocol=sr") == 0) {
            protocol_mode = PROTOCOL_SR;
        } else {
            i++;
            continue;
        }

        for (j = i; j < *argc - 1; j++)
            argv[j] = argv[j + 1];
        (*argc)--;
    }
}

/* 前向声明 send_data_frame（因 send_nak 调用它，而 send_nak 定义在前） */
static void send_data_frame(unsigned char kind, unsigned char frame_nr,
                unsigned char frame_expected,
                unsigned char buffer[]);

/*
 * send_nak(frame_expected)：发送 NAK（否定确认）帧
 *
 * 当接收方检测到帧丢失或错误时，发送 NAK 通知发送方重传特定帧。
 * NAK 帧中 ack 字段指示期望接收的帧序号（即丢失的帧）。
 *
 * 策略：
 *   - 发送 NAK 后设置 no_nak = 0，避免对同一序号重复发送 NAK
 *   - 如果 NAK_REPEAT_INTERVAL > 0，启动 NAK 重发定时器
 *     （因为 NAK 本身也可能丢失，需要定时重发）
 *
 * 参数：
 *   frame_expected：期望接收的帧序号（丢失帧的序号）
 */
static void send_nak(unsigned char frame_expected)
{
    send_data_frame(FRAME_NAK, 0, frame_expected, NULL);
    no_nak = 0;
    if (NAK_REPEAT_INTERVAL > 0)
        start_ack_timer(NAK_REPEAT_INTERVAL);
    else
        stop_ack_timer();
}

/*
 * send_data_frame()：构造并发送一个数据链路层帧
 *
 * 这是核心的帧发送函数，负责将协议数据封装为帧并通过物理层发出。
 * 支持三种帧类型：FRAME_DATA、FRAME_ACK、FRAME_NAK。
 *
 * 帧的字节布局（大端序存储）：
 *   frame[0] = kind          帧类型
 *   frame[1] = frame_nr      本帧序号
 *   frame[2] = ack_nr        捎带确认号
 *   frame[3..] = data        （仅 DATA 帧）数据载荷
 *   末尾 4 字节 = CRC32      小端序存储
 *
 * 捎带确认号的计算：
 *   ack_nr = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1)
 *   = frame_expected - 1 (模 16)，即接收方期望的上一帧
 *   实际上等于 "已成功收到的最后一帧的序号"
 *   对于 NAK 帧，ack_nr 直接设为 frame_expected（请求重传该帧）
 *
 * 参数：
 *   kind            : 帧类型（FRAME_DATA / FRAME_ACK / FRAME_NAK）
 *   frame_nr        : 本帧的发送序号（DATA 帧有效）
 *   frame_expected  : 接收方的期望序号（用于计算捎带确认号）
 *   buffer[]        : 数据载荷指针（仅 DATA 帧有效，可为 NULL）
 */
static void send_data_frame(unsigned char kind, unsigned char frame_nr,
                unsigned char frame_expected,
                unsigned char buffer[])
{
    unsigned char frame[FRAME_MAX_LEN];
    unsigned char ack_nr;
    unsigned int crc;
    int len;

    /* 清空帧缓冲区 */
    memset(frame, 0, sizeof frame);

    /*
     * 计算捎带确认号：
     * ack_nr = (frame_expected - 1) mod (MAX_SEQ+1)
     * 即接收方已正确收到的最后一帧的序号
     */
    ack_nr = (frame_expected + MAX_SEQ) % (MAX_SEQ + 1);
    if (kind == FRAME_NAK)
        ack_nr = frame_expected;  /* NAK 帧：确认号表示请求重传的帧 */

    /* 构造帧头：kind(1字节) + seq(1字节) + ack(1字节) */
    frame[0] = kind;
    frame[1] = frame_nr;
    frame[2] = ack_nr;

    if (kind == FRAME_DATA) {
        /* 数据帧：复制数据载荷到帧中 */
        memcpy(frame + FRAME_HEAD_LEN, buffer, PKT_LEN);
        len = FRAME_DATA_LEN;
        dbg_frame("Send DATA seq=%d ack=%d, ID %d\n",
              frame_nr, ack_nr, *(short *)buffer);
    } else {
        /* ACK/NAK 帧：无数据载荷，仅帧头 */
        len = FRAME_HEAD_LEN;
        if (kind == FRAME_ACK)
            dbg_frame("Send ACK  ack=%d\n", ack_nr);
        else
            dbg_frame("Send NAK  ack=%d\n", ack_nr);
    }

    /* 计算 CRC-32 校验值，覆盖帧头 + 数据 */
    crc = crc32(frame, len);

    /* 将 CRC 以小端序（低位在前）附加到帧尾 */
    frame[len] = (unsigned char)(crc & 0xff);           /* 低 8 位 */
    frame[len + 1] = (unsigned char)((crc >> 8) & 0xff); /* 次低 8 位 */
    frame[len + 2] = (unsigned char)((crc >> 16) & 0xff);/* 次高 8 位 */
    frame[len + 3] = (unsigned char)((crc >> 24) & 0xff);/* 高 8 位 */

    /* 通过物理层发送帧 */
    send_frame(frame, len + FRAME_CRC_LEN);

    /* 数据帧：启动重传定时器 */
    if (kind == FRAME_DATA)
        start_timer(frame_nr,
                protocol_mode == PROTOCOL_GBN ? GBN_DATA_TIMER :
                                DATA_TIMER);

    /*
     * 停止 ACK/NAK 定时器的情况：
     *   1. 发送了 ACK 帧（已完成确认）
     *   2. 发送了 DATA 帧且 no_nak 为真（数据帧捎带了 ACK，无需单独 NAK）
     */
    if (kind == FRAME_ACK || (kind == FRAME_DATA && no_nak))
        stop_ack_timer();
}

static void retransmit_gbn_window(void)
{
    unsigned char n;

    n = ack_expected;
    while (n != next_frame_to_send) {
        send_data_frame(FRAME_DATA, n, frame_expected,
                out_buf[n % OUT_BUFS]);
        inc(n);
    }
}

static void run_gbn(void)
{
    int event, arg;
    struct FRAME f;
    unsigned char frame[FRAME_MAX_LEN];
    int len = 0;

    next_frame_to_send = 0;
    ack_expected = 0;
    frame_expected = 0;
    too_far = NR_BUFS;
    nbuffered = 0;
    no_nak = 1;
    memset(out_buf, 0, sizeof out_buf);
    memset(in_buf, 0, sizeof in_buf);
    memset(arrived, 0, sizeof arrived);
    enable_network_layer();

    for (;;) {
        event = wait_for_event(&arg);

        switch (event) {
        case NETWORK_LAYER_READY:
            if (nbuffered < send_window_size()) {
                get_packet(out_buf[next_frame_to_send % OUT_BUFS]);
                nbuffered++;
                send_data_frame(FRAME_DATA, next_frame_to_send,
                        frame_expected,
                        out_buf[next_frame_to_send % OUT_BUFS]);
                inc(next_frame_to_send);
            }
            update_network_layer();
            break;

        case PHYSICAL_LAYER_READY:
            break;

        case FRAME_RECEIVED:
            len = recv_frame(frame, sizeof frame);
            if (len < FRAME_HEAD_LEN + FRAME_CRC_LEN ||
                crc32(frame, len) != 0) {
                dbg_event("**** Receiver Error, Bad CRC Checksum\n");
                start_ack_timer(0);
                break;
            }

            memset(&f, 0, sizeof f);
            f.kind = frame[0];
            f.seq = frame[1];
            f.ack = frame[2];
            if (f.kind == FRAME_DATA) {
                if (len < FRAME_DATA_LEN + FRAME_CRC_LEN)
                    break;
                memcpy(f.data, frame + FRAME_HEAD_LEN, PKT_LEN);
            }

            if (f.kind != FRAME_NAK) {
                while (between(ack_expected, f.ack,
                           next_frame_to_send)) {
                    nbuffered--;
                    stop_timer(ack_expected);
                    inc(ack_expected);
                }
            }

            if (f.kind == FRAME_ACK) {
                dbg_frame("Recv ACK  ack=%d\n", f.ack);
            } else if (f.kind == FRAME_DATA) {
                dbg_frame("Recv DATA seq=%d ack=%d, ID %d\n", f.seq,
                      f.ack, *(short *)f.data);
                if (f.seq == frame_expected) {
                    put_packet(f.data, PKT_LEN);
                    inc(frame_expected);
                }
                start_ack_timer(ACK_TIMER);
            }
            break;

        case DATA_TIMEOUT:
            dbg_event("---- DATA %d timeout, resend GBN window\n", arg);
            if ((unsigned char)arg == ack_expected && nbuffered > 0)
                retransmit_gbn_window();
            break;

        case ACK_TIMEOUT:
            send_data_frame(FRAME_ACK, 0, frame_expected, NULL);
            break;
        }

        update_network_layer();
    }
}

/*
 * main()：数据链路层协议主循环
 *
 * 本函数是选择重传协议的核心事件循环。它持续等待并处理六种事件：
 *
 *   1. NETWORK_LAYER_READY  : 网络层有数据包要发送
 *   2. PHYSICAL_LAYER_READY : 物理层发送队列有空位
 *   3. FRAME_RECEIVED       : 从物理层收到一个帧
 *   4. DATA_TIMEOUT         : 数据帧超时（需重传）
 *   5. ACK_TIMEOUT          : ACK/NAK 定时器超时
 *
 * 协议状态机通过以下全局变量维护：
 *   - 发送方：ack_expected, next_frame_to_send, nbuffered
 *   - 接收方：frame_expected, too_far, arrived[], no_nak
 *
 * 运行流程：
 *   初始化 → 事件循环（无限） → 处理事件 → 更新网络层状态 → 继续循环
 */
int main(int argc, char **argv)
{
    int event, arg;
    struct FRAME f;
    unsigned char frame[FRAME_MAX_LEN];
    int len = 0;
    unsigned char n;

    strip_datalink_options(&argc, argv);

    /* 初始化协议栈（物理层、网络层参数配置） */
    protocol_init(argc, argv);
    if (protocol_mode == PROTOCOL_GBN) {
        lprintf("Go-Back-N with Piggybacking, build: "
            __DATE__ "  " __TIME__ "\n");
        run_gbn();
    }

    lprintf("Selective Repeat with Piggybacking, build: "
        __DATE__ "  " __TIME__ "\n");

    /* 初始化发送方状态 */
    next_frame_to_send = 0;  /* 下一发送帧序号从 0 开始 */
    ack_expected = 0;        /* 最早未确认帧为 0 */
    /* 初始化接收方状态 */
    frame_expected = 0;      /* 期望接收帧序号从 0 开始 */
    too_far = NR_BUFS;       /* 接收窗口上界 = frame_expected + 窗口大小 */
    nbuffered = 0;           /* 当前无缓冲帧 */
    no_nak = 1;              /* 尚未发送 NAK */
    /* 清空所有缓冲区 */
    memset(out_buf, 0, sizeof out_buf);
    memset(in_buf, 0, sizeof in_buf);
    memset(arrived, 0, sizeof arrived);
    /* 通知网络层可以开始发送数据 */
    enable_network_layer();

    /* ======================== 主事件循环 ======================== */
    for (;;) {
        event = wait_for_event(&arg);  /* 等待下一个事件发生 */

        switch (event) {

        /*
         * NETWORK_LAYER_READY：网络层准备好了一个新数据包
         *
         * 处理流程：
         *   1. 检查发送窗口是否有空位（nbuffered < NR_BUFS）
         *   2. 从网络层获取数据包，存入 out_buf
         *   3. 构造 DATA 帧并发送
         *   4. 递增 next_frame_to_send（窗口上界前移）
         *
         * 如果窗口已满，数据包不会被获取（update_network_layer 会禁用网络层）。
         */
        case NETWORK_LAYER_READY:
            if (nbuffered < send_window_size()) {
                /* 将数据包存入发送缓冲区的对应槽位 */
                get_packet(out_buf[next_frame_to_send % OUT_BUFS]);
                nbuffered++;
                /* 构造并发送 DATA 帧（捎带当前接收方的期望序号） */
                send_data_frame(FRAME_DATA, next_frame_to_send,
                        frame_expected,
                        out_buf[next_frame_to_send %
                            OUT_BUFS]);
                inc(next_frame_to_send);  /* 窗口上界前移 */
            }
            update_network_layer();
            break;

        /*
         * PHYSICAL_LAYER_READY：物理层发送队列有空位
         *
         * 当前实现中不需要特殊处理，因为 send_frame 内部会自动排队。
         * 此事件仅用于唤醒主循环，实际发送在 send_data_frame 中完成。
         */
        case PHYSICAL_LAYER_READY:
            break;

        /*
         * FRAME_RECEIVED：从物理层收到一个完整的帧
         *
         * 处理流程分为以下几个阶段：
         *
         * 第一阶段：帧校验
         *   1. 检查帧长度是否合法（至少 帧头 + CRC）
         *   2. 使用 CRC-32 检查帧是否出错
         *   3. 若出错且尚未发送过 NAK，立即发送 NAK 请求重传
         *
         * 第二阶段：解析帧头，提取 kind、seq、ack 字段
         *
         * 第三阶段：处理捎带确认（非 NAK 帧）
         *   滑动发送窗口：从 ack_expected 到接收到的 ack 之间所有帧被确认
         *   每确认一帧：nbuffered--，停止对应定时器，ack_expected++
         *
         * 第四阶段：根据帧类型分别处理
         *   - ACK 帧：仅记录日志（确认已在第三阶段处理）
         *   - NAK 帧：重传指定序号的 DATA 帧
         *   - DATA 帧：交给接收逻辑处理（第五阶段）
         *
         * 第五阶段：接收数据处理（仅 DATA 帧）
         *   1. 检查序号是否在接收窗口内 [frame_expected, too_far)
         *   2. 检查是否已接收过（避免重复）
         *   3. 标记已接收，缓存数据
         *   4. 若为乱序帧且尚未发送 NAK，发送 NAK
         *   5. 按序交付：从 frame_expected 开始，连续已到达的帧交付网络层
         *   6. 启动 ACK 定时器（累积确认延迟）
         */
        case FRAME_RECEIVED:
            /* --- 第一阶段：接收并校验帧 --- */
            len = recv_frame(frame, sizeof frame);
            if (len < FRAME_HEAD_LEN + FRAME_CRC_LEN ||
                crc32(frame, len) != 0) {
                dbg_event("**** Receiver Error, Bad CRC Checksum\n");
                /* 帧错误，若未发过 NAK 则发送 NAK */
                if (no_nak)
                    send_nak(frame_expected);
                break;
            }

            /* --- 第二阶段：解析帧头 --- */
            memset(&f, 0, sizeof f);
            f.kind = frame[0];  /* 帧类型 */
            f.seq = frame[1];   /* 发送序号 */
            f.ack = frame[2];   /* 捎带确认号 */
            if (f.kind == FRAME_DATA) {
                if (len < FRAME_DATA_LEN + FRAME_CRC_LEN)
                    break;  /* 数据帧长度不足，丢弃 */
                memcpy(f.data, frame + FRAME_HEAD_LEN, PKT_LEN);
            }

            /* --- 第三阶段：处理捎带确认，滑动发送窗口 --- */
            /*
             * 对于 ACK 和 DATA 帧（非 NAK），ack 字段表示对方已正确收到的
             * 所有序号。从 ack_expected 到 f.ack 之间的帧都视为已确认。
             * （注意：between 的语义是 ack_expected <= ... < f.ack）
             * 实际上，while 循环通过 inc(ack_expected) 逐个确认，
             * 直到 ack_expected 追上 f.ack。
             */
            if (f.kind != FRAME_NAK) {
                /*
                 * 循环确认：ack_expected 逐个追赶 f.ack
                 * 每次确认一帧：减少缓冲计数，停止该帧的定时器
                 */
                while (between(ack_expected, f.ack,
                           next_frame_to_send)) {
                    nbuffered--;
                    stop_timer(ack_expected);
                    inc(ack_expected);
                }
            }

            /* --- 第四阶段：按帧类型处理 --- */
            if (f.kind == FRAME_ACK) {
                /* 收到纯 ACK 帧（确认已在第三阶段处理） */
                dbg_frame("Recv ACK  ack=%d\n", f.ack);
            } else if (f.kind == FRAME_NAK) {
                /*
                 * 收到 NAK 帧：对方请求重传指定帧
                 * f.ack 即为需要重传的帧序号（接收方期望但未收到的帧）
                 */
                dbg_frame("Recv NAK  ack=%d\n", f.ack);
                n = f.ack;
                /* 仅在帧仍在发送窗口中时重传（可能已被 ACK 确认） */
                if (between(ack_expected, n, next_frame_to_send))
                    send_data_frame(FRAME_DATA, n,
                            frame_expected,
                            out_buf[n % OUT_BUFS]);
            }

            /* --- 第五阶段：接收数据处理（仅 DATA 帧） --- */
            if (f.kind == FRAME_DATA) {
                dbg_frame("Recv DATA seq=%d ack=%d, ID %d\n", f.seq,
                      f.ack, *(short *)f.data);

                /*
                 * 检查帧序号是否在接收窗口内，且未被接收过：
                 *   between(frame_expected, f.seq, too_far)
                 *     判断 f.seq 是否在 [frame_expected, too_far) 区间
                 *   !arrived[f.seq % NR_BUFS]
                 *     确保该帧之前未收到（防止重复帧导致数据错误）
                 */
                if (between(frame_expected, f.seq, too_far) &&
                    !arrived[f.seq % NR_BUFS]) {
                    /* 标记已接收并缓存数据 */
                    arrived[f.seq % NR_BUFS] = 1;
                    memcpy(in_buf[f.seq % NR_BUFS], f.data, PKT_LEN);

                    /*
                     * 若收到的帧不是期望的按序帧，且尚未发送 NAK：
                     * 发送 NAK 请求重传缺失的帧（frame_expected 即为缺失帧）
                     * 这告知发送方："我收到了乱序帧，但我还缺 frame_expected"
                     */
                    if (f.seq != frame_expected && no_nak)
                        send_nak(frame_expected);

                    /*
                     * 按序交付：
                     * 检查从 frame_expected 开始，有多少连续的帧已到达，
                     * 将它们依次交付给网络层，然后滑动接收窗口。
                     *
                     * 例如：收到 0,1,3（3 乱序），当 2 到达后，
                     *       循环会一次性交付 0,1,2,3，窗口跳到 4。
                     */
                    while (arrived[frame_expected % NR_BUFS]) {
                        put_packet(in_buf[frame_expected % NR_BUFS],
                               PKT_LEN);
                        arrived[frame_expected % NR_BUFS] = 0;
                        inc(frame_expected);  /* 接收窗口下界前移 */
                        inc(too_far);         /* 接收窗口上界同步前移 */
                        no_nak = 1;           /* 已按序接收，清除 NAK 标记 */
                    }
                }

                /*
                 * 启动 ACK 定时器：
                 * 条件：no_nak 为真（无待处理的 NAK），或 NAK 重复间隔为 0
                 * ACK 定时器用于实现累积确认的延迟发送，
                 * 定时器到期时会发送 ACK 帧（或重发 NAK）。
                 */
                if (no_nak || NAK_REPEAT_INTERVAL == 0)
                    start_ack_timer(ACK_TIMER);
            }
            break;

        /*
         * DATA_TIMEOUT：数据帧重传定时器超时
         *
         * arg 为超时的帧序号。如果该帧仍在发送窗口中（未被 ACK 确认），
         * 则重新发送该帧。这是选择重传的核心——只重传超时的帧，
         * 不影响窗口中其他帧。
         */
        case DATA_TIMEOUT:
            dbg_event("---- DATA %d timeout\n", arg);
            if (between(ack_expected, (unsigned char)arg,
                    next_frame_to_send))
                send_data_frame(FRAME_DATA, (unsigned char)arg,
                        frame_expected,
                        out_buf[((unsigned char)arg) %
                            OUT_BUFS]);
            break;

        /*
         * ACK_TIMEOUT：ACK/NAK 定时器超时
         *
         * 两种情况：
         *   1. no_nak == 0 且 NAK_REPEAT_INTERVAL > 0：
         *      重发 NAK（之前的 NAK 可能丢失了）
         *   2. 其他情况：
         *      发送 ACK 帧（累积确认延迟到期，通知发送方当前接收进度）
         */
        case ACK_TIMEOUT:
            if (!no_nak && NAK_REPEAT_INTERVAL > 0)
                send_nak(frame_expected);
            else
                send_data_frame(FRAME_ACK, 0, frame_expected, NULL);
            break;
        }

        /* 每轮事件处理后更新网络层状态（可能需要流量控制） */
        update_network_layer();
    }
}
