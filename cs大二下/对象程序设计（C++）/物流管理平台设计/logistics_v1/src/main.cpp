#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <clocale>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <streambuf>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/**
 * 物流管理平台实验一：单机控制台版。
 *
 整个代码虽然写在一个 main.cpp 里，
 但我按面向对象思想做了分层：工具层、实体层、仓储层、业务层和 UI 层。
 这样主函数非常简单，只负责初始化控制台环境、创建 App 对象并运行，
 具体业务逻辑都封装在类里面。
 
 * 路线：
 * 1. 工具层：字符串、目录、编码、SHA-256、输入校验。
 * 2. 实体层：User/Admin/Express/LogEntry 只保存数据和基础状态变化。
 * 3. 仓储层：Repository 负责对象和 txt 文件之间的转换。
 * 4. 业务层：LogisticsSystem 集中处理权限、扣费、单号、保存回滚。
 * 5. UI 层：ConsoleUI/App 只负责菜单和输入输出。
 *
 * C++ 口径：
 * std::string/std::vector 可理解为自动管理内存的 char* 和动态数组；
 * const 引用参数避免复制且不修改原对象；文件流对象离开作用域会自动关闭，体现 RAII。
 */



/** 快递状态枚举，属于实体层的基础值对象。 */
enum class ExpressStatus {
    WaitingSign = 0,
    Signed = 1
};

/* ========================== 工具层：通用函数和平台兼容 ========================== */
//StringUtil 主要处理字符串、时间、金额格式和文件字段转义。比如数据文件用 | 分隔字段，
//如果用户输入里也有 | 或换行，就会破坏文件格式，所以用 escape 和 unescape 做转义和还原。
/** 字符串、时间、金额和持久化文本的通用工具类。 */
class StringUtil {
public:
    /** 去除字符串左右两侧空白字符。 */
    static std::string trim(const std::string& value) {
        std::size_t begin = 0;
        // static_cast<unsigned char> 避免 char 为负数时传给 ctype 函数产生未定义行为。
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(begin, end - begin);
    }

    /** 判断字符串是否包含指定片段。 */
    static bool contains(const std::string& text, const std::string& part) {
        return part.empty() || text.find(part) != std::string::npos;
    }

    /** 按单个字符分割字符串。 */
    static std::vector<std::string> split(const std::string& line, char delimiter) {
        std::vector<std::string> result;
        std::string current;
        // istringstream 把字符串包装成输入流，逻辑类似从 FILE* 中逐段读取，但类型更安全。
        std::istringstream stream(line);
        while (std::getline(stream, current, delimiter)) {
            result.push_back(current);
        }
        // 保留末尾空字段，确保反序列化时字段数量判断与文件真实结构一致。
        if (!line.empty() && line.back() == delimiter) {
            result.push_back("");
        }
        return result;
    }

    /** 转义持久化文本中的特殊字符。 */
    static std::string escape(const std::string& value) {
        std::string result;
        for (char ch : value) {
            if (ch == '%') {
                result += "%25";
            } else if (ch == '|') {
                result += "%7C";
            } else if (ch == '\n') {
                result += "%0A";
            } else if (ch == '\r') {
                result += "%0D";
            } else {
                result += ch;
            }
        }
        return result;
    }

    /** 还原持久化文本中的转义字符。 */
    static std::string unescape(const std::string& value) {
        std::string result;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '%' && i + 2 < value.size()) {
                std::string code = value.substr(i, 3);
                if (code == "%25") {
                    result += '%';
                    i += 2;
                } else if (code == "%7C") {
                    result += '|';
                    i += 2;
                } else if (code == "%0A") {
                    result += '\n';
                    i += 2;
                } else if (code == "%0D") {
                    result += '\r';
                    i += 2;
                } else {
                    result += value[i];
                }
            } else {
                result += value[i];
            }
        }
        return result;
    }

    /** 将 double 格式化为两位小数。 */
    static std::string moneyToString(double value) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << value;
        return out.str();
    }


    /** 读取当前时间，格式为 YYYY-MM-DD HH:MM:SS。 */
    static std::string now() {
        std::time_t rawTime = std::time(nullptr);
        std::tm timeInfo{};
#ifdef _WIN32
        localtime_s(&timeInfo, &rawTime);
#else
        localtime_r(&rawTime, &timeInfo);
#endif
        std::ostringstream out;
        out << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");
        return out.str();
    }

    /** 读取紧凑时间戳，用于生成快递单号。 */
    static std::string compactNow() {
        std::time_t rawTime = std::time(nullptr);
        std::tm timeInfo{};
#ifdef _WIN32
        localtime_s(&timeInfo, &rawTime);
#else
        localtime_r(&rawTime, &timeInfo);
#endif
        std::ostringstream out;
        out << std::put_time(&timeInfo, "%Y%m%d%H%M%S");
        return out.str();
    }

    /** 判断字符串是否完全由数字组成。 */
    static bool isDigits(const std::string& value) {
        if (value.empty()) {
            return false;
        }
        for (char ch : value) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return false;
            }
        }
        return true;
    }

    /** 严格解析完整 double 文本，拒绝夹杂字符。 */
    static bool tryParseDouble(const std::string& text, double& value) {
        std::istringstream in(text);
        in >> value;
        if (!in) {
            return false;
        }
        char extra = '\0';
        return !(in >> extra);
    }

    /** 尝试解析正数金额，要求最多两位小数。 */
    static bool tryParsePositiveMoney(const std::string& text, double& value) {
        if (text.empty()) {
            return false;
        }
        std::size_t dotCount = 0;
        std::size_t decimalCount = 0;
        bool afterDot = false;
        bool hasDigit = false;
        for (char ch : text) {
            if (ch == '.') {
                ++dotCount;
                afterDot = true;
                if (dotCount > 1) {
                    return false;
                }
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                return false;
            }
            hasDigit = true;
            if (afterDot) {
                ++decimalCount;
                if (decimalCount > 2) {
                    return false;
                }
            }
        }
        if (!hasDigit || text.front() == '.' || text.back() == '.') {
            return false;
        }
        return tryParseDouble(text, value) && value > 0.0;
    }
};


//DirectoryUtil 负责目录判断、创建和路径拼接。
//项目从根目录、src 或 bin 启动时，可能会导致数据目录不一致，
//所以这里有 projectDataDirectory() 来尽量统一主数据目录。

/** 目录和文本文件复制工具，属于仓储层下方的文件系统适配层。 */
class DirectoryUtil {
public:
    /** 判断路径是否存在。 */
    static bool exists(const std::string& path) {
        struct stat info;
        return stat(path.c_str(), &info) == 0;
    }

    /** 创建目录；目录已存在时视为成功。 */
    static bool createDirectory(const std::string& path) {
#ifdef _WIN32
        int result = _mkdir(path.c_str());
#else
        int result = mkdir(path.c_str(), 0755);
#endif
        return result == 0 || errno == EEXIST;
    }

    /** 拼接数据目录和文件名。 */
    static std::string join(const std::string& dir, const std::string& fileName) {
        if (dir.empty()) {
            return fileName;
        }
        char last = dir[dir.size() - 1];
        if (last == '/' || last == '\\') {
            return dir + fileName;
        }
        return dir + "/" + fileName;
    }

    /** 解析项目主数据目录，避免从 bin 或 src 启动时生成错位 data 目录。 */
    static std::string projectDataDirectory() {
        if (exists("src") && exists("bin")) {
            return "data";
        }
        if (exists("../src") && exists("../bin")) {
            return "../data";
        }
        return "data";
    }

    /** 复制文本数据文件到镜像目录。 */
    static void copyTextFile(const std::string& source, const std::string& target) {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!in || !out) {
            return;
        }
        out << in.rdbuf();
    }
};

#ifdef _WIN32
/** Windows 中文控制台输出缓冲区适配器。 */
class WindowsConsoleOutputBuffer : public std::streambuf {
private:
    std::streambuf* fallback_;
    HANDLE console_;
    UINT sourceCodePage_;
    std::string buffer_;

    /** 判断当前标准输出是否仍连接到真实控制台。 */
    bool isConsole() const {
        DWORD mode = 0;
        return console_ != INVALID_HANDLE_VALUE && GetConsoleMode(console_, &mode) != 0;
    }

    /** 将暂存的多字节输出写入控制台或回退缓冲区。 */
    void writeToConsole() {
        if (buffer_.empty()) {
            return;
        }
        if (!isConsole()) {
            fallback_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
            buffer_.clear();
            return;
        }

        // 第一次调用只计算 UTF-16 字符数，第二次调用才真正执行转换。
        int wideLength = MultiByteToWideChar(sourceCodePage_, 0, buffer_.data(),
                                             static_cast<int>(buffer_.size()), nullptr, 0);
        if (wideLength <= 0) {
            fallback_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
            buffer_.clear();
            return;
        }

        std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
        MultiByteToWideChar(sourceCodePage_, 0, buffer_.data(), static_cast<int>(buffer_.size()),
                            &wide[0], wideLength);
        DWORD written = 0;
        WriteConsoleW(console_, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
        buffer_.clear();
    }

protected:
    /** 处理单字符写入，覆盖 streambuf 虚函数以接管 cout 输出路径。 */
    int overflow(int ch) override {
        if (ch != traits_type::eof()) {
            buffer_ += static_cast<char>(ch);
            writeToConsole();
        }
        return ch;
    }

    /** 处理批量字符写入，避免每个字符都单独进入底层控制台 API。 */
    std::streamsize xsputn(const char* text, std::streamsize count) override {
        buffer_.append(text, static_cast<std::size_t>(count));
        writeToConsole();
        return count;
    }

    /** 响应 flush，确保缓冲区内容及时落到控制台。 */
    int sync() override {
        writeToConsole();
        return 0;
    }

public:
    /** 构造输出缓冲区适配器。 */
    explicit WindowsConsoleOutputBuffer(std::streambuf* fallback)
        : fallback_(fallback), console_(GetStdHandle(STD_OUTPUT_HANDLE)), sourceCodePage_(GetACP()) {}
};
#endif

/** 控制台运行环境初始化器。 */
class ConsoleEnvironment {
public:
    /** 初始化控制台中文环境。 */
    static void initialize() {
        std::setlocale(LC_ALL, "");
#ifdef _WIN32
        UINT codePage = GetACP();
        SetConsoleOutputCP(codePage);
        SetConsoleCP(codePage);
        static WindowsConsoleOutputBuffer outputBuffer(std::cout.rdbuf());
        std::cout.rdbuf(&outputBuffer);
#endif
    }
};


//SHA256 和 PasswordHasher 是密码安全相关代码。系统不保存明文密码，而是保存 salt + SHA256 hash。
//登录时用同样的 salt 和输入密码重新算 hash，再和文件里的 hash 比较。
//这样即使数据文件被看到，也不能直接得到用户密码。

/** 自实现 SHA-256 摘要算法工具类。 */   //随机盐
class SHA256 {
private:
    /** SHA-256 的循环右移操作。 */
    static uint32_t rotateRight(uint32_t value, uint32_t bits) {
        return (value >> bits) | (value << (32U - bits));
    }

