/**
 * ============================================================
 * database.c — 数据管理模块实现
 * ============================================================
 *
 * 【模块功能】
 *   本模块实现了 DNS 中继器的数据管理层:
 *
 *   1. ClientTable（客户端查询表）
 *      - 基于循环队列的并发查询管理
 *      - 记录每个待处理查询的客户端地址、原始ID、状态
 *      - 提供入队/出队/查找/标记等操作
 *
 *   2. TXT 文本数据库
 *      - 从纯文本文件读取域名→IP 映射
 *      - 顺序查找（每次从文件头开始扫描）
 *
 *   3. DNS 内存缓存（DNS Cache）
 *      - 固定大小的 TTL 缓存
 *      - 先查缓存再查文件的两级查询机制
 *      - 周期性更新 TTL，自动淘汰过期条目
 *
 * 【循环队列设计细节】
 *
 *   队列状态:
 *     front == rear           → 队空
 *     (rear+1)%N == front     → 队满（牺牲一个位置）
 *
 *   索引范围检查（考虑循环）:
 *     若 front <= rear: 有效索引 ∈ [front, rear)
 *     若 front >  rear: 有效索引 ∈ [front, N) ∪ [0, rear)
 *
 *   例如: N=5, front=3, rear=1
 *     → 有效索引为 [3,4] 和 [0]，共 3 个元素
 *
 * 【两级缓存查询流程】
 *
 *   查询请求进入
 *       ↓
 *   FindInDNSDatabase()
 *       ↓
 *   ① 查内存缓存 (FindInDNSCache)
 *       ├─ 命中 → 直接返回 (最快)
 *       └─ 未命中 ↓
 *   ② 查TXT文件 (FindIPByDNSinTXT)
 *       ├─ 命中 → 插入缓存 → 返回
 *       └─ 未命中 → 返回失败 → 需转发上游DNS
 *
 * 【为什么需要缓存】
 *   1. TXT 文本查询是 O(n) 的文件 I/O，效率很低
 *   2. 缓存将结果保存在内存中，O(1) 查找
 *   3. TTL 机制保证数据不会永久占用缓存空间
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "database.h"

/* ============================================================
 * ClientTable（客户端查询表）实现
 * ============================================================ */

/* 全局唯一的客户端查询表实例（循环队列） */
static CQueue clientTable;

/**
 * InitCTable() — 初始化客户端查询表
 *
 * 将队首、队尾指针归零，表示空队列。
 * 在主函数中绑定 socket 后调用。
 */
void InitCTable() {
    clientTable.front = 0;
    clientTable.rear = 0;
    debugPrintf("ClientTable init.\n");
}

/**
 * DebugCTable() — 打印客户端查询表状态
 *
 * 输出格式: "查询缓冲区 MAX_QUERIES 已使用 used"
 * 用于监控当前正在等待回复的查询数量。
 */
void DebugCTable() {
    int used = clientTable.rear - clientTable.front;
    /* 处理循环情况: 如果 rear < front，需要加上总容量 */
    used = used < 0 ? used + MAX_QUERIES : used;
    debugPrintf("查询缓冲区 %d 已使用%d\n", MAX_QUERIES, used);
}

/**
 * CTableUsage() — 获取当前队列中有效记录的个数
 *
 * 计算公式:
 *   - 若 rear >= front: usage = rear - front
 *   - 若 rear <  front: usage = rear + MAX_QUERIES - front
 *
 * @return 队列中当前的记录数
 */
int CTableUsage() {
    int used = clientTable.rear - clientTable.front;
    used = used < 0 ? used + MAX_QUERIES : used;
    return used;
}

/**
 * PushCRecord() — 将客户端查询记录入队
 *
 * 【操作流程】
 *   1. 检查队列是否已满 → 满则返回 0
 *   2. 在 rear 位置写入客户端地址、原始 ID
 *   3. 设置 r = 0（未回复）
 *   4. 计算超时时间 = 当前Unix时间 + TIMEOUT
 *   5. 将 pId 修改为 rear（作为转发给上游 DNS 的新 ID）
 *   6. rear 前进一位 (取模)
 *
 * 【ID 映射技巧】
 *   这里的设计很巧妙: 用队列索引作为转发 ID。
 *   - 客户端的原始 ID 保存在 originId
 *   - 转发给 DNS 服务器时使用 newId = rear（队列索引）
 *   - 收到回复时用 newId 查 CRecord → 取回 originId → 回复客户端
 *   - 这样避免了 ID 冲突问题
 *
 * @param pAddr 客户端 socket 地址指针
 * @param pId   [输入]客户端原始查询 ID, [输出]替换为队列索引
 * @return 1=成功, 0=队列已满
 */
