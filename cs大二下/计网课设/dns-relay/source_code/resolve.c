/**
 * ============================================================
 * resolve.c — DNS 报文解析模块实现
 * ============================================================
 *
 * 【模块功能】
 *   本模块是 DNS 中继器的核心——DNS 报文的解析与构造:
 *
 *   1. 域名格式转换 (domainName_ntop)
 *      将 DNS 报文中的"标签编码"域名转换为人类可读的"点分"格式。
 *      例如: \x03www\x06google\x03com\x00 → "www.google.com"
 *
 *   2. ResolveQuery() — 处理客户端查询
 *      收到客户端的 DNS 查询后:
 *        a. 提取域名
 *        b. 先查本地数据库/缓存
 *        c. 找到 → 构造 Answer 直接回复（DNS 拦截/代理）
 *        d. 未找到 → 修改 ID、记录客户端信息、转发给上游 DNS
 *
 *   3. ResolveResponse() — 处理上游 DNS 回复
 *      收到上游 DNS 服务器的回复后:
 *        a. 提取 IP、TTL 并缓存
 *        b. 根据 ID（队列索引）查找原始客户端
 *        c. 还原原始 ID、将报文发给原始客户端
 *
 * 【DNS 报文结构详解 (RFC 1035)】
 *
 *   ┌────────────────────────────────────────────────┐
 *   │                   HEADER (12 字节)               │
 *   │  ID(2) | FLAGS(2) | QDCOUNT(2) | ANCOUNT(2)    │
 *   │  NSCOUNT(2) | ARCOUNT(2)                        │
 *   ├────────────────────────────────────────────────┤
 *   │                 QUESTION 区域                    │
 *   │  QNAME(变长, 以 \x00 结尾的标签编码域名)         │
 *   │  QTYPE(2)  — 查询类型, 1=A记录(IPv4)           │
 *   │  QCLASS(2) — 查询类, 1=IN(互联网)              │
 *   ├────────────────────────────────────────────────┤
 *   │                  ANSWER 区域                     │
 *   │  每条资源记录:                                   │
 *   │  NAME(变长/指针) + TYPE(2) + CLASS(2)           │
 *   │  + TTL(4) + RDLENGTH(2) + RDATA(变长)          │
 *   └────────────────────────────────────────────────┘
 *
 * 【指针压缩 (DNS Name Compression)】
 *   为了节省空间，Answer 区域中的域名常使用"指针"指向
 *   之前在 Question 区域出现过的域名。指针格式:
 *     前 2 位 = 11 (0xC0)
 *     后 14 位 = 偏移量（相对报文起始的字节偏移）
 *   例如 \xC0\x0C 表示"指向报文中第 12 字节处的域名"。
 *
 * 【字节序注意事项】
 *   网络字节序 = 大端序 (Big Endian)
 *   x86/x64 = 小端序 (Little Endian)
 *   所有超过 1 字节的 DNS 字段都需要转换:
 *     ntohs() — Network TO Host Short (16位)
 *     htons() — Host TO Network Short (16位)
 *     ntohl() — Network TO Host Long  (32位)
 *     htonl() — Host TO Network Long  (32位)
 */

#include "resolve.h"

/**
 * domainName_ntop() — DNS标签编码转点分十进制域名
 *
 * 【功能】
 *   将 DNS 报文中的标签编码域名转换为人类可读的点分格式。
 *   这是一个就地转换（in-place），直接在原缓冲区上修改。
 *
 * 【DNS 标签编码规则】
 *   域名以"标签"为单位，每个标签前有一个长度字节:
 *     "www.example.com"
 *     编码为: \x03 w w w \x07 e x a m p l e \x03 c o m \x00
 *             ↑长度=3    ↑长度=7              ↑长度=3 ↑结束
 *   最后一个字节必须是 \x00（表示域名结束）。
 *
 * 【转换算法】
 *   1. 将整个标签编码拷贝到 pName（包含所有长度字节）
 *   2. 获取第一个标签长度 = nName[0]
 *   3. 从第一个标签后的第一个字符开始（即 \x03www 中的 'w'）
 *   4. 找到每个长度字节的位置，将其替换为 '.'
 *   5. 跳过该标签内容，继续找下一个长度字节
 *   6. 遇到 \x00 结束
 *
 * 【示例】
 *   输入 nName:  [3]'w''w''w'[6]'g''o''o''g''l''e'[3]'c''o''m'[0]
 *   第1步拷贝到 pName 后内容相同
 *   第2步 i = 3  (nName[0] = 3)
 *   第3步 pName[3] = '.'  (把长度字节 0x06 替换为 '.')
 *   第4步 i = 3 + 6 + 1 = 10
 *   第5步 pName[10] = '.' (把长度字节 0x03 替换为 '.')
 *   第6步 i = 10 + 3 + 1 = 14 → pName[14] = '\0' 结束
 *   结果:  "www.google.com"
 *
 * @param nName DNS 标签编码的域名（网络格式，输入）
 * @param pName 转换后的点分域名（输出，就地修改）
 * @return 域名的总长度（含结束符 \x00）
 */