    /** 将 32 位整数写入十六进制字符串。 */
    static void appendHex(std::ostringstream& out, uint32_t value) {
        out << std::hex << std::setfill('0') << std::setw(8) << value;
    }

public:
    /** 计算输入字符串的 SHA-256 摘要。 */
    static std::string hash(const std::string& input) {
        static const std::array<uint32_t, 64> constants = {
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
        };

        // 把 std::string 视为字节序列处理；vector 自动管理扩容，类似安全版 unsigned char* 缓冲区。
        std::vector<uint8_t> bytes(input.begin(), input.end());
        uint64_t bitLength = static_cast<uint64_t>(bytes.size()) * 8U;
        // SHA-256 标准填充：先追加 1 bit，再补 0，最后写入原始 bit 长度。
        bytes.push_back(0x80U);
        while ((bytes.size() % 64U) != 56U) {
            bytes.push_back(0U);
        }
        for (int i = 7; i >= 0; --i) {
            bytes.push_back(static_cast<uint8_t>((bitLength >> (i * 8)) & 0xffU));
        }

        uint32_t h0 = 0x6a09e667U;
        uint32_t h1 = 0xbb67ae85U;
        uint32_t h2 = 0x3c6ef372U;
        uint32_t h3 = 0xa54ff53aU;
        uint32_t h4 = 0x510e527fU;
        uint32_t h5 = 0x9b05688cU;
        uint32_t h6 = 0x1f83d9abU;
        uint32_t h7 = 0x5be0cd19U;

        // 每 512 bit 为一个分组执行压缩函数。
        for (std::size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
            std::array<uint32_t, 64> words{};
            for (std::size_t i = 0; i < 16U; ++i) {
                std::size_t j = chunk + i * 4U;
                words[i] = (static_cast<uint32_t>(bytes[j]) << 24U) |
                           (static_cast<uint32_t>(bytes[j + 1U]) << 16U) |
                           (static_cast<uint32_t>(bytes[j + 2U]) << 8U) |
                           static_cast<uint32_t>(bytes[j + 3U]);
            }
            for (std::size_t i = 16U; i < 64U; ++i) {
                uint32_t s0 = rotateRight(words[i - 15U], 7U) ^ rotateRight(words[i - 15U], 18U) ^ (words[i - 15U] >> 3U);
                uint32_t s1 = rotateRight(words[i - 2U], 17U) ^ rotateRight(words[i - 2U], 19U) ^ (words[i - 2U] >> 10U);
                words[i] = words[i - 16U] + s0 + words[i - 7U] + s1;
            }

            // 工作变量 a-h 保存本轮压缩的中间状态，最后累加回全局哈希状态。
            uint32_t a = h0;
            uint32_t b = h1;
            uint32_t c = h2;
            uint32_t d = h3;
            uint32_t e = h4;
            uint32_t f = h5;
            uint32_t g = h6;
            uint32_t h = h7;

            for (std::size_t i = 0; i < 64U; ++i) {
                uint32_t s1 = rotateRight(e, 6U) ^ rotateRight(e, 11U) ^ rotateRight(e, 25U);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = h + s1 + ch + constants[i] + words[i];
                uint32_t s0 = rotateRight(a, 2U) ^ rotateRight(a, 13U) ^ rotateRight(a, 22U);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = s0 + maj;
                h = g;
                g = f;
                f = e;
                e = d + temp1;
                d = c;
                c = b;
                b = a;
                a = temp1 + temp2;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
            h5 += f;
            h6 += g;
            h7 += h;
        }

        // 拼接 8 个 32 位状态字，形成标准 SHA-256 十六进制输出。
        std::ostringstream out;
        appendHex(out, h0);
        appendHex(out, h1);
        appendHex(out, h2);
        appendHex(out, h3);
        appendHex(out, h4);
        appendHex(out, h5);
        appendHex(out, h6);
        appendHex(out, h7);
        return out.str();
    }
};

/** 密码加盐哈希工具，封装认证相关的底层安全策略。 */
class PasswordHasher {
public:
    /** 当前版本默认管理员密码。 */
    static std::string defaultAdminPassword() {
        return "Admin0219";
    }

    /** 生成用户专属随机盐。 */
    static std::string makeSalt(const std::string& username) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned long long> dist;
        std::ostringstream out;
        out << username << "-" << StringUtil::compactNow() << "-" << std::hex << dist(gen);
        return SHA256::hash(out.str()).substr(0, 24);
    }

    /** 根据随机盐和明文密码计算哈希值。 */
    static std::string hashPassword(const std::string& salt, const std::string& password) {
        return SHA256::hash(salt + "|logistics_v1|" + password);
    }

    /** 验证明文密码是否与已存储哈希一致。 */
    static bool verify(const std::string& salt, const std::string& password, const std::string& storedHash) {
        return hashPassword(salt, password) == storedHash;
    }
};

//InputValidator 集中做输入校验，比如用户名、手机号、密码、金额、时间、快递单号等。
//这样 UI 层和业务层都可以复用同一套规则，避免每个流程自己写一遍判断。

/** 集中式输入校验器，负责 UI 层和业务层共同使用的合法性规则。 */
class InputValidator {
private:
    /** 将固定宽度数字片段转换为整数，失败时返回 -1。 */
    static int parseNumberPart(const std::string& value, std::size_t begin, std::size_t length) {
        if (begin + length > value.size()) {
            return -1;
        }
        int result = 0;
        for (std::size_t i = begin; i < begin + length; ++i) {
            if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
                return -1;
            }
            result = result * 10 + (value[i] - '0');
        }
        return result;
    }

    /** 返回指定年月的最大天数。 */
    static int daysInMonth(int year, int month) {
        static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12) {
            return 0;
        }
        bool leap = (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
        if (month == 2 && leap) {
            return 29;
        }
        return days[month - 1];
    }

    /** 检查文本中是否含有不可见控制字符。 */
    static bool hasControlCharacter(const std::string& value) {
        for (char ch : value) {
            unsigned char current = static_cast<unsigned char>(ch);
            if (current < 32 || current == 127) {
                return true;
            }
        }
        return false;
    }

public:
    /** 输入校验类集中放置所有格式规则。 */

    /** 检查通用筛选关键字，避免日志和搜索条件中出现控制字符或过长文本。 */
    static bool isValidOptionalKeyword(const std::string& value, const std::string& fieldName,
                                       std::string& reason) {
        if (value.empty()) {
            return true;
        }
        if (hasControlCharacter(value)) {
            reason = fieldName + "不能包含控制字符。";
            return false;
        }
        if (value.size() > 30) {
            reason = fieldName + "长度不能超过 30 个字符。";
            return false;
        }
        return true;
    }

    /** 检查用户名：唯一标识只允许字母、数字和下划线。 */
    static bool isValidUsername(const std::string& username, std::string& reason) {
        if (username.size() < 3 || username.size() > 20) {
            reason = "用户名长度必须为 3-20 位。";
            return false;
        }
        for (char ch : username) {
            bool ok = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
            if (!ok) {
                reason = "用户名只能包含字母、数字和下划线。";
                return false;
            }
        }
        return true;
    }

    /** 检查姓名或地址等必填文本。 */
    static bool isNotBlank(const std::string& value, const std::string& fieldName, std::string& reason) {
        if (StringUtil::trim(value).empty()) {
            reason = fieldName + "不能为空。";
            return false;
        }
        if (hasControlCharacter(value)) {
            reason = fieldName + "不能包含控制字符。";
            return false;
        }
        return true;
    }

    /** 检查姓名：允许中文、字母等普通可见字符。 */
    static bool isValidName(const std::string& name, std::string& reason) {
        if (!isNotBlank(name, "姓名", reason)) {
            return false;
        }
        if (name.size() > 30) {
            reason = "姓名长度不能超过 30 个字符。";
            return false;
        }
        return true;
    }

    /** 检查地址：不能为空，长度适中，不能包含控制字符。 */
    static bool isValidAddress(const std::string& address, std::string& reason) {
        if (!isNotBlank(address, "地址", reason)) {
            return false;
        }
        if (address.size() < 2 || address.size() > 80) {
            reason = "地址长度必须为 2-80 个字符。";
            return false;
        }
        return true;
    }

    /** 检查中国大陆手机号的基本格式。 */
    static bool isValidPhone(const std::string& phone, std::string& reason) {
        if (phone.size() != 11 || !StringUtil::isDigits(phone)) {
            reason = "电话必须为 11 位数字。";
            return false;
        }
        return true;
    }

    /** 检查密码强度：至少包含字母和数字。 */
    static bool isValidPassword(const std::string& password, std::string& reason) {
        if (password.size() < 6 || password.size() > 30) {
            reason = "密码长度必须为 6-30 位。";
            return false;
        }
        bool hasLetter = false;
        bool hasDigit = false;
        for (char ch : password) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                reason = "密码不能包含空白字符。";
                return false;
            }
            if (std::isalpha(static_cast<unsigned char>(ch))) {
                hasLetter = true;
            }
            if (std::isdigit(static_cast<unsigned char>(ch))) {
                hasDigit = true;
            }
        }
        if (!hasLetter || !hasDigit) {
            reason = "密码必须同时包含字母和数字。";
            return false;
        }
        return true;
    }

    /** 检查物品描述。 */
    static bool isValidDescription(const std::string& description, std::string& reason) {
        if (!isNotBlank(description, "物品描述", reason)) {
            return false;
        }
        if (description.size() > 60) {
            reason = "物品描述不能超过 60 个字符。";
            return false;
        }
        return true;
    }

    /** 检查可选用户名条件。 */
    static bool isValidOptionalUsername(const std::string& username, const std::string& fieldName,
                                        std::string& reason) {
        if (username.empty()) {
            return true;
        }
        if (!isValidUsername(username, reason)) {
            reason = fieldName + "不合法：" + reason;
            return false;
        }
        return true;
    }

    /** 检查可选快递单号条件。 */
    static bool isValidOptionalExpressId(const std::string& id, std::string& reason) {
        if (id.empty()) {
            return true;
        }
        if (id.size() < 4 || id.size() > 30 || id.substr(0, 2) != "EX") {
            reason = "快递单号格式不合法，应以 EX 开头。";
            return false;
        }
        for (char ch : id) {
            if (!std::isalnum(static_cast<unsigned char>(ch))) {
                reason = "快递单号只能包含字母和数字。";
                return false;
            }
        }
        return true;
    }

    /** 检查可选时间条件，格式必须为 YYYY-MM-DD HH:MM:SS。 */
    static bool isValidDateTimeOrEmpty(const std::string& value, const std::string& fieldName,
                                       std::string& reason) {
        if (value.empty()) {
            return true;
        }
        if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
            value[10] != ' ' || value[13] != ':' || value[16] != ':') {
            reason = fieldName + "格式必须为 YYYY-MM-DD HH:MM:SS。";
            return false;
        }
        int year = parseNumberPart(value, 0, 4);
        int month = parseNumberPart(value, 5, 2);
        int day = parseNumberPart(value, 8, 2);
        int hour = parseNumberPart(value, 11, 2);
        int minute = parseNumberPart(value, 14, 2);
        int second = parseNumberPart(value, 17, 2);
        if (year < 2000 || month < 1 || month > 12 || day < 1 ||
            day > daysInMonth(year, month) || hour < 0 || hour > 23 ||
            minute < 0 || minute > 59 || second < 0 || second > 59) {
            reason = fieldName + "不是合法日期时间。";
            return false;
        }
        return true;
    }
};

