#include "StorageManager.h"

#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

//最底层文件操作
//创建目录。
//按行读取文本。
//原子替换式保存。

bool StorageManager::ensureDirectory(const std::string& directory) {
    if (directory.empty()) {
        return true;
    }
    std::string current;
    // 同时兼容 Windows 与类 Unix 路径分隔符，并在每一级路径处尝试创建目录。
    for (std::size_t i = 0; i < directory.size(); ++i) {
        const char ch = directory[i];
        current += ch;
        if (ch == '/' || ch == '\\' || i + 1 == directory.size()) {
            if (current == "/" || current == "\\" || current.empty()) {
                continue;
            }
#ifdef _WIN32
            _mkdir(current.c_str());
#else
            mkdir(current.c_str(), 0755);
#endif
        }
    }
    struct stat info {};
    return stat(directory.c_str(), &info) == 0 && (info.st_mode & S_IFDIR) != 0;
}

bool StorageManager::loadLines(const std::string& filePath, std::vector<std::string>& lines) {
    lines.clear();
    std::ifstream input(filePath);
    if (!input.is_open()) {
        // 首次启动时数据文件尚不存在属于正常情况，由后续保存负责创建。
        return true;
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return !input.bad();
}

bool StorageManager::saveLinesAtomically(const std::string& filePath, const std::vector<std::string>& lines) {
    const std::string directory = parentDirectory(filePath);
    if (!directory.empty() && !ensureDirectory(directory)) {
        return false;
    }

    const std::string temp = filePath + ".tmp";
    const std::string backup = filePath + ".bak";

    // 阶段一：完整写入临时文件。只有 flush 后流状态正常才进入替换阶段。
    {
        std::ofstream output(temp, std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        for (const std::string& line : lines) {
            output << line << '\n';
        }
        output.flush();
        if (!output.good()) {
            std::remove(temp.c_str());
            return false;
        }
    }

    // 阶段二：把现有正式文件移为备份，保留最近一次可恢复版本。
    std::remove(backup.c_str());
    if (fileExists(filePath)) {
        if (std::rename(filePath.c_str(), backup.c_str()) != 0) {
            std::remove(temp.c_str());
            return false;
        }
    }

    // 阶段三：临时文件升级为正式文件；失败时尽力恢复刚才的备份。
    if (std::rename(temp.c_str(), filePath.c_str()) != 0) {
        if (fileExists(backup)) {
            std::rename(backup.c_str(), filePath.c_str());
        }
        std::remove(temp.c_str());
        return false;
    }
    return true;
}

std::string StorageManager::parentDirectory(const std::string& filePath) {
    const std::size_t slash = filePath.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "";
    }
    return filePath.substr(0, slash);
}

bool StorageManager::fileExists(const std::string& filePath) {
    std::ifstream input(filePath);
    return input.good();
}
