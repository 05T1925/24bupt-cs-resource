#include "Logger.h"

#include "../security/HashUtil.h"
#include "../security/StringUtil.h"
#include "StorageManager.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

//操作审计日志

Logger::Logger(const std::string& filePath) : filePath_(filePath) {}

Logger::LockGuard::LockGuard(const Logger& logger) : logger_(logger) {
    while (logger_.lock_.test_and_set(std::memory_order_acquire)) {
    }
}

Logger::LockGuard::~LockGuard() {
    logger_.lock_.clear(std::memory_order_release);
}

bool Logger::write(const std::string& actorType, const std::string& actor,
                   const std::string& action, const std::string& result,
                   const std::string& detail) const {
    LockGuard lock(*this);
    int seq = 1;
    std::string prevHash = "GENESIS";
    readLastChainState(seq, prevHash);
    const std::string time = nowText();
    // 哈希基于未转义的逻辑字段计算，落盘时再转义文本分隔符。
    const std::string currentHash = calculateHash(seq, time, actorType, actor, action, result, detail, prevHash);

    const std::size_t slash = filePath_.find_last_of("/\\");
    if (slash != std::string::npos && !StorageManager::ensureDirectory(filePath_.substr(0, slash))) {
        return false;
    }
    std::ofstream output(filePath_, std::ios::app);
    if (!output.is_open()) {
        return false;
    }
    // 固定字段顺序：seq、time、actorType、actor、action、result、detail、prevHash、currentHash。
    output << seq << '|'
           << StringUtil::escape(time) << '|'
           << StringUtil::escape(actorType) << '|'
           << StringUtil::escape(actor) << '|'
           << StringUtil::escape(action) << '|'
           << StringUtil::escape(result) << '|'
           << StringUtil::escape(detail) << '|'
           << prevHash << '|'
           << currentHash << '\n';
    return output.good();
}

bool Logger::verifyHashChain(std::string& message) const {
    LockGuard lock(*this);
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        message = "日志读取失败。";
        return false;
    }
    std::string expectedPrev = "GENESIS";
    int expectedSeq = 1;
    // 同时验证序号连续、前向哈希引用和当前内容哈希，任一失败即判定链损坏。
    for (const std::string& line : lines) {
        const std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 9U) {
            message = "日志格式错误。";
            return false;
        }
        int seq = 0;
        std::istringstream seqStream(parts[0]);
        seqStream >> seq;
        if (seq != expectedSeq) {
            message = "日志序号断裂。";
            return false;
        }
        const std::string time = StringUtil::unescape(parts[1]);
        const std::string actorType = StringUtil::unescape(parts[2]);
        const std::string actor = StringUtil::unescape(parts[3]);
        const std::string action = StringUtil::unescape(parts[4]);
        const std::string result = StringUtil::unescape(parts[5]);
        const std::string detail = StringUtil::unescape(parts[6]);
        const std::string prevHash = parts[7];
        const std::string currentHash = parts[8];
        if (prevHash != expectedPrev) {
            message = "日志 prevHash 断裂。";
            return false;
        }
        if (calculateHash(seq, time, actorType, actor, action, result, detail, prevHash) != currentHash) {
            message = "日志 currentHash 校验失败。";
            return false;
        }
        expectedPrev = currentHash;
        ++expectedSeq;
    }
    message = "日志哈希链完整。";
    return true;
}

bool Logger::readLastChainState(int& nextSeq, std::string& prevHash) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines) || lines.empty()) {
        nextSeq = 1;
        prevHash = "GENESIS";
        return false;
    }
    // 这里只读取链尾以支持追加；完整性检查由 verifyHashChain 显式执行。
    const std::vector<std::string> parts = StringUtil::split(lines.back(), '|');
    if (parts.size() != 9U) {
        nextSeq = 1;
        prevHash = "GENESIS";
        return false;
    }
    int seq = 0;
    std::istringstream stream(parts[0]);
    stream >> seq;
    nextSeq = seq + 1;
    prevHash = parts[8];
    return true;
}

std::vector<std::string> Logger::queryLogs(const std::string& actorType, const std::string& actor,
                                            const std::string& action, const std::string& result) const {
    LockGuard lock(*this);
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return std::vector<std::string>();
    }
    std::vector<std::string> records;
    // 查询结果不暴露 prevHash/currentHash，避免界面层依赖审计存储细节。
    for (const std::string& line : lines) {
        const std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 9U) {
            continue;
        }
        const std::string logType = StringUtil::unescape(parts[2]);
        const std::string logActor = StringUtil::unescape(parts[3]);
        const std::string logAction = StringUtil::unescape(parts[4]);
        const std::string logResult = StringUtil::unescape(parts[5]);
        if (!actorType.empty() && logType != actorType) {
            continue;
        }
        if (!actor.empty() && logActor != actor) {
            continue;
        }
        if (!action.empty() && logAction != action) {
            continue;
        }
        if (!result.empty() && logResult != result) {
            continue;
        }
        std::ostringstream record;
        record << StringUtil::unescape(parts[0]) << "|"
               << StringUtil::unescape(parts[1]) << "|"
               << logType << "|" << logActor << "|"
               << logAction << "|" << logResult << "|"
               << StringUtil::unescape(parts[6]);
        records.push_back(record.str());
    }
    return records;
}

std::string Logger::nowText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t current = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &current);
#else
    localtime_r(&current, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string Logger::calculateHash(int seq, const std::string& time,
                                  const std::string& actorType, const std::string& actor,
                                  const std::string& action, const std::string& result,
                                  const std::string& detail, const std::string& prevHash) {
    // 字段顺序必须与写入和校验保持一致，否则相同记录也会产生不同摘要。
    std::ostringstream stream;
    stream << seq << '|' << time << '|' << actorType << '|' << actor << '|'
           << action << '|' << result << '|' << detail << '|' << prevHash;
    return HashUtil::sha256(stream.str());
}