/* ========================== 实体层：系统中的核心数据对象 ========================== */

//User 表示普通用户，保存用户名、姓名、电话、密码 salt、密码 hash、余额、地址和冻结状态。
//成员变量都是 private，外部不能直接改，只能通过函数访问或修改，这体现了封装。
/** 普通用户实体类，属于系统“实体-仓储-服务-UI”分层中的实体层。 */
class User {
private:
    std::string username_;      // 平台唯一用户名，是登录、权限判断和快递归属判断的主键。
    std::string name_;          // 用户真实姓名，只作为业务展示信息，不参与认证。
    std::string phone_;         // 用户电话，注册时由服务层保证唯一性。
    std::string salt_;          // 密码随机盐；与 passwordHash_ 配对保存，防止相同密码产生相同哈希。
    std::string passwordHash_;  // 加盐后的密码哈希；实体内不保存明文密码，降低数据文件泄漏风险。
    double balance_;            // 用户账户余额；寄件扣费和充值会围绕该字段做保存失败回滚。
    std::string address_;       // 用户默认地址；V1 保存基础收寄信息，V2 可扩展为地址簿。
    bool frozen_;               // 账号是否被冻结；服务层登录和寄件流程会据此做权限拦截。

public:
    /** 构造空用户，便于反序列化时填充。 */
    User() : balance_(0.0), frozen_(false) {}

    /** 构造完整用户对象。 */
    User(const std::string& username, const std::string& name, const std::string& phone,
         const std::string& salt, const std::string& passwordHash, double balance,
         const std::string& address, bool frozen)
        : username_(username), name_(name), phone_(phone), salt_(salt),
          passwordHash_(passwordHash), balance_(balance), address_(address), frozen_(frozen) {}

    /** 获取用户名；const 成员函数承诺不会改变用户对象状态。 */
    const std::string& username() const { return username_; }

    /** 获取姓名；返回 const 引用避免复制，同时禁止外部通过引用改写内部字段。 */
    const std::string& name() const { return name_; }

    /** 获取电话；属于只读展示接口。 */
    const std::string& phone() const { return phone_; }

    /** 获取密码随机盐；认证服务会用它重新计算输入密码的 hash。 */
    const std::string& salt() const { return salt_; }

    /** 获取密码哈希；只暴露 hash，不暴露也不保存明文密码。 */
    const std::string& passwordHash() const { return passwordHash_; }

    /** 获取余额；返回值类型为 double，复制成本很低，无需引用。 */
    double balance() const { return balance_; }

    /** 获取默认地址；const 引用保持只读封装边界。 */
    const std::string& address() const { return address_; }

    /** 判断账号是否被冻结；登录、寄件等服务层流程会基于该值做鉴权。 */
    bool frozen() const { return frozen_; }

    /** 更新密码随机盐和哈希。 */
    void updatePassword(const std::string& salt, const std::string& passwordHash) {
        salt_ = salt;
        passwordHash_ = passwordHash;
    }

    /** 账户充值或余额调整。 */
    void recharge(double amount) {
        balance_ += amount;
    }

    /** 扣除余额。 */
    bool deduct(double amount) {
        if (balance_ + 0.0001 < amount) {
            return false;
        }
        balance_ -= amount;
        return true;
    }

    /** 设置账号冻结状态。 */
    void setFrozen(bool frozen) {
        frozen_ = frozen;
    }

    /** 序列化为 users.txt 中的一行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(username_) << '|'
            << StringUtil::escape(name_) << '|'
            << StringUtil::escape(phone_) << '|'
            << StringUtil::escape(salt_) << '|'
            << StringUtil::escape(passwordHash_) << '|'
            << StringUtil::moneyToString(balance_) << '|'
            << StringUtil::escape(address_) << '|'
            << (frozen_ ? "1" : "0");
        return out.str();
    }

    /** 从 users.txt 的一行反序列化用户。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 8) {
            return false;
        }
        username_ = StringUtil::unescape(parts[0]);
        name_ = StringUtil::unescape(parts[1]);
        phone_ = StringUtil::unescape(parts[2]);
        salt_ = StringUtil::unescape(parts[3]);
        passwordHash_ = StringUtil::unescape(parts[4]);
        if (!StringUtil::tryParseDouble(parts[5], balance_) || balance_ < 0.0) {
            return false;
        }
        address_ = StringUtil::unescape(parts[6]);
        if (parts[7] != "0" && parts[7] != "1") {
            return false;
        }
        frozen_ = parts[7] == "1";
        return true;
    }
};

//Admin 表示管理员和平台账户，里面有管理员账号、密码 hash 和平台余额。用户寄件扣费后，平台余额会增加。
/** 管理员实体类，代表平台管理者和物流公司账户。 */
class Admin {
private:
    std::string username_;      // 管理员用户名，V1 默认 admin。
    std::string name_;          // 管理员展示名称，默认 SystemAdmin。
    std::string salt_;          // 管理员密码随机盐；与用户密码策略保持一致。
    std::string passwordHash_;  // 管理员加盐密码哈希；避免管理员口令明文落盘。
    double balance_;            // 物流公司账户余额；用户寄件扣费成功后会增加该余额。

public:
    /** 构造默认管理员对象。 */
    Admin() : username_("admin"), name_("SystemAdmin"), balance_(0.0) {
        salt_ = PasswordHasher::makeSalt(username_);
        passwordHash_ = PasswordHasher::hashPassword(salt_, PasswordHasher::defaultAdminPassword());
    }

    /** 获取管理员用户名；const 表明该查询不会改变管理员对象。 */
    const std::string& username() const { return username_; }

    /** 获取管理员姓名。 */
    const std::string& name() const { return name_; }

    /** 获取密码随机盐，用于管理员登录验证。 */
    const std::string& salt() const { return salt_; }

    /** 获取密码哈希；认证比较只基于 hash，不接触明文。 */
    const std::string& passwordHash() const { return passwordHash_; }

    /** 获取公司账户余额。 */
    double balance() const { return balance_; }

    /** 增加公司账户余额。 */
    void addBalance(double amount) {
        balance_ += amount;
    }

    /** 重置管理员密码，用于默认密码调整或实验验收前恢复。 */
    void resetPassword(const std::string& password) {
        salt_ = PasswordHasher::makeSalt(username_);
        passwordHash_ = PasswordHasher::hashPassword(salt_, password);
    }

    /** 序列化管理员数据为 admin.txt 中的一行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(username_) << '|'
            << StringUtil::escape(name_) << '|'
            << StringUtil::escape(salt_) << '|'
            << StringUtil::escape(passwordHash_) << '|'
            << StringUtil::moneyToString(balance_);
        return out.str();
    }

    /** 从 admin.txt 的一行反序列化管理员。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 5) {
            return false;
        }
        username_ = StringUtil::unescape(parts[0]);
        name_ = StringUtil::unescape(parts[1]);
        salt_ = StringUtil::unescape(parts[2]);
        passwordHash_ = StringUtil::unescape(parts[3]);
        return StringUtil::tryParseDouble(parts[4], balance_) && balance_ >= 0.0;
    }
};

//Express 表示一张快递运单，包含单号、发件人、收件人、寄送时间、签收时间、状态、物品描述和费用。
//里面有 belongsTo() 判断某个用户是否和快递有关，这是后面权限过滤的基础。
/** 快递实体类，表示一张物流运单。 */
class Express {
private:
    std::string id_;           // 快递单号，服务层生成并保证唯一。
    std::string sender_;       // 发件用户名，用于普通用户查询权限和管理员筛选。
    std::string receiver_;     // 收件用户名，用于签收权限判断。
    std::string sendTime_;     // 寄送时间，格式由 StringUtil::now() 统一生成。
    std::string receiveTime_;  // 签收时间；待签收时为空，签收后写入当前时间。
    ExpressStatus status_;     // 当前快递状态；V1 为待签收/已签收，V2 可扩展待揽收。
    std::string description_;  // 物品描述；V2 可进一步演进为物品分类和多态计费对象。
    double fee_;               // 本次快递费用；V1 固定 15.00，保存实际费用便于审计。

public:
    /** 构造空快递对象，便于反序列化。 */
    Express() : status_(ExpressStatus::WaitingSign), fee_(15.0) {}

    /** 构造完整快递对象。 */
    Express(const std::string& id, const std::string& sender, const std::string& receiver,
            const std::string& sendTime, const std::string& description, double fee)
        : id_(id), sender_(sender), receiver_(receiver), sendTime_(sendTime),
          receiveTime_(""), status_(ExpressStatus::WaitingSign),
          description_(description), fee_(fee) {}

    /** 获取快递单号；const 引用避免复制较长字符串。 */
    const std::string& id() const { return id_; }

    /** 获取发件人用户名。 */
    const std::string& sender() const { return sender_; }

    /** 获取收件人用户名。 */
    const std::string& receiver() const { return receiver_; }

    /** 获取寄送时间。 */
    const std::string& sendTime() const { return sendTime_; }

    /** 获取签收时间；待签收时为空字符串。 */
    const std::string& receiveTime() const { return receiveTime_; }

    /** 获取快递状态；枚举按值返回，复制成本低。 */
    ExpressStatus status() const { return status_; }

    /** 获取物品描述。 */
    const std::string& description() const { return description_; }

    /** 获取快递费用。 */
    double fee() const { return fee_; }

    /** 判断指定用户是否与本快递有关。 */
    bool belongsTo(const std::string& username) const {
        return sender_ == username || receiver_ == username;
    }

    /** 判断本快递是否尚未签收，用于防止重复签收。 */
    bool isWaitingSign() const {
        return status_ == ExpressStatus::WaitingSign;
    }

    /** 将快递设置为已签收状态。 */
    void sign() {
        status_ = ExpressStatus::Signed;
        receiveTime_ = StringUtil::now();
    }

    /** 获取状态中文名，供 UI 表格和日志详情展示。 */
    std::string statusName() const {
        return status_ == ExpressStatus::WaitingSign ? "待签收" : "已签收";
    }

