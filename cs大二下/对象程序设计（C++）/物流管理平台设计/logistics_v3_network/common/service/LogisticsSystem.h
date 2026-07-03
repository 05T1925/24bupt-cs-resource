// =============================================================================
// LogisticsSystem.h — 物流业务核心服务
// =============================================================================
// 文件用途：定义物流管理平台全部业务方法和并发保护机制。
// 所属模块：common/service（业务服务层，被 server 独占调用）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 业务方法分类（46 个 total）：
//   [初始化]    initialize
//   [游客]      registerUser, loginUser, loginCourier, loginAdmin
//   [用户]      queryBalance, recharge, sendExpress, queryMyExpress,
//               queryWaitingSign, signExpress, signBatch, changePassword,
//               updateMyProfile, updateExpressNote, rateExpress
//   [快递员]    queryMyPickupTasks, pickupExpress, pickupBatch,
//               queryMyTasks, viewMyPerformance, changePassword, updateCourierProfile
//   [管理员]    createCourier, queryUsers, queryCouriers, removeCourier,
//               setUserFrozen, setCourierFrozen, queryAllExpress,
//               assignCourier, autoAssignCourier, autoAssignAll,
//               viewDashboard, viewCourierPerformance, queryLogs, verifyLogChain,
//               adminUpdateUser, adminUpdateCourier, reassignCourier, updateExpressNote
//   [通知]      queryMyNotifications, markNotificationRead, countUnreadNotifications
//   [安全审计]  auditSecurityEvent
//   [工具]      checkUsernameAvailable, checkPhoneAvailable
//
// 并发保护机制（CoreMutex/CoreLock）：
//   - CoreMutex：基于 std::atomic_flag 的自旋锁（MinGW std::mutex 兼容性方案）
//   - CoreLock：RAII 风格的锁守卫（构造时 lock，析构时 unlock）
//   - 保护范围：所有 LogisticsSystem 公共方法入口
//   - 策略：读写均加锁（保守策略，保证读到的是一致性快照）
//   - 红线：不要轻易替换为标准库 mutex，需确认工具链编译通过
//
// 资金流转模型：
//   寄件：user.balance -= fee → admin.balance += fee（100% 归平台）
//   揽收：admin.balance -= fee*0.5 → courier.income += fee*0.5（50% 提成）
//   签收：无资金变化（仅状态变更）
//
// 事务回滚策略：
//   - 修改型方法在执行前保存旧对象快照
//   - saveAll() 失败时恢复旧快照到内存
//   - 快递创建失败时 pop_back() 回滚
//   - 通知写入为 best-effort，失败不影响核心业务
//
// 设计红线（不可违反）：
//   - 不信任客户端传来的当前用户名（身份从 token 解析）
//   - 费用由服务端多态计费（客户端只提交物品类型和数量）
//   - 明文密码和完整 token 不入日志
//   - 所有状态变更必须在 CoreLock 内完成
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SERVICE_LOGISTICS_SYSTEM_H
#define LOGISTICS_V3_COMMON_SERVICE_LOGISTICS_SYSTEM_H

#include "../storage/ServiceResult.h"
#include "../models/Entities.h"
#include "../storage/Logger.h"
#include "../storage/Repositories.h"

#include <atomic>
#include <string>
#include <vector>

// ===========================================================================
// CoreMutex — 全局业务互斥锁（基于 std::atomic_flag 自旋锁）
// 用途：保护 LogisticsSystem 的全部业务入口，防止并发状态冲突。
// 背景：当前 MinGW 环境 std::mutex/std::shared_mutex 编译不稳定，
//       因此使用 atomic_flag 自旋锁作为兼容性替代方案。
// 注意：自旋锁在竞争激烈时消耗 CPU，适合当前单 server 低并发场景。
//       如迁移到现代工具链并确认编译通过后，可考虑替换为 std::mutex。
// ===========================================================================
class CoreMutex {
public:
    CoreMutex() = default;
    CoreMutex(const CoreMutex&) = delete;            // 禁止拷贝
    CoreMutex& operator=(const CoreMutex&) = delete; // 禁止赋值
    void lock() const;    // 自旋等待直到获取锁（memory_order_acquire）
    void unlock() const;  // 释放锁（memory_order_release）

private:
    mutable std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

// ===========================================================================
// CoreLock — RAII 锁守卫（构造时 lock，析构时 unlock）
// 用法：CoreLock lock(mutex_);  // 作用域结束时自动释放
// ===========================================================================
class CoreLock {
public:
    explicit CoreLock(const CoreMutex& mutex);
    ~CoreLock();
    CoreLock(const CoreLock&) = delete;
    CoreLock& operator=(const CoreLock&) = delete;

private:
    const CoreMutex& mutex_;
};

// ===========================================================================
// LogisticsSystem — 物流业务核心服务类
// ===========================================================================
class LogisticsSystem {
public:
    // 构造函数：绑定数据目录，初始化 6 个 Repository 和 Logger
    explicit LogisticsSystem(const std::string& dataDirectory);

