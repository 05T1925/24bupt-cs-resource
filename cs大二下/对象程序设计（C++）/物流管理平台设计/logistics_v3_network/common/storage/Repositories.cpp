#include "Repositories.h"

#include "StorageManager.h"
  
//实现实体与文件的转换   不实现业务规则，只负责持久化映射。

UserRepository::UserRepository(const std::string& filePath) : filePath_(filePath) {}

bool UserRepository::load(std::vector<User>& users) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    users.clear();
    // 单条历史脏数据不会阻断其余合法用户的加载。
    for (const std::string& line : lines) {
        User user;
        if (User::deserialize(line, user)) {
            users.push_back(user);
        }
    }
    return true;
}

bool UserRepository::save(const std::vector<User>& users) const {
    std::vector<std::string> lines;
    // 先完成内存序列化，再一次性交给原子保存流程。
    for (const User& user : users) {
        lines.push_back(user.serialize());
    }
    return StorageManager::saveLinesAtomically(filePath_, lines);
}

AdminRepository::AdminRepository(const std::string& filePath) : filePath_(filePath) {}

bool AdminRepository::load(Admin& admin) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    if (lines.empty()) {
        // 空文件保留调用方构造的默认管理员对象。
        return true;
    }
    return Admin::deserialize(lines.front(), admin);
}

bool AdminRepository::save(const Admin& admin) const {
    return StorageManager::saveLinesAtomically(filePath_, std::vector<std::string>{admin.serialize()});
}

CourierRepository::CourierRepository(const std::string& filePath) : filePath_(filePath) {}

bool CourierRepository::load(std::vector<Courier>& couriers) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    couriers.clear();
    // 与用户仓库一致：跳过无法反序列化的单行，继续恢复其余记录。
    for (const std::string& line : lines) {
        Courier courier;
        if (Courier::deserialize(line, courier)) {
            couriers.push_back(courier);
        }
    }
    return true;
}

bool CourierRepository::save(const std::vector<Courier>& couriers) const {
    std::vector<std::string> lines;
    for (const Courier& courier : couriers) {
        lines.push_back(courier.serialize());
    }
    return StorageManager::saveLinesAtomically(filePath_, lines);
}

ExpressRepository::ExpressRepository(const std::string& filePath) : filePath_(filePath) {}

bool ExpressRepository::load(std::vector<Express>& expresses) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    expresses.clear();
    // Express::deserialize 内部负责兼容历史 12 至 15 字段格式。
    for (const std::string& line : lines) {
        Express express;
        if (Express::deserialize(line, express)) {
            expresses.push_back(express);
        }
    }
    return true;
}

bool ExpressRepository::save(const std::vector<Express>& expresses) const {
    std::vector<std::string> lines;
    for (const Express& express : expresses) {
        lines.push_back(express.serialize());
    }
    return StorageManager::saveLinesAtomically(filePath_, lines);
}

AuthStateRepository::AuthStateRepository(const std::string& filePath) : filePath_(filePath) {}

bool AuthStateRepository::load(std::vector<AuthState>& states) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    states.clear();
    // 认证状态是辅助安全数据，损坏的单条状态不会影响其他账号恢复。
    for (const std::string& line : lines) {
        AuthState state;
        if (AuthState::deserialize(line, state)) {
            states.push_back(state);
        }
    }
    return true;
}

bool AuthStateRepository::save(const std::vector<AuthState>& states) const {
    std::vector<std::string> lines;
    for (const AuthState& state : states) {
        lines.push_back(state.serialize());
    }
    return StorageManager::saveLinesAtomically(filePath_, lines);
}

NotificationRepository::NotificationRepository(const std::string& filePath) : filePath_(filePath) {}

bool NotificationRepository::load(std::vector<Notification>& notifications) const {
    std::vector<std::string> lines;
    if (!StorageManager::loadLines(filePath_, lines)) {
        return false;
    }
    notifications.clear();
    // 通知采用尽力恢复策略，非法行被忽略，不阻断核心业务数据加载。
    for (const std::string& line : lines) {
        Notification notification;
        if (Notification::deserialize(line, notification)) {
            notifications.push_back(notification);
        }
    }
    return true;
}

bool NotificationRepository::save(const std::vector<Notification>& notifications) const {
    std::vector<std::string> lines;
    for (const Notification& notification : notifications) {
        lines.push_back(notification.serialize());
    }
    return StorageManager::saveLinesAtomically(filePath_, lines);
}