    /** 序列化为 expresses.txt 中的一行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(id_) << '|'
            << StringUtil::escape(sender_) << '|'
            << StringUtil::escape(receiver_) << '|'
            << StringUtil::escape(sendTime_) << '|'
            << StringUtil::escape(receiveTime_) << '|'
            << static_cast<int>(status_) << '|'
            << StringUtil::escape(description_) << '|'
            << StringUtil::moneyToString(fee_);
        return out.str();
    }

    /** 从 expresses.txt 的一行反序列化快递。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 8) {
            return false;
        }
        id_ = StringUtil::unescape(parts[0]);
        sender_ = StringUtil::unescape(parts[1]);
        receiver_ = StringUtil::unescape(parts[2]);
        sendTime_ = StringUtil::unescape(parts[3]);
        receiveTime_ = StringUtil::unescape(parts[4]);
        int statusValue = 0;
        std::istringstream statusInput(parts[5]);
        statusInput >> statusValue;
        if (!statusInput || (statusValue != 0 && statusValue != 1)) {
            return false;
        }
        status_ = static_cast<ExpressStatus>(statusValue);
        description_ = StringUtil::unescape(parts[6]);
        return StringUtil::tryParseDouble(parts[7], fee_) && fee_ > 0.0;
    }
};

//LogEntry 表示操作日志，记录时间、身份、账号、操作、结果和详情。
//比如登录失败、越权查询、保存失败都会记录，方便验收时追踪系统行为。
//这些实体类都有 serialize() 和 deserialize()，负责对象和文本文件一行数据之间的转换。
/** 操作日志实体类，记录系统可审计行为。 */
class LogEntry {
private:
    std::string time_;       // 操作时间，统一使用 YYYY-MM-DD HH:MM:SS。
    std::string actorType_;  // 操作人类型，例如 USER、ADMIN、SYSTEM、GUEST。
    std::string actor_;      // 操作人用户名；未登录场景可记录 guest 或输入账号。
    std::string action_;     // 操作名称，例如 LOGIN、SEND_EXPRESS、SIGN_EXPRESS。
    std::string result_;     // 操作结果，例如 SUCCESS、FAILED、DENIED。
    std::string detail_;     // 操作详情，用于保存失败原因、越权单号、筛选条件等审计信息。

public:
    /** 构造空日志记录，便于 Logger::load() 反序列化时逐行填充。 */
    LogEntry() {}

    /** 构造完整日志记录。 */
    LogEntry(const std::string& time, const std::string& actorType, const std::string& actor,
             const std::string& action, const std::string& result, const std::string& detail)
        : time_(time), actorType_(actorType), actor_(actor), action_(action),
          result_(result), detail_(detail) {}

    /** 获取操作时间；const getter 体现日志记录的只读查询语义。 */
    const std::string& time() const { return time_; }

    /** 获取操作人类型。 */
    const std::string& actorType() const { return actorType_; }

    /** 获取操作人用户名。 */
    const std::string& actor() const { return actor_; }

    /** 获取操作名称。 */
    const std::string& action() const { return action_; }

    /** 获取操作结果。 */
    const std::string& result() const { return result_; }

    /** 获取操作详情。 */
    const std::string& detail() const { return detail_; }

    /** 序列化日志记录为 operations.log 中的一行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(time_) << '|'
            << StringUtil::escape(actorType_) << '|'
            << StringUtil::escape(actor_) << '|'
            << StringUtil::escape(action_) << '|'
            << StringUtil::escape(result_) << '|'
            << StringUtil::escape(detail_);
        return out.str();
    }

    /** 从 operations.log 的一行反序列化日志记录。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 6) {
            return false;
        }
        time_ = StringUtil::unescape(parts[0]);
        actorType_ = StringUtil::unescape(parts[1]);
        actor_ = StringUtil::unescape(parts[2]);
        action_ = StringUtil::unescape(parts[3]);
        result_ = StringUtil::unescape(parts[4]);
        detail_ = StringUtil::unescape(parts[5]);
        return true;
    }
};

/* ========================== 仓储层：文件持久化和反序列化隔离 ========================== */

//Repository 的作用是专门负责文件读写。比如 UserRepository 只负责把 users.txt 读成 vector<User>，
//或者把 vector<User> 保存回文件。业务层不用关心每一行怎么拆分，UI 层也不会直接操作文件。

/** 普通用户文件仓储，负责 users.txt 与 std::vector<User> 之间的转换。 */
class UserRepository {
private:
    std::string filePath_;  // 用户数据文件路径；仓储对象只绑定一个明确的数据文件。
    mutable int lastInvalidLineCount_;  // 最近一次加载时跳过的异常行数；mutable 允许 const load() 更新统计信息。

public:
    /** 构造用户仓储。 */
    explicit UserRepository(const std::string& filePath) : filePath_(filePath), lastInvalidLineCount_(0) {}

    /** 从文件加载用户列表。 */
    std::vector<User> load() const {
        std::vector<User> users;
        lastInvalidLineCount_ = 0;
        std::ifstream in(filePath_);
        std::string line;
        while (std::getline(in, line)) {
            // 空行不算脏数据，便于人工查看或编辑文件时保留少量空白。
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            User user;
            if (user.deserialize(line)) {
                users.push_back(user);
            } else {
                ++lastInvalidLineCount_;
            }
        }
        return users;
    }

    /** 获取最近一次加载时跳过的异常行数。 */
    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    /** 保存用户列表到文件。 */
    bool save(const std::vector<User>& users) const {
        std::ofstream out(filePath_, std::ios::trunc);
        if (!out) {
            return false;
        }
        // 基于实体自己的 serialize() 输出，仓储层不重复拼接字段，避免格式规则分散。
        for (const User& user : users) {
            out << user.serialize() << '\n';
        }
        return true;
    }
};

/** 管理员文件仓储，负责 admin.txt 的加载与保存。 */
class AdminRepository {
private:
    std::string filePath_;  // 管理员数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时发现的异常行数；用于启动日志告警。

public:
    /** 构造管理员仓储，绑定 admin.txt 路径。 */
    explicit AdminRepository(const std::string& filePath) : filePath_(filePath), lastInvalidLineCount_(0) {}

    /** 从文件加载管理员；文件不存在、空文件或异常行时返回默认管理员对象。 */
    Admin load() const {
        lastInvalidLineCount_ = 0;
        std::ifstream in(filePath_);
        std::string line;
        Admin admin;
        if (std::getline(in, line)) {
            if (!admin.deserialize(line)) {
                ++lastInvalidLineCount_;
            }
        }
        return admin;
    }

    /** 获取最近一次加载时发现的异常行数，供系统启动日志记录。 */
    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    /** 保存管理员到文件。 */
    bool save(const Admin& admin) const {
        std::ofstream out(filePath_, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << admin.serialize() << '\n';
        return true;
    }
};

/** 快递文件仓储，负责 expresses.txt 与 std::vector<Express> 的互相转换。 */
class ExpressRepository {
private:
    std::string filePath_;  // 快递数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时跳过的异常行数。

public:
    /** 构造快递仓储，绑定 expresses.txt 路径。 */
    explicit ExpressRepository(const std::string& filePath) : filePath_(filePath), lastInvalidLineCount_(0) {}

    /** 从文件加载快递列表。 */
    std::vector<Express> load() const {
        std::vector<Express> expresses;
        lastInvalidLineCount_ = 0;
        std::ifstream in(filePath_);
        std::string line;
        while (std::getline(in, line)) {
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            Express express;
            if (express.deserialize(line)) {
                expresses.push_back(express);
            } else {
                ++lastInvalidLineCount_;
            }
        }
        return expresses;
    }

    /** 获取最近一次加载时跳过的异常行数。 */
    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    /** 保存快递列表到文件。 */
    bool save(const std::vector<Express>& expresses) const {
        std::ofstream out(filePath_, std::ios::trunc);
        if (!out) {
            return false;
        }
        for (const Express& express : expresses) {
            out << express.serialize() << '\n';
        }
        return true;
    }
};

/** 操作日志仓储，负责 operations.log 的追加写入和查询加载。 */
class Logger {
private:
    std::string filePath_;  // 操作日志文件路径，通常为 data/operations.log。

public:
    /** 构造日志记录器，绑定 operations.log 路径。 */
    explicit Logger(const std::string& filePath) : filePath_(filePath) {}

    /** 追加一条操作日志。 */
    void write(const std::string& actorType, const std::string& actor, const std::string& action,
               const std::string& result, const std::string& detail) const {
        std::ofstream out(filePath_, std::ios::app);
        if (!out) {
            return;
        }
        LogEntry entry(StringUtil::now(), actorType, actor, action, result, detail);
        out << entry.serialize() << '\n';
    }

    /** 加载全部日志。 */
    std::vector<LogEntry> load() const {
        std::vector<LogEntry> entries;
        std::ifstream in(filePath_);
        std::string line;
        while (std::getline(in, line)) {
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            LogEntry entry;
            if (entry.deserialize(line)) {
                entries.push_back(entry);
            }
        }
        return entries;
    }
};

/* ========================== UI 层：控制台输入输出公共能力 ========================== */

//ConsoleUI 负责通用输入输出，比如标题、提示、读取整数、读取金额、确认操作、打印表格。它不做业务处理，只负责交互。
/** 控制台 UI 通用工具类，负责显示、输入和表格输出。 */
class ConsoleUI {
public:
    /** 控制台交互类只负责显示、读取和表格输出。 */

    /** 清屏：使用多行空白避免依赖平台命令。 */
    static void clear() {
        for (int i = 0; i < 30; ++i) {
            std::cout << '\n';
        }
    }

    /** 打印页面标题，统一 CLI 页面风格。 */
    static void title(const std::string& text) {
        std::cout << "\n============================================================\n";
        std::cout << "  " << text << '\n';
        std::cout << "============================================================\n";
    }

    /** 打印提示信息，便于 App 流程统一输出业务层返回的 message。 */
    static void message(const std::string& text) {
        std::cout << text << '\n';
    }

    /** 等待用户按回车继续。 */
    static void pause() {
        std::cout << "\n按回车键继续...";
        std::string ignored;
        std::getline(std::cin, ignored);
    }

    /** 读取一行文本并去掉首尾空白。 */
    static std::string readLine(const std::string& prompt) {
        std::cout << prompt;
        std::string value;
        if (!std::getline(std::cin, value)) {
            return "__EOF__";
        }
        return StringUtil::trim(value);
    }

    /** 判断 readLine 返回值是否表示输入流结束。 */
    static bool isEofValue(const std::string& value) {
        return value == "__EOF__";
    }

    /** 读取范围内整数。 */
    static bool readInt(const std::string& prompt, int minValue, int maxValue, int& value) {
        while (true) {
            std::string text = readLine(prompt);
            if (isEofValue(text)) {
                return false;
            }
            std::istringstream in(text);
            int parsed = 0;
            char extra = '\0';
            if ((in >> parsed) && !(in >> extra) && parsed >= minValue && parsed <= maxValue) {
                value = parsed;
                return true;
            }
            std::cout << "输入无效，请输入 " << minValue << "-" << maxValue << " 之间的整数。\n";
        }
    }

    /** 读取正数金额。 */
    static bool readMoney(const std::string& prompt, double& amount) {
        while (true) {
            std::string text = readLine(prompt);
            if (isEofValue(text)) {
                return false;
            }
            double value = 0.0;
            if (StringUtil::tryParsePositiveMoney(text, value)) {
                amount = value;
                return true;
            }
            std::cout << "金额格式无效，请输入正数，最多保留两位小数。\n";
        }
    }

    /** 读取确认选项，用于充值、寄件、批量签收等可能改变状态的操作。 */
    static bool confirm(const std::string& prompt) {
        while (true) {
            std::string value = readLine(prompt + " (Y/N): ");
            if (value == "__EOF__") {
                return false;
            }
            if (value == "Y" || value == "y") {
                return true;
            }
            if (value == "N" || value == "n") {
                return false;
            }
            std::cout << "请输入 Y 或 N。\n";
        }
    }

