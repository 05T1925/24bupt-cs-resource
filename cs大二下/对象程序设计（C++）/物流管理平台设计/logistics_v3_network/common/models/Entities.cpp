// =============================================================================
// Entities.cpp — 核心实体序列化/反序列化实现
// =============================================================================
// 文件用途：实现 User/Admin/Courier/Express/AuthState/Notification 的
//          序列化(serialize)、反序列化(deserialize)和业务辅助方法。
// 所属模块：common/models（纯业务模型层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 序列化规则：
//   - 所有字段经过 StringUtil::escape 转义后以 pipe(|) 连接
//   - 金额使用 StringUtil::formatMoney（保留2位小数）
//   - frozen/removed/readFlag 布尔值序列化为 "0"/"1"
//
// 反序列化规则：
//   - 按 pipe 拆分后严格校验字段数量
//   - 金额通过 StringUtil::parseDoubleStrict 解析（拒绝非法格式）
//   - 布尔字段只接受 "0" 或 "1"
//   - Express 支持 12-15 字段向后兼容（旧数据可能无 note/ratingScore/ratingComment）
// =============================================================================

#include "Entities.h"

#include "../security/StringUtil.h"

#include <sstream>

// ===========================================================================
// User 实现
// ===========================================================================

User::User(const std::string& username, const std::string& name, const std::string& phone,
           const std::string& salt, const std::string& passwordHash, double balance,
           const std::string& address, bool frozen)
    : username_(username), name_(name), phone_(phone), salt_(salt), passwordHash_(passwordHash),
      balance_(balance), address_(address), frozen_(frozen) {}

const std::string& User::username() const { return username_; }
const std::string& User::name() const { return name_; }
const std::string& User::phone() const { return phone_; }
const std::string& User::salt() const { return salt_; }
const std::string& User::passwordHash() const { return passwordHash_; }
void User::setPasswordHash(const std::string& hash) { passwordHash_ = hash; }
void User::setSalt(const std::string& salt) { salt_ = salt; }
void User::setName(const std::string& name) { name_ = name; }
void User::setPhone(const std::string& phone) { phone_ = phone; }
void User::setAddress(const std::string& address) { address_ = address; }
double User::balance() const { return balance_; }
const std::string& User::address() const { return address_; }
bool User::frozen() const { return frozen_; }
void User::recharge(double amount) { balance_ += amount; }

// 扣款操作：使用 0.000001 容差避免浮点精度导致的误判
// 例如 balance=16.000000, amount=16.000001 时拒绝扣款
bool User::deduct(double amount) {
    if (balance_ + 0.000001 < amount) {
        return false;
    }
    balance_ -= amount;
    return true;
}

void User::setFrozen(bool frozen) { frozen_ = frozen; }

// 序列化为 8 字段 pipe 分隔行
// 格式：username|name|phone|salt|passwordHash|balance|address|frozen
// 所有文本字段经过 StringUtil::escape 转义（% → %25, | → %7C 等）
std::string User::serialize() const {
    return StringUtil::escape(username_) + "|" + StringUtil::escape(name_) + "|" +
           StringUtil::escape(phone_) + "|" + StringUtil::escape(salt_) + "|" +
           StringUtil::escape(passwordHash_) + "|" + StringUtil::formatMoney(balance_) + "|" +
           StringUtil::escape(address_) + "|" + (frozen_ ? "1" : "0");
}

// 从文件行反序列化 User
// 严格校验：字段数==8 + balance 可解析为 double + frozen 为 0/1
bool User::deserialize(const std::string& line, User& user) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() != 8U) {
        return false;
    }
    double balance = 0.0;
    if (!StringUtil::parseDoubleStrict(parts[5], balance)) {
        return false;
    }
    if (parts[7] != "0" && parts[7] != "1") {
        return false;
    }
    user = User(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                StringUtil::unescape(parts[2]), StringUtil::unescape(parts[3]),
                StringUtil::unescape(parts[4]), balance, StringUtil::unescape(parts[6]),
                parts[7] == "1");
    return true;
}

// ===========================================================================
// Admin 实现
// ===========================================================================

Admin::Admin() = default;

Admin::Admin(const std::string& username, const std::string& name, const std::string& salt,
             const std::string& passwordHash, double balance)
    : username_(username), name_(name), salt_(salt), passwordHash_(passwordHash),
      balance_(balance) {}

const std::string& Admin::username() const { return username_; }
const std::string& Admin::name() const { return name_; }
const std::string& Admin::salt() const { return salt_; }
const std::string& Admin::passwordHash() const { return passwordHash_; }
void Admin::setPasswordHash(const std::string& hash) { passwordHash_ = hash; }
void Admin::setSalt(const std::string& salt) { salt_ = salt; }
double Admin::balance() const { return balance_; }
void Admin::addBalance(double amount) { balance_ += amount; }

