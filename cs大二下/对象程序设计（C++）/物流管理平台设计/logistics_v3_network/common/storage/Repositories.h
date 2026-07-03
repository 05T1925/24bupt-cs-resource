// =============================================================================
// Repositories.h - 业务实体仓库接口
// =============================================================================
// 每个 Repository 绑定一个数据文件，并调用对应实体的 serialize/deserialize。
// 上层 LogisticsSystem 只操作对象集合，不直接拼接文本或打开文件。
//
// 调用关系：
//   LogisticsSystem -> XxxRepository -> Entity::serialize/deserialize
//                                  -> StorageManager::load/save
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_STORAGE_REPOSITORIES_H
#define LOGISTICS_V3_COMMON_STORAGE_REPOSITORIES_H

#include "../models/Entities.h"

#include <string>
#include <vector>

// users.txt 对应内存中的 vector<User>。
class UserRepository {
public:
    explicit UserRepository(const std::string& filePath);
    bool load(std::vector<User>& users) const;
    bool save(const std::vector<User>& users) const;

private:
    std::string filePath_;
};

// admin.txt 只保存一个 Admin 对象，加载时读取首条有效记录。
class AdminRepository {
public:
    explicit AdminRepository(const std::string& filePath);
    bool load(Admin& admin) const;
    bool save(const Admin& admin) const;

private:
    std::string filePath_;
};

// couriers.txt 对应 vector<Courier>，停用快递员仍保留历史记录。
class CourierRepository {
public:
    explicit CourierRepository(const std::string& filePath);
    bool load(std::vector<Courier>& couriers) const;
    bool save(const std::vector<Courier>& couriers) const;

private:
    std::string filePath_;
};

// expresses.txt 对应 vector<Express>，反序列化兼容历史字段数量。
class ExpressRepository {
public:
    explicit ExpressRepository(const std::string& filePath);
    bool load(std::vector<Express>& expresses) const;
    bool save(const std::vector<Express>& expresses) const;

private:
    std::string filePath_;
};

// auth_state.txt 保存各角色登录失败次数，与账号主体数据分离。
class AuthStateRepository {
public:
    explicit AuthStateRepository(const std::string& filePath);
    bool load(std::vector<AuthState>& states) const;
    bool save(const std::vector<AuthState>& states) const;

private:
    std::string filePath_;
};

// notifications.txt 是附加数据；业务核心采用尽力持久化策略。
class NotificationRepository {
public:
    explicit NotificationRepository(const std::string& filePath);
    bool load(std::vector<Notification>& notifications) const;
    bool save(const std::vector<Notification>& notifications) const;

private:
    std::string filePath_;
};

#endif