    /** 打印快递表格。 */
    static void printExpressTable(const std::vector<Express>& expresses, bool withIndex) {
        if (expresses.empty()) {
            std::cout << "暂无快递记录。\n";
            return;
        }
        if (withIndex) {
            std::cout << std::left << std::setw(6) << "序号";
        }
        std::cout << std::left << std::setw(20) << "单号"
                  << std::setw(14) << "发件人"
                  << std::setw(14) << "收件人"
                  << std::setw(10) << "状态"
                  << std::setw(20) << "寄送时间"
                  << std::setw(10) << "费用"
                  << "描述\n";
        std::cout << "------------------------------------------------------------------------------------------\n";
        for (std::size_t i = 0; i < expresses.size(); ++i) {
            if (withIndex) {
                std::cout << std::left << std::setw(6) << (i + 1);
            }
            std::cout << std::left << std::setw(20) << expresses[i].id()
                      << std::setw(14) << expresses[i].sender()
                      << std::setw(14) << expresses[i].receiver()
                      << std::setw(10) << expresses[i].statusName()
                      << std::setw(20) << expresses[i].sendTime()
                      << std::setw(10) << StringUtil::moneyToString(expresses[i].fee())
                      << expresses[i].description() << '\n';
        }
    }

    /** 打印用户表格，仅管理员菜单调用。 */
    static void printUserTable(const std::vector<User>& users) {
        if (users.empty()) {
            std::cout << "暂无用户。\n";
            return;
        }
        std::cout << std::left << std::setw(16) << "用户名"
                  << std::setw(14) << "姓名"
                  << std::setw(14) << "电话"
                  << std::setw(12) << "余额"
                  << std::setw(10) << "状态"
                  << "地址\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        for (const User& user : users) {
            std::cout << std::left << std::setw(16) << user.username()
                      << std::setw(14) << user.name()
                      << std::setw(14) << user.phone()
                      << std::setw(12) << StringUtil::moneyToString(user.balance())
                      << std::setw(10) << (user.frozen() ? "冻结" : "正常")
                      << user.address() << '\n';
        }
    }

    /** 打印日志表格，仅管理员菜单调用。 */
    static void printLogTable(const std::vector<LogEntry>& entries) {
        if (entries.empty()) {
            std::cout << "暂无日志记录。\n";
            return;
        }
        std::cout << std::left << std::setw(20) << "时间"
                  << std::setw(8) << "身份"
                  << std::setw(14) << "账号"
                  << std::setw(18) << "操作"
                  << std::setw(10) << "结果"
                  << "详情\n";
        std::cout << "------------------------------------------------------------------------------------------\n";
        for (const LogEntry& entry : entries) {
            std::cout << std::left << std::setw(20) << entry.time()
                      << std::setw(8) << entry.actorType()
                      << std::setw(14) << entry.actor()
                      << std::setw(18) << entry.action()
                      << std::setw(10) << entry.result()
                      << entry.detail() << '\n';
        }
    }
};

/* ========================== 业务层：权限、事务和核心业务规则 ========================== */

//它负责注册、登录、充值、寄件、签收、查询、冻结用户和查看日志。
//我的设计是：UI 层只负责收集输入，真正的业务规则都在 LogisticsSystem 里兜底。
/** 核心业务服务类，承担物流系统的业务编排、权限控制和持久化事务协调。 */
class LogisticsSystem {
private:
    static constexpr double BaseFee = 15.0;        // 实验一固定快递费用；V2 可迁移到 ExpressItem::getPrice() 多态计费。
    std::string dataDir_;                          // 主数据目录，真实读写以该目录为准。
    UserRepository userRepo_;                      // 用户仓储，负责 users.txt 的对象化读写。
    AdminRepository adminRepo_;                    // 管理员仓储，负责 admin.txt。
    ExpressRepository expressRepo_;                // 快递仓储，负责 expresses.txt。
    Logger logger_;                                // 操作日志仓储，负责 operations.log 追加与读取。
    std::vector<User> users_;                      // 内存中的用户列表；服务层在此基础上执行注册、充值、冻结等操作。
    std::vector<Express> expresses_;               // 内存中的快递列表；服务层在此基础上执行寄件、签收、查询等操作。
    Admin admin_;                                  // 管理员对象，同时保存平台账户余额。

    /** 查找用户下标，不存在时返回 users_.size()。 */
    std::size_t findUserIndex(const std::string& username) const {
        for (std::size_t i = 0; i < users_.size(); ++i) {
            if (users_[i].username() == username) {
                return i;
            }
        }
        return users_.size();
    }

    /** 查找快递下标，不存在时返回 expresses_.size()。 */
    std::size_t findExpressIndex(const std::string& id) const {
        for (std::size_t i = 0; i < expresses_.size(); ++i) {
            if (expresses_[i].id() == id) {
                return i;
            }
        }
        return expresses_.size();
    }

//单号生成在 makeExpressId() 中完成，格式是 EX + 时间戳 + 四位序号。
//如果生成的单号已经存在，就递增序号继续查找，保证不会重复。
    /** 生成不会重复的快递单号：EX + 时间戳 + 四位序号，冲突时递增序号继续查找。 */
    std::string makeExpressId() const {
        std::string prefix = "EX" + StringUtil::compactNow();
        int serial = static_cast<int>(expresses_.size()) + 1;
        while (true) {
            std::ostringstream out;
            out << prefix << std::setw(4) << std::setfill('0') << serial;
            if (findExpressIndex(out.str()) == expresses_.size()) {
                return out.str();
            }
            ++serial;
        }
    }

    /** 保存用户、管理员、快递三份核心数据；成功后再镜像到 src/data 和 bin/data。 */
    bool saveAll() const {
        bool ok = userRepo_.save(users_) && adminRepo_.save(admin_) && expressRepo_.save(expresses_);
        if (ok) {
            mirrorCoreDataFiles();
        }
        return ok;
    }

    /** 统一处理保存失败提示和日志。 */
    void reportSaveFailure(const std::string& actorType, const std::string& actor,
                           const std::string& action, std::string& message) const {
        message = "数据保存失败，请检查 data 目录权限，操作已回滚。";
        logger_.write(actorType, actor, action, "FAILED", message);
    }

    /** 将核心数据镜像到 bin/data 和 src/data。 */
    void mirrorCoreDataFiles() const {
        std::vector<std::string> mirrorDirs;
        if (DirectoryUtil::exists("src") && DirectoryUtil::exists("bin")) {
            mirrorDirs.push_back("src/data");
            mirrorDirs.push_back("bin/data");
        } else if (DirectoryUtil::exists("../src") && DirectoryUtil::exists("../bin")) {
            mirrorDirs.push_back("../src/data");
            mirrorDirs.push_back("../bin/data");
        }
        for (const std::string& dir : mirrorDirs) {
            DirectoryUtil::createDirectory(dir);
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, "admin.txt"),
                                        DirectoryUtil::join(dir, "admin.txt"));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, "users.txt"),
                                        DirectoryUtil::join(dir, "users.txt"));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, "expresses.txt"),
                                        DirectoryUtil::join(dir, "expresses.txt"));
        }
    }

    /** 记录启动时发现的数据文件异常，便于管理员验收时追踪。 */
    void logDataLoadWarnings() const {
        int badUsers = userRepo_.lastInvalidLineCount();
        int badAdmin = adminRepo_.lastInvalidLineCount();
        int badExpresses = expressRepo_.lastInvalidLineCount();
        if (badUsers == 0 && badAdmin == 0 && badExpresses == 0) {
            return;
        }
        std::ostringstream detail;
        detail << "加载数据时跳过异常行：users=" << badUsers
               << ", admin=" << badAdmin
               << ", expresses=" << badExpresses
               << "；程序继续运行并保留可恢复数据";
        logger_.write("SYSTEM", "system", "DATA_VALIDATE", "FAILED", detail.str());
    }

    /** 判断快递是否满足查询条件。 */
    bool matchExpress(const Express& express, const std::string& sender, const std::string& receiver,
                      const std::string& id, const std::string& startTime, const std::string& endTime,
                      int statusFilter) const {
        if (!sender.empty() && express.sender() != sender) {
            return false;
        }
        if (!receiver.empty() && express.receiver() != receiver) {
            return false;
        }
        if (!id.empty() && express.id() != id) {
            return false;
        }
        if (!startTime.empty() && express.sendTime() < startTime) {
            return false;
        }
        if (!endTime.empty() && express.sendTime() > endTime) {
            return false;
        }
        if (statusFilter >= 0 && static_cast<int>(express.status()) != statusFilter) {
            return false;
        }
        return true;
    }

