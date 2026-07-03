// =============================================================================
// ProtocolCodec.cpp — 协议编解码器实现
// =============================================================================
// 文件用途：实现 REQ/RES 帧的编码、解码、转义和校验逻辑。
// 所属模块：common/protocol
// 
// 日期：2026-06-10（Phase 9 终态版）
//
// 编码流程 (encodeRequest)：
//   1. validateCommand — 校验命令字字符集
//   2. validateFieldLength — 校验 token 长度
//   3. 校验 args.size() <= MaxVectorItems
//   4. 输出 "REQ|command(token转义)|token(转义)|argCount|arg1|arg2|...\n"
//   5. validateMessageLength — 校验整帧长度 <= 8192
//
// 解码流程 (decodeRequest)：
//   1. normalizeFrame — 去尾 \n/\r，拒绝含未转义换行的帧
//   2. splitRawFields — 按 | 切分原始字段（不处理转义）
//   3. 校验 fields[0]=="REQ"，fields.size() >= 4
//   4. 解析 command(转义还原)、token(转义还原)
//   5. 解析 argCount，校验 fields.size() == 4 + argCount
//   6. 逐个 arg 转义还原 + 长度校验
//
// 字段转义算法 (escapeField)：
//   逐字符扫描 → %→%25 |→%7C \n→%0A \r→%0D，其他保留原样
// 反转义算法 (unescapeField)：
//   逐字符扫描 → 遇到 % 时读取后2字符 → 匹配 %25/%7C/%0A/%0D → 还原
//   不完整序列或未知序列 → 抛出 ProtocolError
//
// 关键安全设计：
//   - splitRawFields 在转义还原之前执行，避免用户输入中的 %7C 被误切割
//   - 转义/反转义在字段级别独立进行，不破坏帧结构
//   - 所有数量解析使用 parseCount（逐字符验证数字），不接受负数或非数字字符
// =============================================================================

#include "ProtocolCodec.h"

#include <cctype>
#include <sstream>