bool Admin::deduct(double amount) {
    if (balance_ + 0.000001 < amount) {
        return false;
    }
    balance_ -= amount;
    return true;
}

// 序列化为 5 字段：username|name|salt|passwordHash|balance
std::string Admin::serialize() const {
    return StringUtil::escape(username_) + "|" + StringUtil::escape(name_) + "|" +
           StringUtil::escape(salt_) + "|" + StringUtil::escape(passwordHash_) + "|" +
           StringUtil::formatMoney(balance_);
}

// Admin 反序列化：严格校验 5 字段
bool Admin::deserialize(const std::string& line, Admin& admin) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() != 5U) {
        return false;
    }
    double balance = 0.0;
    if (!StringUtil::parseDoubleStrict(parts[4], balance)) {
        return false;
    }
    admin = Admin(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                  StringUtil::unescape(parts[2]), StringUtil::unescape(parts[3]), balance);
    return true;
}

// ===========================================================================
// Courier 实现
// ===========================================================================

Courier::Courier(const std::string& username, const std::string& name, const std::string& phone,
                 const std::string& salt, const std::string& passwordHash, double income,
                 bool frozen, bool removed)
    : username_(username), name_(name), phone_(phone), salt_(salt), passwordHash_(passwordHash),
      income_(income), frozen_(frozen), removed_(removed) {}

const std::string& Courier::username() const { return username_; }
const std::string& Courier::name() const { return name_; }
const std::string& Courier::phone() const { return phone_; }
const std::string& Courier::salt() const { return salt_; }
const std::string& Courier::passwordHash() const { return passwordHash_; }
void Courier::setPasswordHash(const std::string& hash) { passwordHash_ = hash; }
void Courier::setSalt(const std::string& salt) { salt_ = salt; }
void Courier::setName(const std::string& name) { name_ = name; }
void Courier::setPhone(const std::string& phone) { phone_ = phone; }
double Courier::income() const { return income_; }
bool Courier::frozen() const { return frozen_; }
bool Courier::removed() const { return removed_; }
void Courier::addIncome(double amount) { income_ += amount; }
void Courier::setFrozen(bool frozen) { frozen_ = frozen; }
void Courier::markRemoved() { removed_ = true; }  // 停用不可逆

// 序列化为 8 字段：username|name|phone|salt|passwordHash|income|frozen|removed
std::string Courier::serialize() const {
    return StringUtil::escape(username_) + "|" + StringUtil::escape(name_) + "|" +
           StringUtil::escape(phone_) + "|" + StringUtil::escape(salt_) + "|" +
           StringUtil::escape(passwordHash_) + "|" + StringUtil::formatMoney(income_) + "|" +
           (frozen_ ? "1" : "0") + "|" + (removed_ ? "1" : "0");
}

// Courier 反序列化：严格校验 8 字段 + income 可解析 + frozen/removed 为 0/1
bool Courier::deserialize(const std::string& line, Courier& courier) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() != 8U) {
        return false;
    }
    double income = 0.0;
    if (!StringUtil::parseDoubleStrict(parts[5], income)) {
        return false;
    }
    if ((parts[6] != "0" && parts[6] != "1") || (parts[7] != "0" && parts[7] != "1")) {
        return false;
    }
    courier = Courier(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                      StringUtil::unescape(parts[2]), StringUtil::unescape(parts[3]),
                      StringUtil::unescape(parts[4]), income, parts[6] == "1", parts[7] == "1");
    return true;
}

// ===========================================================================
// Express 实现
// ===========================================================================

Express::Express(const std::string& id, const std::string& sender, const std::string& receiver,
                 const std::string& courier, const std::string& sendTime,
                 const std::string& pickupTime, const std::string& receiveTime,
                 ExpressStatus status, const std::string& itemType, double itemAmount,
                 const std::string& description, double fee, const std::string& note,
                 int ratingScore, const std::string& ratingComment)
    : id_(id), sender_(sender), receiver_(receiver), courier_(courier), sendTime_(sendTime),
      pickupTime_(pickupTime), receiveTime_(receiveTime), status_(status), itemType_(itemType),
      itemAmount_(itemAmount), description_(description), note_(note),
      ratingScore_(ratingScore), ratingComment_(ratingComment), fee_(fee) {}