int PushCRecord(const SOCKADDR_IN *pAddr, DNSID *pId) {
    /* 队满判断: (rear+1) % N == front（故意空一个位置区分队空和队满）*/
    if ((clientTable.rear + 1) % MAX_QUERIES == clientTable.front) {
        debugPrintf("缓冲区满, 丢弃报文.\n");
        return 0;
    } else {
        /* 保存客户端地址 */
        clientTable.base[clientTable.rear].addr = *pAddr;
        /* 保存客户端的原始 DNS 报文 ID */
        clientTable.base[clientTable.rear].originId = *pId;
        /* 标记为未回复 */
        clientTable.base[clientTable.rear].r = 0;
        /* 计算超时时间: 当前时间 + TIMEOUT 秒 */
        clientTable.base[clientTable.rear].expireTime = (int)time(NULL) + TIMEOUT;
        /* ★ 用队列索引替换原来的 ID，作为转发给上游 DNS 的新 ID */
        *pId = clientTable.rear;
        /* rear 前进一位，注意取模实现循环 */
        clientTable.rear = (clientTable.rear + 1) % MAX_QUERIES;
        return 1;
    }
}

/**
 * PopCRecord() — 从队首弹出一条记录
 *
 * 仅移动 front 指针，不实际清除数据（会被新入队数据覆盖）。
 * 通常在收到回复并成功转发、或记录超时后调用。
 *
 * @return 1=成功, 0=队列为空
 */
int PopCRecord() {
    if (clientTable.rear == clientTable.front) {
        /* 队空: front == rear */
        debugPrintf("队列为空，PopCRecord()失败。\n");
        return 0;
    } else {
        /* front 前进一位 */
        clientTable.front = (clientTable.front + 1) % MAX_QUERIES;
        return 1;
    }
}

/**
 * SetCRecordR() — 标记指定 ID 的记录为"已回复"
 *
 * 先检查 ID 是否在有效范围内（队列中存在的记录），
 * 然后将该记录的 r 字段设为 1。
 *
 * 【有效性检查逻辑】
 *   队列的有效索引范围需分两种情况:
 *     情况 A (front <= rear): ID ∈ [front, rear)
 *     情况 B (front >  rear): ID ∈ [front, N) 或 ID ∈ [0, rear)
 *   使用 || 和 && 的组合来表示这两种情况。
 *
 * @param id 队列中的索引
 * @return 1=成功, 0=ID 越界
 */
int SetCRecordR(DNSID id) {
    if ((
        clientTable.front <= clientTable.rear
        /* 情况 A: 队列未绕回, ID 必须在 [front, rear) 之间 */
        && (id < clientTable.rear && id >= clientTable.front)
    ) || (
        clientTable.front > clientTable.rear
        /* 情况 B: 队列已绕回, ID 可以在 [front, N) 或 [0, rear) */
        && (id < clientTable.rear || id >= clientTable.front))
    ) {
        clientTable.base[id].r = 1;
        return 1;
    } else {
        debugPrintf("要修改的记录不存在, 索引越界, SetCRecordR()失败 ID=%d\n", id);
        return 0;
    }
}

/**
 * FindCRecord() — 根据 ID 查找记录
 *
 * 在队列中查找指定索引的记录，将其内容拷贝到 pRecord。
 * 同时打印调试信息（过期时间和原始 ID）。
 *
 * 【使用场景】
 *   收到上游 DNS 的回复时，报文中包含我们转发时使用的 ID（即队列索引）。
 *   通过 FindCRecord 可以获取:
 *     - 原始客户端地址（addr），用于发送回复
 *     - 原始查询 ID（originId），用于还原报文 ID
 *
 * @param id      队列索引
 * @param pRecord 输出参数，接收找到的记录
 * @return 1=找到, 0=未找到（索引越界）
 */
int FindCRecord(DNSID id, CRecord *pRecord) {
    if ((
        clientTable.front <= clientTable.rear
        && (id < clientTable.rear && id >= clientTable.front)
    ) || (
        clientTable.front > clientTable.rear
        && (id < clientTable.rear || id >= clientTable.front))
    ) {
        /* 索引有效，拷贝整个结构体 */
        *pRecord = clientTable.base[id];
        debugPrintf("FindCRecord:ID=%d, expireTime=%d, originId=%d\n",
            id, clientTable.base[id].expireTime, clientTable.base[id].originId);
        return 1;
    } else {
        debugPrintf("要查找的记录不存在, 索引越界, FindCRecord()失败\n");
        return 0;
    }
}

/**
 * GetCTableRearIndex() — 获取队尾元素索引
 *
 * 返回实际最后一个元素的索引（即 rear-1 取模后的值）。
 * 用于 PushCRecord 之后获取刚刚分配的索引。
 *
 * @return 队尾元素的索引
 */
int GetCTableRearIndex() {
    /* rear 指向下一个可插入位置，所以最后一个元素在 rear-1
     * 加上 MAX_QUERIES 再取模，避免负数 */
    return ((clientTable.rear + MAX_QUERIES - 1) % MAX_QUERIES);
}