    // =====================================================================
    // [初始化]
    // =====================================================================

    // initialize：加载数据文件，如 admin 无密码则创建默认管理员 admin/Admin0219
    // 锁：CoreLock（初始化阶段单线程调用，但为一致性保留锁）
    // 失败码：STORAGE_FAILED
    ServiceResult initialize();

    // =====================================================================
    // [登录] — GUEST 命令
    // =====================================================================

    // loginUser：普通用户登录
    // 校验：用户存在 → 非冻结 → 密码哈希匹配 → 3次错误自动冻结
    // 成功返回：data = ["USER", username]
    ServiceResult loginUser(const std::string& username, const std::string& password);

    // loginCourier：快递员登录
    // 校验：存在 → 非 frozen/removed → 密码哈希匹配 → 3次错误自动冻结
    ServiceResult loginCourier(const std::string& username, const std::string& password);

    // loginAdmin：管理员登录（不自动冻结，避免系统锁死）
    ServiceResult loginAdmin(const std::string& username, const std::string& password);

    // =====================================================================
    // [注册与校验] — GUEST 命令
    // =====================================================================

    // registerUser：注册普通用户
    // 校验：username/phone/password/name/address 格式 + 用户名查重
    // 事务：创建失败时 pop_back() 回滚
    // 失败码：INVALID_ARGUMENT / DUPLICATE / STORAGE_FAILED
    ServiceResult registerUser(const std::string& username, const std::string& name,
                               const std::string& phone, const std::string& password,
                               const std::string& address);

    // checkUsernameAvailable：用户名查重（跨 USER/COURIER/ADMIN）
    ServiceResult checkUsernameAvailable(const std::string& username);

    // checkPhoneAvailable：手机号格式校验（仅校验格式，不校验唯一性）
    ServiceResult checkPhoneAvailable(const std::string& phone);

    // =====================================================================
    // [查询] — 只读方法
    // =====================================================================

    ServiceResult queryUsers() const;                          // 管理员：查询全部用户
    ServiceResult queryBalance(const std::string& username) const;  // 用户：查询余额
    ServiceResult queryCouriers() const;                       // 管理员：查询全部快递员
    ServiceResult queryUserExpresses(const std::string& username) const;       // 用户：查询我的快递
    ServiceResult queryWaitingSignExpresses(const std::string& username) const; // 用户：查询待签收
    ServiceResult queryCourierPickupTasks(const std::string& courierUsername) const;  // 快递员：待揽收任务
    ServiceResult queryCourierExpresses(const std::string& courierUsername) const;    // 快递员：全部任务
    ServiceResult queryAllExpresses() const;                    // 管理员：全部快递
    ServiceResult queryLogs(const std::string& actorType, const std::string& actor,
                           const std::string& action, const std::string& result) const; // 管理员：日志查询
    ServiceResult verifyLogHashChain() const;                   // 管理员：日志哈希链校验

    // =====================================================================
    // [资金] — USER 命令（CoreLock 保护 + saveAll 失败回滚）
    // =====================================================================

    // recharge：用户充值 amount 元
    ServiceResult recharge(const std::string& username, double amount);

    // sendExpress：寄件（服务端多态计费）
    // 参数：sender(来自token), receiver, description, itemType, itemAmount [, note]
    // 计费：ExpressItemFactory::createItem → getPrice()
    // 资金：sender.balance -= fee, admin.balance += fee
    // 返回：data=[expressId, fee, remainingBalance]
    // 余额不足：data=[balance=..., fee=..., shortage=...]（供客户端即时充值）
    ServiceResult sendExpress(const std::string& sender, const std::string& receiver,
                              const std::string& description, const std::string& itemType,
                              double itemAmount, const std::string& note = "");

