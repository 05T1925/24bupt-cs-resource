/**
 * ============================================================
 * control.h — 公共控制模块 (Common Control Module)
 * ============================================================
 *
 * 【模块功能】
 *   本模块是整个 DNS 中继器的公共头文件，提供:
 *   1. 全局宏定义（缓冲区大小、超时时间等）
 *   2. 全局变量声明（调试等级、DNS服务器地址、数据库文件名等）
 *   3. 调试工具函数声明（内存打印、日志输出）
 *   4. 命令行参数解析函数声明
 *
 * 【DNS 中继器项目背景】
 *   DNS (Domain Name System) 是互联网的核心服务之一，负责将域名解析为 IP 地址。
 *   本程序实现了一个 DNS 中继服务器，工作流程如下:
 *     客户端 → [DNS中继器] → 上游DNS服务器
 *              ↓
 *           本地数据库 (dnsrelay.txt)
 *
 *   当收到客户端的 DNS 查询时:
 *     - 先在本地数据库(文件 + 内存缓存)中查找
 *     - 如果找到，直接返回结果（拦截模式）
 *     - 如果未找到，转发给上游 DNS 服务器（中继模式）
 *     - 将上游返回的结果缓存到内存中，加速后续查询
 *
 * 【编码规范】
 *   - 变量名: camelCase 风格
 *   - 函数名: PascalCase 风格
 *   - 文件名: snake_case 风格
 *   - 所有全局变量用 static 修饰，通过接口函数访问
 *   - 外部函数声明加 extern 关键字
 *   - 一个字节的内存区统一使用 unsigned char
 *   - 注释统一使用 C 风格 (斜杠星号)
 */

#pragma once

/* ---- 标准库头文件 ---- */
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "database.h"

/* ============================================================
 * 全局宏定义
 * ============================================================ */

/* UDP 报文最大缓冲区大小（字节）
 * DNS 协议规定 UDP 报文最大为 512 字节（不含 TCP/IP 头）*/
#define MAX_BUFSIZE 512

/* IP 地址字符串的最大长度
 * IPv4 地址最长形式: "255.255.255.255" = 15 个字符 + '\0' = 16 */
#define MAX_IP_BUFSIZE 16

/* 域名的最大长度（字符数）
 * DNS 规范中完整域名最大为 253 字节，这里取 100 已足够 */
#define MAX_DOMAINNAME 100

/* TTL (Time To Live) 初始值，单位：秒
 * 当从本地数据库查到记录并插入缓存时，使用此默认 TTL = 120 秒 */
#define TTL 120

/* 客户端查询超时时间，单位：秒
 * 如果向 DNS 服务器转发的查询超过 3 秒未收到回复，
 * 该记录将被从等待队列中移除 */
#define TIMEOUT 3

/* ============================================================
 * 事件类型枚举
 * ============================================================
 * 用于主循环中 select() 返回后判断发生了什么事件:
 *   dgram_arrival : 有 UDP 数据报到达，需要读取并处理
 *   timeout       : 等待超时，没有数据到达
 *   nothing       : 没有任何事件发生
 */
typedef enum { dgram_arrival, timeout, nothing } event_type;

/* ============================================================
 * 全局变量声明（定义在 control.c 中）
 * ============================================================ */

/* 用户指定的 TXT 格式数据库文件名，默认 "dnsrelay.txt" */
extern char gDBtxt[100];

/* 上游 DNS 服务器的 IPv4 地址字符串，如 "202.106.0.20" */
extern char addrDNSserv[16];

/* 本机回环地址 "127.0.0.1" */
extern char addrDNSlocalhost[16];

/* SQLite 数据库文件名（预留，当前未启用），默认 "dnsrelay.db" */
extern char gDBsqlite[100];

/* 调试等级:
 *   0 = 不输出任何调试信息
 *   1 = 输出基本的日志信息（debugPrintf 生效）
 *   2 = 输出详细信息（包括 DebugBuffer 十六进制 dump）*/
extern int gDebugLevel;

/* ============================================================
 * 公共函数声明
 * ============================================================ */

/**
 * DebugBuffer() — 以十六进制格式打印内存缓冲区内容
 *
 * 常用于调试 DNS 报文的二进制内容，方便分析报文结构。
 * 每行打印 16 个字节，格式如 "0a 1b 3c ..."。
 *
 * @param buf     指向要打印的内存区域
 * @param bufSize 要打印的字节数
 *
 * @note 仅在 gDebugLevel >= 2 时生效
 */
extern void DebugBuffer(const unsigned char *buf, int bufSize);

/**
 * ClearBuffer() — 清空（置零）内存缓冲区
 *
 * 在每次接收/发送报文前调用，避免旧数据干扰。
 * 本质是调用 memset(buf, 0, bufSize)。
 *
 * @param buf     指向要清空的内存区域
 * @param bufSize 要清空的字节数
 */
extern void ClearBuffer(unsigned char *buf, int bufSize);

/**
 * dealOpts() — 解析命令行参数
 *
 * 支持的参数格式:
 *   dnsrelay [-d|-dd] [dns-server-ipaddr] [filename]
 *
 *   -d      : 设置调试等级为 1（输出日志）
 *   -dd     : 设置调试等级为 2（输出日志 + 十六进制 dump）
 *   -dd 后跟 IP 地址 : 指定上游 DNS 服务器地址
 *   .txt 结尾的参数 : 指定本地数据库文件名
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 1 = 格式正确, 0 = 格式错误
 */
extern int dealOpts(int argc, char *argv[]);

/**
 * debugPrintf() — 条件打印函数（重新封装 printf）
 *
 * 仅在 gDebugLevel >= 1 时输出，支持可变参数（类似 printf）。
 * 用于输出运行时的调试信息，方便追踪程序行为。
 *
 * @param cmd  格式化字符串
 * @param ...  可变参数列表
 */
extern void debugPrintf(const char *cmd, ...);