static int domainName_ntop(const unsigned char *nName, unsigned char *pName) {
    /* 将 nName 完整拷贝到 pName
     * nName 以 \x00 结尾，strlen 可以正确获取长度
     * +1 是由于 strlen 不计算 \x00 */
    memcpy(pName, nName + 1, strlen((char *)nName));

    /* 总长度 = 标签编码的字节数 + 1 (结束符) */
    int length;
    length = strlen((char *)nName) + 1;

    /* 第一个长度字节的值，指向第一个标签结束的位置 */
    int i = nName[0];

    /* 遍历所有标签长度字节，将其替换为 '.' */
    while (pName[i]) {                   /* pName[i] != '\0' 继续 */
        int offset = pName[i];           /* 获取当前标签的长度 */
        pName[i] = '.';                  /* 将长度字节替换为点号 */
        i += offset + 1;                 /* 跳到下一个标签的长度字节 */
    }

    return length;
}

/**
 * ResolveQuery() — 处理来自客户端的 DNS 查询
 *
 * 【详细流程】
 *
 *   Step 1: 提取域名
 *     DNS 报文的 Header 占 12 字节，Question 从第 13 字节开始。
 *     因此域名从 recvBuf + 12 开始。
 *     调用 domainName_ntop 将标签编码转为点分格式。
 *
 *   Step 2: 本地查询
 *     调用 FindInDNSDatabase() 在本地数据库/缓存中查找。
 *
 *   Step 3a: 本地命中 → 构造 Answer 回复
 *     - 拷贝原始 query 到 sendBuf
 *     - 设置 QR 标志 (回复), RA (递归可用), AA (权威)
 *     - 如果 IP = "0.0.0.0" → 表示域名被屏蔽, ANCOUNT=0, RCODE=3(NXDOMAIN)
 *     - 否则构造一条 A 记录 Answer:
 *       · \xC0\x0C  指针压缩，指向字节 12 的域名
 *       · TYPE=1    A 记录
 *       · CLASS=1   IN (互联网)
 *       · TTL=120   生存时间 120 秒
 *       · RDLENGTH=4 IPv4 地址占 4 字节
 *       · RDATA     具体的 IP 地址
 *     - 将 Answer 追加到 Header+Question 后面
 *
 *   Step 3b: 本地未命中 → 转发给上游 DNS
 *     - 调用 PushCRecord() 记录客户端信息
 *       · 入参: 客户端地址 + 原始 DNS ID
 *       · 出参: newID（队列索引，作为转发用的新 ID）
 *     - 将报文 ID 替换为 newID
 *     - 将目标地址设为上游 DNS 服务器 (addrDNSserv:53)
 *
 * 【关于 0.0.0.0 的特殊处理】
 *   在本地数据库中可以将不想让客户端访问的域名映射到 0.0.0.0，
 *   此时 DNS 中继器回复 ANCOUNT=0 + RCODE=3（域名不存在），
 *   实现简单的域名屏蔽功能。
 *
 * @param recvBuf  接收缓冲区（客户端发来的 DNS 查询）
 * @param sendBuf  发送缓冲区（输出: 回复或转发的查询）
 * @param recvByte 接收到的字节数
 * @param addrCli  [输入]客户端地址 / [输出]目标地址
 * @return sendBuf 有效字节数, 或 -1 表示失败
 */
