// =============================================================================
// HashUtil.h - 无状态 SHA-256 基础工具
// =============================================================================
// 被 PasswordHasher、SessionManager 和 Logger 共同使用：
//   密码摘要、会话 Token、审计日志哈希链。
// 本类只提供摘要算法，不负责盐值策略、Token 生命周期或日志字段设计。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_SECURITY_HASH_UTIL_H
#define LOGISTICS_V3_COMMON_SECURITY_HASH_UTIL_H

#include <cstdint>
#include <string>

class HashUtil {
public:
    // 返回标准 SHA-256 的 64 位小写十六进制摘要。
    static std::string sha256(const std::string& input);

private:
    // 以下函数对应 SHA-256 规范中的布尔函数和大小 Sigma 函数。
    static uint32_t rotateRight(uint32_t value, uint32_t bits);
    static uint32_t choose(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t majority(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t bigSigma0(uint32_t x);
    static uint32_t bigSigma1(uint32_t x);
    static uint32_t smallSigma0(uint32_t x);
    static uint32_t smallSigma1(uint32_t x);
};

#endif
