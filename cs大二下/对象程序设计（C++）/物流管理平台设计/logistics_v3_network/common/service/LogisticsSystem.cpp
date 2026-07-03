// =============================================================================
// LogisticsSystem.cpp — 物流业务核心服务实现
// =============================================================================
// 文件用途：实现全部 46 个业务方法，含并发保护、事务回滚和通知生成。
// 所属模块：common/service（业务服务层，被 server 独占调用）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 并发保护：所有公共方法入口均有 CoreLock lock(mutex_)，包括只读方法
// 事务回滚：修改型方法在 saveAll 失败时恢复旧对象快照
// 通知生成：7 个业务触点自动写入通知（best-effort）
// 日志写入：所有关键操作写入 operations.log（9字段哈希链）
// =============================================================================

#include "LogisticsSystem.h"

#include "../models/ExpressItem.h"
#include "../security/InputValidator.h"
#include "../security/PasswordHasher.h"
#include "../security/StringUtil.h"
#include "../storage/StorageManager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

// ===========================================================================
// CoreMutex/CoreLock 实现 — 基于 std::atomic_flag 的自旋锁
// ===========================================================================
// 背景：当前 MinGW 环境对 std::mutex/std::shared_mutex 支持不稳定，
//       因此使用 atomic_flag 自旋锁作为兼容性替代方案。
// 工作原理：
//   test_and_set(memory_order_acquire) — 原子地设置 flag 为 true 并返回旧值
//     旧值为 true  → 锁已被占用，自旋等待
//     旧值为 false → 成功获取锁
//   clear(memory_order_release) — 原子地设置 flag 为 false，释放锁
// 注意：自旋锁在竞争激烈时消耗 CPU，适合当前单 server 低并发场景
// ===========================================================================

void CoreMutex::lock() const {
    while (flag_.test_and_set(std::memory_order_acquire)) {
        // 自旋等待 — 不做 yield/sleep，保证最小延迟
    }
}

void CoreMutex::unlock() const {
    flag_.clear(std::memory_order_release);
}

// RAII 锁守卫：构造时获取锁，析构时自动释放（异常安全）
CoreLock::CoreLock(const CoreMutex& mutex) : mutex_(mutex) {
    mutex_.lock();
}

CoreLock::~CoreLock() {
    mutex_.unlock();
}

LogisticsSystem::LogisticsSystem(const std::string& dataDirectory)
    : dataDirectory_(dataDirectory),
      userRepository_(dataDirectory + "/users.txt"),
      adminRepository_(dataDirectory + "/admin.txt"),
      courierRepository_(dataDirectory + "/couriers.txt"),
      expressRepository_(dataDirectory + "/expresses.txt"),
      authStateRepository_(dataDirectory + "/auth_state.txt"),
      notificationRepository_(dataDirectory + "/notifications.txt"),
      logger_(dataDirectory + "/operations.log") {}

ServiceResult LogisticsSystem::initialize() {
    CoreLock lock(mutex_);
    if (!StorageManager::ensureDirectory(dataDirectory_)) {
        return ServiceResult::failure("STORAGE_FAILED", "数据目录创建失败。");
    }
    if (!userRepository_.load(users_) || !courierRepository_.load(couriers_) ||
        !expressRepository_.load(expresses_) || !authStateRepository_.load(authStates_)) {
        return ServiceResult::failure("STORAGE_FAILED", "数据文件读取失败。");
    }
    // 通知是附加能力：文件缺失或加载异常不阻断核心用户、快递和资金数据启动。
    notificationRepository_.load(notifications_);
    if (!adminRepository_.load(admin_)) {
        return ServiceResult::failure("STORAGE_FAILED", "管理员文件读取失败。");
    }
    if (admin_.salt().empty() || admin_.passwordHash().empty()) {
        const std::string salt = PasswordHasher::makeSalt("admin");
        admin_ = Admin("admin", "SystemAdmin", salt, PasswordHasher::hashPassword(salt, "Admin0219"), 0.0);
        if (!adminRepository_.save(admin_)) {
            return ServiceResult::failure("STORAGE_FAILED", "默认管理员保存失败。");
        }
    }
    logger_.write("SYSTEM", "server", "INIT", "SUCCESS", "V3 common core initialized");
    return ServiceResult::success("SUCCESS", "业务核心初始化成功。");
}

ServiceResult LogisticsSystem::loginUser(const std::string& username, const std::string& password) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        logger_.write("GUEST", username, "LOGIN_USER", "FAILED", "user not found");
        return ServiceResult::failure("AUTH_FAILED", "用户名或密码错误。");
    }
    if (users_[index].frozen()) {
        logger_.write("USER", username, "LOGIN_USER", "DENIED", "account frozen");
        return ServiceResult::failure("ACCOUNT_FROZEN", "账号已冻结，请联系管理员。");
    }
    const std::string hash = PasswordHasher::hashPassword(users_[index].salt(), password);
    if (hash != users_[index].passwordHash()) {
        return handleAuthFailure("USER", username, "LOGIN_USER", true);
    }
    clearAuthFailure("USER", username);
    if (!saveAll()) {
        return ServiceResult::failure("STORAGE_FAILED", "登录状态保存失败。");
    }
    logger_.write("USER", username, "LOGIN_USER", "SUCCESS", "login success");
    return ServiceResult::successWithData("SUCCESS", "用户登录成功。", std::vector<std::string>{"USER", username});
}

ServiceResult LogisticsSystem::loginCourier(const std::string& username, const std::string& password) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(username);
    if (index == couriers_.size()) {
        logger_.write("GUEST", username, "LOGIN_COURIER", "FAILED", "courier not found");
        return ServiceResult::failure("AUTH_FAILED", "用户名或密码错误。");
    }
    if (couriers_[index].frozen() || couriers_[index].removed()) {
        logger_.write("COURIER", username, "LOGIN_COURIER", "DENIED", "courier unavailable");
        return ServiceResult::failure("ACCOUNT_FROZEN", "快递员账号已冻结或停用。");
    }
    const std::string hash = PasswordHasher::hashPassword(couriers_[index].salt(), password);
    if (hash != couriers_[index].passwordHash()) {
        return handleAuthFailure("COURIER", username, "LOGIN_COURIER", true);
    }
    clearAuthFailure("COURIER", username);
    if (!saveAll()) {
        return ServiceResult::failure("STORAGE_FAILED", "登录状态保存失败。");
    }
    logger_.write("COURIER", username, "LOGIN_COURIER", "SUCCESS", "login success");
    return ServiceResult::successWithData("SUCCESS", "快递员登录成功。", std::vector<std::string>{"COURIER", username});
}

ServiceResult LogisticsSystem::loginAdmin(const std::string& username, const std::string& password) {
    CoreLock lock(mutex_);
    if (username != admin_.username()) {
        logger_.write("GUEST", username, "LOGIN_ADMIN", "FAILED", "admin not found");
        return ServiceResult::failure("AUTH_FAILED", "用户名或密码错误。");
    }
    const std::string hash = PasswordHasher::hashPassword(admin_.salt(), password);
    if (hash != admin_.passwordHash()) {
        return handleAuthFailure("ADMIN", username, "LOGIN_ADMIN", false);
    }
    clearAuthFailure("ADMIN", username);
    if (!saveAll()) {
        return ServiceResult::failure("STORAGE_FAILED", "登录状态保存失败。");
    }
    logger_.write("ADMIN", username, "LOGIN_ADMIN", "SUCCESS", "login success");
    return ServiceResult::successWithData("SUCCESS", "管理员登录成功。", std::vector<std::string>{"ADMIN", username});
}

