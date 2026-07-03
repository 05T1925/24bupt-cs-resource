// =============================================================================
// InputValidator.h - 统一输入规则
// =============================================================================
// 客户端可用 bool 接口提前提示，服务端必须使用详细接口再次校验。
// 客户端校验只改善体验，不能替代 LogisticsSystem 的服务端安全边界。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SECURITY_INPUT_VALIDATOR_H
#define LOGISTICS_V3_COMMON_SECURITY_INPUT_VALIDATOR_H

#include <string>

class InputValidator {
public:
    // 兼容旧调用点的 bool 别名，内部委托给增强校验器。
    static bool validUsername(const std::string& username);
    static bool validPassword(const std::string& password);
    static bool validPhone(const std::string& phone);
    static bool validPositiveAmount(double value);

    // 简洁接口：适合只关心合法/非法的调用方。
    static bool validateUsername(const std::string& username);
    static bool validatePhone(const std::string& phone);
    static bool validatePasswordStrength(const std::string& password);
    static bool validateName(const std::string& name);
    static bool validateAddress(const std::string& address);
    static bool validateAmount(double value);

    // 详细接口：空字符串表示合法，否则返回可直接展示的中文原因。
    static std::string checkUsername(const std::string& username);
    static std::string checkPhone(const std::string& phone);
    static std::string checkPasswordStrength(const std::string& password);
    static std::string checkName(const std::string& name);
    static std::string checkAddress(const std::string& address);

    // 返回 true 表示新旧密码相同，调用方应拒绝本次修改。
    static bool passwordUnchanged(const std::string& oldPassword, const std::string& newPassword);
};

#endif