    // =====================================================================
    // [管理员] — ADMIN 命令
    // =====================================================================

    ServiceResult createCourier(const std::string& username, const std::string& name,
                                const std::string& phone, const std::string& password);
    ServiceResult removeCourier(const std::string& username);          // 停用快递员（有未完成任务返回冲突清单）
    ServiceResult setUserFrozen(const std::string& username, bool frozen);     // 冻结/解冻用户
    ServiceResult setCourierFrozen(const std::string& username, bool frozen);  // 冻结/解冻快递员

    // assignCourier：手动分配快递员（仅 WaitingPickup）
    ServiceResult assignCourier(const std::string& expressId, const std::string& courierUsername);

    // autoAssignCourier：自动分配单条（selectBestCourierIndex 三优先级排序）
    ServiceResult autoAssignCourier(const std::string& expressId);

    // autoAssignAllWaitingPickup：一键分配全部未分配待揽收快递
    ServiceResult autoAssignAllWaitingPickup();

    // 管理员统计与绩效
    ServiceResult viewDashboard() const;                  // 统计看板（8项指标）
    ServiceResult viewCourierPerformance() const;         // 全部快递员绩效排行
    ServiceResult viewMyCourierPerformance(const std::string& courierUsername) const;

    // 管理员修改用户/快递员信息
    ServiceResult adminUpdateUser(const std::string& targetUsername, const std::string& name,
                                  const std::string& phone, const std::string& address, int frozen);
    ServiceResult adminUpdateCourier(const std::string& targetUsername, const std::string& name,
                                     const std::string& phone, int frozen);

    // reassignCourier：改派快递（仅 WaitingPickup，发双向通知）
    ServiceResult reassignCourier(const std::string& expressId, const std::string& newCourierUsername,
                                  const std::string& reason);

    // updateExpressNote（管理员版）：修改快递备注
    ServiceResult updateExpressNote(const std::string& username, const std::string& role,
                                    const std::string& expressId, const std::string& note);

    // =====================================================================
    // [快递员] — COURIER 命令（CoreLock 保护 + 事务回滚）
    // =====================================================================

    // pickupExpress：揽收单个快递（最关键的原子事务）
    // 事务步骤（12步，全部在 CoreLock 内）：
    //   1. 校验 expressId 存在
    //   2. 校验 courier 存在、非 frozen、非 removed
    //   3. 校验 express.courier == courierUsername（权限红线）
    //   4. 校验 express.status == WaitingPickup（状态红线）
    //   5. commission = fee * 0.5
    //   6. admin.deduct(commission)（平台付款）
    //   7. courier.addIncome(commission)（快递员收款）
    //   8. express.pickup(now)（状态变更）
    //   9. addNotification → receiver（EXPRESS_PICKED_UP）
    //   10. saveAll 5 文件 + 通知独立保存
    //   11. 失败 → 恢复 oldExpress/oldCourier/oldAdmin 快照
    //   12. write log
    // 越权/状态冲突：写入 COURIER_PICKUP_PERMISSION_DENIED / COURIER_PICKUP_STATE_CONFLICT 审计日志
    ServiceResult pickupExpress(const std::string& expressId, const std::string& courierUsername);

    // pickupBatchExpresses：批量揽收（逐单调用 pickupExpress，返回逐单结果）
    ServiceResult pickupBatchExpresses(const std::vector<std::string>& expressIds, const std::string& courierUsername);

    // =====================================================================
    // [签收] — USER 命令
    // =====================================================================

    // signExpress：签收快递
    // 校验：收件人匹配 → 非 WaitingPickup → 非重复签收
    ServiceResult signExpress(const std::string& username, const std::string& expressId);
    ServiceResult signBatchExpresses(const std::vector<std::string>& expressIds, const std::string& username);

    // =====================================================================
    // [密码] — 三身份通用
    // =====================================================================