ServiceResult LogisticsSystem::registerUser(const std::string& username, const std::string& name,
                                            const std::string& phone, const std::string& password,
                                            const std::string& address) {
    CoreLock lock(mutex_);
    std::string error = InputValidator::checkUsername(username);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPasswordStrength(password);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkAddress(address);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    if (findUserIndex(username) != users_.size()) {
        return ServiceResult::failure("DUPLICATE", "用户名已存在。");
    }
    // 盐值与摘要随实体一起保存，明文密码不会进入仓库或日志。
    const std::string salt = PasswordHasher::makeSalt(username + phone);
    users_.push_back(User(username, name, phone, salt, PasswordHasher::hashPassword(salt, password), 0.0, address, false));
    if (!saveAll()) {
        users_.pop_back();
        return ServiceResult::failure("STORAGE_FAILED", "用户保存失败，已回滚。");
    }
    logger_.write("GUEST", username, "REGISTER_USER", "SUCCESS", "register user");
    return ServiceResult::success("SUCCESS", "用户注册成功。");
}

ServiceResult LogisticsSystem::checkUsernameAvailable(const std::string& username) {
    CoreLock lock(mutex_);
    const std::string error = InputValidator::checkUsername(username);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    if (findUserIndex(username) != users_.size()) {
        return ServiceResult::failure("DUPLICATE", "用户名已存在。");
    }
    // Also check couriers — username must be unique across roles
    if (findCourierIndex(username) != couriers_.size()) {
        return ServiceResult::failure("DUPLICATE", "用户名已存在。");
    }
    // Also check admin username
    if (username == admin_.username()) {
        return ServiceResult::failure("DUPLICATE", "用户名已存在。");
    }
    return ServiceResult::success("SUCCESS", "用户名可用。");
}

ServiceResult LogisticsSystem::checkPhoneAvailable(const std::string& phone) {
    CoreLock lock(mutex_);
    const std::string error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    // Note: this project allows multiple users to share the same phone number,
    // so we only validate format, not uniqueness.
    return ServiceResult::success("SUCCESS", "手机号格式合法。");
}

ServiceResult LogisticsSystem::queryUsers() const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const User& user : users_) {
        std::ostringstream stream;
        stream << "username=" << user.username()
               << ";name=" << user.name()
               << ";phone=" << user.phone()
               << ";balance=" << StringUtil::formatMoney(user.balance())
               << ";address=" << user.address()
               << ";frozen=" << (user.frozen() ? "1" : "0");
        records.push_back(stream.str());
    }
    return ServiceResult::successWithData("SUCCESS", "用户列表查询成功。", records);
}

ServiceResult LogisticsSystem::recharge(const std::string& username, double amount) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    if (!InputValidator::validPositiveAmount(amount)) {
        return ServiceResult::failure("INVALID_INPUT", "充值金额必须大于 0。");
    }
    const User oldUser = users_[index];
    users_[index].recharge(amount);
    if (!saveAll()) {
        users_[index] = oldUser;
        return ServiceResult::failure("STORAGE_FAILED", "充值保存失败，已回滚。");
    }
    logger_.write("USER", username, "RECHARGE", "SUCCESS", "amount=" + StringUtil::formatMoney(amount));
    return ServiceResult::success("SUCCESS", "充值成功，当前余额：" + StringUtil::formatMoney(users_[index].balance()));
}

ServiceResult LogisticsSystem::queryBalance(const std::string& username) const {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    return ServiceResult::successWithData("SUCCESS", "余额查询成功。",
                                          std::vector<std::string>{StringUtil::formatMoney(users_[index].balance())});
}

ServiceResult LogisticsSystem::createCourier(const std::string& username, const std::string& name,
                                             const std::string& phone, const std::string& password) {
    CoreLock lock(mutex_);
    std::string error = InputValidator::checkUsername(username);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPasswordStrength(password);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    if (findCourierIndex(username) != couriers_.size()) {
        return ServiceResult::failure("DUPLICATE", "快递员用户名已存在。");
    }
    const std::string salt = PasswordHasher::makeSalt(username + phone);
    couriers_.push_back(Courier(username, name, phone, salt, PasswordHasher::hashPassword(salt, password), 0.0, false, false));
    if (!saveAll()) {
        couriers_.pop_back();
        return ServiceResult::failure("STORAGE_FAILED", "快递员保存失败，已回滚。");
    }
    logger_.write("ADMIN", admin_.username(), "CREATE_COURIER", "SUCCESS", username);
    return ServiceResult::success("SUCCESS", "快递员创建成功。");
}

ServiceResult LogisticsSystem::queryCouriers() const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Courier& courier : couriers_) {
        std::ostringstream stream;
        stream << "username=" << courier.username()
               << ";name=" << courier.name()
               << ";phone=" << courier.phone()
               << ";income=" << StringUtil::formatMoney(courier.income())
               << ";frozen=" << (courier.frozen() ? "1" : "0")
               << ";removed=" << (courier.removed() ? "1" : "0")
               << ";unfinished=" << unfinishedCountForCourier(courier.username());
        records.push_back(stream.str());
    }
    return ServiceResult::successWithData("SUCCESS", "快递员列表查询成功。", records);
}

ServiceResult LogisticsSystem::removeCourier(const std::string& username) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(username);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    if (couriers_[index].removed()) {
        return ServiceResult::failure("STATE_CONFLICT", "快递员已停用。");
    }

    std::vector<std::string> conflicts;
    // 采用逻辑停用而非物理删除；停用前必须确保不存在仍引用该快递员的未完成任务。
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(username) && !express.isSigned()) {
            conflicts.push_back(express.toRecord());
        }
    }
    if (!conflicts.empty()) {
        logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "DENIED",
                      "unfinished tasks block courier=" + username);
        ServiceResult result = ServiceResult::failure("COURIER_HAS_UNFINISHED_TASKS",
                                                      "快递员存在未完成任务，拒绝停用。冲突任务如下。");
        result.data = conflicts;
        return result;
    }

    const Courier oldCourier = couriers_[index];
    couriers_[index].markRemoved();
    if (!saveAll()) {
        couriers_[index] = oldCourier;
        return ServiceResult::failure("STORAGE_FAILED", "停用快递员保存失败，已回滚。");
    }
    logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "SUCCESS", username);
    return ServiceResult::success("SUCCESS", "快递员停用成功。");
}

ServiceResult LogisticsSystem::setUserFrozen(const std::string& username, bool frozen) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    const User oldUser = users_[index];
    users_[index].setFrozen(frozen);
    addNotification("USER", username, "ACCOUNT_STATUS_CHANGED",
                    frozen ? "账号已冻结" : "账号已解冻",
                    frozen ? "您的账号已被管理员冻结。" : "您的账号已被管理员解冻。");
    if (!saveAll()) {
        users_[index] = oldUser;
        return ServiceResult::failure("STORAGE_FAILED", "用户冻结状态保存失败，已回滚。");
    }
    logger_.write("ADMIN", admin_.username(), "SET_USER_FROZEN", "SUCCESS",
                  username + "=" + (frozen ? "1" : "0"));
    return ServiceResult::success("SUCCESS", frozen ? "用户已冻结。" : "用户已解冻。");
}

ServiceResult LogisticsSystem::setCourierFrozen(const std::string& username, bool frozen) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(username);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    const Courier oldCourier = couriers_[index];
    couriers_[index].setFrozen(frozen);
    addNotification("COURIER", username, "ACCOUNT_STATUS_CHANGED",
                    frozen ? "账号已冻结" : "账号已解冻",
                    frozen ? "您的快递员账号已被管理员冻结。" : "您的快递员账号已被管理员解冻。");
    if (!saveAll()) {
        couriers_[index] = oldCourier;
        return ServiceResult::failure("STORAGE_FAILED", "快递员冻结状态保存失败，已回滚。");
    }
    logger_.write("ADMIN", admin_.username(), "SET_COURIER_FROZEN", "SUCCESS",
                  username + "=" + (frozen ? "1" : "0"));
    return ServiceResult::success("SUCCESS", frozen ? "快递员已冻结。" : "快递员已解冻。");
}

