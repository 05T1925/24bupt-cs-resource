// =============================================================================
// StringUtil.h - 跨模型、存储和界面的字符串基础工具
// =============================================================================
// Entities/Logger 使用 escape、split；LogisticsSystem/ClientApp 使用金额格式化
// 和严格数值解析。网络协议另由 ProtocolCodec 负责更严格的转义错误检查。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SECURITY_STRING_UTIL_H
#define LOGISTICS_V3_COMMON_SECURITY_STRING_UTIL_H

#include <string>
#include <vector>

class StringUtil {
public:
    // 去除首尾空白，不改变字符串中间内容。
    static std::string trim(const std::string& value);
    // 保留末尾空字段，适合解析固定字段数量的数据文件。
    static std::vector<std::string> split(const std::string& value, char delimiter);
    // 数据文件字段转义：保护百分号、竖线和换行符，不等同于网络帧校验。
    static std::string escape(const std::string& value);
    static std::string unescape(const std::string& value);
    // 金额与数量当前均保留两位小数，分开命名表达业务语义。
    static std::string formatMoney(double value);
    static std::string formatAmount(double value);
    // 只有整个去空白字符串均可解析为 double 时才返回成功。
    static bool parseDoubleStrict(const std::string& value, double& result);
};

#endif
