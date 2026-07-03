// =============================================================================
// StorageManager.h - 最底层文本文件 I/O 工具
// =============================================================================
// Repository 负责“实体 <-> 文本行”，StorageManager 负责“文本行 <-> 文件”。
// 它不知道 User/Express 等业务类型，因此可以被所有仓库和 Logger 复用。
// =============================================================================

#ifndef LOGISTICS_V3_COMMON_STORAGE_STORAGE_MANAGER_H
#define LOGISTICS_V3_COMMON_STORAGE_STORAGE_MANAGER_H

#include <string>
#include <vector>

class StorageManager {
public:
    // 按路径逐级创建目录；目录已存在时也视为成功。
    static bool ensureDirectory(const std::string& directory);
    // 文本文件不存在表示“暂无数据”，因此返回成功并给出空集合。
    static bool loadLines(const std::string& filePath, std::vector<std::string>& lines);
    // 先写临时文件，再用备份文件保护旧版本，避免直接覆盖导致数据半写入。
    static bool saveLinesAtomically(const std::string& filePath, const std::vector<std::string>& lines);

private:
    // 从目标文件路径中提取目录，供保存前创建父目录。
    static std::string parentDirectory(const std::string& filePath);
    // 仅用于决定是否需要把旧正式文件移动为 .bak。
    static bool fileExists(const std::string& filePath);
};

#endif