// ---------------------------------------------------------------------------
// ProtocolError 实现
// ---------------------------------------------------------------------------
ProtocolError::ProtocolError(const std::string& code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

const std::string& ProtocolError::code() const {
    return code_;
}

// ===========================================================================
// 编码方法
// ===========================================================================

// encodeRequest：将 Request 结构编码为 REQ 帧文本
// 输出格式：REQ|command|token|argCount|arg1|arg2|...\n
std::string ProtocolCodec::encodeRequest(const Request& request) {
    // 编码顺序必须与 decodeRequest 读取顺序完全一致，双方才能解释同一串字节。
    validateCommand(request.command);
    validateFieldLength(request.token, "token");
    if (request.args.size() > MaxVectorItems) {
        throw ProtocolError("INVALID_ARGUMENT", "请求参数数量超过协议限制。");
    }

    std::string output = "REQ";
    // 每追加一个字段，appendEscapedField 都会先写分隔符 '|'.
    appendEscapedField(output, request.command);   // command 字段也经过转义编码
    appendEscapedField(output, request.token);     // token 字段经过转义编码
    appendEscapedField(output, std::to_string(request.args.size()));  // argCount
    for (const std::string& arg : request.args) {
        appendEscapedField(output, arg);           // 每个参数独立转义编码
    }
    output += '\n';
    validateMessageLength(output);
    return output;
}

// encodeResponse：将 Response 结构编码为 RES 帧文本
// 输出格式：RES|1/0|code|message|recordCount|record1|...\n
std::string ProtocolCodec::encodeResponse(const Response& response) {
    validateFieldLength(response.code, "code");
    validateFieldLength(response.message, "message");
    if (response.records.size() > MaxVectorItems) {
        throw ProtocolError("INVALID_ARGUMENT", "响应记录数量超过协议限制。");
    }

    std::string output = "RES";
    appendEscapedField(output, response.ok ? "1" : "0");  // ok 字段：1=成功，0=失败
    appendEscapedField(output, response.code);
    appendEscapedField(output, response.message);
    appendEscapedField(output, std::to_string(response.records.size()));
    for (const std::string& record : response.records) {
        appendEscapedField(output, record);
    }
    output += '\n';
    validateMessageLength(output);
    return output;
}

// ===========================================================================
// 解码方法
// ===========================================================================

// decodeRequest：将 REQ 帧文本解析为 Request 结构
// 抛出 ProtocolError 当帧格式不合法时
Request ProtocolCodec::decodeRequest(const std::string& raw) {
    // raw 此时应当已经是 SocketServer 从 TCP 缓冲区切出的“一整帧”。
    const std::string frame = normalizeFrame(raw);     // 去尾 \n/\r + 检查未转义换行
    const std::vector<std::string> fields = splitRawFields(frame);  // 按 | 切分原始字段
    if (fields.size() < 4U) {
        throw ProtocolError("PROTOCOL_ERROR", "请求字段数量不足。");
    }
    if (fields[0] != "REQ") {
        throw ProtocolError("PROTOCOL_ERROR", "请求帧类型不是 REQ。");
    }

    Request request;
    request.command = unescapeField(fields[1]);  // 转义还原命令字
    request.token = unescapeField(fields[2]);    // 转义还原 token
    validateCommand(request.command);
    validateFieldLength(request.token, "token");

    // 解析 argCount 并验证字段数量一致性
    const std::size_t argCount = parseCount(unescapeField(fields[3]), "argCount");
    if (argCount > MaxVectorItems) {
        throw ProtocolError("INVALID_ARGUMENT", "请求参数数量超过协议限制。");
    }
    if (fields.size() != 4U + argCount) {
        throw ProtocolError("PROTOCOL_ERROR", "请求参数数量与 argCount 不一致。");
    }
    // 逐个还原参数
    for (std::size_t i = 0; i < argCount; ++i) {
        request.args.push_back(unescapeField(fields[4U + i]));
        validateFieldLength(request.args.back(), "arg");
    }
    return request;
}

// decodeResponse：将 RES 帧文本解析为 Response 结构
Response ProtocolCodec::decodeResponse(const std::string& raw) {
    // 客户端使用与服务端对称的流程：先验证结构，再恢复各业务字段。
    const std::string frame = normalizeFrame(raw);
    const std::vector<std::string> fields = splitRawFields(frame);
    if (fields.size() < 5U) {
        throw ProtocolError("PROTOCOL_ERROR", "响应字段数量不足。");
    }
    if (fields[0] != "RES") {
        throw ProtocolError("PROTOCOL_ERROR", "响应帧类型不是 RES。");
    }

    const std::string okText = unescapeField(fields[1]);
    if (okText != "0" && okText != "1") {
        throw ProtocolError("PROTOCOL_ERROR", "响应 ok 字段非法。");
    }

    Response response;
    response.ok = okText == "1";
    response.code = unescapeField(fields[2]);
    response.message = unescapeField(fields[3]);
    validateFieldLength(response.code, "code");
    validateFieldLength(response.message, "message");

    // 解析 recordCount 并验证字段数量一致性
    const std::size_t recordCount = parseCount(unescapeField(fields[4]), "recordCount");
    if (recordCount > MaxVectorItems) {
        throw ProtocolError("INVALID_ARGUMENT", "响应记录数量超过协议限制。");
    }
    if (fields.size() != 5U + recordCount) {
        throw ProtocolError("PROTOCOL_ERROR", "响应记录数量与 recordCount 不一致。");
    }
    for (std::size_t i = 0; i < recordCount; ++i) {
        response.records.push_back(unescapeField(fields[5U + i]));
        validateFieldLength(response.records.back(), "record");
    }
    return response;
}

// ===========================================================================
// 字段转义
// ===========================================================================

// escapeField：将字段值中的特殊字符替换为转义序列
// 规则：% → %25, | → %7C, \n → %0A, \r → %0D
// 目的：防止用户输入中的这些字符破坏 pipe 分隔和换行边界
std::string ProtocolCodec::escapeField(const std::string& value) {
    validateFieldLength(value, "field");
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        if (ch == '%') {
            result += "%25";
        } else if (ch == '|') {
            result += "%7C";
        } else if (ch == '\n') {
            result += "%0A";
        } else if (ch == '\r') {
            result += "%0D";
        } else {
            result += ch;
        }
    }
    return result;
}