// ===========================================================================
// sendExpress — 寄件（服务端多态计费 + 资金流转 + 余额不足处理）
// ===========================================================================
// 锁保护：CoreLock lock(mutex_)
// 资金流转：sender.balance -= fee → admin.balance += fee（100%归平台）
// 计费方式：ExpressItemFactory::createItem → getPrice()（客户端不传费用）
// 余额不足：返回 BALANCE_NOT_ENOUGH + data=[balance=, fee=, shortage=]
//          客户端根据此信息触发即时充值+重试流程
// 事务回滚：express.push_back 后 saveAll 失败 → pop_back + 恢复 sender + admin 快照
ServiceResult LogisticsSystem::sendExpress(const std::string& sender, const std::string& receiver,
                                           const std::string& description, const std::string& itemType,
                                           double itemAmount, const std::string& note) {
    CoreLock lock(mutex_);
    const std::size_t senderIndex = findUserIndex(sender);
    const std::size_t receiverIndex = findUserIndex(receiver);
    if (senderIndex == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "发件用户不存在。");
    }
    if (receiverIndex == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "收件用户不存在。");
    }
    if (sender == receiver) {
        return ServiceResult::failure("INVALID_INPUT", "不能给自己寄件。");
    }
    if (users_[senderIndex].frozen() || users_[receiverIndex].frozen()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "发件人或收件人账号已冻结。");
    }
    std::unique_ptr<ExpressItem> item = ExpressItemFactory::createItem(itemType, itemAmount);
    if (!item) {
        return ServiceResult::failure("INVALID_INPUT", "快递类型或数量非法。");
    }
    const double fee = item->getPrice();
    if (users_[senderIndex].balance() + 0.000001 < fee) {
        const double shortage = fee - users_[senderIndex].balance();
        ServiceResult result = ServiceResult::failure(
            "BALANCE_NOT_ENOUGH",
            "余额不足，当前余额：" + StringUtil::formatMoney(users_[senderIndex].balance()) +
                "，本次费用：" + StringUtil::formatMoney(fee) +
                "，缺口：" + StringUtil::formatMoney(shortage) + "。");
        result.data.push_back("balance=" + StringUtil::formatMoney(users_[senderIndex].balance()));
        result.data.push_back("fee=" + StringUtil::formatMoney(fee));
        result.data.push_back("shortage=" + StringUtil::formatMoney(shortage));
        return result;
    }

    const User oldSender = users_[senderIndex];
    const Admin oldAdmin = admin_;
    const std::string expressId = generateExpressId();
    users_[senderIndex].deduct(fee);
    admin_.addBalance(fee);
    expresses_.push_back(Express(expressId, sender, receiver, "", nowText(), "", "",
                                 ExpressStatus::WaitingPickup, item->typeCode(), itemAmount,
                                 description, fee, note));
    if (!saveAll()) {
        expresses_.pop_back();
        users_[senderIndex] = oldSender;
        admin_ = oldAdmin;
        return ServiceResult::failure("STORAGE_FAILED", "寄件保存失败，已回滚。");
    }
    logger_.write("USER", sender, "SEND_EXPRESS", "SUCCESS", expressId + " fee=" + StringUtil::formatMoney(fee));
    return ServiceResult::successWithData(
        "SUCCESS",
        "寄件成功，快递单号：" + expressId + "，费用：" + StringUtil::formatMoney(fee) +
            "，剩余余额：" + StringUtil::formatMoney(users_[senderIndex].balance()),
        std::vector<std::string>{expressId, StringUtil::formatMoney(fee),
                                 StringUtil::formatMoney(users_[senderIndex].balance())});
}

ServiceResult LogisticsSystem::assignCourier(const std::string& expressId, const std::string& courierUsername) {
    CoreLock lock(mutex_);
    const std::size_t expressIndex = findExpressIndex(expressId);
    const std::size_t courierIndex = findCourierIndex(courierUsername);
    if (expressIndex == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (courierIndex == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    if (couriers_[courierIndex].frozen() || couriers_[courierIndex].removed()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "快递员不可用。");
    }
    if (!expresses_[expressIndex].isWaitingPickup()) {
        return ServiceResult::failure("STATE_CONFLICT", "只有待揽收快递可以分配快递员。");
    }
    const Express oldExpress = expresses_[expressIndex];
    expresses_[expressIndex].assignCourier(courierUsername);
    addNotification("COURIER", courierUsername, "TASK_ASSIGNED",
                    "新的待揽收任务", "您有一个新的待揽收任务，快递单号：" + expressId);
    if (!saveAll()) {
        expresses_[expressIndex] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "分配保存失败，已回滚。");
    }
    logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "SUCCESS", expressId + "->" + courierUsername);
    return ServiceResult::success("SUCCESS", "分配成功。");
}

ServiceResult LogisticsSystem::autoAssignCourier(const std::string& expressId) {
    CoreLock lock(mutex_);
    const std::size_t expressIndex = findExpressIndex(expressId);
    if (expressIndex == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (!expresses_[expressIndex].isWaitingPickup()) {
        return ServiceResult::failure("STATE_CONFLICT", "只有待揽收快递可以自动分配。");
    }
    const std::size_t courierIndex = selectBestCourierIndex();
    if (courierIndex == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "当前没有可用快递员。");
    }
    const Express oldExpress = expresses_[expressIndex];
    const std::string assignedCourier = couriers_[courierIndex].username();
    expresses_[expressIndex].assignCourier(assignedCourier);
    if (!saveAll()) {
        expresses_[expressIndex] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "自动分配保存失败，已回滚。");
    }
    addNotification("COURIER", assignedCourier, "TASK_ASSIGNED",
                    "新的待揽收任务", "您有一个新的待揽收任务，快递单号：" + expressId);
    logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "SUCCESS",
                  expressId + "->" + assignedCourier);
    return ServiceResult::successWithData("SUCCESS", "自动分配成功。",
                                          std::vector<std::string>{expressId, assignedCourier});
}

ServiceResult LogisticsSystem::autoAssignAllWaitingPickup() {
    CoreLock lock(mutex_);
    const std::vector<Express> oldExpresses = expresses_;
    std::vector<std::string> messages;
    std::vector<std::pair<std::string, std::string> > assignments;
    int successCount = 0;
    for (Express& express : expresses_) {
        if (!express.isWaitingPickup() || !express.courier().empty()) {
            continue;
        }
        const std::size_t courierIndex = selectBestCourierIndex();
        if (courierIndex == couriers_.size()) {
            messages.push_back(express.id() + ";FAILED;no available courier");
            continue;
        }
        const std::string assignedCourier = couriers_[courierIndex].username();
        express.assignCourier(assignedCourier);
        ++successCount;
        assignments.push_back(std::make_pair(express.id(), assignedCourier));
        messages.push_back(express.id() + ";SUCCESS;" + assignedCourier);
    }
    if (!saveAll()) {
        expresses_ = oldExpresses;
        return ServiceResult::failure("STORAGE_FAILED", "一键自动分配保存失败，已回滚。");
    }
    for (const std::pair<std::string, std::string>& assignment : assignments) {
        addNotification("COURIER", assignment.second, "TASK_ASSIGNED",
                        "新的待揽收任务", "您有一个新的待揽收任务，快递单号：" + assignment.first);
    }
    logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_ALL", "SUCCESS",
                  "successCount=" + std::to_string(successCount));
    return ServiceResult::successWithData("SUCCESS", "一键自动分配完成，成功：" + std::to_string(successCount), messages);
}

