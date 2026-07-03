#include "StringUtil.h"

#include <cctype>
#include <iomanip>
#include <sstream>

//字符串通用工具
//去除首尾空白。
//按分隔符拆分。
//数据字段转义和反转义。
//金额与数量保留两位小数。
//严格解析 double，拒绝 12abc等输入。

std::string StringUtil::trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::vector<std::string> StringUtil::split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(value);
    while (std::getline(stream, current, delimiter)) {
        parts.push_back(current);
    }
    // getline 不会为末尾分隔符生成最后一个空字段，这里显式补齐以保留字段数。
    if (!value.empty() && value.back() == delimiter) {
        parts.emplace_back();
    }
    return parts;
}

std::string StringUtil::escape(const std::string& value) {
    std::string result;
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

std::string StringUtil::unescape(const std::string& value) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            const std::string code = value.substr(i, 3);
            if (code == "%25") {
                result += '%';
                i += 2;
            } else if (code == "%7C") {
                result += '|';
                i += 2;
            } else if (code == "%0A") {
                result += '\n';
                i += 2;
            } else if (code == "%0D") {
                result += '\r';
                i += 2;
            } else {
                // 未知百分号序列按原文保留，兼容历史数据；协议层使用更严格的解码器。
                result += value[i];
            }
        } else {
            result += value[i];
        }
    }
    return result;
}

std::string StringUtil::formatMoney(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

std::string StringUtil::formatAmount(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}

bool StringUtil::parseDoubleStrict(const std::string& value, double& result) {
    // eof 检查用于拒绝“12abc”这类只解析出前缀数值的输入。
    std::istringstream stream(trim(value));
    stream >> result;
    return !stream.fail() && stream.eof();
}