/**
 * GetCTableFrontIndex() — 获取队首元素索引
 *
 * @return 队首索引 front
 */
int GetCTableFrontIndex() {
    return clientTable.front;
}

/* ============================================================
 * TXT 文本数据库实现
 * ============================================================ */

/* TXT 数据库文件指针 */
static FILE *dbTXT = NULL;

/* 文件起始位置的 fpos_t，用于每次查询时复位到文件开头 */
static fpos_t dbHome = 0;

/**
 * FindIPByDNSinTXT() — 在 TXT 文本文件中查找域名对应的 IP
 *
 * 【查找算法】
 *   1. 将文件指针复位到开头（fsetpos）
 *   2. 逐行读取 "IP 域名" 格式的记录
 *   3. 比较域名是否匹配（精确匹配，大小写敏感）
 *   4. 匹配成功 → 返回 1
 *   5. 读到文件末尾仍未匹配 → 返回 0
 *
 * 【性能考量】
 *   这是最简单但效率最低的查找方式，每次都扫描整个文件。
 *   因此引入内存缓存（DNSCache）来加速重复查询。
 *   在大规模场景下可替换为 SQLite 或哈希表。
 *
 * 【文件格式示例】
 *   127.0.0.1 localhost.local
 *   192.168.1.1 router.home
 *   10.0.0.1 server.internal
 *
 * @param dbTXT 已打开的 TXT 数据库文件指针
 * @param name  要查找的域名
 * @param ip    输出参数，存储找到的 IP 地址字符串
 * @return 1=找到, 0=未找到
 */
static int FindIPByDNSinTXT(FILE *dbTXT, const char *name, char *ip) {
    char retName[MAX_DOMAINNAME] = { '\0' };

    /* 重置文件指针到开头，准备从头扫描 */
    fsetpos(dbTXT, &dbHome);

    /* 逐行读取格式: "%s %s" → IP地址 域名 */
    while (!feof(dbTXT)) {
        fscanf(dbTXT, "%s %s", ip, retName);
        /* 比较域名是否匹配 */
        if (!strcmp(retName, name)) break;
    }

    /* 如果是因为读到文件末尾而退出循环 → 未找到 */
    if (feof(dbTXT)) return 0;
    return 1;
}

/* ============================================================
 * DNS 内存缓存（DNS Cache）实现
 * ============================================================ */

/**
 * DNScache — DNS 缓存条目结构
 *
 * 每个条目包含完整的域名、IP 地址和剩余 TTL。
 * TTL <= 0 表示该条目无效（空闲或已过期）。
 */
typedef struct {
    char domainName[MAX_DOMAINNAME]; /* 域名字符串 */
    char ip[MAX_IP_BUFSIZE];         /* IP 地址字符串 */
    int ttl;                         /* 剩余生存时间 (秒), <=0 表示无效 */
} DNScache;

/* 缓存数组（固定大小，不可动态增长）*/
static DNScache cache[MAX_CACHE_SIZE] = { 0 };

/* 上次检查缓存的时间戳（Unix 时间，秒）
 * 初始值为 0，确保首次 UpdateCache 时就会更新 */
static time_t cacheLastCheckTime = 0;

/**
 * FindInDNSCache() — 在内存缓存中查找域名
 *
 * 【查找算法】
 *   遍历整个缓存数组，寻找:
 *     1. ttl > 0  （条目有效）
 *     2. domainName 与查询域名精确匹配
 *
 *   找到第一个匹配的条目即返回。
 *
 * 【复杂度】
 *   - 最好: O(1)（命中第一个位置）
 *   - 最坏: O(MAX_CACHE_SIZE)
 *   - 平均: 远快于文件 I/O
 *
 * @param domainName 要查找的域名
 * @param ip         输出参数，存储 IP 地址
 * @return 1=命中, 0=未命中
 */
static int FindInDNSCache(const char *domainName, char *ip) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        /* 条目有效 且 域名匹配 */
        if (cache[i].ttl > 0 && !strcmp(cache[i].domainName, domainName)) {
            /* 拷贝 IP 到输出参数 */
            sprintf(ip, "%s", cache[i].ip);
            debugPrintf("cache命中: %s,%s\n", cache[i].domainName, domainName);
            return 1;
        }
    }
    return 0;
}

/**
 * InsertIntoDNSCache() — 向缓存中插入一条 DNS 记录
 *
 * 【插入策略】
 *   遍历缓存数组，找到第一个 ttl <= 0 的空位 (或已过期位)。
 *   将域名、IP、TTL 写入该位置。
 *
 * 【缓存满的处理】
 *   如果遍历完数组未找到空位，返回 0。
 *   此时不强制淘汰任何条目 — 依赖 UpdateCache() 让旧条目
 *   自然过期，下次插入时就能找到空位。
 *
 * @param domainName 域名
 * @param ip         IP 地址
 * @param ttl        TTL 值（秒）
 * @return 1=插入成功, 0=缓存已满
 */
