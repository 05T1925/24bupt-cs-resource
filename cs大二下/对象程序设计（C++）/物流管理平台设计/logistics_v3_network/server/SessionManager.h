// =============================================================================
// SessionManager.h — 会话生命周期管理器
// =============================================================================
// 文件用途：管理客户端登录后的 token 会话（创建/验证/刷新/销毁）。
// 所属模块：server（服务端会话管理，独立于业务锁的 SessionMutex 保护）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// Token 生成算法：
//   SHA-256(username | role | high_resolution_clock ticks |
//           random_device 随机数A | random_device 随机数B | this 指针)
//   熵源：时间戳（纳秒级） + 两个独立硬件随机数 + 指针地址
//   输出：64 位十六进制字符串
//
// Session 结构：
//   token / username / role / loginTime / lastActiveTime
//
// 并发保护：
//   SessionMutex/SessionLock — 独立的 atomic_flag 自旋锁
//   与业务锁 CoreMutex 分离：session 操作不阻塞业务，业务不阻塞 session
//
// 生命周期：
//   创建：login 成功时 → createSession
//   使用：每次请求 → getSession 解析身份
//   刷新：每次请求 → touch 更新 lastActiveTime
//   销毁：LOGOUT / 客户端断线 → removeSession / cleanupClientSessions
//
// Token 与 TCP 连接的关系：
//   - TCP 连接解决“字节从哪条通道传输”
//   - Token 解决“这次请求代表哪个登录用户”
//   - 二者概念不同，因此每个需要鉴权的 Request 都显式携带 Token
//   - 当前服务端会记录连接见过的 Token，在连接断开时进行清理
//
// 安全特性：
//   - hasActiveSession：重复登录拦截（同 username+role 只能有一个活跃 session）
//   - getSessionSummary：安全日志用摘要，不暴露完整 token
//   - 日志中不记录完整 token 和明文密码
// =============================================================================

#ifndef LOGISTICS_V3_SERVER_SESSION_MANAGER_H
#define LOGISTICS_V3_SERVER_SESSION_MANAGER_H

#include <atomic>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// SessionMutex — Session 表互斥锁（atomic_flag 自旋锁）
// ---------------------------------------------------------------------------
class SessionMutex {
public:
    void lock() const;
    void unlock() const;

private:
    mutable std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

// ---------------------------------------------------------------------------
// SessionLock — RAII 锁守卫（构造时 lock，析构时 unlock）
// ---------------------------------------------------------------------------
class SessionLock {
public:
    explicit SessionLock(const SessionMutex& mutex);
    ~SessionLock();
    SessionLock(const SessionLock&) = delete;
    SessionLock& operator=(const SessionLock&) = delete;

private:
    const SessionMutex& mutex_;
};

// ---------------------------------------------------------------------------
// Session — 会话信息
// ---------------------------------------------------------------------------
struct Session {
    std::string token;           // 客户端后续请求携带的随机化令牌
    std::string username;        // 登录用户名
    std::string role;            // USER / COURIER / ADMIN
    std::string loginTime;       // 登录时间
    std::string lastActiveTime;  // 最后活跃时间（每次请求刷新）
};

// ---------------------------------------------------------------------------
// SessionManager — 会话管理器
// ---------------------------------------------------------------------------
class SessionManager {
public:
    // createSession：登录成功后创建会话，返回 SHA-256 token
    std::string createSession(const std::string& username, const std::string& role);

    // getSession：根据 token 查找会话（用于权限校验和身份解析）
    // 返回 false 当 token 不存在时
    bool getSession(const std::string& token, Session& session) const;

    // touch：刷新会话的最后活跃时间（每次业务请求时调用）
    bool touch(const std::string& token);

    // removeSession：销毁指定 token 的会话
    void removeSession(const std::string& token);

    // getSessionSummary：安全日志用摘要（格式 "role|username" 或 "GUEST|"）
    // 不暴露完整 token，用于 SocketServer::logRequestSummary
    std::string getSessionSummary(const std::string& token) const;

    // hasActiveSession：检查 username+role 是否已持有活跃会话
    // 用于重复登录拦截（ALREADY_LOGGED_IN）
    bool hasActiveSession(const std::string& username, const std::string& role) const;

    // --- 会话统计（运维展示用） ---
    int totalSessionCount() const;                     // 总会话数
    int sessionCountByRole(const std::string& role) const; // 按角色统计
    int countActiveSessions() const;                    // 活跃会话数（同 totalSessionCount）

private:
    mutable SessionMutex mutex_;                       // 独立于业务锁的 Session 锁
    std::unordered_map<std::string, Session> sessions_; // token → Session 映射

    // generateToken：生成 SHA-256 随机 token（含时间戳+随机数+指针熵）
    std::string generateToken(const std::string& username, const std::string& role) const;
    static std::string nowText();  // 当前时间格式化
};

#endif
