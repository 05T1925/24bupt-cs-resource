/**
 * ============================================================
 * control.c — 公共控制模块实现
 * ============================================================
 *
 * 【模块功能】
 *   本模块实现了 DNS 中继器的公共控制功能:
 *   1. 调试输出 —— 条件打印（debugPrintf）与内存十六进制 dump（DebugBuffer）
 *   2. 缓冲区管理 —— 清空缓冲区（ClearBuffer）
 *   3. 命令行参数解析 —— 解析调试等级、DNS服务器地址、数据库文件名
 *
 * 【全局变量说明】
 *   - gDebugLevel  : 调试等级，0=无输出, 1=日志, 2=详细dump
 *   - addrDNSserv  : 上游 DNS 服务器 IP 地址字符串
 *   - gDBtxt       : TXT 文本数据库文件名
 *   - gDBsqlite    : SQLite 数据库文件名（预留）
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* 禁用 VS 的 scanf/strcpy 安全警告 */
#endif

#include "control.h"

/* 空缓冲区常量，用于比照/初始化 */
static const char emptyBuffer[MAX_BUFSIZE] = { '\0' };

/* ---- 全局变量定义 ---- */
char gDBtxt[100] = "dnsrelay.txt";            /* 默认 TXT 数据库文件名 */
char gDBsqlite[100] = "dnsrelay.db";           /* 默认 SQLite 数据库文件名 */
char addrDNSserv[16] = "202.106.0.20";         /* 默认上游 DNS 服务器 (北京电信 DNS) */
char addrDNSlocalhost[16] = "127.0.0.1";       /* 本机回环地址 */
int gDebugLevel = 0;                            /* 默认调试等级 0: 无输出 */

/**
 * DebugBuffer() — 以十六进制格式逐字节打印内存内容
 *
 * 输出格式: 每行 16 个字节，每个字节以两位十六进制显示 (00-ff)
 * 示例输出:
 *   00 01 80 00 00 01 00 00 00 00 00 00 03 77 77 77
 *   06 67 6f 6f 67 6c 65 03 63 6f 6d 00 00 01 00 01
 *
 * 这在分析 DNS 报文时非常有用，因为 DNS 报文是二进制格式的。
 * 例如可以直观地看到报文头部的 ID、FLAGS、各计数字段等。
 *
 * @param buf     要打印的内存起始地址
 * @param bufSize 要打印的字节数
 *
 * @note 仅在 gDebugLevel >= 2 时才会输出
 */
void DebugBuffer(const unsigned char *buf, int bufSize) {
    /* 调试等级低于 2，不打印 buffer 内容 */
    if (gDebugLevel < 2) return;

    char isEnd = 0; /* 标记最后一行是否已经换行 */

    if (bufSize > MAX_BUFSIZE) {
        /* 缓冲区大小异常检查 */
        debugPrintf("DebugBuffer() failed, bufSize too big: %d>%d", bufSize, MAX_BUFSIZE);
    } else {
        /* 逐字节打印 */
        for (int i = 0; i < bufSize; ++i) {
            /* %02x 确保每个字节用两位十六进制数显示，不足两位补零 */
            debugPrintf("%02x ", buf[i]);
            isEnd = 0;

            /* 每打印 16 个字节换一行，便于阅读 */
            if (i % 16 == 15) {
                debugPrintf("\n");
                isEnd = 1;
            }
        }
        /* 如果最后一行不足 16 字节，补一个换行 */
        if (!isEnd)
            debugPrintf("\n");
    }
}

/**
 * ClearBuffer() — 将缓冲区全部字节置零
 *
 * 在每次接收新报文前清空缓冲区，避免上次的数据残留影响本次处理。
 * 如果 bufSize 超过 MAX_BUFSIZE 或小于 0，则报错并跳过。
 *
 * @param buf     要清空的缓冲区
 * @param bufSize 缓冲区大小（字节数）
 */
void ClearBuffer(unsigned char *buf, int bufSize) {
    if (bufSize > MAX_BUFSIZE) {
        debugPrintf("ClearBuffer() failed, bufSize too big: %d>%d\n", bufSize, MAX_BUFSIZE);
    } else if (bufSize < 0) {
        debugPrintf("ClearBuffer() failed, bufSize error: %d\n", bufSize);
    } else {
        /* 使用 memset 将缓冲区所有字节设为 0 */
        memset(buf, 0, bufSize);
    }
}

