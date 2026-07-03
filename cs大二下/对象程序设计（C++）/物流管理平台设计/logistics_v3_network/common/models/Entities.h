// =============================================================================
// Entities.h — 物流管理平台 V3 核心实体类定义
// =============================================================================
// 文件用途：定义物流系统的 6 个核心实体类和快递状态枚举。
// 所属模块：common/models（纯业务模型层，严禁 UI/IO 依赖）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 实体清单与持久化格式：
//   ExpressStatus  — 快递三态枚举（待揽收=0 / 待签收=1 / 已签收=2）
//   User           — 普通用户 | 8字段 pipe分隔 | users.txt
//                    username|name|phone|salt|passwordHash|balance|address|frozen
//   Admin          — 管理员(单例) | 5字段 | admin.txt
//                    username|name|salt|passwordHash|balance
//   Courier        — 快递员 | 8字段 | couriers.txt
//                    username|name|phone|salt|passwordHash|income|frozen|removed
//   Express        — 快递 | 12-15字段(向后兼容) | expresses.txt
//                    id|sender|receiver|courier|sendTime|pickupTime|receiveTime|
//                    status|itemType|itemAmount|description|fee[|note][|ratingScore][|ratingComment]
//   AuthState      — 认证失败状态 | 4字段 | auth_state.txt
//                    role|username|failedCount|lastFailedTime
//   Notification   — 站内通知 | 8字段 | notifications.txt (缺失不阻断启动)
//                    id|time|targetRole|targetUsername|type|title|content|readFlag
//
// 设计红线（不可违反）：
//   - 序列化使用 pipe(|) 分隔 + StringUtil::escape/unescape 转义特殊字符
//   - 反序列化严格校验字段数量和格式，失败返回 false
//   - 金额使用 double + 0.000001 容差（避免浮点精度导致误判）
//   - 密码仅存储 salted SHA-256 哈希值，绝不存储明文
//   - 所有实体为纯数据载体，不依赖网络、文件系统或 UI
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_MODELS_ENTITIES_H
#define LOGISTICS_V3_COMMON_MODELS_ENTITIES_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ExpressStatus — 快递状态枚举
// 状态流转路径（单向不可逆）：
//   WaitingPickup(0) → WaitingSign(1) → Signed(2)
// - WaitingPickup(0)：待揽收 — 快递已寄出但快递员尚未揽收
// - WaitingSign(1)：  待签收 — 快递员已揽收，等待收件人签收
// - Signed(2)：       已签收 — 收件人已签收，物流生命周期终结
// 业务约束：已签收的快递不能回退到之前任何状态
// ---------------------------------------------------------------------------
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};

// ===========================================================================
// User — 普通用户实体
// ===========================================================================
// 持久化格式：username|name|phone|salt|passwordHash|balance|address|frozen
// 字段对应：   [0]       [1]   [2]    [3]       [4]       [5]     [6]    [7]
// 业务角色：USER — 可寄件、签收、充值、评分、修改个人信息
// 冻结机制：frozen_=true 时禁止登录，连续3次密码错误自动冻结
// 资金操作：recharge() 增加余额，deduct() 扣减余额（含 0.000001 容差）
// ===========================================================================
class User {
public:
    User() = default;
    User(const std::string& username, const std::string& name, const std::string& phone,
         const std::string& salt, const std::string& passwordHash, double balance,
         const std::string& address, bool frozen);

    // --- 只读访问器 ---
    const std::string& username() const;
    const std::string& name() const;
    const std::string& phone() const;
    const std::string& salt() const;          // 密码盐值（每用户随机生成）
    const std::string& passwordHash() const;   // SHA-256(salt + password)
    double balance() const;                    // 账户余额（元）
    const std::string& address() const;
    bool frozen() const;                       // 是否被管理员冻结

    // --- 修改器 ---
    void setPasswordHash(const std::string& hash);
    void setSalt(const std::string& salt);
    void setName(const std::string& name);
    void setPhone(const std::string& phone);
    void setAddress(const std::string& address);

    // --- 资金操作（含容差比较） ---
    void recharge(double amount);              // 充值：balance += amount
    bool deduct(double amount);                // 扣款：balance >= amount 时成功

    void setFrozen(bool frozen);               // 设置冻结状态

    // --- 持久化 ---
    std::string serialize() const;             // 序列化为 8字段 pipe分隔行（含转义）
    static bool deserialize(const std::string& line, User& user); // 反序列化，校验失败返回 false

private:
    std::string username_;
    std::string name_;
    std::string phone_;
    std::string salt_;                         // 随机生成的密码盐值
    std::string passwordHash_;                 // salted SHA-256 哈希
    double balance_ = 0.0;
    std::string address_;
    bool frozen_ = false;                      // 0=正常, 1=冻结（禁止登录）
};