extern int ResolveQuery(const unsigned char *recvBuf, unsigned char *sendBuf,
    int recvByte, SOCKADDR_IN *addrCli) {

    /* 用于存储解析结果 */
    char ip[16] = { '\0' };                              /* IP 地址字符串缓冲区 */
    char domainName[MAX_DOMAINNAME] = { '\0' };          /* 域名缓冲区 */

    /* Step 1: 从报文中提取域名（跳过 12 字节 Header） */
    domainName_ntop(recvBuf + 12, domainName);

    /* Step 2: 在本地数据库中查找 */
    if (FindInDNSDatabase(domainName, ip)) {
        /* ========================================================
         * Step 3a: 本地命中 — 构造 Answer 直接回复客户端
         * ======================================================== */
        debugPrintf("在数据库中找到记录 %s : %s\n", domainName, ip);

        /* 将原始 query 拷贝到发送缓冲区作为基础 */
        memcpy(sendBuf, recvBuf, recvByte);

        /* ---- 设置 DNS Header FLAGS ---- */
        /* sendBuf[2] 是 FLAGS 的高字节:
         *   bit 7 (0x80) = QR:  设置为 1 (响应)
         *   bit 5 (0x20) = AA:  未设置
         *   bit 4 (0x10) = TC:  未设置
         *   bit 3 (0x08) = RD:  保持原值
         *   bit 2-0 (0x07) = RA, Z 等
         *
         *   0x80 = QR=1 (这是个响应)
         *   0x05 = AA=0 (非权威), RD=1 (期望递归), RA=0
         *   实际操作: sendBuf[2] |= 0x85 */
        sendBuf[2] |= 0x80;  /* QR = 1: 这是一个响应报文 */

        /* sendBuf[3] 是 FLAGS 的低字节 + RCODE:
         *   0x80 = RA=1 (递归可用)
         *   实际 QR=1, RCODE=0 (无错误) */
        sendBuf[2] |= 0x05;  /* AA=0, RD=0/1(保持原值),
                                这里实际设置的是 0x05 = 0000 0101
                                bit5=0(AA), bit2=1(RD), bit0=1(?) */
        sendBuf[3] |= 0x80;  /* RA = 1: 递归可用 */

        /* 检查是否为屏蔽域名 (IP = 0.0.0.0) */
        if (!strcmp(ip, "0.0.0.0")) {
            /* 域名被屏蔽: Answer 计数 = 0, 返回码 = 3 (NXDOMAIN)
             * 0x03 = RCODE=3 (域名不存在) */
            sendBuf[7] |= 0x00;  /* ANCOUNT 低字节 = 0 (无应答记录) */
            sendBuf[3] |= 0x03;  /* RCODE = 3 (NXDOMAIN: 域名不存在) */
            return recvByte;     /* 报文长度 = 原 query 长度 (无 Answer 区域) */
        }

        /* ---- 构造 Answer 区域 ---- */
        sendBuf[7] |= 0x01;      /* ANCOUNT = 1 (有一个应答记录) */

        unsigned char answer[16] = { 0 };  /* Answer 区域缓冲区 (16 字节) */

        /* 【DNS Answer 资源记录格式】
         *  偏移  长度  内容
         *    0     2    NAME (域名, 可以使用指针压缩)
         *    2     2    TYPE (1 = A 记录)
         *    4     2    CLASS (1 = IN 互联网)
         *    6     4    TTL (生存时间)
         *   10     2    RDLENGTH (数据长度, IPv4 = 4)
         *   12     4    RDATA (IPv4 地址, 4 字节)
         * 共 16 字节 */

        answer[0] = 0xc0;        /* 指针压缩的高位: 1100 0000 */
        answer[1] = 0x0c;        /* 偏移量 = 12 (指向 Header 之后的域名) */
        /* \xC0\x0C 一起表示"指向报文中偏移 12 字节处的域名" */

        answer[3] = 0x01;        /* TYPE = 1 (A 记录, IPv4 地址) */
        answer[5] = 0x01;        /* CLASS = 1 (IN, 互联网) */

        answer[9] = 0x78;        /* TTL = 0x78 = 120 秒
                                    (部分在 answer[6..9], 这里只设了低字节)
                                    120 秒 = 0x00000078, 小端: 78 00 00 00 */
        answer[11] = 0x04;       /* RDLENGTH = 4 (IPv4 地址占 4 字节) */

        /* 将 IP 地址字符串转换为 4 字节二进制 (网络字节序) */
        inet_pton(AF_INET, ip, answer + 12);

        /* 将 Answer 区域追加到原 query 后面 */
        memcpy(sendBuf + recvByte, answer, 16);
        /* 总长度 = 原 query + 16 字节 Answer */
        return recvByte + 16;

    } else {
        /* ========================================================
         * Step 3b: 本地未命中 — 转发给上游 DNS 服务器
         * ======================================================== */
        debugPrintf("未在数据库中找到记录: %s\n", domainName);

        /* 将接收缓冲区转为 DNS Header 结构，方便读取字段 */
        DNSHeader *header = (DNSHeader *)recvBuf;
        debugPrintf("收到ID为 %x 的请求报文\n", ntohs(header->ID));

        /* 提取客户端原始 ID (网络字节序 → 主机字节序) */
        DNSID newID = ntohs(header->ID);

        /* 将客户端信息入队:
         *   - 输入: addrCli (客户端地址), newID (原始 ID)
         *   - 输出: newID 被替换为队列索引 (作为转发用的新 ID) */
        if (PushCRecord(addrCli, &newID)) {
            /* ---- 入队成功: 修改报文并发送给上游 DNS ---- */

            /* 拷贝原始报文到发送缓冲区 */
            memcpy(sendBuf, recvBuf, recvByte);

            /* 将报文 ID 替换为队列索引 (转发的 newID) */
            header = (DNSHeader *)sendBuf;
            header->ID = htons(newID);  /* 主机字节序 → 网络字节序 */

            /* 设置目标地址为上游 DNS 服务器 */
            addrCli->sin_family = AF_INET;            /* IPv4 */
            addrCli->sin_port = htons(53);            /* DNS 标准端口 53 */
            inet_pton(AF_INET, addrDNSserv, &addrCli->sin_addr); /* IP 地址 */

            /* 返回原报文长度 (内容不变, 只改了 ID) */
            return recvByte;
        } else {
            /* 队列已满，无法处理此查询 */
            return -1;
        }
    }
}