// ===========================================================================
// pickupExpress — 揽收快递（V3 最危险的状态+资金变更点）
// ===========================================================================
// 权限要求：COURIER token（session.username == express.courier）
// 锁保护：CoreLock lock(mutex_) — 全流程在临界区内
//
// 12步原子事务（任一步失败即回滚）：
//   1. 校验 expressId 存在（NOT_FOUND）
//   2. 校验 courier 存在且可用 — 非 frozen/removed（ACCOUNT_FROZEN）
//   3. 校验 express.courier == courierUsername（PERMISSION_DENIED + 审计日志）
//   4. 校验 express.status == WaitingPickup（STATE_CONFLICT + 审计日志）
//   5. commission = express.fee * 0.5（计算提成）
//   6. admin.deduct(commission)（平台付款 — 含容差检查）
//   7. courier.addIncome(commission)（快递员收款）
//   8. express.pickup(now)（状态变为 WaitingSign）
//   9. addNotification → receiver（EXPRESS_PICKED_UP 通知）
//   10. saveAll() → 5 文件串行保存
//   11. 保存失败 → 恢复 oldExpress/oldCourier/oldAdmin 内存快照
//   12. logger_.write(PICKUP_EXPRESS SUCCESS)
//
// 并发安全：
//   - CoreMutex 保证两个线程不会同时进入 pickupExpress
//   - 先检查状态→再变更状态，第二个线程进入时状态已变 → STATE_CONFLICT
//   - 资金流转（扣款+加收入+改状态）不可分割
// 红线：
//   - 不能揽收分配给其他快递员的快递
//   - 不能重复揽收同一快递
//   - 不能出现"状态改了但钱没转"或"钱转了但状态没改"
// ===========================================================================

ServiceResult LogisticsSystem::pickupExpress(const std::string& expressId, const std::string& courierUsername) {
    CoreLock lock(mutex_);
    const std::size_t expressIndex = findExpressIndex(expressId);
    const std::size_t courierIndex = findCourierIndex(courierUsername);
    if (expressIndex == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (courierIndex == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    if (couriers_[courierIndex].frozen() || couriers_[courierIndex].removed()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "快递员不可用。");
    }
    if (!expresses_[expressIndex].belongsToCourier(courierUsername)) {
        logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "DENIED",
                      "express=" + expressId + " assignedCourier=" + expresses_[expressIndex].courier());
        logger_.write("COURIER", courierUsername, "COURIER_PICKUP_PERMISSION_DENIED", "DENIED",
                      "express=" + expressId + " assignedCourier=" + expresses_[expressIndex].courier());
        return ServiceResult::failure("PERMISSION_DENIED", "不能揽收未分配给自己的快递。");
    }
    if (!expresses_[expressIndex].isWaitingPickup()) {
        logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "DENIED",
                      "state conflict express=" + expressId);
        logger_.write("COURIER", courierUsername, "COURIER_PICKUP_STATE_CONFLICT", "DENIED",
                      "express=" + expressId);
        return ServiceResult::failure("STATE_CONFLICT", "该快递当前状态不可揽收。");
    }
    const double commission = expresses_[expressIndex].fee() * 0.5;
    const Express oldExpress = expresses_[expressIndex];
    const Courier oldCourier = couriers_[courierIndex];
    const Admin oldAdmin = admin_;
    if (!admin_.deduct(commission)) {
        return ServiceResult::failure("BALANCE_NOT_ENOUGH", "管理员账户余额不足，无法支付提成。");
    }
    couriers_[courierIndex].addIncome(commission);
    expresses_[expressIndex].pickup(nowText());
    addNotification("USER", expresses_[expressIndex].receiver(), "EXPRESS_PICKED_UP",
                    "快递已揽收", "您的快递已揽收，待签收，快递单号：" + expressId);
    if (!saveAll()) {
        expresses_[expressIndex] = oldExpress;
        couriers_[courierIndex] = oldCourier;
        admin_ = oldAdmin;
        return ServiceResult::failure("STORAGE_FAILED", "揽收保存失败，已回滚。");
    }
    logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "SUCCESS",
                  expressId + " commission=" + StringUtil::formatMoney(commission));
    return ServiceResult::success("SUCCESS", "揽收成功，提成：" + StringUtil::formatMoney(commission));
}

ServiceResult LogisticsSystem::pickupBatchExpresses(const std::vector<std::string>& expressIds,
                                                    const std::string& courierUsername) {
    // 批处理不持有一把跨全部订单的锁；每个 pickupExpress 都是独立加锁、保存和回滚的事务。
    std::vector<std::string> records;
    int successCount = 0;
    int failureCount = 0;
    for (const std::string& expressId : expressIds) {
        ServiceResult result = pickupExpress(expressId, courierUsername);
        if (result.ok) {
            ++successCount;
        } else {
            ++failureCount;
        }
        records.push_back(expressId + ";" + (result.ok ? "SUCCESS" : "FAILED") + ";" +
                          result.code + ";" + result.message);
    }
    ServiceResult summary = ServiceResult::successWithData(
        "PARTIAL_SUCCESS",
        "批量揽收完成，成功：" + std::to_string(successCount) + "，失败：" + std::to_string(failureCount),
        records);
    // 只要有一单成功，整体可继续按成功响应展示逐单结果；全失败时改为 FAILED。
    summary.ok = successCount > 0;
    if (successCount == 0) {
        summary.code = "FAILED";
    }
    return summary;
}

ServiceResult LogisticsSystem::changePassword(const std::string& username, const std::string& oldPassword,
                                               const std::string& newPassword) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    const std::string oldHash = PasswordHasher::hashPassword(users_[index].salt(), oldPassword);
    if (oldHash != users_[index].passwordHash()) {
        logger_.write("USER", username, "CHANGE_PASSWORD", "FAILED", "old password mismatch");
        return ServiceResult::failure("AUTH_FAILED", "原密码错误。");
    }
    if (InputValidator::passwordUnchanged(oldPassword, newPassword)) {
        return ServiceResult::failure("PASSWORD_UNCHANGED", "新密码不能与原密码一致。");
    }
    const std::string error = InputValidator::checkPasswordStrength(newPassword);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    const User oldUser = users_[index];
    // 修改密码时重新派生盐值，保存失败则连同盐和摘要一起恢复。
    const std::string newSalt = PasswordHasher::makeSalt(username + users_[index].phone());
    users_[index].setPasswordHash(PasswordHasher::hashPassword(newSalt, newPassword));
    users_[index].setSalt(newSalt);
    if (!saveAll()) {
        users_[index] = oldUser;
        return ServiceResult::failure("STORAGE_FAILED", "密码修改保存失败，已回滚。");
    }
    logger_.write("USER", username, "CHANGE_PASSWORD", "SUCCESS", "password changed");
    return ServiceResult::success("SUCCESS", "密码修改成功。");
}

ServiceResult LogisticsSystem::changeCourierPassword(const std::string& username, const std::string& oldPassword,
                                                     const std::string& newPassword) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(username);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    if (couriers_[index].frozen() || couriers_[index].removed()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "快递员账号已冻结或停用。");
    }
    const std::string oldHash = PasswordHasher::hashPassword(couriers_[index].salt(), oldPassword);
    if (oldHash != couriers_[index].passwordHash()) {
        logger_.write("COURIER", username, "CHANGE_PASSWORD", "FAILED", "old password mismatch");
        return ServiceResult::failure("AUTH_FAILED", "原密码错误。");
    }
    if (InputValidator::passwordUnchanged(oldPassword, newPassword)) {
        return ServiceResult::failure("PASSWORD_UNCHANGED", "新密码不能与原密码一致。");
    }
    const std::string error = InputValidator::checkPasswordStrength(newPassword);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    const Courier oldCourier = couriers_[index];
    const std::string newSalt = PasswordHasher::makeSalt(username + couriers_[index].phone());
    couriers_[index].setPasswordHash(PasswordHasher::hashPassword(newSalt, newPassword));
    couriers_[index].setSalt(newSalt);
    if (!saveAll()) {
        couriers_[index] = oldCourier;
        return ServiceResult::failure("STORAGE_FAILED", "密码修改保存失败，已回滚。");
    }
    logger_.write("COURIER", username, "CHANGE_PASSWORD", "SUCCESS", "password changed");
    return ServiceResult::success("SUCCESS", "密码修改成功。");
}

