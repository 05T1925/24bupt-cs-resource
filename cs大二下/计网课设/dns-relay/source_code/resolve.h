/**
 * ============================================================
 * resolve.h — DNS 报文解析模块头文件
 * ============================================================
 *
 * 【模块功能】
 *   本模块负责 DNS 报文的解析与构造，包括:
 *
 *   1. DNS 报文头部结构定义 (DNSHeader)
 *      - 遵循 RFC 1035 标准
 *      - 12 字节固定头部 + 变长的 Question/Answer/Authority/Additional 段
 *
 *   2. 处理客户端 Query（查询请求）
 *      - 本地数据库命中 → 构造 DNS Answer 直接回复
 *      - 本地数据库未命中 → 修改 ID 后转发给上游 DNS 服务器
 *
 *   3. 处理上游 DNS 服务器 Response（回复）
 *      - 匹配 ClientTable 中的记录
 *      - 还原原始 ID 后转发给原始客户端
 *      - 将结果缓存到 DNSCache
 *
 * 【DNS 协议基础（RFC 1035）】
 *
 *   DNS 报文结构:
 *   ┌───────────────────┐
 *   │   Header (12字节)  │ ← DNSHeader 结构体
 *   ├───────────────────┤
 *   │   Question 区域    │ ← 要查询的域名 + 类型 + 类
 *   ├───────────────────┤
 *   │   Answer 区域      │ ← 资源记录 (RR)
 *   ├───────────────────┤
 *   │   Authority 区域   │
 *   ├───────────────────┤
 *   │   Additional 区域  │
 *   └───────────────────┘
 *
 *   Header 各字段含义:
 *   - ID       (2B): 查询标识，响应必须与查询一致
 *   - FLAGS    (2B): 包含 QR/Opcode/AA/TC/RD/RA/Z/RCODE
 *     · QR     (bit 15) : 0=查询, 1=响应
 *     · Opcode (bit 14-11): 操作码，0=标准查询
 *     · AA     (bit 10) : 权威应答
 *     · TC     (bit 9)  : 截断标志
 *     · RD     (bit 8)  : 期望递归
 *     · RA     (bit 7)  : 可用递归
 *     · Z      (bit 6-4): 保留字段，必须为 0
 *     · RCODE  (bit 3-0): 返回码，0=无错误, 3=域名不存在
 *   - QDCOUNT  (2B): 问题区域条目数
 *   - ANCOUNT  (2B): 回答区域条目数
 *   - NSCOUNT  (2B): 权威区域条目数
 *   - ARCOUNT  (2B): 附加区域条目数
 *
 *   【域名编码方式】
 *   DNS 报文中域名使用"标签编码"而非纯文本:
 *     "www.example.com" 编码为:
 *     \x03 w w w \x07 e x a m p l e \x03 c o m \x00
 *      ↑长度3    ↑长度7               ↑长度3  ↑结束标记
 *
 *   【指针压缩】
 *   当域名重复出现时，可用 2 字节指针代替: \xC0 \xXX
 *   指向报文中之前出现过的域名位置（相对报文起始的偏移）。
 *   例如 answer 中的 name 常编码为 \xC0 \x0C (指向第 12 字节)。
 */

#pragma once

/* ---- 标准库头文件 ---- */
#include <string.h>
#include <winsock2.h>
#include <WS2tcpip.h>   /* 提供 inet_pton / inet_ntop */
#include "database.h"
#include "control.h"

/* ============================================================
 * DNS 报文头部结构体 (RFC 1035 Section 4.1.1)
 * ============================================================
 *
 * 共 12 字节，所有多字节字段均使用网络字节序（大端）。
 * 使用时需用 ntohs/htons 在主机字节序和网络字节序间转换。
 */