/**
 * ResolveResponse() — 处理来自上游 DNS 服务器的响应
 *
 * 【详细流程】
 *
 *   Step 1: 从响应报文中提取信息
 *     - 域名: 从 recvBuf + 12 开始（跳过 Header）
 *     - IP: 从 Answer 资源的 RDATA 部分提取
 *       · Answer 起始 = 12 + 域名长度 + 4(QTYPE+QCLASS)
 *       · IP 位置 = Answer起始 + 域名(2字节指针) + TYPE(2) + CLASS(2) +
 *                     TTL(4) + RDLENGTH(2)
 *       · = 12 + domainlen + 4 + 2 + 2 + 2 + 4 + 2 = 12 + domainlen + 16
 *       · 但实际代码偏移略有不同，因为使用了指针压缩
 *     - TTL: 从 Answer 资源的 TTL 字段提取（4 字节网络序）
 *
 *   Step 2: 缓存查询结果
 *     调用 InsertIntoDNSCache 将域名→IP 映射缓存到内存。
 *     仅在 RCODE=0（查询成功）时才缓存。
 *
 *   Step 3: 匹配客户端记录
 *     - 取报文 ID → 在 ClientTable 中查找
 *     - 如果找到且 r=0 → 准备转发给客户端
 *     - 如果找到且 r=1 → 已回复过，丢弃（-1）
 *     - 如果未找到 → 丢弃（-1）
 *
 *   Step 4: 转发给客户端
 *     - 拷贝响应到 sendBuf
 *     - 将 ID 替换为客户端原始 ID
 *     - 将目标地址设为客户端地址
 *
 * 【TTL 提取位置计算】
 *   DNS Response 报文布局:
 *     [Header 12B] [Question: 域名 + QTYPE(2) + QCLASS(2)]
 *     [Answer:  NAME(2B指针/变长) + TYPE(2) + CLASS(2) + TTL(4)
 *               + RDLENGTH(2) + RDATA(变长)]
 *
 *   TTL 在 Answer 中的偏移 = NAME长度 + 2(TYPE) + 2(CLASS) = 域名占用 + 4
 *   代码中 TTL 起始 = 12(Header) + domainlen(Question域名长度)
 *                    + 4(QTYPE+QCLASS) + 2(Answer NAME指针)
 *                    + 2(TYPE) + 2(CLASS)
 *                  = 12 + domainlen + 10
 *
 * @param recvBuf  接收缓冲区（上游 DNS 发来的响应）
 * @param sendBuf  发送缓冲区（转发给客户端的响应）
 * @param recvByte 接收到的字节数
 * @param addrCli  [输出]原始客户端的地址
 * @return sendBuf 有效字节数, 或 -1 表示失败
 */