ServiceResult LogisticsSystem::changeAdminPassword(const std::string& username, const std::string& oldPassword,
                                                   const std::string& newPassword) {
    CoreLock lock(mutex_);
    if (username != admin_.username()) {
        return ServiceResult::failure("NOT_FOUND", "管理员不存在。");
    }
    const std::string oldHash = PasswordHasher::hashPassword(admin_.salt(), oldPassword);
    if (oldHash != admin_.passwordHash()) {
        logger_.write("ADMIN", username, "CHANGE_PASSWORD", "FAILED", "old password mismatch");
        return ServiceResult::failure("AUTH_FAILED", "原密码错误。");
    }
    if (InputValidator::passwordUnchanged(oldPassword, newPassword)) {
        return ServiceResult::failure("PASSWORD_UNCHANGED", "新密码不能与原密码一致。");
    }
    const std::string error = InputValidator::checkPasswordStrength(newPassword);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    const Admin oldAdmin = admin_;
    const std::string newSalt = PasswordHasher::makeSalt(username + "admin");
    admin_.setPasswordHash(PasswordHasher::hashPassword(newSalt, newPassword));
    admin_.setSalt(newSalt);
    if (!saveAll()) {
        admin_ = oldAdmin;
        return ServiceResult::failure("STORAGE_FAILED", "密码修改保存失败，已回滚。");
    }
    logger_.write("ADMIN", username, "CHANGE_PASSWORD", "SUCCESS", "password changed");
    return ServiceResult::success("SUCCESS", "密码修改成功。");
}

// ============================================================
// Profile update commands (Phase 3)
// ============================================================

ServiceResult LogisticsSystem::updateMyProfile(const std::string& username, const std::string& name,
                                               const std::string& phone, const std::string& address) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(username);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    std::string error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkAddress(address);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    const User oldUser = users_[index];
    users_[index].setName(name);
    users_[index].setPhone(phone);
    users_[index].setAddress(address);
    if (!saveAll()) {
        users_[index] = oldUser;
        return ServiceResult::failure("STORAGE_FAILED", "个人信息保存失败，已回滚。");
    }
    logger_.write("USER", username, "UPDATE_MY_PROFILE", "SUCCESS", "profile updated");
    std::vector<std::string> records;
    records.push_back("username=" + users_[index].username());
    records.push_back("name=" + users_[index].name());
    records.push_back("phone=" + users_[index].phone());
    records.push_back("address=" + users_[index].address());
    return ServiceResult::successWithData("SUCCESS", "个人信息修改成功。", records);
}

ServiceResult LogisticsSystem::updateCourierProfile(const std::string& username, const std::string& name,
                                                    const std::string& phone) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(username);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    if (couriers_[index].frozen() || couriers_[index].removed()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "快递员账号已冻结或停用。");
    }
    std::string error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    const Courier oldCourier = couriers_[index];
    couriers_[index].setName(name);
    couriers_[index].setPhone(phone);
    if (!saveAll()) {
        couriers_[index] = oldCourier;
        return ServiceResult::failure("STORAGE_FAILED", "个人信息保存失败，已回滚。");
    }
    logger_.write("COURIER", username, "UPDATE_COURIER_PROFILE", "SUCCESS", "profile updated");
    std::vector<std::string> records;
    records.push_back("username=" + couriers_[index].username());
    records.push_back("name=" + couriers_[index].name());
    records.push_back("phone=" + couriers_[index].phone());
    return ServiceResult::successWithData("SUCCESS", "个人信息修改成功。", records);
}

ServiceResult LogisticsSystem::adminUpdateUser(const std::string& targetUsername, const std::string& name,
                                               const std::string& phone, const std::string& address, int frozen) {
    CoreLock lock(mutex_);
    const std::size_t index = findUserIndex(targetUsername);
    if (index == users_.size()) {
        return ServiceResult::failure("NOT_FOUND", "用户不存在。");
    }
    std::string error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkAddress(address);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    if (frozen != 0 && frozen != 1) {
        return ServiceResult::failure("INVALID_ARGUMENT", "frozen 参数必须为 0 或 1。");
    }
    const User oldUser = users_[index];
    const bool oldFrozen = users_[index].frozen();
    users_[index].setName(name);
    users_[index].setPhone(phone);
    users_[index].setAddress(address);
    users_[index].setFrozen(frozen == 1);
    if (!saveAll()) {
        users_[index] = oldUser;
        return ServiceResult::failure("STORAGE_FAILED", "用户信息保存失败，已回滚。");
    }
    if (oldFrozen != (frozen == 1)) {
        logger_.write("ADMIN", admin_.username(), "ADMIN_UPDATE_USER", "SUCCESS",
                      targetUsername + " frozen=" + (frozen == 1 ? "1" : "0"));
    }
    std::vector<std::string> records;
    records.push_back("username=" + users_[index].username());
    records.push_back("name=" + users_[index].name());
    records.push_back("phone=" + users_[index].phone());
    records.push_back("address=" + users_[index].address());
    records.push_back("frozen=" + std::string(users_[index].frozen() ? "1" : "0"));
    return ServiceResult::successWithData("SUCCESS", "用户信息修改成功。", records);
}

ServiceResult LogisticsSystem::adminUpdateCourier(const std::string& targetUsername, const std::string& name,
                                                  const std::string& phone, int frozen) {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(targetUsername);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    std::string error = InputValidator::checkName(name);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    error = InputValidator::checkPhone(phone);
    if (!error.empty()) {
        return ServiceResult::failure("INVALID_ARGUMENT", error);
    }
    if (frozen != 0 && frozen != 1) {
        return ServiceResult::failure("INVALID_ARGUMENT", "frozen 参数必须为 0 或 1。");
    }
    const Courier oldCourier = couriers_[index];
    const bool oldFrozen = couriers_[index].frozen();
    couriers_[index].setName(name);
    couriers_[index].setPhone(phone);
    couriers_[index].setFrozen(frozen == 1);
    if (!saveAll()) {
        couriers_[index] = oldCourier;
        return ServiceResult::failure("STORAGE_FAILED", "快递员信息保存失败，已回滚。");
    }
    if (oldFrozen != (frozen == 1)) {
        logger_.write("ADMIN", admin_.username(), "ADMIN_UPDATE_COURIER", "SUCCESS",
                      targetUsername + " frozen=" + (frozen == 1 ? "1" : "0"));
    }
    std::vector<std::string> records;
    records.push_back("username=" + couriers_[index].username());
    records.push_back("name=" + couriers_[index].name());
    records.push_back("phone=" + couriers_[index].phone());
    records.push_back("frozen=" + std::string(couriers_[index].frozen() ? "1" : "0"));
    records.push_back("removed=" + std::string(couriers_[index].removed() ? "1" : "0"));
    return ServiceResult::successWithData("SUCCESS", "快递员信息修改成功。", records);
}