// ===========================================================================
// Admin — 管理员实体（全系统唯一实例）
// ===========================================================================
// 持久化格式：username|name|salt|passwordHash|balance
// 业务角色：ADMIN — 管理用户/快递员、分配任务、查看统计、校验日志
// 特殊处理：admin.txt 文件不存在或密码为空时，自动创建默认管理员
//          默认账号 admin / Admin0219（初始化时通过 PasswordHasher 加盐哈希）
// 安全性：管理员不自动冻结（避免系统锁死），但仍记录失败次数
// 资金角色：平台资金池 — 用户寄件时收款，快递员揽收时付款（50%提成）
// ===========================================================================
class Admin {
public:
    Admin();
    Admin(const std::string& username, const std::string& name, const std::string& salt,
          const std::string& passwordHash, double balance);

    const std::string& username() const;
    const std::string& name() const;
    const std::string& salt() const;
    const std::string& passwordHash() const;
    void setPasswordHash(const std::string& hash);
    void setSalt(const std::string& salt);
    double balance() const;                    // 平台资金池余额
    void addBalance(double amount);            // 平台收款（用户寄件时）
    bool deduct(double amount);                // 平台付款（快递员揽收提成时）
    std::string serialize() const;
    static bool deserialize(const std::string& line, Admin& admin);

private:
    std::string username_ = "admin";
    std::string name_ = "SystemAdmin";
    std::string salt_;
    std::string passwordHash_;
    double balance_ = 0.0;
};

// ===========================================================================
// Courier — 快递员实体
// ===========================================================================
// 持久化格式：username|name|phone|salt|passwordHash|income|frozen|removed
// 业务角色：COURIER — 揽收分配给自己的快递，查看任务和绩效
// 双重禁用标记：
//   - frozen_：管理员主动冻结/解冻（可逆）
//   - removed_：管理员停用（不可逆，标记后不再分配新任务）
// 收入：income_ 为累计揽收提成（每次揽收 = 快递费 × 50%）
// ===========================================================================
class Courier {
public:
    Courier() = default;
    Courier(const std::string& username, const std::string& name, const std::string& phone,
            const std::string& salt, const std::string& passwordHash, double income,
            bool frozen, bool removed);

    const std::string& username() const;
    const std::string& name() const;
    const std::string& phone() const;
    const std::string& salt() const;
    const std::string& passwordHash() const;
    void setPasswordHash(const std::string& hash);
    void setSalt(const std::string& salt);
    void setName(const std::string& name);
    void setPhone(const std::string& phone);
    double income() const;                     // 累计收入（揽收提成总和）
    bool frozen() const;                       // 是否被冻结
    bool removed() const;                      // 是否被停用（不可逆）
    void addIncome(double amount);             // 增加收入（揽收时）
    void setFrozen(bool frozen);               // 设置冻结（可逆）
    void markRemoved();                        // 标记停用（不可逆）
    std::string serialize() const;
    static bool deserialize(const std::string& line, Courier& courier);

private:
    std::string username_;
    std::string name_;
    std::string phone_;
    std::string salt_;
    std::string passwordHash_;
    double income_ = 0.0;                     // 累计揽收提成
    bool frozen_ = false;                      // 冻结标记
    bool removed_ = false;                     // 停用标记（不可逆）
};

// ===========================================================================
// Express — 快递实体
// ===========================================================================
// 持久化格式（15字段）：
//   id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|
//   itemType|itemAmount|description|fee|note|ratingScore|ratingComment
// 向后兼容：deserialize 接受 12-15 字段（旧格式无 note/ratingScore/ratingComment）
// 状态流转（由业务方法控制，不可直接修改 status_）：
//   assignCourier → 绑定快递员（不限状态，但业务层仅对 WaitingPickup 调用）
//   pickup → status=WaitingSign, pickupTime=now
//   sign → status=Signed, receiveTime=now
// 权限校验：
//   belongsToUser：sender 或 receiver 匹配
//   belongsToCourier：courier 匹配
// 评分约束：ratingScore_==0 表示未评分，>0 表示已评分（不可重复）
// ===========================================================================
class Express {
public:
    Express() = default;
    Express(const std::string& id, const std::string& sender, const std::string& receiver,
            const std::string& courier, const std::string& sendTime,
            const std::string& pickupTime, const std::string& receiveTime,
            ExpressStatus status, const std::string& itemType, double itemAmount,
            const std::string& description, double fee, const std::string& note = "",
            int ratingScore = 0, const std::string& ratingComment = "");

    // --- 只读访问器 ---
    const std::string& id() const;             // 快递单号（EX000001 格式）
    const std::string& sender() const;         // 发件人用户名
    const std::string& receiver() const;       // 收件人用户名
    const std::string& courier() const;        // 分配的快递员用户名
    const std::string& sendTime() const;
    const std::string& pickupTime() const;
    const std::string& receiveTime() const;
    ExpressStatus status() const;
    const std::string& itemType() const;       // Normal / Fragile / Book
    double itemAmount() const;                 // 重量(kg) 或 册数
    const std::string& description() const;
    const std::string& note() const;           // 备注（发件人或管理员可修改）
    int ratingScore() const;                   // 评分 1-5，0=未评分
    const std::string& ratingComment() const;  // 评价内容
    double fee() const;                        // 快递费用（服务端计算）