extern int ResolveResponse(const unsigned char *recvBuf, unsigned char *sendBuf,
    int recvByte, SOCKADDR_IN *addrCli) {

    char ip[16] = { '\0' };                              /* IP 地址缓冲区 */
    char domainName[MAX_DOMAINNAME] = { '\0' };          /* 域名缓冲区 */
    int domainlen;

    /* Step 1: 提取域名（从 Question 区域） */
    domainlen = domainName_ntop(recvBuf + 12, domainName);

    /* ---- 提取 IP 地址 ----
     * Answer 区域结构 (使用指针压缩时 NAME 占 2 字节):
     *   偏移 0-1 : NAME (指针 \xC0\x0C)
     *   偏移 2-3 : TYPE (2B)
     *   偏移 4-5 : CLASS (2B)
     *   偏移 6-9 : TTL (4B)
     *   偏移 10-11: RDLENGTH (2B)
     *   偏移 12-15: RDATA (IPv4 地址 4B)
     * IP 地址起始 = (12 + domainlen + 4) + 2 + 2 + 2 + 4 + 2
     *              = 12 + domainlen + 16
     * 即跳过 Header(12) + Question(domainlen + 4) + Answer前12B
     */
    inet_ntop(AF_INET, recvBuf + (12 + domainlen + 16), ip, 16);

    /* ---- 提取 TTL ----
     * TTL 起始位置 = 12 + domainlen + 10 (跳过 Header + Question + Answer NAME/TYPE/CLASS)
     * 连续 4 字节，网络字节序，使用 ntohl 转换。
     * (int *)(ptr) 从指定位置读取 4 字节整数 */
    int ttl = ntohl(*((int *)(recvBuf + (12 + domainlen + 10))));

    /* Step 2: 缓存结果
     * recvBuf[3] 是 FLAGS 的低字节 + RCODE:
     *   如果 RCODE=0 → 查询成功 → 缓存结果
     *   如果 RCODE≠0 → 查询失败 → 不缓存 */
    if (!recvBuf[3]) {
        /* RCODE = 0 (No Error)，缓存到内存 */
        InsertIntoDNSCache(domainName, ip, ttl);
    }

    /* Step 3: 匹配客户端记录 */

    /* 获取响应报文中的 ID（即我们转发时使用的队列索引） */
    const DNSHeader *recvHeader = (DNSHeader *)recvBuf;
    debugPrintf("收到ID为 %x 的响应报文\n", ntohs(recvHeader->ID));

    /* 网络字节序 → 主机字节序 */
    DNSID newID = ntohs(recvHeader->ID);

    /* 准备接收查找结果 */
    CRecord record = { 0 };
    CRecord *pRecord = &record;

    /* 在 ClientTable 中根据 ID (队列索引) 查找对应记录 */
    if (FindCRecord((DNSID)newID, (CRecord *)pRecord) == 1) {
        /* 找到了记录 */

        /* 检查是否已经回复过 */
        if (pRecord->r == 0) {
            /* 未回复 → 立即标记为已回复，防止重复回复 */
            SetCRecordR(newID);
        } else {
            /* 已回复过 → 重复响应，丢弃 */
            return -1;
        }

        /* Step 4: 准备转发给原始客户端 */

        /* 拷贝完整的 DNS 响应 */
        memcpy(sendBuf, recvBuf, recvByte);

        /* 还原原始 ID: 将报文中的 ID 替换为客户端原始 ID */
        DNSHeader *sendHeader = (DNSHeader *)sendBuf;
        sendHeader->ID = htons(pRecord->originId); /* 主机字节序 → 网络字节序 */

        /* 设置目标地址为原始客户端的地址 */
        *addrCli = pRecord->addr;

        /* 返回响应报文长度 */
        return recvByte;
    } else {
        /* 在 ClientTable 中找不到对应记录 → 丢弃 */
        return -1;
    }
}