// unescapeField：将转义序列还原为原始字符
// 遇到 % 时读取后2字符 → 匹配 %25/%7C/%0A/%0D → 还原
// 不完整（如 "%2"）或未知序列（如 "%FF"）→ 抛出 ProtocolError
// 目的：防止攻击者通过未定义的转义序列绕过校验
std::string ProtocolCodec::unescapeField(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%') {
            if (i + 2U >= value.size()) {
                throw ProtocolError("PROTOCOL_ERROR", "字段转义序列不完整。");
            }
            const std::string code = value.substr(i, 3U);
            if (code == "%25") {
                result += '%';
            } else if (code == "%7C") {
                result += '|';
            } else if (code == "%0A") {
                result += '\n';
            } else if (code == "%0D") {
                result += '\r';
            } else {
                throw ProtocolError("PROTOCOL_ERROR", "字段包含未知转义序列。");
            }
            i += 2U;
        } else {
            result += value[i];
        }
    }
    validateFieldLength(result, "field");
    return result;
}

// ===========================================================================
// 辅助函数
// ===========================================================================

// splitRawFields：按原始 pipe 字符切分（不处理转义）
// 注意：转义还原在 unescapeField 阶段进行，此处只做机械切分
// 这是关键设计 — 先切分再还原，避免 %7C 被当做字段分隔符
std::vector<std::string> ProtocolCodec::splitRawFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    // 循环结束后 current 中仍有最后一个字段，必须显式加入结果。
    fields.push_back(current);
    return fields;
}

// normalizeFrame：标准化帧文本 — 去尾 \n/\r，检查未转义换行
// 帧内的 \n 或 \r 说明存在未经转义的控制字符（协议违规）
std::string ProtocolCodec::normalizeFrame(const std::string& raw) {
    // 长度先于其他解析检查，可尽早拒绝异常大输入。
    validateMessageLength(raw);
    if (raw.empty()) {
        throw ProtocolError("PROTOCOL_ERROR", "空协议帧。");
    }
    std::string frame = raw;
    if (!frame.empty() && frame.back() == '\n') {
        frame.pop_back();
    }
    if (!frame.empty() && frame.back() == '\r') {
        frame.pop_back();
    }
    if (frame.empty()) {
        throw ProtocolError("PROTOCOL_ERROR", "空协议帧。");
    }
    if (frame.find('\n') != std::string::npos || frame.find('\r') != std::string::npos) {
        throw ProtocolError("PROTOCOL_ERROR", "协议帧包含未转义换行。");
    }
    return frame;
}

// parseCount：严格解析非负整数（逐字符验证）
// 拒绝：空字符串、非数字字符、负数、超过 MaxVectorItems
std::size_t ProtocolCodec::parseCount(const std::string& value, const std::string& fieldName) {
    if (value.empty()) {
        throw ProtocolError("PROTOCOL_ERROR", fieldName + " 为空。");
    }
    std::size_t result = 0;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            throw ProtocolError("PROTOCOL_ERROR", fieldName + " 不是非负整数。");
        }
        result = result * 10U + static_cast<std::size_t>(ch - '0');
        if (result > MaxVectorItems) {
            throw ProtocolError("INVALID_ARGUMENT", fieldName + " 超过协议限制。");
        }
    }
    return result;
}

// appendEscapedField：输出 "|" + 转义后的字段值
void ProtocolCodec::appendEscapedField(std::string& output, const std::string& value) {
    output += '|';
    output += escapeField(value);
}

// ===========================================================================
// 校验函数
// ===========================================================================

void ProtocolCodec::validateFieldLength(const std::string& value, const std::string& fieldName) {
    if (value.size() > MaxFieldLength) {
        throw ProtocolError("INVALID_ARGUMENT", fieldName + " 字段超过长度限制。");
    }
}

void ProtocolCodec::validateMessageLength(const std::string& value) {
    if (value.size() > MaxMessageLength) {
        throw ProtocolError("INVALID_ARGUMENT", "协议帧超过 8KB 长度限制。");
    }
}

// validateCommand：命令字只允许 [A-Z]、[0-9]、[_]
// 这是防止命令注入的第一道防线 — 拒绝含小写字母、特殊字符、空格的命令
void ProtocolCodec::validateCommand(const std::string& command) {
    if (command.empty()) {
        throw ProtocolError("PROTOCOL_ERROR", "命令字为空。");
    }
    validateFieldLength(command, "command");
    for (char ch : command) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!std::isupper(value) && !std::isdigit(value) && ch != '_') {
            throw ProtocolError("UNKNOWN_COMMAND", "命令字只能包含大写字母、数字和下划线。");
        }
    }
}
