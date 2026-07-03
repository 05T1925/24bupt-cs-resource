#include "SessionManager.h"

#include "../common/security/HashUtil.h"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

void SessionMutex::lock() const {
    // acquire/release 配对保证进入临界区后能看到其他线程已提交的会话修改。
    while (flag_.test_and_set(std::memory_order_acquire)) {
    }
}

void SessionMutex::unlock() const {
    flag_.clear(std::memory_order_release);
}

SessionLock::SessionLock(const SessionMutex& mutex) : mutex_(mutex) {
    mutex_.lock();
}

SessionLock::~SessionLock() {
    mutex_.unlock();
}

std::string SessionManager::createSession(const std::string& username, const std::string& role) {
    SessionLock lock(mutex_);
    // Token 是 sessions_ 的唯一键；登录时间和最近活跃时间在创建时保持一致。
    const std::string token = generateToken(username, role);
    const std::string now = nowText();
    Session session;
    session.token = token;
    session.username = username;
    session.role = role;
    session.loginTime = now;
    session.lastActiveTime = now;
    sessions_[token] = session;
    return token;
}

bool SessionManager::getSession(const std::string& token, Session& session) const {
    SessionLock lock(mutex_);
    const std::unordered_map<std::string, Session>::const_iterator iter = sessions_.find(token);
    if (iter == sessions_.end()) {
        return false;
    }
    // 返回副本，调用方不会在锁外直接修改 sessions_ 中的对象。
    session = iter->second;
    return true;
}

bool SessionManager::touch(const std::string& token) {
    SessionLock lock(mutex_);
    std::unordered_map<std::string, Session>::iterator iter = sessions_.find(token);
    if (iter == sessions_.end()) {
        return false;
    }
    iter->second.lastActiveTime = nowText();
    return true;
}

void SessionManager::removeSession(const std::string& token) {
    SessionLock lock(mutex_);
    sessions_.erase(token);
}

std::string SessionManager::getSessionSummary(const std::string& token) const {
    if (token.empty()) {
        return "GUEST|";
    }
    SessionLock lock(mutex_);
    const std::unordered_map<std::string, Session>::const_iterator iter = sessions_.find(token);
    if (iter == sessions_.end()) {
        return "GUEST|";
    }
    // 摘要只含身份和账号，服务端请求日志不会泄露完整 Token。
    return iter->second.role + "|" + iter->second.username;
}

int SessionManager::totalSessionCount() const {
    SessionLock lock(mutex_);
    return static_cast<int>(sessions_.size());
}

bool SessionManager::hasActiveSession(const std::string& username, const std::string& role) const {
    SessionLock lock(mutex_);
    // 当前会话量较小，采用 O(n) 扫描即可完成 username+role 维度的重复登录检查。
    for (const auto& pair : sessions_) {
        if (pair.second.username == username && pair.second.role == role) {
            return true;
        }
    }
    return false;
}

int SessionManager::sessionCountByRole(const std::string& role) const {
    SessionLock lock(mutex_);
    int count = 0;
    for (const auto& pair : sessions_) {
        if (pair.second.role == role) {
            ++count;
        }
    }
    return count;
}

int SessionManager::countActiveSessions() const {
    SessionLock lock(mutex_);
    return static_cast<int>(sessions_.size());
}

std::string SessionManager::generateToken(const std::string& username, const std::string& role) const {
    // 时间、两份随机数和对象地址共同参与种子，账号信息仅用于区分会话上下文。
    std::random_device randomDevice;
    std::mt19937_64 generator(randomDevice());
    std::uniform_int_distribution<unsigned long long> distribution;
    const unsigned long long randomA = distribution(generator);
    const unsigned long long randomB = distribution(generator);
    const long long ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    std::ostringstream seed;
    seed << username << '|' << role << '|' << ticks << '|'
         << randomA << '|' << randomB << '|' << this;
    // 对复合种子做 SHA-256，网络上传输固定长度摘要而非原始熵源。
    return HashUtil::sha256(seed.str());
}

std::string SessionManager::nowText() {
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
