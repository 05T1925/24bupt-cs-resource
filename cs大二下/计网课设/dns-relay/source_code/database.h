/**
 * ============================================================
 * database.h — 数据管理模块头文件
 * ============================================================
 *
 * 【模块功能】
 *   本模块是 DNS 中继器的数据管理核心，负责:
 *
 *   1. 客户端查询表 (ClientTable / CQueue)
 *      - 记录每个来自客户端的 DNS 查询请求
 *      - 使用循环队列实现，支持并发查询的追踪
 *      - 维护请求的源地址、原始ID、回复状态、超时时间
 *
 *   2. DNS 本地数据库 (DNS Database)
 *      - 从 TXT 文本文件中加载域名→IP 的映射表
 *      - 支持快速查找（顺序扫描 + 文件指针复位）
 *
 *   3. DNS 缓存 (DNS Cache)
 *      - 基于 TTL 的内存缓存层
 *      - 本地查到或上游返回的结果都缓存到这里
 *      - 定期更新 TTL，过期自动失效
 *
 * 【数据结构关系图】
 *
 *   客户端查询到达
 *        |
 *   入队 → ClientTable (循环队列, 最大 MAX_QUERIES=25)
 *        |
 *   转发给上游 DNS 服务器
 *        |
 *   收到回复 → 在 ClientTable 中匹配 ID → 回复原客户端
 *
 *   域名查询流程:
 *   DNS Cache (内存, TTL) → TXT 文件 → 上游 DNS 服务器
 *        ↑                                    |
 *        └──────── 缓存查询结果 ←─────────────┘
 */

#pragma once

/* ---- 标准库头文件 ---- */
#include <stdio.h>
#include <winsock2.h>   /* Windows Socket API v2 */
#include <string.h>
#include "control.h"

/* ============================================================
 * 全局宏定义
 * ============================================================ */

/* 客户端查询表（循环队列）的最大容量
 * 即服务器最多同时处理 25 个 DNS 查询请求
 * 队列满时新到达的查询包将被丢弃 */
#define MAX_QUERIES 25

/* DNS 缓存的最大条目数
 * 缓存满后不再插入新条目，旧条目随 TTL 过期自然淘汰 */
#define MAX_CACHE_SIZE 100

/* ============================================================
 * 类型定义
 * ============================================================ */

/* DNS 报文 ID 类型（2 字节无符号整数）
 * 源客户端的查询 ID 和转发给上游的 ID 共用此类型 */
typedef unsigned short DNSID;

/* ============================================================
 * 结构体定义
 * ============================================================ */

/**
 * CRecord — 客户端查询记录
 *
 * 每当收到一个客户端发来的 DNS 查询，就在 ClientTable 中
 * 创建一条 CRecord，记录该客户端的地址、原始查询 ID 等信息。
 *
 * 【字段说明】
 *   addr       客户端的 socket 地址（含 IP + 端口），用于回复
 *   originId   客户端原始 DNS 查询报文的 ID 字段
 *   r          回复标记: 0=尚未回复, 1=已回复
 *   expireTime 该记录的过期时间戳（Unix 时间，秒）
 *              超时后该记录将被从队列中移除
 *
 * 【ID 映射机制】
 *   客户端发送的查询 ID（originId）与转发给上游 DNS 服务器的 ID
 *   可能不同。这里利用队列索引作为新的 ID（newId = rear），
 *   收到回复后通过 newId 反查 originId，再回复正确的 ID 给客户端。
 */
typedef struct {
    SOCKADDR_IN addr;       /* 客户端 socket 地址（IPv4 地址 + 端口） */
    DNSID originId;         /* 客户端原始 DNS 查询报文 ID */
    unsigned char r;        /* 回复状态: 0=未回复, 1=已回复 */
    int expireTime;         /* 超时时间戳（Unix 时间） */
} CRecord;

/**
 * CQueue — 客户端查询表（循环队列实现）
 *
 * 使用数组实现的循环队列，避免频繁的内存分配。
 * 队列的索引范围是 [0, MAX_QUERIES-1]。
 *
 * 【队列状态判断】
 *   - 队空: front == rear
 *   - 队满: (rear + 1) % MAX_QUERIES == front
 *     （故意空一个位置，以区分队空和队满）
 *
 * 【循环索引计算】
 *   每次 rear 或 front 前进都需要取模: (index + 1) % MAX_QUERIES
 */
typedef struct {
    CRecord base[MAX_QUERIES]; /* 存储记录的数组 */
    int front;                  /* 队首索引（指向最早入队的有效记录） */
    int rear;                   /* 队尾索引（指向下一个可插入的位置） */
} CQueue;

/* ============================================================
 * ClientTable（客户端查询表）API
 * ============================================================ */

/**
 * InitCTable() — 初始化客户端查询表
 *
 * 将 front 和 rear 都设为 0，表示空队列。
 * 程序启动时调用一次。
 */
extern void InitCTable();

/**
 * DebugCTable() — 打印客户端查询表状态
 *
 * 输出当前队列已使用空间/总容量，用于调试监控。
 */
extern void DebugCTable();

/**
 * CTableUsage() — 获取客户端查询表已使用空间
 *
 * @return 队列中当前等待处理的记录数
 *
 * 循环队列的使用量计算:
 *   当 rear >= front: usage = rear - front
 *   当 rear <  front: usage = rear + MAX_QUERIES - front
 */
extern int CTableUsage();