const std::string& Express::id() const { return id_; }
const std::string& Express::sender() const { return sender_; }
const std::string& Express::receiver() const { return receiver_; }
const std::string& Express::courier() const { return courier_; }
const std::string& Express::sendTime() const { return sendTime_; }
const std::string& Express::pickupTime() const { return pickupTime_; }
const std::string& Express::receiveTime() const { return receiveTime_; }
ExpressStatus Express::status() const { return status_; }
const std::string& Express::itemType() const { return itemType_; }
double Express::itemAmount() const { return itemAmount_; }
const std::string& Express::description() const { return description_; }
const std::string& Express::note() const { return note_; }
void Express::setNote(const std::string& note) { note_ = note; }
int Express::ratingScore() const { return ratingScore_; }
const std::string& Express::ratingComment() const { return ratingComment_; }
void Express::setRating(int score, const std::string& comment) { ratingScore_ = score; ratingComment_ = comment; }
double Express::fee() const { return fee_; }

// belongsToUser：发件人或收件人匹配即视为相关
bool Express::belongsToUser(const std::string& username) const { return sender_ == username || receiver_ == username; }
// belongsToCourier：快递员匹配
bool Express::belongsToCourier(const std::string& username) const { return courier_ == username; }
bool Express::isWaitingPickup() const { return status_ == ExpressStatus::WaitingPickup; }
bool Express::isWaitingSign() const { return status_ == ExpressStatus::WaitingSign; }
bool Express::isSigned() const { return status_ == ExpressStatus::Signed; }

void Express::assignCourier(const std::string& courier) { courier_ = courier; }
// pickup：揽收操作 — 状态变为待签收，记录揽收时间
void Express::pickup(const std::string& time) { pickupTime_ = time; status_ = ExpressStatus::WaitingSign; }
// sign：签收操作 — 状态变为已签收，记录签收时间
void Express::sign(const std::string& time) { receiveTime_ = time; status_ = ExpressStatus::Signed; }

// 返回中文状态名称（用于客户端展示）
std::string Express::statusName() const {
    if (status_ == ExpressStatus::WaitingPickup) {
        return "待揽收";
    }
    if (status_ == ExpressStatus::WaitingSign) {
        return "待签收";
    }
    return "已签收";
}

// 序列化为 15 字段（完整格式）：
// id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee|note|ratingScore|ratingComment
// 注意：即使 note/ratingScore/ratingComment 为空也输出（保证格式统一）
std::string Express::serialize() const {
    return StringUtil::escape(id_) + "|" + StringUtil::escape(sender_) + "|" +
           StringUtil::escape(receiver_) + "|" + StringUtil::escape(courier_) + "|" +
           StringUtil::escape(sendTime_) + "|" + StringUtil::escape(pickupTime_) + "|" +
           StringUtil::escape(receiveTime_) + "|" + std::to_string(static_cast<int>(status_)) + "|" +
           StringUtil::escape(itemType_) + "|" + StringUtil::formatAmount(itemAmount_) + "|" +
           StringUtil::escape(description_) + "|" + StringUtil::formatMoney(fee_) + "|" +
           StringUtil::escape(note_) + "|" +
           std::to_string(ratingScore_) + "|" +
           StringUtil::escape(ratingComment_);
}

// toRecord：生成客户端可读的 key=value;key=value 格式
// 用于 Response.records 表格展示
// 示例：EX000001;sender=u1;receiver=u2;courier=c1;status=待揽收;itemType=Fragile;amount=2.00;fee=16.00
std::string Express::toRecord() const {
    std::ostringstream stream;
    stream << id_ << ";sender=" << sender_ << ";receiver=" << receiver_
           << ";courier=" << courier_ << ";status=" << statusName()
           << ";itemType=" << itemType_ << ";amount=" << StringUtil::formatAmount(itemAmount_)
           << ";fee=" << StringUtil::formatMoney(fee_);
    if (!note_.empty()) {
        stream << ";note=" << note_;
    }
    if (ratingScore_ > 0) {
        stream << ";ratingScore=" << ratingScore_
               << ";ratingComment=" << ratingComment_;
    }
    return stream.str();
}