ServiceResult LogisticsSystem::reassignCourier(const std::string& expressId, const std::string& newCourierUsername,
                                               const std::string& reason) {
    CoreLock lock(mutex_);
    const std::size_t expressIndex = findExpressIndex(expressId);
    const std::size_t courierIndex = findCourierIndex(newCourierUsername);
    if (expressIndex == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (courierIndex == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "新快递员不存在。");
    }
    if (couriers_[courierIndex].frozen() || couriers_[courierIndex].removed()) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "新快递员不可用（已冻结或停用）。");
    }
    if (expresses_[expressIndex].isSigned()) {
        return ServiceResult::failure("STATE_CONFLICT", "已签收快递不允许改派。");
    }
    if (expresses_[expressIndex].isWaitingSign()) {
        // 已揽收订单已经发生平台向快递员付款，当前版本不实现提成冲正。
        return ServiceResult::failure("STATE_CONFLICT", "快递已揽收，当前版本不支持涉及提成回滚的改派。");
    }
    const std::string oldCourier2 = expresses_[expressIndex].courier();
    const Express oldExpress = expresses_[expressIndex];
    expresses_[expressIndex].assignCourier(newCourierUsername);
    addNotification("COURIER", newCourierUsername, "TASK_REASSIGNED",
                    "改派任务", "您收到改派任务，快递单号：" + expressId);
    if (!oldCourier2.empty()) {
        addNotification("COURIER", oldCourier2, "TASK_REASSIGNED_AWAY",
                        "任务已被改派", "您的任务已被改派给其他快递员，快递单号：" + expressId);
    }
    if (!saveAll()) {
        expresses_[expressIndex] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "改派保存失败，已回滚。");
    }
    const std::string logDetail = expressId + " oldCourier=" + (oldCourier2.empty() ? "(none)" : oldCourier2) +
                                  " newCourier=" + newCourierUsername +
                                  " reason=" + reason;
    logger_.write("ADMIN", admin_.username(), "ADMIN_REASSIGN_COURIER", "SUCCESS", logDetail);
    return ServiceResult::success("SUCCESS", "快递改派成功，新快递员：" + newCourierUsername + "。");
}

// ============================================================
// Note and Rating commands (Phase 4)
// ============================================================

ServiceResult LogisticsSystem::updateExpressNote(const std::string& username, const std::string& role,
                                                 const std::string& expressId, const std::string& note) {
    CoreLock lock(mutex_);
    const std::size_t index = findExpressIndex(expressId);
    if (index == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    // 备注只能在揽收前修改：普通用户限发件人，管理员可处理任意待揽收订单。
    if (role == "USER" && expresses_[index].sender() == username && expresses_[index].isWaitingPickup()) {
        // allowed
    }
    // Admin can modify note while WaitingPickup
    else if (role == "ADMIN" && expresses_[index].isWaitingPickup()) {
        // allowed
    }
    else {
        return ServiceResult::failure("PERMISSION_DENIED", "您无权修改该快递的备注，或当前状态不允许修改。");
    }
    const Express oldExpress = expresses_[index];
    expresses_[index].setNote(note);
    if (!saveAll()) {
        expresses_[index] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "备注保存失败，已回滚。");
    }
    logger_.write(role, username, "UPDATE_EXPRESS_NOTE", "SUCCESS", expressId);
    return ServiceResult::success("SUCCESS", "备注修改成功。");
}

ServiceResult LogisticsSystem::rateExpress(const std::string& username, const std::string& expressId,
                                           int score, const std::string& comment) {
    CoreLock lock(mutex_);
    const std::size_t index = findExpressIndex(expressId);
    if (index == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (expresses_[index].receiver() != username) {
        return ServiceResult::failure("PERMISSION_DENIED", "只有收件人才能评分。");
    }
    if (!expresses_[index].isSigned()) {
        return ServiceResult::failure("STATE_CONFLICT", "只有已签收的快递才能评分。");
    }
    if (expresses_[index].ratingScore() != 0) {
        return ServiceResult::failure("DUPLICATE_RATING", "该快递已经评过分，不能重复评分。");
    }
    if (score < 1 || score > 5) {
        return ServiceResult::failure("INVALID_ARGUMENT", "评分必须为 1 到 5 的整数。");
    }
    if (comment.size() > 1024) {
        return ServiceResult::failure("INVALID_ARGUMENT", "评价内容过长。");
    }
    const Express oldExpress = expresses_[index];
    expresses_[index].setRating(score, comment);
    if (!expresses_[index].courier().empty()) {
        addNotification("COURIER", expresses_[index].courier(), "RATING_RECEIVED",
                        "收到服务评分", "您收到新的服务评分 " + std::to_string(score) + " 分，快递单号：" + expressId);
    }
    if (!saveAll()) {
        expresses_[index] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "评分保存失败，已回滚。");
    }
    logger_.write("USER", username, "RATE_EXPRESS", "SUCCESS",
                  expressId + " score=" + std::to_string(score));
    return ServiceResult::success("SUCCESS", "评分成功，感谢您的评价！");
}

// ===========================================================================
// signExpress — 签收快递
// ===========================================================================
// 校验顺序：
//   1. 快递存在 → NOT_FOUND
//   2. 收件人匹配（express.receiver == username）→ PERMISSION_DENIED
//      （红线：发件人不能签收自己寄出的快递）
//   3. 非 WaitingPickup 状态 → STATE_CONFLICT（待揽收不能签收）
//   4. 非 Signed 状态 → STATE_CONFLICT（不能重复签收）
// 通知：sender 收到 EXPRESS_SIGNED / courier 收到 EXPRESS_COMPLETED
ServiceResult LogisticsSystem::signExpress(const std::string& username, const std::string& expressId) {
    CoreLock lock(mutex_);
    const std::size_t expressIndex = findExpressIndex(expressId);
    if (expressIndex == expresses_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递单号不存在。");
    }
    if (expresses_[expressIndex].receiver() != username) {
        return ServiceResult::failure("PERMISSION_DENIED", "只能签收寄给自己的快递。");
    }
    if (expresses_[expressIndex].isWaitingPickup()) {
        return ServiceResult::failure("STATE_CONFLICT", "快递仍处于待揽收状态，不能签收。");
    }
    if (expresses_[expressIndex].isSigned()) {
        return ServiceResult::failure("STATE_CONFLICT", "快递已签收，不能重复签收。");
    }
    const Express oldExpress = expresses_[expressIndex];
    expresses_[expressIndex].sign(nowText());
    addNotification("USER", expresses_[expressIndex].sender(), "EXPRESS_SIGNED",
                    "快递已签收", "您的快递已签收，快递单号：" + expressId);
    if (!expresses_[expressIndex].courier().empty()) {
        addNotification("COURIER", expresses_[expressIndex].courier(), "EXPRESS_COMPLETED",
                        "快递已完成", "快递已完成签收，快递单号：" + expressId);
    }
    if (!saveAll()) {
        expresses_[expressIndex] = oldExpress;
        return ServiceResult::failure("STORAGE_FAILED", "签收保存失败，已回滚。");
    }
    logger_.write("USER", username, "SIGN_EXPRESS", "SUCCESS", expressId);
    return ServiceResult::success("SUCCESS", "签收成功。");
}

ServiceResult LogisticsSystem::signBatchExpresses(const std::vector<std::string>& expressIds,
                                                   const std::string& username) {
    // 与批量揽收相同，每个单号独立提交，部分失败不会撤销已成功签收的订单。
    std::vector<std::string> records;
    int successCount = 0;
    int failureCount = 0;
    for (const std::string& expressId : expressIds) {
        ServiceResult result = signExpress(username, expressId);
        if (result.ok) {
            ++successCount;
        } else {
            ++failureCount;
        }
        records.push_back(expressId + ";" + (result.ok ? "SUCCESS" : "FAILED") + ";" +
                          result.code + ";" + result.message);
    }
    ServiceResult summary = ServiceResult::successWithData(
        "PARTIAL_SUCCESS",
        "批量签收完成，成功：" + std::to_string(successCount) + "，失败：" + std::to_string(failureCount),
        records);
    summary.ok = successCount > 0;
    if (successCount == 0) {
        summary.code = "FAILED";
    }
    return summary;
}

ServiceResult LogisticsSystem::queryUserExpresses(const std::string& username) const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Express& express : expresses_) {
        if (express.belongsToUser(username)) {
            records.push_back(express.toRecord());
        }
    }
    return ServiceResult::successWithData("SUCCESS", "查询成功。", records);
}