/**
 * PushCRecord() — 向客户端查询表队尾添加记录
 *
 * 将客户端查询请求入队。如果队列已满，返回 0 表示失败。
 * 入队成功后:
 *   - 保存客户端的 socket 地址（用于后续回复）
 *   - 保存原始查询 ID
 *   - 将回复标记 r 设为 0（未回复）
 *   - 自动计算过期时间 = 当前时间 + TIMEOUT
 *   - **重要**: 将 pId 指向的值修改为队尾索引（作为转发用的新 ID）
 *
 * @param pAddr 指向客户端 socket 地址的指针（输入参数）
 * @param pId   指向 DNS 查询 ID 的指针（输入输出参数）
 *               输入时为原始 ID，输出时被替换为队尾索引
 * @return 1 = 入队成功, 0 = 队列已满
 */
extern int PushCRecord(const SOCKADDR_IN *pAddr, DNSID *pId);

/**
 * PopCRecord() — 从客户端查询表队首移除记录
 *
 * 将 front 指针前进一个位置。通常在以下情况调用:
 *   1. 收到的回复已成功发送给客户端（r=1）
 *   2. 查询超时，放弃等待
 *
 * @return 1 = 出队成功, 0 = 队列为空
 */
extern int PopCRecord();

/**
 * SetCRecordR() — 将指定 ID 的记录标记为"已回复"
 *
 * 当收到上游 DNS 服务器的回复并成功转发给客户端后，
 * 将该记录的 r 标记设为 1，表示已完成。
 *
 * @param id 记录在队列中的索引（注意: 不是原始 DNS ID）
 * @return 1 = 标记成功, 0 = ID 越界
 */
extern int SetCRecordR(DNSID id);

/**
 * FindCRecord() — 根据 ID 查找记录
 *
 * 在队列中查找指定索引的记录，并将其内容拷贝到 pRecord 中。
 * 用于回复客户端时获取原始的 socket 地址和查询 ID。
 *
 * @param id       记录在队列中的索引
 * @param pRecord  输出参数，接收找到的记录内容
 * @return 1 = 找到, 0 = ID 越界或无效
 *
 * 【有效性检查】
 *   队列的 ID 必须处于 [front, rear) 范围内，考虑循环情况:
 *   - 如果 front <= rear: 有效区间 [front, rear)
 *   - 如果 front >  rear: 有效区间 [front, MAX_QUERIES) ∪ [0, rear)
 */
extern int FindCRecord(DNSID id, CRecord *pRecord);

/**
 * GetCTableRearIndex() — 获取队尾元素索引
 *
 * 返回 rear - 1（最新的入队记录索引），用于 PushCRecord 后
 * 获取新分配的 ID。
 *
 * @return 队尾记录的索引
 */
extern int GetCTableRearIndex();

/**
 * GetCTableFrontIndex() — 获取队首元素索引
 *
 * 返回 front 值，即最早入队记录的索引。
 *
 * @return 队首记录的索引
 */
extern int GetCTableFrontIndex();

/* ============================================================
 * DNS 数据库 API
 * ============================================================ */

/**
 * BuildDNSDatabase() — 从 TXT 文件构建 DNS 数据库
 *
 * 打开 gDBtxt 指定的文本文件，准备查询。
 * 文件格式: 每行一条记录，格式为 "IP地址 域名"，空格分隔。
 * 例如:
 *   127.0.0.1 localhost.local
 *   192.168.1.1 myrouter.home
 *
 * @return 1 = 成功, 0 = 文件打开失败
 */
extern int BuildDNSDatabase();

/**
 * FindInDNSDatabase() — 在 DNS 数据库中查找域名对应的 IP
 *
 * 查找顺序（两级缓存机制）:
 *   1. 先查内存缓存 (DNSCache) — 快路径
 *   2. 缓存未命中时查 TXT 文件 — 慢路径
 *   3. TXT 命中后自动插入缓存（使用默认 TTL=120s）
 *
 * @param domainName 要查询的域名字符串
 * @param ip         输出参数，接收查到的 IP 地址字符串
 *                   调用者需保证至少有 MAX_IP_BUFSIZE(16) 字节空间
 * @return 1 = 找到, 0 = 未找到
 */
extern int FindInDNSDatabase(const char *domainName, char *ip);

/* ============================================================
 * DNS 缓存 API
 * ============================================================ */

/**
 * InsertIntoDNSCache() — 将 DNS 记录插入内存缓存
 *
 * 当从 TXT 文件或上游 DNS 服务器获取到结果后，将其缓存。
 * 缓存满时返回 0（新记录丢弃，但旧记录随 TTL 过期会腾出空间）。
 *
 * @param domainName 域名
 * @param ip         IP 地址字符串
 * @param ttl        TTL 值（秒），从 DNS 响应中获取或使用默认值
 * @return 1 = 插入成功, 0 = 缓存已满
 */
extern int InsertIntoDNSCache(const char *domainName, const char *ip, int ttl);

/**
 * UpdateCache() — 更新所有缓存条目的 TTL
 *
 * 在主循环中每次迭代调用。根据上次检查到现在经过的时间差，
 * 递减所有有效条目的 TTL。TTL 降到 0 或以下后，该条目视为失效。
 *
 * 【TTL 更新算法】
 *   newTTL = oldTTL - (当前时间 - 上次检查时间)
 *   只在实际有时间流逝时才更新（diff > 0），避免无谓操作。
 */
extern void UpdateCache();