typedef struct dnsheader {
    unsigned short ID;       /* 会话标识 (Transaction ID)
                                响应必须与查询使用相同的 ID */
    unsigned short FLAGS;    /* 标志字段 (2字节)
                                bit 15    : QR    0=查询, 1=响应
                                bit 14-11 : OPCODE 0=标准查询
                                bit 10    : AA    权威应答
                                bit 9     : TC    截断
                                bit 8     : RD    期望递归
                                bit 7     : RA    可用递归
                                bit 6-4   : Z     保留(必须为0)
                                bit 3-0   : RCODE 返回码 */
    unsigned short QDCOUNT;  /* 问题计数 (Question Count)
                                通常为 1（一次查一个域名）*/
    unsigned short ANCOUNT;  /* 回答计数 (Answer Count)
                                Answer 区域中的资源记录数 */
    unsigned short NSCOUNT;  /* 权威计数 (Authority Count)
                                Authority 区域中的资源记录数 */
    unsigned short ARCOUNT;  /* 附加计数 (Additional Count)
                                Additional 区域中的资源记录数 */
} DNSHeader;

/* ============================================================
 * 公共函数声明
 * ============================================================ */

/**
 * ResolveQuery() — 处理来自客户端的 DNS 查询请求
 *
 * 【处理流程】
 *   1. 从报文中提取域名
 *   2. 在本地数据库/缓存中查找
 *      - 找到 → 构造 DNS Answer 回复（sendBuf）
 *      - 未找到 → 转发给上游 DNS 服务器
 *        · 调用 PushCRecord 记录客户端信息
 *        · 将报文 ID 替换为队列索引
 *        · 将目标地址设为上游 DNS 服务器
 *
 * 【返回值含义】
 *   > 0  : sendBuf 中有效数据的字节数
 *   < 0  : 处理失败（队列满等）
 *
 * 【构造 Answer 的细节】
 *   本地回复时，需要在 query 报文后面追加 Answer 区域:
 *   - 前 2 字节: \xC0\x0C (指针压缩，指向第 12 字节的域名)
 *   - 接着 TYPE=1 (A记录), CLASS=1 (IN)
 *   - TTL = 120 秒
 *   - DATA LENGTH = 4 (IPv4 地址 4 字节)
 *   - 最后 4 字节: IP 地址
 *
 * @param recvBuf  接收缓冲区（收到的原始查询报文）
 * @param sendBuf  发送缓冲区（输出的报文: 回复或转发查询）
 * @param recvByte 接收报文的字节数
 * @param addrCli  [输入]客户端地址, [输出]转发目标地址
 * @return sendBuf 中有效数据的字节数, -1 表示失败
 */
extern int ResolveQuery(const unsigned char *recvBuf, unsigned char *sendBuf,
    int recvByte, SOCKADDR_IN *addrCli);

/**
 * ResolveResponse() — 处理来自上游 DNS 服务器的响应
 *
 * 【处理流程】
 *   1. 从响应报文中提取域名、IP、TTL
 *   2. 将查询结果插入缓存 (InsertIntoDNSCache)
 *   3. 取报文中的 ID（即队列索引），在 ClientTable 中查找
 *   4. 如果找到且未回复:
 *      - 将报文 ID 还原为客户端原始 ID
 *      - 将目标地址设为客户端地址
 *      - 标记记录为已回复 (r=1)
 *   5. 如果已回复，丢弃（重复响应）
 *
 * 【TTL 提取】
 *   从 Answer 区域的资源记录中读取 TTL（4字节网络字节序）。
 *   Answer 布局: Name(变长) + Type(2B) + Class(2B) + TTL(4B) + DataLen(2B) + Data(变长)
 *
 * @param recvBuf  接收缓冲区（收到的 DNS 响应报文）
 * @param sendBuf  发送缓冲区（输出的报文: 转发给原始客户端的响应）
 * @param recvByte 接收报文的字节数
 * @param addrCli  [输出]原始客户端的地址
 * @return sendBuf 中有效数据的字节数, -1 表示失败
 */
extern int ResolveResponse(const unsigned char *recvBuf, unsigned char *sendBuf,
    int recvByte, SOCKADDR_IN *addrCli);