    ServiceResult changePassword(const std::string& username, const std::string& oldPassword,
                                 const std::string& newPassword);
    ServiceResult changeCourierPassword(const std::string& username, const std::string& oldPassword,
                                        const std::string& newPassword);
    ServiceResult changeAdminPassword(const std::string& username, const std::string& oldPassword,
                                      const std::string& newPassword);

    // =====================================================================
    // [个人信息] — USER/COURIER 自助修改
    // =====================================================================

    ServiceResult updateMyProfile(const std::string& username, const std::string& name,
                                   const std::string& phone, const std::string& address);
    ServiceResult updateCourierProfile(const std::string& username, const std::string& name,
                                       const std::string& phone);

    // =====================================================================
    // [评分] — USER 命令
    // =====================================================================

    // rateExpress：收件人评分（1-5分，仅已签收，不可重复）
    ServiceResult rateExpress(const std::string& username, const std::string& expressId,
                              int score, const std::string& comment);

    // =====================================================================
    // [通知中心] — 三身份通用
    // =====================================================================

    // addNotification：写入通知（调用者必须持有 CoreLock，best-effort 策略）
    ServiceResult addNotification(const std::string& targetRole, const std::string& targetUsername,
                                  const std::string& type, const std::string& title,
                                  const std::string& content);
    ServiceResult queryMyNotifications(const std::string& role, const std::string& username,
                                       bool unreadOnly) const;
    ServiceResult markNotificationRead(const std::string& role, const std::string& username,
                                       const std::string& notificationId);
    ServiceResult countUnreadNotifications(const std::string& role, const std::string& username) const;

    // =====================================================================
    // [安全审计] — 越权记录
    // =====================================================================

    // auditSecurityEvent：写入安全审计日志（在 ServerController 权限校验失败时调用）
    // 当前审计事件：ADMIN_PERMISSION_DENIED / COURIER_PERMISSION_DENIED
    ServiceResult auditSecurityEvent(const std::string& actorType, const std::string& actor,
                                     const std::string& action, const std::string& detail);

private:
    // --- 数据路径 ---
    std::string dataDirectory_;

    // --- 持久化仓库 ---
    UserRepository userRepository_;
    AdminRepository adminRepository_;
    CourierRepository courierRepository_;
    ExpressRepository expressRepository_;
    AuthStateRepository authStateRepository_;
    NotificationRepository notificationRepository_;

    // --- 日志 ---
    Logger logger_;

    // --- 内存数据 ---
    std::vector<User> users_;               // 全部用户
    Admin admin_;                            // 管理员（单例）
    std::vector<Courier> couriers_;          // 全部快递员
    std::vector<Express> expresses_;         // 全部快递
    std::vector<AuthState> authStates_;      // 认证失败状态
    std::vector<Notification> notifications_; // 全部通知

    // --- 全局业务锁 ---
    mutable CoreMutex mutex_;

    // --- 内部辅助 ---
    std::size_t findUserIndex(const std::string& username) const;
    std::size_t findCourierIndex(const std::string& username) const;
    std::size_t findExpressIndex(const std::string& expressId) const;
    std::size_t findAuthStateIndex(const std::string& role, const std::string& username) const;

    int unfinishedCountForCourier(const std::string& username) const;  // 未完成任务数（WaitingPickup + WaitingSign）
    int signedCountForCourier(const std::string& username) const;      // 已完成任务数

    // selectBestCourierIndex：自动分配调度算法
    // 三优先级：1.未完成任务少 → 2.收入低 → 3.username 字典序
    std::size_t selectBestCourierIndex() const;

    std::string courierPerformanceRecord(const Courier& courier) const;  // 绩效记录格式化

    // 认证失败管理
    AuthState& ensureAuthState(const std::string& role, const std::string& username);
    ServiceResult handleAuthFailure(const std::string& role, const std::string& username,
                                    const std::string& action, bool freezeAllowed);
    void clearAuthFailure(const std::string& role, const std::string& username);

    // saveAll：串行保存 5 文件（user/admin/courier/express/auth_state）
    // && 短路：第一个失败阻止后续保存
    bool saveAll();

    // ID 生成
    std::string generateExpressId() const;        // EX000001 格式（基于 expresses_.size()）
    std::string generateNotificationId() const;    // NT000001 格式（基于 notifications_.size()）

    static std::string nowText();                  // 当前时间格式化（YYYY-MM-DD HH:MM:SS）
};

#endif
