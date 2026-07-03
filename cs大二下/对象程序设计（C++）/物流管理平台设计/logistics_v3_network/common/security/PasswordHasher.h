// =============================================================================
// PasswordHasher.h - 密码存储策略封装
// =============================================================================
// LogisticsSystem 在注册、登录和改密时调用本类；Entity 只保存 salt/hash。
// HashUtil 提供底层 SHA-256，本类负责加入应用域字符串并组织输入。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SECURITY_PASSWORD_HASHER_H
#define LOGISTICS_V3_COMMON_SECURITY_PASSWORD_HASHER_H

#include <string>

class PasswordHasher {
public:
    // 由账号相关种子派生固定长度盐值，存储时只保留盐和摘要。
    static std::string makeSalt(const std::string& seed);
    // 将盐、应用域标识和密码组合后计算摘要，避免直接保存密码。
    static std::string hashPassword(const std::string& salt, const std::string& password);
};

#endif
