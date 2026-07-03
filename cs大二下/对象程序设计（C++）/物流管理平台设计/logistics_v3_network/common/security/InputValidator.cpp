#include "InputValidator.h"

#include <cctype>
#include <algorithm>

// ============================================================
// Legacy aliases — delegate to enhanced validators
// ============================================================

bool InputValidator::validUsername(const std::string& username) {
    return validateUsername(username);
}

bool InputValidator::validPassword(const std::string& password) {
    return validatePasswordStrength(password);
}

bool InputValidator::validPhone(const std::string& phone) {
    return validatePhone(phone);
}

bool InputValidator::validPositiveAmount(double value) {
    return validateAmount(value);
}

// ============================================================
// Enhanced validators — return bool
// ============================================================

bool InputValidator::validateUsername(const std::string& username) {
    return checkUsername(username).empty();
}

bool InputValidator::validatePhone(const std::string& phone) {
    return checkPhone(phone).empty();
}

bool InputValidator::validatePasswordStrength(const std::string& password) {
    return checkPasswordStrength(password).empty();
}

bool InputValidator::validateName(const std::string& name) {
    return checkName(name).empty();
}

bool InputValidator::validateAddress(const std::string& address) {
    return checkAddress(address).empty();
}

bool InputValidator::validateAmount(double value) {
    // 金额和物品数量共用该基础规则，更细的业务上限由调用方负责。
    return value > 0.0;
}

// ============================================================
// Detailed validation — return error message (empty = valid)
// ============================================================

std::string InputValidator::checkUsername(const std::string& username) {
    if (username.size() < 3) {
        return "用户名长度至少为 3 个字符。";
    }
    if (username.size() > 32) {
        return "用户名长度不能超过 32 个字符。";
    }
    for (char ch : username) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!std::isalnum(value) && ch != '_') {
            return "用户名只能包含字母、数字和下划线。";
        }
    }
    return "";
}

std::string InputValidator::checkPhone(const std::string& phone) {
    if (phone.size() != 11) {
        return "手机号必须为 11 位数字。";
    }
    if (phone[0] != '1') {
        return "手机号必须以 1 开头。";
    }
    for (char ch : phone) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return "手机号只能包含数字。";
        }
    }
    return "";
}

std::string InputValidator::checkPasswordStrength(const std::string& password) {
    if (password.size() < 8) {
        return "密码长度至少为 8 位。";
    }
    if (password.size() > 64) {
        return "密码长度不能超过 64 位。";
    }
    bool hasAlpha = false;
    bool hasDigit = false;
    bool allSpace = true;
    // 使用 unsigned char 调用 cctype，避免负 char 值造成未定义行为。
    for (char ch : password) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!std::isspace(value)) {
            allSpace = false;
        }
        if (std::isalpha(value)) {
            hasAlpha = true;
        }
        if (std::isdigit(value)) {
            hasDigit = true;
        }
    }
    if (allSpace) {
        return "密码不能全为空格。";
    }
    if (!hasAlpha || !hasDigit) {
        return "密码必须同时包含字母和数字。";
    }
    return "";
}

std::string InputValidator::checkName(const std::string& name) {
    if (name.empty()) {
        return "姓名不能为空。";
    }
    if (name.size() > 64) {
        return "姓名长度不能超过 64 个字符。";
    }
    return "";
}

std::string InputValidator::checkAddress(const std::string& address) {
    if (address.empty()) {
        return "地址不能为空。";
    }
    if (address.size() > 256) {
        return "地址长度不能超过 256 个字符。";
    }
    return "";
}

bool InputValidator::passwordUnchanged(const std::string& oldPassword, const std::string& newPassword) {
    return oldPassword == newPassword;
}