ServiceResult LogisticsSystem::queryWaitingSignExpresses(const std::string& username) const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Express& express : expresses_) {
        if (express.receiver() == username && express.isWaitingSign()) {
            records.push_back(express.toRecord());
        }
    }
    return ServiceResult::successWithData("SUCCESS", "待签收快递查询成功。", records);
}

ServiceResult LogisticsSystem::queryCourierPickupTasks(const std::string& courierUsername) const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(courierUsername) && express.isWaitingPickup()) {
            records.push_back(express.toRecord());
        }
    }
    return ServiceResult::successWithData("SUCCESS", "待揽收任务查询成功。", records);
}

ServiceResult LogisticsSystem::queryCourierExpresses(const std::string& courierUsername) const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(courierUsername)) {
            records.push_back(express.toRecord());
        }
    }
    return ServiceResult::successWithData("SUCCESS", "查询成功。", records);
}

ServiceResult LogisticsSystem::queryAllExpresses() const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Express& express : expresses_) {
        records.push_back(express.toRecord());
    }
    return ServiceResult::successWithData("SUCCESS", "查询成功。", records);
}

ServiceResult LogisticsSystem::viewDashboard() const {
    CoreLock lock(mutex_);
    int waitingPickup = 0;
    int waitingSign = 0;
    int signedCount = 0;
    int normalCount = 0;
    int fragileCount = 0;
    int bookCount = 0;
    double normalFee = 0.0;
    double fragileFee = 0.0;
    double bookFee = 0.0;
    double courierPayout = 0.0;
    // 预估快递员支出按已揽收订单累计；待揽收订单尚未发生提成支付。
    for (const Express& express : expresses_) {
        if (express.isWaitingPickup()) {
            ++waitingPickup;
        } else if (express.isWaitingSign()) {
            ++waitingSign;
            courierPayout += express.fee() * 0.5;
        } else {
            ++signedCount;
            courierPayout += express.fee() * 0.5;
        }
        if (express.itemType() == "Normal") {
            ++normalCount;
            normalFee += express.fee();
        } else if (express.itemType() == "Fragile") {
            ++fragileCount;
            fragileFee += express.fee();
        } else if (express.itemType() == "Book") {
            ++bookCount;
            bookFee += express.fee();
        }
    }
    std::vector<std::string> records;
    records.push_back("平台余额;" + StringUtil::formatMoney(admin_.balance()));
    records.push_back("预估快递员支出;" + StringUtil::formatMoney(courierPayout));
    records.push_back("待揽收;" + std::to_string(waitingPickup));
    records.push_back("待签收;" + std::to_string(waitingSign));
    records.push_back("已签收;" + std::to_string(signedCount));
    records.push_back("普通快递;count=" + std::to_string(normalCount) + ";fee=" + StringUtil::formatMoney(normalFee));
    records.push_back("易碎品;count=" + std::to_string(fragileCount) + ";fee=" + StringUtil::formatMoney(fragileFee));
    records.push_back("图书;count=" + std::to_string(bookCount) + ";fee=" + StringUtil::formatMoney(bookFee));
    return ServiceResult::successWithData("SUCCESS", "统计看板生成成功。", records);
}

ServiceResult LogisticsSystem::viewCourierPerformance() const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Courier& courier : couriers_) {
        records.push_back(courierPerformanceRecord(courier));
    }
    return ServiceResult::successWithData("SUCCESS", "快递员绩效生成成功。", records);
}

ServiceResult LogisticsSystem::viewMyCourierPerformance(const std::string& courierUsername) const {
    CoreLock lock(mutex_);
    const std::size_t index = findCourierIndex(courierUsername);
    if (index == couriers_.size()) {
        return ServiceResult::failure("NOT_FOUND", "快递员不存在。");
    }
    return ServiceResult::successWithData("SUCCESS", "个人绩效生成成功。",
                                          std::vector<std::string>{courierPerformanceRecord(couriers_[index])});
}

ServiceResult LogisticsSystem::queryLogs(const std::string& actorType, const std::string& actor,
                                          const std::string& action, const std::string& result) const {
    CoreLock lock(mutex_);
    const std::vector<std::string> records = logger_.queryLogs(actorType, actor, action, result);
    const std::string count = std::to_string(records.size());
    return ServiceResult::successWithData("SUCCESS", "日志查询成功，共 " + count + " 条。", records);
}

ServiceResult LogisticsSystem::verifyLogHashChain() const {
    // Logger 自身只读日志文件，不访问业务集合，因此无需持有 CoreMutex。
    std::string message;
    const bool ok = logger_.verifyHashChain(message);
    return ok ? ServiceResult::success("SUCCESS", message)
              : ServiceResult::failure("LOG_CHAIN_BROKEN", message);
}

ServiceResult LogisticsSystem::auditSecurityEvent(const std::string& actorType, const std::string& actor,
                                                  const std::string& action, const std::string& detail) {
    // 该入口由路由鉴权失败路径调用，只追加审计日志，不读取或修改业务实体。
    logger_.write(actorType, actor, action, "DENIED", detail);
    return ServiceResult::success("SUCCESS", "安全审计已记录。");
}

// ============================================================
// Notification center (Phase 5)
// ============================================================

std::string LogisticsSystem::generateNotificationId() const {
    std::ostringstream stream;
    stream << "NT" << std::setw(6) << std::setfill('0') << (notifications_.size() + 1U);
    return stream.str();
}

ServiceResult LogisticsSystem::addNotification(const std::string& targetRole, const std::string& targetUsername,
                                               const std::string& type, const std::string& title,
                                               const std::string& content) {
    // Caller MUST hold CoreLock — this is always called from within a business method's lock.
    const std::string id = generateNotificationId();
    notifications_.push_back(Notification(id, nowText(), targetRole, targetUsername, type, title, content, false));
    // Best-effort: notification save failure must not rollback core business
    if (!notificationRepository_.save(notifications_)) {
        notifications_.pop_back();
        logger_.write("SYSTEM", "server", "NOTIFICATION_SAVE_FAILED", "FAILED",
                      "notification save failed id=" + id);
        return ServiceResult::failure("STORAGE_FAILED", "通知写入失败，但核心业务不受影响。");
    }
    return ServiceResult::success("SUCCESS", "通知已写入。");
}

ServiceResult LogisticsSystem::queryMyNotifications(const std::string& role, const std::string& username,
                                                    bool unreadOnly) const {
    CoreLock lock(mutex_);
    std::vector<std::string> records;
    for (const Notification& n : notifications_) {
        if (n.targetRole() == role && n.targetUsername() == username) {
            if (unreadOnly && n.readFlag()) continue;
            std::ostringstream stream;
            stream << "id=" << n.id()
                   << ";time=" << n.time()
                   << ";type=" << n.type()
                   << ";title=" << n.title()
                   << ";content=" << n.content()
                   << ";readFlag=" << (n.readFlag() ? "1" : "0");
            records.push_back(stream.str());
        }
    }
    return ServiceResult::successWithData("SUCCESS", "通知查询成功，共 " + std::to_string(records.size()) + " 条。", records);
}

ServiceResult LogisticsSystem::markNotificationRead(const std::string& role, const std::string& username,
                                                    const std::string& notificationId) {
    CoreLock lock(mutex_);
    for (Notification& n : notifications_) {
        // 同时匹配角色和账号，防止仅凭通知 ID 越权修改他人的通知。
        if (n.id() == notificationId && n.targetRole() == role && n.targetUsername() == username) {
            n.markRead();
            if (!notificationRepository_.save(notifications_)) {
                return ServiceResult::failure("STORAGE_FAILED", "标记已读保存失败。");
            }
            return ServiceResult::success("SUCCESS", "通知已标记为已读。");
        }
    }
    return ServiceResult::failure("NOT_FOUND", "通知不存在或不属于您。");
}