// Express 反序列化：支持 12-15 字段向后兼容
// 12字段：原始格式（无 note/ratingScore/ratingComment）
// 13字段：+note
// 14字段：+ratingScore
// 15字段：+ratingComment（当前完整格式）
// 缺失字段使用默认值（note=""、ratingScore=0、ratingComment=""）
bool Express::deserialize(const std::string& line, Express& express) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() < 12U || parts.size() > 15U) {
        return false;
    }
    double amount = 0.0;
    double fee = 0.0;
    int statusValue = 0;
    if (!StringUtil::parseDoubleStrict(parts[9], amount) ||
        !StringUtil::parseDoubleStrict(parts[11], fee)) {
        return false;
    }
    std::istringstream statusStream(parts[7]);
    statusStream >> statusValue;
    if (statusStream.fail() || !statusStream.eof() || statusValue < 0 || statusValue > 2) {
        return false;
    }
    // 向后兼容：缺失字段使用默认值
    const std::string note = parts.size() >= 13U ? StringUtil::unescape(parts[12]) : "";
    int ratingScore = 0;
    if (parts.size() >= 14U) {
        std::istringstream rs(parts[13]);
        rs >> ratingScore;
        if (rs.fail()) ratingScore = 0;
    }
    const std::string ratingComment = parts.size() >= 15U ? StringUtil::unescape(parts[14]) : "";
    express = Express(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                      StringUtil::unescape(parts[2]), StringUtil::unescape(parts[3]),
                      StringUtil::unescape(parts[4]), StringUtil::unescape(parts[5]),
                      StringUtil::unescape(parts[6]), static_cast<ExpressStatus>(statusValue),
                      StringUtil::unescape(parts[8]), amount, StringUtil::unescape(parts[10]),
                      fee, note, ratingScore, ratingComment);
    return true;
}

// ===========================================================================
// AuthState 实现
// ===========================================================================

AuthState::AuthState(const std::string& role, const std::string& username,
                     int failedCount, const std::string& lastFailedTime)
    : role_(role), username_(username), failedCount_(failedCount), lastFailedTime_(lastFailedTime) {}

const std::string& AuthState::role() const { return role_; }
const std::string& AuthState::username() const { return username_; }
int AuthState::failedCount() const { return failedCount_; }
const std::string& AuthState::lastFailedTime() const { return lastFailedTime_; }

void AuthState::recordFailure(const std::string& time) {
    ++failedCount_;
    lastFailedTime_ = time;
}

void AuthState::clearFailures() {
    failedCount_ = 0;
    lastFailedTime_.clear();
}

// 序列化：role|username|failedCount|lastFailedTime
std::string AuthState::serialize() const {
    return StringUtil::escape(role_) + "|" + StringUtil::escape(username_) + "|" +
           std::to_string(failedCount_) + "|" + StringUtil::escape(lastFailedTime_);
}

// 反序列化：严格 4字段 + failedCount 为非负整数
bool AuthState::deserialize(const std::string& line, AuthState& state) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() != 4U) {
        return false;
    }
    int failedCount = 0;
    std::istringstream stream(parts[2]);
    stream >> failedCount;
    if (stream.fail() || !stream.eof() || failedCount < 0) {
        return false;
    }
    state = AuthState(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                      failedCount, StringUtil::unescape(parts[3]));
    return true;
}

// ===========================================================================
// Notification 实现
// ===========================================================================

Notification::Notification(const std::string& id, const std::string& time,
                           const std::string& targetRole, const std::string& targetUsername,
                           const std::string& type, const std::string& title,
                           const std::string& content, bool readFlag)
    : id_(id), time_(time), targetRole_(targetRole), targetUsername_(targetUsername),
      type_(type), title_(title), content_(content), readFlag_(readFlag) {}

const std::string& Notification::id() const { return id_; }
const std::string& Notification::time() const { return time_; }
const std::string& Notification::targetRole() const { return targetRole_; }
const std::string& Notification::targetUsername() const { return targetUsername_; }
const std::string& Notification::type() const { return type_; }
const std::string& Notification::title() const { return title_; }
const std::string& Notification::content() const { return content_; }
bool Notification::readFlag() const { return readFlag_; }
void Notification::markRead() { readFlag_ = true; }

// 序列化：id|time|targetRole|targetUsername|type|title|content|readFlag
std::string Notification::serialize() const {
    return StringUtil::escape(id_) + "|" + StringUtil::escape(time_) + "|" +
           StringUtil::escape(targetRole_) + "|" + StringUtil::escape(targetUsername_) + "|" +
           StringUtil::escape(type_) + "|" + StringUtil::escape(title_) + "|" +
           StringUtil::escape(content_) + "|" + (readFlag_ ? "1" : "0");
}

// 反序列化：严格 8字段 + readFlag 为 0/1
bool Notification::deserialize(const std::string& line, Notification& notification) {
    const std::vector<std::string> parts = StringUtil::split(line, '|');
    if (parts.size() != 8U) {
        return false;
    }
    if (parts[7] != "0" && parts[7] != "1") {
        return false;
    }
    notification = Notification(StringUtil::unescape(parts[0]), StringUtil::unescape(parts[1]),
                                StringUtil::unescape(parts[2]), StringUtil::unescape(parts[3]),
                                StringUtil::unescape(parts[4]), StringUtil::unescape(parts[5]),
                                StringUtil::unescape(parts[6]), parts[7] == "1");
    return true;
}
