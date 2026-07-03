#include "PasswordHasher.h"

#include "HashUtil.h"

std::string PasswordHasher::makeSalt(const std::string& seed) {
    // 加入固定域前缀，避免与项目中其他 SHA-256 用途直接复用同一输入空间。
    return HashUtil::sha256("logistics_v3_salt|" + seed).substr(0, 16);
}

std::string PasswordHasher::hashPassword(const std::string& salt, const std::string& password) {
    // 盐值放在密码之前，并加入应用标识；登录时使用同样顺序才能得到相同摘要。
    return HashUtil::sha256(salt + "|logistics_v3|" + password);
}