    // --- 修改器 ---
    void setNote(const std::string& note);
    void setRating(int score, const std::string& comment);

    // --- 权限与状态判断 ---
    bool belongsToUser(const std::string& username) const;     // sender或receiver匹配
    bool belongsToCourier(const std::string& username) const;  // courier匹配
    bool isWaitingPickup() const;   // status==0
    bool isWaitingSign() const;     // status==1
    bool isSigned() const;          // status==2

    // --- 状态转换（由业务层加锁后调用） ---
    void assignCourier(const std::string& courier);  // 绑定快递员
    void pickup(const std::string& time);            // 揽收：status→WaitingSign
    void sign(const std::string& time);              // 签收：status→Signed

    // --- 展示与持久化 ---
    std::string statusName() const;      // 返回中文状态名（待揽收/待签收/已签收）
    std::string serialize() const;       // 序列化为 15字段
    std::string toRecord() const;        // 生成可读记录（key=value;key=value 格式）
    static bool deserialize(const std::string& line, Express& express); // 12-15字段兼容

private:
    std::string id_;                     // 快递单号 EX000001
    std::string sender_;                 // 发件人
    std::string receiver_;               // 收件人
    std::string courier_;                // 分配的快递员
    std::string sendTime_;
    std::string pickupTime_;
    std::string receiveTime_;
    ExpressStatus status_ = ExpressStatus::WaitingPickup;
    std::string itemType_ = "Normal";    // Normal / Fragile / Book
    double itemAmount_ = 0.0;
    std::string description_;
    std::string note_;
    int ratingScore_ = 0;                // 0=未评分，1-5=已评分
    std::string ratingComment_;
    double fee_ = 0.0;                   // 快递费用（服务端多态计费）
};

// ===========================================================================
// AuthState — 认证失败状态（安全审计）
// ===========================================================================
// 持久化格式：role|username|failedCount|lastFailedTime
// 用途：记录每个账号的连续密码错误次数，实现"3次错误自动冻结"机制
// 冻结规则：
//   USER/COURIER：failedCount >= 3 → 自动设置 frozen=true
//   ADMIN：记录失败次数但不自动冻结（避免系统锁死，系统唯一管理员不可被锁）
// 登录成功后：failedCount 清零
// 日志红线：不在此实体中存储明文密码
// ===========================================================================
class AuthState {
public:
    AuthState() = default;
    AuthState(const std::string& role, const std::string& username,
              int failedCount, const std::string& lastFailedTime);

    const std::string& role() const;           // USER / COURIER / ADMIN
    const std::string& username() const;
    int failedCount() const;                   // 连续失败次数
    const std::string& lastFailedTime() const;
    void recordFailure(const std::string& time);  // 失败次数+1，更新时间
    void clearFailures();                         // 登录成功时清零
    std::string serialize() const;
    static bool deserialize(const std::string& line, AuthState& state);

private:
    std::string role_;
    std::string username_;
    int failedCount_ = 0;
    std::string lastFailedTime_;
};

// ===========================================================================
// Notification — 站内通知实体
// ===========================================================================
// 持久化格式：id|time|targetRole|targetUsername|type|title|content|readFlag
// ID格式：NT000001 递增
// 用途：7个业务触点自动生成通知（冻结/分配/揽收/签收/改派/评分/完成）
// 持久化策略：best-effort — 通知写入失败不回滚核心业务
// 文件缺失行为：notifications.txt 不存在时服务端正常启动
// 通知隔离：按 targetRole + targetUsername 双重过滤，角色间不可见
// ===========================================================================
class Notification {
public:
    Notification() = default;
    Notification(const std::string& id, const std::string& time,
                 const std::string& targetRole, const std::string& targetUsername,
                 const std::string& type, const std::string& title,
                 const std::string& content, bool readFlag);

    const std::string& id() const;
    const std::string& time() const;
    const std::string& targetRole() const;     // USER / COURIER / ADMIN
    const std::string& targetUsername() const; // 目标用户名
    const std::string& type() const;           // 通知类型（如 TASK_ASSIGNED）
    const std::string& title() const;
    const std::string& content() const;
    bool readFlag() const;                     // 0=未读, 1=已读
    void markRead();                           // 标记为已读
    std::string serialize() const;
    static bool deserialize(const std::string& line, Notification& notification);

private:
    std::string id_;                // NT000001 格式
    std::string time_;
    std::string targetRole_;        // USER / COURIER / ADMIN
    std::string targetUsername_;
    std::string type_;              // 如 ACCOUNT_STATUS_CHANGED, TASK_ASSIGNED 等
    std::string title_;
    std::string content_;
    bool readFlag_ = false;         // false=未读, true=已读
};

#endif
