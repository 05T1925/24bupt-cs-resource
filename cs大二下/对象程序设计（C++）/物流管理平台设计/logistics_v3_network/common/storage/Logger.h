// =============================================================================
// Logger.h - 可校验的操作审计日志
// =============================================================================
// 与 Repository 的覆盖保存不同，Logger 以追加方式写 operations.log。
// 每条记录包含 prevHash/currentHash，任何历史行被修改都会使后续链校验失败。
// Logger 记录行为轨迹，不承担余额等业务事实的主存储职责。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_STORAGE_LOGGER_H
#define LOGISTICS_V3_COMMON_STORAGE_LOGGER_H

#include <atomic>
#include <string>
#include <vector>

class Logger {
public:
    explicit Logger(const std::string& filePath);
    // 追加九字段审计记录，并把上一条记录的哈希纳入本条哈希。
    bool write(const std::string& actorType, const std::string& actor,
               const std::string& action, const std::string& result,
               const std::string& detail) const;
    // 从 GENESIS 开始重算整条链，message 返回适合界面展示的校验结论。
    bool verifyHashChain(std::string& message) const;
    // 空筛选条件表示不过滤；返回值移除内部哈希字段，供界面展示。
    std::vector<std::string> queryLogs(const std::string& actorType, const std::string& actor,
                                       const std::string& action, const std::string& result) const;

private:
    // 日志链的“读取链尾—计算哈希—追加写入”必须作为一个不可分割步骤，
    // 否则多个客户端并发记录时可能取得相同序号和 prevHash。
    class LockGuard {
    public:
        explicit LockGuard(const Logger& logger);
        ~LockGuard();
        LockGuard(const LockGuard&) = delete;
        LockGuard& operator=(const LockGuard&) = delete;

    private:
        const Logger& logger_;
    };

    std::string filePath_;
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;

    // 从末行恢复下一个序号和链头；空日志从 1/GENESIS 开始。
    bool readLastChainState(int& nextSeq, std::string& prevHash) const;
    // 统一生成日志时间，避免调用方自行传入不一致格式。
    static std::string nowText();
    // 对八个逻辑字段计算摘要；currentHash 本身不参与本条哈希。
    static std::string calculateHash(int seq, const std::string& time,
                                     const std::string& actorType, const std::string& actor,
                                     const std::string& action, const std::string& result,
                                     const std::string& detail, const std::string& prevHash);
};

#endif