/**
 * dealOpts() — 解析命令行参数
 *
 * 【支持的命令行格式】
 *   dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]
 *
 * 【参数说明】
 *   -d          设置调试等级为 1，开启日志输出
 *   -dd         设置调试等级为 2，开启日志 + 十六进制 dump
 *   IP 地址     指定上游 DNS 服务器地址，如 8.8.8.8
 *   .txt 文件   指定本地数据库文件名
 *
 * 【解析策略】
 *   按顺序解析参数:
 *     1. 先检查 -d / -dd 标志
 *     2. 再检查是否为有效 IP 地址格式 (x.x.x.x)
 *     3. 最后检查是否为 .txt 文件
 *
 *   IP 地址解析时做了多重校验:
 *     - sscanf 提取 4 个整数
 *     - 每个整数 < 256（合法 IPv4 范围）
 *     - 去除前导零的影响，正确转换为字符串
 *
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 1 = 参数格式正确, 0 = 格式错误
 */
extern int dealOpts(int argc, char *argv[]) {
    int i = 1; /* 从第 1 个参数开始（跳过程序名 argv[0]） */

    /* 没有额外参数也视为合法（使用默认配置） */
    if (i >= argc) return 1;

    /* ---- 第一步: 解析调试等级 ---- */
    if (!strcmp(argv[i], "-dd")) {
        /* -dd: 最详细的调试输出 */
        gDebugLevel = 2;
        ++i; /* 移到下一个参数 */
    } else if (!strcmp(argv[i], "-d")) {
        /* -d: 基本调试输出 */
        gDebugLevel = 1;
        ++i;
    }

    if (i >= argc) return 1; /* 参数已全部处理 */

    /* ---- 第二步: 解析上游 DNS 服务器 IP 地址 ---- */
    int ipInt[4]; /* 存储 IP 地址的 4 个十进制整数 */
    /* 使用 sscanf 从字符串中提取 4 个整数（以 . 分隔）
     * 返回值 = 4 表示成功提取 4 个整数 */
    if ((4 == sscanf(argv[i], "%d.%d.%d.%d", ipInt + 0, ipInt + 1, ipInt + 2, ipInt + 3))
        && ipInt[0] < 256 && ipInt[1] < 256 && ipInt[2] < 256 && ipInt[3] < 256) {

        /* IP 地址格式有效，将其转换为字符串存入 addrDNSserv */
        int stri = 0; /* addrDNSserv 的写入位置 */
        for (int ipInti = 0; ipInti < 4; ipInti++) {
            /* 不是第一个数字时，在前面加 '.' */
            if (stri) { addrDNSserv[stri++] = '.'; }

            int zeroValid = 0; /* 标记是否已经开始写入有效数字（去前导零） */

            /* 从百位到个位依次处理（div=100, 10, 1） */
            for (int div = 100; div > 0; div /= 10) {
                addrDNSserv[stri] = ipInt[ipInti] / div; /* 计算当前位的数字 */

                if (zeroValid || addrDNSserv[stri]) {
                    /* 如果已经开始有效数字，或当前位非零，则写入 */
                    addrDNSserv[stri++] += 48;  /* 数字转 ASCII 字符 (+'0') */
                    ipInt[ipInti] %= div;       /* 去掉已处理的最高位 */
                    zeroValid = 1;               /* 后续的零也需要写入 */
                }
                /* 否则是前导零，跳过不写 */
            }
            /* 如果是纯 0（如 "0.0.0.0" 中的某个 0），补一个 '0' */
            if (!zeroValid) { addrDNSserv[stri++] = '0'; }
        }
        addrDNSserv[stri] = '\0'; /* 字符串结尾 */
        ++i; /* 移到下一个参数 */
    }

    if (i >= argc) return 1; /* 参数已全部处理 */

    /* ---- 第三步: 解析 TXT 数据库文件名 ---- */
    /* 检查参数是否以 ".txt" 结尾 */
    if (!strcmp(argv[i] + strlen(argv[i]) - 4, ".txt")) {
        strcpy(gDBtxt, argv[i]); /* 记录数据库文件名 */
        ++i;
    }

    /* 所有参数都已正确处理 → 成功 */
    if (i >= argc) return 1;
    /* 还有未识别的参数 → 失败 */
    else return 0;
}

/**
 * debugPrintf() — 条件打印函数
 *
 * 封装了 vprintf，仅在 gDebugLevel >= 1 时生效。
 * 使用 C 标准库的可变参数机制 (stdarg.h):
 *   - va_list  : 参数列表指针
 *   - va_start : 初始化参数列表
 *   - vprintf  : 使用参数列表格式化输出
 *   - va_end   : 清理参数列表
 *
 * @param cmd 格式化字符串
 * @param ... 可变参数
 */
void debugPrintf(const char *cmd, ...) {
    /* 调试等级 < 1 时静默 */
    if (gDebugLevel < 1) return;

    va_list args;               /* 定义参数列表变量 */
    va_start(args, cmd);        /* 以 cmd 为起点初始化参数列表 */
    vprintf(cmd, args);         /* 格式化输出到标准输出 */
    va_end(args);               /* 清理 */
}
