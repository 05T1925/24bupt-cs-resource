// =============================================================================
// ProtocolCodec.h — 自定义应用层协议编解码器
// =============================================================================
// 文件用途：定义 Logistics V3 的 C/S 通信协议格式和编解码逻辑。
// 所属模块：common/protocol（协议层，同时被 server 和 client 编译）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 协议帧格式（纯文本，\n 为消息边界）：
//   请求帧：REQ|command|token|argCount|arg1|arg2|...\n
//   响应帧：RES|1/0|code|message|recordCount|record1|record2|...\n
//
// 给网络初学者的理解：
//   TCP 只传输连续字节，并不知道 Request/Response 是什么。
//   ProtocolCodec 相当于双方共同遵守的“装箱/拆箱规则”：
//     编码：C++ 结构体 -> 可发送的文本帧
//     解码：收到的文本帧 -> C++ 结构体
//   最后的 '\n' 是本项目自定义的消息结束标志，不是 TCP 自动添加的。
//
// 字段转义规则（解决用户输入中含控制字符的问题）：
//   %  → %25
//   |  → %7C
//   \n → %0A
//   \r → %0D
//   未知或不完整转义序列 → 抛出 ProtocolError
//
// 协议安全常量：
//   MaxMessageLength = 8192  单帧最大字节数（防止内存溢出）
//   MaxFieldLength   = 1024  单字段最大字节数（防止恶意大字段）
//   MaxVectorItems   = 256   参数/记录数量上限（防止畸形帧）
//
// TCP 流式处理说明：
//   - 协议层本身不处理 TCP 缓冲 — 该职责由 SocketServer::handleClient
//     和 SocketClient::receiveFrame 的 buffer 机制处理
//   - ProtocolCodec 只负责单帧的编解码和校验
//   - normalizeFrame 会检查并拒绝帧内包含未转义换行符的情况
//   - 一条完整帧被取出后才能调用 decodeRequest/decodeResponse
//
// 命令字约束（validateCommand）：
//   - 只允许大写字母 [A-Z]、数字 [0-9] 和下划线 [_]
//   - 空命令拒绝
//   - 这是防止命令注入的第一道防线
//
// 异常体系：
//   ProtocolError(code, message) — 所有协议层错误，含错误码
//   错误码：PROTOCOL_ERROR / UNKNOWN_COMMAND / INVALID_ARGUMENT
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_PROTOCOL_PROTOCOL_CODEC_H
#define LOGISTICS_V3_COMMON_PROTOCOL_PROTOCOL_CODEC_H

#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Request — 客户端请求结构
// command：命令字（大写下划线格式，如 SEND_EXPRESS）
// token：  会话令牌（GUEST 命令为空字符串）
// args：   命令参数列表（不含 argCount，argCount 由 encodeRequest 自动添加）
// ---------------------------------------------------------------------------
struct Request {
    // command 决定服务端要执行什么；token 决定“谁在请求”；args 携带业务输入。
    std::string command;
    std::string token;
    std::vector<std::string> args;
};

// ---------------------------------------------------------------------------
// Response — 服务端响应结构
// ok：      操作是否成功（true=成功, false=失败）
// code：    结果码（如 SUCCESS / PERMISSION_DENIED / STATE_CONFLICT）
// message： 可读消息（用于客户端展示）
// records： 数据记录列表（表格数据，不含 recordCount，由 encodeResponse 自动添加）
// ---------------------------------------------------------------------------
struct Response {
    // ok 适合程序判断，code 适合分支处理，message 适合人阅读，records 携带结果数据。
    bool ok = false;
    std::string code;
    std::string message;
    std::vector<std::string> records;
};

// ---------------------------------------------------------------------------
// ProtocolError — 协议异常
// 包含错误码和可读消息，由 server/client 的 try-catch 捕获
// ---------------------------------------------------------------------------
class ProtocolError : public std::runtime_error {
public:
    ProtocolError(const std::string& code, const std::string& message);
    const std::string& code() const;

private:
    std::string code_;
};

// ---------------------------------------------------------------------------
// ProtocolCodec — 协议编解码器（纯静态方法）
// ---------------------------------------------------------------------------
class ProtocolCodec {
public:
    // --- 协议安全常量 ---
    static const std::size_t MaxMessageLength = 8192U;   // 单帧最大 8KB
    static const std::size_t MaxFieldLength = 1024U;     // 单字段最大 1KB
    static const std::size_t MaxVectorItems = 256U;      // 参数/记录上限 256 项

    // --- 编码方法：结构化数据 → 协议帧文本 ---
    // 客户端发送前调用 encodeRequest；服务端发送前调用 encodeResponse。
    static std::string encodeRequest(const Request& request);
    static std::string encodeResponse(const Response& response);

    // --- 解码方法：协议帧文本 → 结构化数据 ---
    // 抛出 ProtocolError 当帧格式不合法时
    // 服务端收到请求后调用 decodeRequest；客户端收到响应后调用 decodeResponse。
    static Request decodeRequest(const std::string& raw);
    static Response decodeResponse(const std::string& raw);

private:
    // --- 字段转义 ---
    static std::string escapeField(const std::string& value);
    static std::string unescapeField(const std::string& value);

    // --- 辅助函数 ---
    static std::vector<std::string> splitRawFields(const std::string& line);  // 按 | 切分（不处理转义）
    static std::string normalizeFrame(const std::string& raw);                // 去尾 \n/\r，检查未转义换行
    static std::size_t parseCount(const std::string& value, const std::string& fieldName);  // 解析计数
    static void appendEscapedField(std::string& output, const std::string& value);          // 追加转义字段

    // --- 校验函数 ---
    static void validateFieldLength(const std::string& value, const std::string& fieldName);
    static void validateMessageLength(const std::string& value);
    static void validateCommand(const std::string& command);  // 命令字字符集校验
};

#endif