int InsertIntoDNSCache(const char *domainName, const char *ip, int ttl) {
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        if (cache[i].ttl <= 0) {
            /* 找到空闲位置 */
            debugPrintf("cachesave: %s, %s, %d\n", domainName, ip, ttl);
            sprintf(cache[i].domainName, "%s", domainName);
            sprintf(cache[i].ip, "%s", ip);
            cache[i].ttl = ttl;
            return 1;
        }
    }
    /* 遍历完未找到空位 */
    return 0;
}

/**
 * UpdateCache() — 更新所有缓存条目的 TTL
 *
 * 【TTL 更新机制】
 *   1. 计算 "当前时间 - 上次检查时间" 的差值（秒）
 *   2. 如果差值 > 0，遍历所有有效条目（ttl > 0），执行 ttl -= diff
 *   3. TTL 降为 0 或负数后，该条目视为失效
 *
 * 【调用时机】
 *   在 WaitForEvent() 的每次迭代中调用，即主循环的每一轮。
 *   因为 select() 的超时是 1 秒，所以大致每秒更新一次。
 *
 * 【为什么不直接删除过期条目】
 *   不需要额外操作：FindInDNSCache 检查 ttl > 0，
 *   过期条目自然不会被命中；InsertIntoDNSCache 查找 ttl <= 0 的空位，
 *   过期条目自然成为可覆盖位置。
 */
void UpdateCache() {
    time_t newTime = time(0);       /* 获取当前 Unix 时间戳 */
    time_t diff = newTime - cacheLastCheckTime; /* 计算时间差 */

    debugPrintf("距离上一次检查过去了 %lld 秒\n", diff);

    if (diff) {
        /* 确实有时间流逝，更新记录并推进 lastCheckTime */
        cacheLastCheckTime = newTime;

        /* 遍历所有缓存条目 */
        for (int i = 0; i < MAX_CACHE_SIZE; i++) {
            if (cache[i].ttl > 0) {
                /* 递减 TTL（但不能减为负数） */
                cache[i].ttl -= (int)diff;
                /* 调试: 打印每个有效条目的信息 */
                /* printf("%s : %s  TTL= %d\n", cache[i].domainName, cache[i].ip, cache[i].ttl); */
            }
        }
    }
}

/* ============================================================
 * 统一数据库接口
 * ============================================================ */

/**
 * BuildDNSDatabase() — 打开 TXT 数据库文件
 *
 * 使用全局变量 gDBtxt 指定的文件名。
 * 同时保存文件初始位置到 dbHome，用于后续查询时的复位。
 *
 * 【扩展性】
 *   注释掉的代码显示原本计划支持 SQLite 数据库，
 *   包括建表、导入 TXT 数据等功能。当前仅使用 TXT 实现。
 *
 * @return 1=成功打开文件, 0=失败
 */
int BuildDNSDatabase() {
    /* 打开 TXT 文本数据库 */
    dbTXT = fopen(gDBtxt, "r");
    /* 保存文件初始位置 */
    fgetpos(dbTXT, &dbHome);
    if (!dbTXT) return 0;
    return 1;
}

/**
 * FindInDNSDatabase() — 统一的域名查询接口
 *
 * 【两级查询流程】
 *   ① 先查内存缓存 (FindInDNSCache)
 *       → 命中则直接返回，路径最短
 *   ② 未命中则查 TXT 文件 (FindIPByDNSinTXT)
 *       → 命中后将结果插入缓存（TTL=120s），方便下次查询
 *       → 未命中则返回 0，调用者需要转发到上游 DNS 服务器
 *
 * 【为什么文件命中要写入缓存】
 *   文件扫描是 O(n) 的，缓存是 O(k) 的。
 *   将结果缓存可以极大加速后续对同一域名的查询。
 *   默认 TTL=120 秒对本地静态映射来说已经足够。
 *
 * @param domainName 域名
 * @param ip         输出参数，IP 地址
 * @return 1=找到, 0=未找到
 */
int FindInDNSDatabase(const char *domainName, char *ip) {
    /* 第一级: 查询内存缓存 */
    if (FindInDNSCache(domainName, ip)) {
        debugPrintf("在 cache 中找到了\n");
        return 1;
    }

    /* 第二级: 查询 TXT 文件 */
    if (dbTXT && FindIPByDNSinTXT(dbTXT, domainName, ip)) {
        /* TXT 命中后自动插入缓存，加速后续查询 */
        InsertIntoDNSCache(domainName, ip, TTL);
        return 1;
    } else {
        return 0;
    }
}