ServiceResult LogisticsSystem::countUnreadNotifications(const std::string& role, const std::string& username) const {
    CoreLock lock(mutex_);
    int count = 0;
    for (const Notification& n : notifications_) {
        if (n.targetRole() == role && n.targetUsername() == username && !n.readFlag()) {
            ++count;
        }
    }
    return ServiceResult::successWithData("SUCCESS", "未读通知数：" + std::to_string(count),
                                          std::vector<std::string>{std::to_string(count)});
}

std::size_t LogisticsSystem::findUserIndex(const std::string& username) const {
    for (std::size_t i = 0; i < users_.size(); ++i) {
        if (users_[i].username() == username) {
            return i;
        }
    }
    return users_.size();
}

std::size_t LogisticsSystem::findCourierIndex(const std::string& username) const {
    for (std::size_t i = 0; i < couriers_.size(); ++i) {
        if (couriers_[i].username() == username) {
            return i;
        }
    }
    return couriers_.size();
}

std::size_t LogisticsSystem::findExpressIndex(const std::string& expressId) const {
    for (std::size_t i = 0; i < expresses_.size(); ++i) {
        if (expresses_[i].id() == expressId) {
            return i;
        }
    }
    return expresses_.size();
}

// ===========================================================================
// saveAll — 原子持久化：串行保存 5 个数据文件
// ===========================================================================
// 保存顺序：users.txt → admin.txt → couriers.txt → expresses.txt → auth_state.txt
// 短路机制：&& 运算符 — 第一个失败的 save 阻止后续保存
// 注意：每个 save 内部使用 saveLinesAtomically（.tmp→.bak→正式），
//       但 5 个 save 之间不是数据库事务。已保存的文件在失败时不会被回滚到旧状态。
//       当前缓解：内存对象在 saveAll 失败时由调用者恢复旧快照
// 通知独立保存（不在 saveAll 中）：best-effort 策略，失败不影响核心业务
bool LogisticsSystem::saveAll() {
    return userRepository_.save(users_) &&
           adminRepository_.save(admin_) &&
           courierRepository_.save(couriers_) &&
           expressRepository_.save(expresses_) &&
           authStateRepository_.save(authStates_);
}

std::size_t LogisticsSystem::findAuthStateIndex(const std::string& role, const std::string& username) const {
    for (std::size_t i = 0; i < authStates_.size(); ++i) {
        if (authStates_[i].role() == role && authStates_[i].username() == username) {
            return i;
        }
    }
    return authStates_.size();
}

int LogisticsSystem::unfinishedCountForCourier(const std::string& username) const {
    int count = 0;
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(username) && !express.isSigned()) {
            ++count;
        }
    }
    return count;
}

int LogisticsSystem::signedCountForCourier(const std::string& username) const {
    int count = 0;
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(username) && express.isSigned()) {
            ++count;
        }
    }
    return count;
}

std::size_t LogisticsSystem::selectBestCourierIndex() const {
    std::size_t best = couriers_.size();
    // 稳定优先级：未完成任务少、累计收入低、用户名词典序小。
    for (std::size_t i = 0; i < couriers_.size(); ++i) {
        if (couriers_[i].frozen() || couriers_[i].removed()) {
            continue;
        }
        if (best == couriers_.size()) {
            best = i;
            continue;
        }
        const int currentLoad = unfinishedCountForCourier(couriers_[i].username());
        const int bestLoad = unfinishedCountForCourier(couriers_[best].username());
        if (currentLoad < bestLoad ||
            (currentLoad == bestLoad && couriers_[i].income() < couriers_[best].income()) ||
            (currentLoad == bestLoad && couriers_[i].income() == couriers_[best].income() &&
             couriers_[i].username() < couriers_[best].username())) {
            best = i;
        }
    }
    return best;
}

std::string LogisticsSystem::courierPerformanceRecord(const Courier& courier) const {
    const int unfinished = unfinishedCountForCourier(courier.username());
    const int signedCount = signedCountForCourier(courier.username());
    int waitingPickup = 0;
    int waitingSign = 0;
    int ratingSum = 0;
    int ratingCount = 0;
    for (const Express& express : expresses_) {
        if (express.belongsToCourier(courier.username())) {
            if (express.isWaitingPickup()) {
                ++waitingPickup;
            } else if (express.isWaitingSign()) {
                ++waitingSign;
            }
            if (express.ratingScore() > 0) {
                ratingSum += express.ratingScore();
                ++ratingCount;
            }
        }
    }
    const int total = unfinished + signedCount;
    // 无任务时完成率和无评分时平均分均定义为 0，避免除零和无意义数值。
    const double completionRate = total == 0 ? 0.0 : static_cast<double>(signedCount) * 100.0 / total;
    const double avgRating = ratingCount == 0 ? 0.0 : static_cast<double>(ratingSum) / ratingCount;
    std::ostringstream stream;
    stream << "username=" << courier.username()
           << ";name=" << courier.name()
           << ";waitingPickup=" << waitingPickup
           << ";waitingSign=" << waitingSign
           << ";unfinished=" << unfinished
           << ";signed=" << signedCount
           << ";total=" << total
           << ";completionRate=" << StringUtil::formatAmount(completionRate) << "%"
           << ";income=" << StringUtil::formatMoney(courier.income())
           << ";avgRating=" << StringUtil::formatAmount(avgRating)
           << ";ratingCount=" << ratingCount
           << ";frozen=" << (courier.frozen() ? "1" : "0")
           << ";removed=" << (courier.removed() ? "1" : "0");
    return stream.str();
}

AuthState& LogisticsSystem::ensureAuthState(const std::string& role, const std::string& username) {
    const std::size_t index = findAuthStateIndex(role, username);
    if (index != authStates_.size()) {
        return authStates_[index];
    }
    authStates_.push_back(AuthState(role, username, 0, ""));
    return authStates_.back();
}

ServiceResult LogisticsSystem::handleAuthFailure(const std::string& role, const std::string& username,
                                                 const std::string& action, bool freezeAllowed) {
    // 调用方已经持有 CoreLock；失败计数和可能的冻结状态在同一临界区内更新。
    AuthState& state = ensureAuthState(role, username);
    state.recordFailure(nowText());
    const int failedCount = state.failedCount();
    bool frozenNow = false;
    if (freezeAllowed && failedCount >= 3) {
        if (role == "USER") {
            const std::size_t index = findUserIndex(username);
            if (index != users_.size()) {
                users_[index].setFrozen(true);
                frozenNow = true;
            }
        } else if (role == "COURIER") {
            const std::size_t index = findCourierIndex(username);
            if (index != couriers_.size()) {
                couriers_[index].setFrozen(true);
                frozenNow = true;
            }
        }
    }
    // 管理员只累计失败次数，不启用三次自动冻结，避免唯一管理入口被锁死。
    saveAll();
    logger_.write(role, username, action, frozenNow ? "DENIED" : "FAILED",
                  "password mismatch failedCount=" + std::to_string(failedCount));
    if (frozenNow) {
        return ServiceResult::failure("ACCOUNT_FROZEN", "密码连续错误 3 次，账号已自动冻结。");
    }
    return ServiceResult::failure("AUTH_FAILED", "用户名或密码错误，连续失败次数：" + std::to_string(failedCount));
}

void LogisticsSystem::clearAuthFailure(const std::string& role, const std::string& username) {
    ensureAuthState(role, username).clearFailures();
}

std::string LogisticsSystem::generateExpressId() const {
    // 当前 ID 由集合长度递增生成，调用点均位于 CoreLock 保护的新增流程中。
    std::ostringstream stream;
    stream << "EX" << std::setw(6) << std::setfill('0') << (expresses_.size() + 1U);
    return stream.str();
}

std::string LogisticsSystem::nowText() {
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