public:
    /** LogisticsSystem 是业务核心类。 */

    /** 构造物流系统并绑定数据目录。 */
    explicit LogisticsSystem(const std::string& dataDir)
        : dataDir_(dataDir),
          userRepo_(DirectoryUtil::join(dataDir_, "users.txt")),
          adminRepo_(DirectoryUtil::join(dataDir_, "admin.txt")),
          expressRepo_(DirectoryUtil::join(dataDir_, "expresses.txt")),
          logger_(DirectoryUtil::join(dataDir_, "operations.log")) {}

    /** 初始化目录并加载数据。 */
    void initialize() {
        DirectoryUtil::createDirectory(dataDir_);
        users_ = userRepo_.load();
        admin_ = adminRepo_.load();
        expresses_ = expressRepo_.load();
        logDataLoadWarnings();
        if (!saveAll()) {
            logger_.write("SYSTEM", "system", "SAVE_DATA", "FAILED", "数据文件保存失败，请检查目录权限");
        }
        logger_.write("SYSTEM", "system", "INIT", "SUCCESS", "系统启动并加载数据");
    }

    /** 获取全部用户。 */
    const std::vector<User>& users() const {
        return users_;
    }

    /** 获取管理员只读视图，供 UI 展示平台余额和默认账号。 */
    const Admin& admin() const {
        return admin_;
    }

    /** 根据用户名获取用户指针。 */
    const User* getUser(const std::string& username) const {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            return nullptr;
        }
        return &users_[index];
    }

    /** 注册新用户：业务层再次校验输入，生成 salt/hash，保存失败则 pop_back 回滚。 */
    bool registerUser(const std::string& username, const std::string& name, const std::string& phone,
                      const std::string& password, const std::string& address, std::string& message) {
        std::string reason;
        if (!InputValidator::isValidUsername(username, reason) ||
            !InputValidator::isValidName(name, reason) ||
            !InputValidator::isValidPhone(phone, reason) ||
            !InputValidator::isValidPassword(password, reason) ||
            !InputValidator::isValidAddress(address, reason)) {
            message = reason;
            logger_.write("GUEST", username, "REGISTER", "FAILED", reason);
            return false;
        }
        if (findUserIndex(username) != users_.size() || username == admin_.username()) {
            message = "用户名已存在。";
            logger_.write("GUEST", username, "REGISTER", "FAILED", message);
            return false;
        }
        for (const User& user : users_) {
            if (user.phone() == phone) {
                message = "该手机号已被其他用户使用。";
                logger_.write("GUEST", username, "REGISTER", "FAILED", message);
                return false;
            }
        }
        std::string salt = PasswordHasher::makeSalt(username);
        std::string hash = PasswordHasher::hashPassword(salt, password);
        users_.push_back(User(username, name, phone, salt, hash, 0.0, address, false));
        if (!saveAll()) {
            // 保存失败补偿：撤销刚加入的用户，防止“内存注册成功但文件未保存”的状态漂移。
            users_.pop_back();
            saveAll();
            reportSaveFailure("GUEST", username, "REGISTER", message);
            return false;
        }
        message = "注册成功。";
        logger_.write("GUEST", username, "REGISTER", "SUCCESS", "新用户注册");
        return true;
    }

    /** 用户登录校验。 */
    bool loginUser(const std::string& username, const std::string& password, std::string& message) const {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            message = "用户不存在。";
            logger_.write("USER", username, "LOGIN", "FAILED", message);
            return false;
        }
        const User& user = users_[index];
        if (user.frozen()) {
            message = "账号已被冻结，请联系管理员。";
            logger_.write("USER", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!PasswordHasher::verify(user.salt(), password, user.passwordHash())) {
            message = "密码错误。";
            logger_.write("USER", username, "LOGIN", "FAILED", message);
            return false;
        }
        message = "登录成功。";
        logger_.write("USER", username, "LOGIN", "SUCCESS", message);
        return true;
    }

    /** 管理员登录校验。 */
    bool loginAdmin(const std::string& username, const std::string& password, std::string& message) const {
        if (username != admin_.username()) {
            message = "管理员账号不存在。";
            logger_.write("ADMIN", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!PasswordHasher::verify(admin_.salt(), password, admin_.passwordHash())) {
            message = "管理员密码错误。";
            logger_.write("ADMIN", username, "LOGIN", "FAILED", message);
            return false;
        }
        message = "管理员登录成功。";
        logger_.write("ADMIN", username, "LOGIN", "SUCCESS", message);
        return true;
    }

    /** 修改普通用户密码。 */
    bool changeUserPassword(const std::string& username, const std::string& oldPassword,
                            const std::string& newPassword, std::string& message) {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            message = "用户不存在。";
            return false;
        }
        User& user = users_[index];
        if (!PasswordHasher::verify(user.salt(), oldPassword, user.passwordHash())) {
            message = "旧密码错误。";
            logger_.write("USER", username, "CHANGE_PASSWORD", "FAILED", message);
            return false;
        }
        std::string reason;
        if (!InputValidator::isValidPassword(newPassword, reason)) {
            message = reason;
            logger_.write("USER", username, "CHANGE_PASSWORD", "FAILED", reason);
            return false;
        }
        if (oldPassword == newPassword) {
            message = "新密码不能与旧密码相同。";
            logger_.write("USER", username, "CHANGE_PASSWORD", "FAILED", message);
            return false;
        }
        std::string salt = PasswordHasher::makeSalt(username);
        std::string oldSalt = user.salt();
        std::string oldHash = user.passwordHash();
        user.updatePassword(salt, PasswordHasher::hashPassword(salt, newPassword));
        if (!saveAll()) {
            // 保存失败补偿：恢复旧 salt/hash，保证认证状态回到操作前。
            user.updatePassword(oldSalt, oldHash);
            saveAll();
            reportSaveFailure("USER", username, "CHANGE_PASSWORD", message);
            return false;
        }
        message = "密码修改成功。";
        logger_.write("USER", username, "CHANGE_PASSWORD", "SUCCESS", message);
        return true;
    }

    /** 用户充值。 */
    bool recharge(const std::string& username, double amount, std::string& message) {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            message = "用户不存在。";
            return false;
        }
        double oldBalance = users_[index].balance();
        users_[index].recharge(amount);
        if (!saveAll()) {
            // 保存失败补偿：用差额把余额恢复到 oldBalance。
            users_[index].recharge(oldBalance - users_[index].balance());
            saveAll();
            reportSaveFailure("USER", username, "RECHARGE", message);
            return false;
        }
        message = "充值成功，当前余额：" + StringUtil::moneyToString(users_[index].balance()) + " 元。";
        logger_.write("USER", username, "RECHARGE", "SUCCESS", "充值 " + StringUtil::moneyToString(amount));
        return true;
    }

    /** 发送快递：检查双方账号、扣用户余额、加平台余额、生成单号，保存失败则全部反向回滚。 */
    bool sendExpress(const std::string& sender, const std::string& receiver,
                     const std::string& description, std::string& expressId, std::string& message) {
        std::size_t senderIndex = findUserIndex(sender);
        std::size_t receiverIndex = findUserIndex(receiver);
        if (senderIndex == users_.size()) {
            message = "发件用户不存在。";
            return false;
        }
        if (users_[senderIndex].frozen()) {
            message = "发件用户账号已冻结，不能发送快递。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        if (receiverIndex == users_.size()) {
            message = "收件用户不存在。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        if (sender == receiver) {
            message = "不能给自己发送快递。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        if (users_[receiverIndex].frozen()) {
            message = "收件用户账号已冻结，不能接收新快递。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        if (StringUtil::trim(description).empty()) {
            message = "物品描述不能为空。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        // 先扣费再创建运单，保证余额不足时不会产生孤立快递。
        if (!users_[senderIndex].deduct(BaseFee)) {
            message = "余额不足，当前余额：" + StringUtil::moneyToString(users_[senderIndex].balance()) +
                      " 元，本次需要：" + StringUtil::moneyToString(BaseFee) + " 元。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        admin_.addBalance(BaseFee);
        expressId = makeExpressId();
        expresses_.push_back(Express(expressId, sender, receiver, StringUtil::now(), description, BaseFee));
        if (!saveAll()) {
            // 保存失败补偿：撤销“新增快递 + 平台入账 + 用户扣费”三个内存变化。
            expresses_.pop_back();
            admin_.addBalance(-BaseFee);
            users_[senderIndex].recharge(BaseFee);
            expressId.clear();
            saveAll();
            reportSaveFailure("USER", sender, "SEND_EXPRESS", message);
            return false;
        }
        message = "发送成功，快递单号：" + expressId + "。";
        logger_.write("USER", sender, "SEND_EXPRESS", "SUCCESS", "单号 " + expressId + "，收件人 " + receiver);
        return true;
    }

    /** 签收指定快递：只有收件人可签收；修改前备份 Express，保存失败时整体恢复。 */
    bool signExpress(const std::string& username, const std::string& expressId, std::string& message) {
        std::size_t index = findExpressIndex(expressId);
        if (index == expresses_.size()) {
            message = "快递单号不存在。";
            logger_.write("USER", username, "SIGN_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        Express& express = expresses_[index];
        if (express.receiver() != username) {
            message = "无权签收该快递。";
            logger_.write("USER", username, "SIGN_EXPRESS", "DENIED", "试图签收 " + expressId);
            return false;
        }
        if (!express.isWaitingSign()) {
            message = "该快递已经签收，不能重复签收。";
            logger_.write("USER", username, "SIGN_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        Express oldExpress = express;
        express.sign();
        if (!saveAll()) {
            // 保存失败补偿：恢复签收前完整快递对象，包括状态和 receiveTime。
            express = oldExpress;
            saveAll();
            reportSaveFailure("USER", username, "SIGN_EXPRESS", message);
            return false;
        }
        message = "签收成功：" + expressId;
        logger_.write("USER", username, "SIGN_EXPRESS", "SUCCESS", expressId);
        return true;
    }

    /** 获取指定用户的未签收快递。 */
    std::vector<Express> waitingExpressesFor(const std::string& username) const {
        std::vector<Express> result;
        for (const Express& express : expresses_) {
            if (express.receiver() == username && express.isWaitingSign()) {
                result.push_back(express);
            }
        }
        return result;
    }

    /** 用户查询快递：先过滤非本人快递；若按他人单号查询，返回空结果并写 DENIED 日志。 */
    std::vector<Express> queryUserExpresses(const std::string& username, const std::string& sender,
                                            const std::string& receiver, const std::string& id,
                                            const std::string& startTime, const std::string& endTime,
                                            int statusFilter) const {
        std::vector<Express> result;
        bool denied = false;
        for (const Express& express : expresses_) {
            if (!id.empty() && express.id() == id && !express.belongsTo(username)) {
                denied = true;
            }
            if (!express.belongsTo(username)) {
                continue;
            }
            if (matchExpress(express, sender, receiver, id, startTime, endTime, statusFilter)) {
                result.push_back(express);
            }
        }
        logger_.write("USER", username, "QUERY_EXPRESS", denied ? "DENIED" : "SUCCESS",
                      denied ? "存在越权查询尝试" : "查询快递记录");
        return result;
    }

    /** 管理员查询全部快递。 */
    std::vector<Express> queryAllExpresses(const std::string& sender, const std::string& receiver,
                                           const std::string& id, const std::string& startTime,
                                           const std::string& endTime, int statusFilter) const {
        std::vector<Express> result;
        for (const Express& express : expresses_) {
            if (matchExpress(express, sender, receiver, id, startTime, endTime, statusFilter)) {
                result.push_back(express);
            }
        }
        logger_.write("ADMIN", admin_.username(), "QUERY_EXPRESS", "SUCCESS", "管理员查询快递");
        return result;
    }

    /** 管理员冻结或解冻用户。 */
    bool setUserFrozen(const std::string& username, bool frozen, std::string& message) {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            message = "用户不存在。";
            logger_.write("ADMIN", admin_.username(), "SET_USER_STATUS", "FAILED", message + " " + username);
            return false;
        }
        bool oldFrozen = users_[index].frozen();
        users_[index].setFrozen(frozen);
        if (!saveAll()) {
            // 保存失败补偿：恢复管理员操作前的冻结状态。
            users_[index].setFrozen(oldFrozen);
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "SET_USER_STATUS", message);
            return false;
        }
        message = frozen ? "用户已冻结。" : "用户已解冻。";
        logger_.write("ADMIN", admin_.username(), "SET_USER_STATUS", "SUCCESS", username + " " + message);
        return true;
    }

    /** 管理员读取日志并支持筛选。 */
    std::vector<LogEntry> queryLogs(const std::string& actor, const std::string& action,
                                    const std::string& result) const {
        std::vector<LogEntry> entries = logger_.load();
        std::vector<LogEntry> filtered;
        for (const LogEntry& entry : entries) {
            if (!actor.empty() && entry.actor() != actor) {
                continue;
            }
            if (!action.empty() && !StringUtil::contains(entry.action(), action)) {
                continue;
            }
            if (!result.empty() && entry.result() != result) {
                continue;
            }
            filtered.push_back(entry);
        }
        logger_.write("ADMIN", admin_.username(), "VIEW_LOG", "SUCCESS", "管理员查看日志");
        return filtered;
    }
};

/* ========================== 流程层：菜单跳转和用户操作步骤 ========================== */

//App 是流程控制类，负责主菜单、用户注册流程、用户登录流程、管理员登录流程、用户菜单和管理员菜单。
//比如用户选择“发送快递”，App 会读取收件人和物品描述，然后调用 `LogisticsSystem
/** 顶层应用流程控制类，负责把控制台菜单串成完整用户旅程。 */
class App {
private:
    LogisticsSystem system_;  // 物流系统核心对象；App 只通过其公开接口执行业务。

    /** App 是程序流程控制层。 */

    /** 判断输入流是否已经结束，用于脚本输入、管道输入或 Ctrl+Z 场景下安全退出当前流程。 */
    bool isInputEnded(const std::string& value) const {
        return ConsoleUI::isEofValue(value);
    }

    /** 读取菜单选项的统一入口。 */
    bool readMenuChoice(const std::string& prompt, int minValue, int maxValue, int& choice) const {
        return ConsoleUI::readInt(prompt, minValue, maxValue, choice);
    }

    /** 读取注册用户名，并即时校验格式、重复和管理员保留名。 */
    std::string readRegisterUsername() const {
        while (true) {
            std::string username = ConsoleUI::readLine("用户名(3-20位字母/数字/下划线): ");
            if (isInputEnded(username)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidUsername(username, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            if (username == system_.admin().username() || system_.getUser(username) != nullptr) {
                std::cout << "用户名已存在，请更换。\n";
                continue;
            }
            return username;
        }
    }

    /** 读取登录或查询用用户名，并即时校验格式。 */
    std::string readUsername(const std::string& prompt) const {
        while (true) {
            std::string username = ConsoleUI::readLine(prompt);
            if (isInputEnded(username)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidUsername(username, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return username;
        }
    }

    /** 读取姓名并即时校验。 */
    std::string readName() const {
        while (true) {
            std::string name = ConsoleUI::readLine("姓名: ");
            if (isInputEnded(name)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidName(name, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return name;
        }
    }

    /** 读取手机号并即时校验格式和重复。 */
    std::string readPhone() const {
        while (true) {
            std::string phone = ConsoleUI::readLine("电话(11位数字): ");
            if (isInputEnded(phone)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidPhone(phone, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            bool used = false;
            for (const User& user : system_.users()) {
                if (user.phone() == phone) {
                    used = true;
                    break;
                }
            }
            if (used) {
                std::cout << "该手机号已被其他用户使用，请更换。\n";
                continue;
            }
            return phone;
        }
    }

    /** 读取地址并即时校验，防止空地址或控制字符进入后续注册流程。 */
    std::string readAddress() const {
        while (true) {
            std::string address = ConsoleUI::readLine("地址: ");
            if (isInputEnded(address)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidAddress(address, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return address;
        }
    }

    /** 读取密码并即时校验复杂度。 */
    std::string readPassword(const std::string& prompt) const {
        while (true) {
            std::string password = ConsoleUI::readLine(prompt);
            if (isInputEnded(password)) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidPassword(password, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return password;
        }
    }

    /** 读取两次密码并确保一致，降低用户注册或改密时误输入风险。 */
    std::string readConfirmedPassword(const std::string& prompt, const std::string& confirmPrompt) const {
        while (true) {
            std::string password = readPassword(prompt);
            if (password.empty()) {
                return "";
            }
            std::string confirm = ConsoleUI::readLine(confirmPrompt);
            if (isInputEnded(confirm)) {
                return "";
            }
            if (password != confirm) {
                std::cout << "两次密码不一致，请重新输入。\n";
                continue;
            }
            return password;
        }
    }

    /** 读取收件用户名，并提前拦截不存在、给自己寄件等异常。 */
    std::string readReceiverUsername(const std::string& sender) const {
        while (true) {
            std::string receiver = readUsername("收件用户名: ");
            if (receiver.empty()) {
                return "";
            }
            if (receiver == sender) {
                std::cout << "不能给自己发送快递，请重新输入收件人。\n";
                continue;
            }
            const User* user = system_.getUser(receiver);
            if (user == nullptr) {
                std::cout << "收件用户不存在，请先确认用户名。\n";
                continue;
            }
            if (user->frozen()) {
                std::cout << "收件用户账号已冻结，不能接收新快递。\n";
                continue;
            }
            return receiver;
        }
    }

    /** 读取可选用户名查询条件；空值表示不按该字段筛选。 */
    std::string readOptionalUsername(const std::string& prompt, const std::string& fieldName) const {
        while (true) {
            std::string username = ConsoleUI::readLine(prompt);
            if (isInputEnded(username) || username.empty()) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidOptionalUsername(username, fieldName, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return username;
        }
    }

    /** 读取可选快递单号查询条件；格式校验与权限过滤分离。 */
    std::string readOptionalExpressId() const {
        while (true) {
            std::string id = ConsoleUI::readLine("快递单号(可留空): ");
            if (isInputEnded(id) || id.empty()) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidOptionalExpressId(id, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return id;
        }
    }

    /** 读取日志结果筛选条件，只允许 SUCCESS/FAILED/DENIED 或空值。 */
    std::string readLogResultFilter() const {
        while (true) {
            std::string result = ConsoleUI::readLine("结果(可留空，SUCCESS/FAILED/DENIED): ");
            if (isInputEnded(result) || result.empty()) {
                return "";
            }
            if (result == "SUCCESS" || result == "FAILED" || result == "DENIED") {
                return result;
            }
            std::cout << "结果只能填写 SUCCESS、FAILED、DENIED，或直接留空。\n";
        }
    }

    /** 读取日志关键字筛选条件，限制长度并排除控制字符。 */
    std::string readOptionalKeyword(const std::string& prompt, const std::string& fieldName) const {
        while (true) {
            std::string value = ConsoleUI::readLine(prompt);
            if (isInputEnded(value) || value.empty()) {
                return "";
            }
            std::string reason;
            if (!InputValidator::isValidOptionalKeyword(value, fieldName, reason)) {
                std::cout << reason << '\n';
                continue;
            }
            return value;
        }
    }

    /** 打印主菜单，是普通用户、管理员两个角色入口的统一起点。 */
    void showMainMenu() const {
        ConsoleUI::title("物流管理平台 V1 - 单机控制台版");
        std::cout << "1. 用户注册\n";
        std::cout << "2. 用户登录\n";
        std::cout << "3. 管理员登录\n";
        std::cout << "0. 退出系统\n";
    }

    /** 执行用户注册流程。 */
    void registerFlow() {
        ConsoleUI::title("用户注册");
        std::string username = readRegisterUsername();
        std::string name = readName();
        std::string phone = readPhone();
        std::string address = readAddress();
        std::string password = readConfirmedPassword("密码(6-30位，必须含字母和数字): ", "确认密码: ");
        if (username.empty() || name.empty() || phone.empty() || address.empty() || password.empty()) {
            ConsoleUI::message("输入不完整，已取消注册。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.registerUser(username, name, phone, password, address, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 执行用户登录流程，连续三次失败后返回。 */
    void userLoginFlow() {
        ConsoleUI::title("用户登录");
        for (int attempt = 1; attempt <= 3; ++attempt) {
            std::string username = readUsername("用户名: ");
            std::string password = ConsoleUI::readLine("密码: ");
            if (username.empty()) {
                ConsoleUI::message("用户名输入不完整，已返回上级菜单。");
                ConsoleUI::pause();
                return;
            }
            if (password.empty() || isInputEnded(password)) {
                ConsoleUI::message("密码不能为空。");
                --attempt;
                continue;
            }
            std::string message;
            if (system_.loginUser(username, password, message)) {
                ConsoleUI::message(message);
                userMenu(username);
                return;
            }
            ConsoleUI::message(message + " 剩余尝试次数：" + std::to_string(3 - attempt));
        }
        ConsoleUI::pause();
    }

    /** 执行管理员登录流程。 */
    void adminLoginFlow() {
        ConsoleUI::title("管理员登录");
        for (int attempt = 1; attempt <= 3; ++attempt) {
            std::string username = readUsername("管理员账号: ");
            std::string password = ConsoleUI::readLine("管理员密码: ");
            if (username.empty()) {
                ConsoleUI::message("管理员账号输入不完整，已返回上级菜单。");
                ConsoleUI::pause();
                return;
            }
            if (password.empty() || isInputEnded(password)) {
                ConsoleUI::message("管理员密码不能为空。");
                --attempt;
                continue;
            }
            std::string message;
            if (system_.loginAdmin(username, password, message)) {
                ConsoleUI::message(message);
                adminMenu();
                return;
            }
            ConsoleUI::message(message + " 剩余尝试次数：" + std::to_string(3 - attempt));
        }
        ConsoleUI::pause();
    }

    /** 打印用户菜单顶部状态，展示当前用户、余额和未签收数量。 */
    void printUserHeader(const std::string& username) const {
        const User* user = system_.getUser(username);
        std::size_t waitingCount = system_.waitingExpressesFor(username).size();
        if (user != nullptr) {
            std::cout << "当前用户：" << user->name() << "(" << user->username() << ")"
                      << " | 余额：" << StringUtil::moneyToString(user->balance()) << " 元"
                      << " | 未签收：" << waitingCount << " 件\n";
            std::cout << "------------------------------------------------------------\n";
        }
    }

    /** 用户登录后的功能菜单。 */
    void userMenu(const std::string& username) {
        while (true) {
            ConsoleUI::title("用户功能菜单");
            printUserHeader(username);
            std::cout << "1. 修改账户密码\n";
            std::cout << "2. 查询余额\n";
            std::cout << "3. 账户充值\n";
            std::cout << "4. 发送快递\n";
            std::cout << "5. 接收快递\n";
            std::cout << "6. 查询快递\n";
            std::cout << "0. 退出登录\n";
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 6, choice)) {
                return;
            }
            if (choice == 0) {
                return;
            } else if (choice == 1) {
                changePasswordFlow(username);
            } else if (choice == 2) {
                showBalanceFlow(username);
            } else if (choice == 3) {
                rechargeFlow(username);
            } else if (choice == 4) {
                sendExpressFlow(username);
            } else if (choice == 5) {
                receiveExpressFlow(username);
            } else if (choice == 6) {
                userQueryExpressFlow(username);
            }
        }
    }

    /** 修改密码流程；旧密码验证、新密码保存和回滚由服务层完成。 */
    void changePasswordFlow(const std::string& username) {
        ConsoleUI::title("修改账户密码");
        std::string oldPassword = ConsoleUI::readLine("旧密码: ");
        if (oldPassword.empty() || isInputEnded(oldPassword)) {
            ConsoleUI::message("旧密码不能为空。");
            ConsoleUI::pause();
            return;
        }
        std::string newPassword = readConfirmedPassword("新密码(6-30位，必须含字母和数字): ", "确认新密码: ");
        if (newPassword.empty()) {
            ConsoleUI::message("新密码输入不完整，已取消修改。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.changeUserPassword(username, oldPassword, newPassword, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 展示余额流程，只读取用户对象，不改变系统状态。 */
    void showBalanceFlow(const std::string& username) const {
        ConsoleUI::title("账户余额");
        const User* user = system_.getUser(username);
        if (user != nullptr) {
            std::cout << "当前余额：" << StringUtil::moneyToString(user->balance()) << " 元\n";
        }
        ConsoleUI::pause();
    }

    /** 账户充值流程；金额读取与二次确认在 UI 层，余额变更和保存回滚在服务层。 */
    void rechargeFlow(const std::string& username) {
        ConsoleUI::title("账户充值");
        double amount = 0.0;
        if (!ConsoleUI::readMoney("充值金额: ", amount)) {
            ConsoleUI::message("充值金额输入不完整，已取消充值。");
            ConsoleUI::pause();
            return;
        }
        if (!ConsoleUI::confirm("确认充值 " + StringUtil::moneyToString(amount) + " 元吗")) {
            ConsoleUI::message("已取消充值。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.recharge(username, amount, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 选择物品描述，支持快捷选项。 */
    std::string chooseDescription() const {
        ConsoleUI::title("物品描述快捷选项");
        std::cout << "1. 文件资料\n";
        std::cout << "2. 衣物鞋帽\n";
        std::cout << "3. 数码配件\n";
        std::cout << "4. 食品零食\n";
        std::cout << "5. 日用品\n";
        std::cout << "6. 自定义输入\n";
        int choice = 6;
        if (!readMenuChoice("请选择物品描述: ", 1, 6, choice)) {
            return "";
        }
        if (choice == 1) {
            return "文件资料";
        }
        if (choice == 2) {
            return "衣物鞋帽";
        }
        if (choice == 3) {
            return "数码配件";
        }
        if (choice == 4) {
            return "食品零食";
        }
        if (choice == 5) {
            return "日用品";
        }
        while (true) {
            std::string custom = ConsoleUI::readLine("请输入自定义物品描述: ");
            std::string reason;
            if (InputValidator::isValidDescription(custom, reason)) {
                return custom;
            }
            std::cout << reason << '\n';
        }
    }

    /** 发送快递流程。 */
    void sendExpressFlow(const std::string& username) {
        ConsoleUI::title("发送快递");
        std::string receiver = readReceiverUsername(username);
        if (receiver.empty()) {
            ConsoleUI::message("收件人输入无效，已取消发送。");
            ConsoleUI::pause();
            return;
        }
        std::string description = chooseDescription();
        const User* user = system_.getUser(username);
        if (user != nullptr && user->balance() < 15.0) {
            std::cout << "当前余额：" << StringUtil::moneyToString(user->balance())
                      << " 元，本次需要 15.00 元。\n";
            if (ConsoleUI::confirm("余额不足，是否立即充值")) {
                rechargeFlow(username);
            }
            return;
        }
        if (!ConsoleUI::confirm("本次寄件将扣除 15.00 元，确认发送")) {
            ConsoleUI::message("已取消发送。");
            ConsoleUI::pause();
            return;
        }
        std::string expressId;
        std::string message;
        system_.sendExpress(username, receiver, description, expressId, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 接收快递流程：展示当前用户待签收列表，支持单个、批量和全部签收。 */
    void receiveExpressFlow(const std::string& username) {
        while (true) {
            ConsoleUI::title("接收快递");
            std::vector<Express> waiting = system_.waitingExpressesFor(username);
            ConsoleUI::printExpressTable(waiting, true);
            if (waiting.empty()) {
                ConsoleUI::pause();
                return;
            }
            std::cout << "\n1. 签收单个\n";
            std::cout << "2. 批量签收\n";
            std::cout << "3. 全部签收\n";
            std::cout << "0. 返回\n";
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 3, choice)) {
                return;
            }
            if (choice == 0) {
                return;
            }
            if (choice == 1) {
                int index = 1;
                if (!readMenuChoice("请输入序号: ", 1, static_cast<int>(waiting.size()), index)) {
                    return;
                }
                std::string message;
                system_.signExpress(username, waiting[static_cast<std::size_t>(index - 1)].id(), message);
                ConsoleUI::message(message);
                ConsoleUI::pause();
            } else if (choice == 2) {
                std::string text = ConsoleUI::readLine("请输入序号，使用空格或逗号分隔: ");
                signByIndexText(username, waiting, text);
                ConsoleUI::pause();
            } else if (choice == 3) {
                if (ConsoleUI::confirm("确认签收全部未签收快递")) {
                    for (const Express& express : waiting) {
                        std::string message;
                        system_.signExpress(username, express.id(), message);
                        std::cout << message << '\n';
                    }
                }
                ConsoleUI::pause();
            }
        }
    }

    /** 根据编号文本批量签收：逗号转空格后逐个解析，非法、越界、重复序号直接跳过。 */
    void signByIndexText(const std::string& username, const std::vector<Express>& waiting,
                         const std::string& text) {
        std::string normalized = text;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');
        std::istringstream in(normalized);
        std::vector<int> signedIndexes;
        std::string token;
        bool signedAny = false;
        while (in >> token) {
            if (!StringUtil::isDigits(token)) {
                std::cout << "序号 \"" << token << "\" 不是数字，已跳过。\n";
                continue;
            }
            int index = 0;
            std::istringstream tokenInput(token);
            tokenInput >> index;
            if (index < 1 || index > static_cast<int>(waiting.size())) {
                std::cout << "序号 " << index << " 越界，已跳过。\n";
                continue;
            }
            if (std::find(signedIndexes.begin(), signedIndexes.end(), index) != signedIndexes.end()) {
                std::cout << "序号 " << index << " 重复，已跳过。\n";
                continue;
            }
            std::string message;
            system_.signExpress(username, waiting[static_cast<std::size_t>(index - 1)].id(), message);
            std::cout << message << '\n';
            signedIndexes.push_back(index);
            signedAny = true;
        }
        if (!signedAny) {
            std::cout << "没有有效序号。\n";
        }
    }

    /** 读取快递查询条件：多个输出参数用引用传回，类似 C 里传指针写结果。 */
    void readExpressCriteria(std::string& sender, std::string& receiver, std::string& id,
                             std::string& startTime, std::string& endTime, int& statusFilter) const {
        while (true) {
            sender = readOptionalUsername("发件人用户名(可留空): ", "发件人用户名");
            receiver = readOptionalUsername("收件人用户名(可留空): ", "收件人用户名");
            id = readOptionalExpressId();
            startTime = ConsoleUI::readLine("开始时间 YYYY-MM-DD HH:MM:SS(可留空): ");
            endTime = ConsoleUI::readLine("结束时间 YYYY-MM-DD HH:MM:SS(可留空): ");
            std::string reason;
            if (!InputValidator::isValidDateTimeOrEmpty(startTime, "开始时间", reason) ||
                !InputValidator::isValidDateTimeOrEmpty(endTime, "结束时间", reason)) {
                std::cout << reason << '\n';
                continue;
            }
            if (!startTime.empty() && !endTime.empty() && startTime > endTime) {
                std::cout << "开始时间不能晚于结束时间，请重新输入。\n";
                continue;
            }
            std::cout << "状态筛选：0. 全部  1. 待签收  2. 已签收\n";
            int statusChoice = 0;
            if (!readMenuChoice("请选择: ", 0, 2, statusChoice)) {
                statusFilter = -1;
                return;
            }
            if (statusChoice == 0) {
                statusFilter = -1;
            } else if (statusChoice == 1) {
                statusFilter = static_cast<int>(ExpressStatus::WaitingSign);
            } else {
                statusFilter = static_cast<int>(ExpressStatus::Signed);
            }
            return;
        }
    }

    /** 用户查询快递流程。 */
    void userQueryExpressFlow(const std::string& username) const {
        ConsoleUI::title("查询快递");
        std::string sender;
        std::string receiver;
        std::string id;
        std::string startTime;
        std::string endTime;
        int statusFilter = -1;
        readExpressCriteria(sender, receiver, id, startTime, endTime, statusFilter);
        std::vector<Express> result = system_.queryUserExpresses(username, sender, receiver, id,
                                                                 startTime, endTime, statusFilter);
        ConsoleUI::printExpressTable(result, false);
        ConsoleUI::pause();
    }

    /** 管理员菜单。 */
    void adminMenu() {
        while (true) {
            ConsoleUI::title("管理员功能菜单");
            std::cout << "公司账户余额：" << StringUtil::moneyToString(system_.admin().balance()) << " 元\n";
            std::cout << "------------------------------------------------------------\n";
            std::cout << "1. 查看所有用户\n";
            std::cout << "2. 查询全部快递\n";
            std::cout << "3. 冻结/解冻用户\n";
            std::cout << "4. 查看操作日志\n";
            std::cout << "0. 退出登录\n";
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 4, choice)) {
                return;
            }
            if (choice == 0) {
                return;
            } else if (choice == 1) {
                ConsoleUI::title("所有用户信息");
                ConsoleUI::printUserTable(system_.users());
                ConsoleUI::pause();
            } else if (choice == 2) {
                adminQueryExpressFlow();
            } else if (choice == 3) {
                setUserStatusFlow();
            } else if (choice == 4) {
                viewLogFlow();
            }
        }
    }

    /** 管理员查询快递流程；管理员可查看全量快递，不受 belongsTo 权限过滤限制。 */
    void adminQueryExpressFlow() const {
        ConsoleUI::title("管理员查询快递");
        std::string sender;
        std::string receiver;
        std::string id;
        std::string startTime;
        std::string endTime;
        int statusFilter = -1;
        readExpressCriteria(sender, receiver, id, startTime, endTime, statusFilter);
        std::vector<Express> result = system_.queryAllExpresses(sender, receiver, id,
                                                                startTime, endTime, statusFilter);
        ConsoleUI::printExpressTable(result, false);
        ConsoleUI::pause();
    }

    /** 冻结或解冻用户流程。 */
    void setUserStatusFlow() {
        ConsoleUI::title("冻结/解冻用户");
        std::string username = readUsername("用户名: ");
        if (username.empty()) {
            ConsoleUI::message("用户名输入不完整，已取消操作。");
            ConsoleUI::pause();
            return;
        }
        if (system_.getUser(username) == nullptr) {
            ConsoleUI::message("用户不存在。");
            ConsoleUI::pause();
            return;
        }
        std::cout << "1. 冻结\n";
        std::cout << "2. 解冻\n";
        int choice = 1;
        if (!readMenuChoice("请选择: ", 1, 2, choice)) {
            ConsoleUI::message("操作选择不完整，已取消。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.setUserFrozen(username, choice == 1, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 查看日志流程。 */
    void viewLogFlow() const {
        ConsoleUI::title("查看操作日志");
        std::cout << "可留空表示不过滤。\n";
        std::string actor = readOptionalKeyword("操作人账号(可留空): ", "操作人账号");
        std::string action = readOptionalKeyword("操作类型关键字(可留空): ", "操作类型关键字");
        std::string result = readLogResultFilter();
        std::vector<LogEntry> entries = system_.queryLogs(actor, action, result);
        ConsoleUI::printLogTable(entries);
        ConsoleUI::pause();
    }

public:
    /** 构造应用对象。 */
    App() : system_(DirectoryUtil::projectDataDirectory()) {}

    /** 运行主循环：初始化系统后不断显示主菜单，main 函数本身不写业务逻辑。 */
    void run() {
        system_.initialize();
        while (true) {
            showMainMenu();
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 3, choice)) {
                return;
            }
            if (choice == 0) {
                ConsoleUI::message("感谢使用，再见。");
                return;
            } else if (choice == 1) {
                registerFlow();
            } else if (choice == 2) {
                userLoginFlow();
            } else if (choice == 3) {
                adminLoginFlow();
            }
        }
    }
};

/* ========================== 程序入口：环境初始化后启动 App ========================== */

/** 程序入口函数。 */
int main() {
    ConsoleEnvironment::initialize();
    App app;
    app.run();
    return 0;
}
