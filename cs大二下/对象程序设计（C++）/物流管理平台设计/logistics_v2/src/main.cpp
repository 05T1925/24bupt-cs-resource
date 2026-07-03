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
#include <memory>
#include <random>
#include <cstdio>
#include <sstream>
#include <streambuf>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/**
 * 物流管理平台 V2：快递员任务管理系统。
 * 设计原则：
 * 1. 除 main 外，所有业务逻辑均放在类成员函数中。
 * 2. 不改变对象状态的成员函数均显式标注 const。
 * 3. 用户密码采用随机盐 + SHA-256 哈希存储。
 * 4. 普通用户只能访问与自己相关的快递，管理员可以查看全部数据。
 */
/*
 * [总览注释] V2 代码分层讲解：
 * 1. 工具层：StringUtil、InputValidator、PasswordHasher、SHA256、StorageManager、TablePrinter，
 * 负责字符串、校验、安全、保存和表格显示。
 * 2. 实体层：User、Admin、Courier、Express、ExpressItem，负责表达真实业务对象和对象自身状态。
 * 3. 仓储层：各 Repository 和 Logger，负责文本文件加载、序列化、原子保存和日志审计。
 * 4. 业务层：LogisticsSystem，负责权限校验、状态流转、资金分账、自动调度和失败回滚。
 * 5. UI 层：ConsoleUI 和 App，负责菜单、输入输出和流程组织，不直接修改业务数据。
 *
 * jide ；  先讲分层，再讲 Express 三状态机，然后讲多态计费、资金一致性、日志哈希链和智能调度。
 */
// [代码块说明] ProjectConfig 是全局配置入口：所有数据文件名、窗口标题、密码哈希作用域都集中在这里。
// [设计说明] 以后升级 V3 或调整数据目录时，不需要在业务代码里到处搜索硬编码常量。
// [配置中心]：集中管理文件名、窗口标题和密码哈希域，避免常量散落造成版本漂移。
class ProjectConfig {
public:
    static constexpr const char* DisplayTitle = "【物流管理平台 V2 - 快递员任务管理系统】";
    static constexpr const wchar_t* WindowTitle = L"【物流管理平台 V2 - 快递员任务管理系统】";
    static constexpr const char* DataDirectoryName = "data";
    static constexpr const char* UserFileName = "users.txt";
    static constexpr const char* AdminFileName = "admin.txt";
    static constexpr const char* ExpressFileName = "expresses.txt";
    static constexpr const char* CourierFileName = "couriers.txt";
    static constexpr const char* LogFileName = "operations.log";
    static constexpr const char* AuthStateFileName = "auth_state.txt";
    static constexpr const char* PasswordHashScope = "|logistics_v2|";
};

// [状态机]：用强类型枚举约束快递只能在待揽收、待签收、已签收三态之间流转。
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};

// [查询条件对象]：把多条件查询参数封装为结构体，避免函数参数膨胀。
struct ExpressQueryCondition {
    std::string sender;      // 发件人用户名，可为空。
    std::string receiver;    // 收件人用户名，可为空。
    std::string courier;     // 快递员用户名，可为空。
    std::string expressId;   // 快递单号，可为空。
    std::string startTime;   // 寄送起始时间，可为空。
    std::string endTime;     // 寄送结束时间，可为空。
    int statusFilter = -1;   // -1 表示全部状态。
    std::string itemType;    // 物品类型代码，可为空。
};

// [统计看板模型]：聚合平台收入、状态数量和物品类型收入，用于管理员数据大屏。
struct SystemStatistics {
    double platformNetIncome = 0.0;
    double courierPayout = 0.0;
    int waitingPickupCount = 0;
    int waitingSignCount = 0;
    int signedCount = 0;
    int normalCount = 0;
    int fragileCount = 0;
    int bookCount = 0;
    double normalFee = 0.0;
    double fragileFee = 0.0;
    double bookFee = 0.0;
};

// [绩效模型]：封装快递员任务量、完成率和收入，支撑个人/全局绩效排行。
struct CourierPerformance {
    std::string username;       // 快递员用户名。
    std::string name;           // 快递员姓名。
    int waitingPickupCount = 0; // 待揽收任务数量。
    int waitingSignCount = 0;   // 待签收任务数量。
    int signedCount = 0;        // 已完成任务数量。
    int totalTaskCount = 0;     // 总任务数量。
    double completionRate = 0.0; // 完成率，已完成 / 总任务。
    double income = 0.0;        // 累计收入。
};

// [代码块说明] StringUtil 是最基础的工具类：负责 trim、split、escape/unescape、金额格式化和时间生成。
// [设计说明] 数据文件用 | 分隔，用户输入里如果也有 |，就必须先 escape，避免破坏持久化协议。
//如果用户输入里也有 | 或换行，就会破坏文件格式，所以用 escape 和 unescape 做转义和还原。
// [基础工具]：负责字符串清洗、转义、时间和金额格式化，是持久化协议的底层支撑。
class StringUtil {
public:
    /** 去除字符串左右两侧空白字符。 */
    static std::string trim(const std::string& value) {
        std::size_t begin = 0;
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
        std::istringstream stream(line);
        while (std::getline(stream, current, delimiter)) {
            result.push_back(current);
        }
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

    /** 读取当前日期和时间，用于生成快递单号。 */
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

// [多态与继承]：ExpressItem 定义统一计费接口，子类通过虚函数分发价格逻辑，避免硬编码 if-else。
// [代码定位]：这里可以说明多态派发，业务层只调用 ExpressItem::getPrice()，不关心具体物品子类。
// [代码块说明] ExpressItem 是多态计费的抽象基类，NormalItem、FragileItem、BookItem 都通过重写 getPrice() 实现差异化计费。
// [设计说明] 寄件函数只调用 item->getPrice()，不关心具体类型，体现多态派发和开闭原则。
// [抽象基类]：描述所有快递物品共有的展示与计费能力，是物品多态体系的根节点。
class ExpressItem {
public:
    /** 物品基类需要虚析构，保证通过基类指针释放派生类对象时行为正确。 */
    virtual ~ExpressItem() = default;

    /** 获取持久化类型代码。 */
    virtual std::string typeCode() const = 0;

    /** 获取用于界面展示的中文类型名。 */
    virtual std::string typeName() const = 0;

    /** 获取重量或册数的展示文本。 */
    virtual std::string amountText() const = 0;

    /** 多态计算快递费用。 */
    virtual double getPrice() const = 0;
};

// [多态子类]：易碎品按重量采用更高单价计费，体现差异化业务规则。
class FragileItem : public ExpressItem {
private:
    double weight_;  // 易碎品重量，单位 kg。

public:
    explicit FragileItem(double weight) : weight_(weight) {}

    std::string typeCode() const override { return "Fragile"; }

    std::string typeName() const override { return "易碎品"; }

    std::string amountText() const override { return StringUtil::moneyToString(weight_) + " kg"; }

    double getPrice() const override { return weight_ * 8.0; }
};

// [多态子类]：图书按册数计费，与按重量计费的普通/易碎品形成清晰扩展点。
class BookItem : public ExpressItem {
private:
    int count_;  // 图书册数。

public:
    explicit BookItem(int count) : count_(count) {}

    std::string typeCode() const override { return "Book"; }

    std::string typeName() const override { return "图书"; }

    std::string amountText() const override { return std::to_string(count_) + " 本"; }

    double getPrice() const override { return count_ * 2.0; }
};

// [多态子类]：普通快递按重量采用基础单价计费，是默认物品类型。
class NormalItem : public ExpressItem {
private:
    double weight_;  // 普通快递重量，单位 kg。

public:
    explicit NormalItem(double weight) : weight_(weight) {}

    std::string typeCode() const override { return "Normal"; }

    std::string typeName() const override { return "普通快递"; }

    std::string amountText() const override { return StringUtil::moneyToString(weight_) + " kg"; }

    double getPrice() const override { return weight_ * 5.0; }
};

// [工厂组件]：屏蔽物品子类创建细节，让业务层只依赖 ExpressItem 抽象接口。
class ExpressItemFactory {
public:
    /** 根据类型代码和计费权值创建多态物品对象，非法参数返回空指针。 */
    static std::unique_ptr<ExpressItem> createItem(const std::string& type, double amount) {
        if (amount <= 0.0) {
            return nullptr;
        }
        if (type == "Fragile") {
            return std::unique_ptr<ExpressItem>(new FragileItem(amount));
        }
        if (type == "Book") {
            int count = static_cast<int>(amount);
            if (amount != static_cast<double>(count) || count <= 0) {
                return nullptr;
            }
            return std::unique_ptr<ExpressItem>(new BookItem(count));
        }
        if (type == "Normal") {
            return std::unique_ptr<ExpressItem>(new NormalItem(amount));
        }
        return nullptr;
    }
};

// [代码块说明] DirectoryUtil 负责判断目录、创建目录、拼接路径和复制数据文件。
// [设计说明] 从项目根目录、bin 目录或 src 目录启动时，都尽量定位到正确的 data 目录。
// [目录工具]：统一处理数据目录定位和镜像复制，降低从不同工作目录启动的风险。
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

    /** 解析 V2 项目数据目录，避免从 bin 或 src 启动时生成错位 data 目录。 */
    static std::string projectDataDirectory() {
        if (exists("src") && exists("bin")) {
            return ProjectConfig::DataDirectoryName;
        }
        if (exists("../src") && exists("../bin")) {
            return std::string("../") + ProjectConfig::DataDirectoryName;
        }
        return ProjectConfig::DataDirectoryName;
    }

    /** 复制文本数据文件，目标打开失败时静默跳过，主数据不受影响。 */
    static void copyTextFile(const std::string& source, const std::string& target) {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!in || !out) {
            return;
        }
        out << in.rdbuf();
    }
};

// [原子化保存]：统一封装 .tmp/.bak 文件轮转，防范写入中断导致正式数据损坏。
// [代码块说明] StorageManager 是所有仓储保存动作的底层保护层，统一采用 tmp/bak 轮转。
// [设计说明] 写文件失败时返回 false，业务层据此回滚，避免向用户提示假成功。
// [仓储基础设施]：为各 Repository 提供单文件原子保存能力。
class StorageManager {
public:
    /**
     * 原子化保存文本行：先写 .tmp，再把旧正式文件轮转到 .bak，最后提交 .tmp。
     * 任一步失败都会尽量恢复旧正式文件，并向上层返回 false。
     */
    static bool saveLinesAtomically(const std::string& filePath, const std::vector<std::string>& lines) {
        std::string tmpPath = filePath + ".tmp";
        std::string bakPath = filePath + ".bak";

        // 先完整写入临时文件，正式文件在此阶段完全不受影响。
        {
            std::ofstream out(tmpPath, std::ios::trunc);
            if (!out) {
                return false;
            }
            for (const std::string& line : lines) {
                out << line << '\n';
                if (!out) {
                    return false;
                }
            }
            out.flush();
            if (!out) {
                return false;
            }
        }

        bool hadOldFile = DirectoryUtil::exists(filePath);
        if (hadOldFile) {
            std::remove(bakPath.c_str());
            // [验收亮点：原子化保存]：旧正式文件先轮转为 .bak，提交失败时仍有可恢复副本。
            if (std::rename(filePath.c_str(), bakPath.c_str()) != 0) {
                std::remove(tmpPath.c_str());
                return false;
            }
        }

        // [验收亮点：原子化保存]：最后一步才把 .tmp 重命名为正式文件，缩短危险窗口。
        if (std::rename(tmpPath.c_str(), filePath.c_str()) != 0) {
            if (hadOldFile) {
                // 提交失败时尽力把 .bak 恢复为正式文件，向上层返回失败，避免假成功。
                std::rename(bakPath.c_str(), filePath.c_str());
            }
            std::remove(tmpPath.c_str());
            return false;
        }
        return true;
    }
};

#ifdef _WIN32
// [控制台适配]：在 Windows 下把 UTF-8 输出转换为控制台宽字符输出，保障中文显示。
class WindowsConsoleOutputBuffer : public std::streambuf {
private:
    std::streambuf* fallback_;
    HANDLE console_;
    UINT sourceCodePage_;
    std::string buffer_;

    bool isConsole() const {
        DWORD mode = 0;
        return console_ != INVALID_HANDLE_VALUE && GetConsoleMode(console_, &mode) != 0;
    }

    void writeToConsole() {
        if (buffer_.empty()) {
            return;
        }
        if (!isConsole()) {
            fallback_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
            buffer_.clear();
            return;
        }

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
    int overflow(int ch) override {
        if (ch != traits_type::eof()) {
            buffer_ += static_cast<char>(ch);
            writeToConsole();
        }
        return ch;
    }

    std::streamsize xsputn(const char* text, std::streamsize count) override {
        buffer_.append(text, static_cast<std::size_t>(count));
        writeToConsole();
        return count;
    }

    int sync() override {
        writeToConsole();
        return 0;
    }

public:
    explicit WindowsConsoleOutputBuffer(std::streambuf* fallback)
        : fallback_(fallback), console_(GetStdHandle(STD_OUTPUT_HANDLE)), sourceCodePage_(GetACP()) {}
};
#endif

// [运行环境初始化]：统一设置控制台编码、窗口标题和本地化环境。
class ConsoleEnvironment {
public:
    /**
     * 初始化控制台中文环境。
     * Windows 下使用系统中文代码页读入文本，输出时转换为 UTF-16 写入控制台，避开显示兼容问题。
     */
    static void initialize() {
        std::setlocale(LC_ALL, "");
#ifdef _WIN32
        UINT codePage = GetACP();
        SetConsoleOutputCP(codePage);
        SetConsoleCP(codePage);
        SetConsoleTitleW(ProjectConfig::WindowTitle);
        static WindowsConsoleOutputBuffer outputBuffer(std::cout.rdbuf());
        std::cout.rdbuf(&outputBuffer);
#endif
    }
};

// [代码块说明] SHA256 提供摘要算法能力，上层 PasswordHasher 和 Logger 都依赖它。
// [设计说明] 密码保存和日志防篡改都不是明文比较，而是通过哈希摘要完成。
//我这里给操作日志加了哈希链。每条日志都会引用上一条日志的哈希，所以如果中间任意一条被手动篡改，
//管理员执行日志完整性校验时就能发现断链位置。这比普通文本日志更安全，具备防篡改审计能力。
// [密码学组件]：提供 SHA-256 摘要能力，支撑密码哈希与日志哈希链。
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

        std::vector<uint8_t> bytes(input.begin(), input.end());
        uint64_t bitLength = static_cast<uint64_t>(bytes.size()) * 8U;
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

// [代码块说明] PasswordHasher 负责生成 salt、计算密码哈希和校验密码。
// [设计说明] 数据文件里保存的是 salt + hash，不保存用户输入的明文密码。
// [安全组件]：封装随机盐、默认密码和作用域哈希，避免明文密码落盘。
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
        return SHA256::hash(salt + ProjectConfig::PasswordHashScope + password);
    }

    /** 验证明文密码是否与已存储哈希一致。 */
    static bool verify(const std::string& salt, const std::string& password, const std::string& storedHash) {
        return hashPassword(salt, password) == storedHash;
    }
};

// [代码块说明] InputValidator 统一做用户名、手机号、密码、地址等格式校验。
// [设计说明] UI 层先做基础输入约束，业务层再做权限和状态判断，形成两道防线。
// [输入校验器]：统一限制用户名、手机号、密码和地址格式，拦截非法数据进入业务层。
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
    /**
     * 输入校验类集中放置所有格式规则。
     * 验收说明：这样做可以避免注册、查询、日志筛选等流程各写一套判断，
     * 后续如果要修改规则，只需要改这里，业务流程只负责调用校验结果。
     */

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

// [代码块说明] User 表示普通用户，保存账号资料、余额、密码哈希和冻结状态。
// [设计说明] 寄件扣款、充值、冻结账号都围绕 User 对象状态展开。
// [实体模型]：普通用户聚合身份、余额、冻结状态和密码哈希，是用户侧业务的状态载体。
class User {
private:
    std::string username_;      // 平台唯一用户名。
    std::string name_;          // 用户真实姓名。
    std::string phone_;         // 用户电话。
    std::string salt_;          // 密码随机盐。
    std::string passwordHash_;  // 加盐后的密码哈希。
    double balance_;            // 用户账户余额。
    std::string address_;       // 用户默认地址。
    bool frozen_;               // 账号是否被冻结。

public:
    /** 构造空用户，便于反序列化时填充。 */
    User() : balance_(0.0), frozen_(false) {}

    /** 构造完整用户对象。 */
    User(const std::string& username, const std::string& name, const std::string& phone,
         const std::string& salt, const std::string& passwordHash, double balance,
         const std::string& address, bool frozen)
        : username_(username), name_(name), phone_(phone), salt_(salt),
          passwordHash_(passwordHash), balance_(balance), address_(address), frozen_(frozen) {}

    /** 获取用户名。 */
    const std::string& username() const { return username_; }

    /** 获取姓名。 */
    const std::string& name() const { return name_; }

    /** 获取电话。 */
    const std::string& phone() const { return phone_; }

    /** 获取密码随机盐。 */
    const std::string& salt() const { return salt_; }

    /** 获取密码哈希。 */
    const std::string& passwordHash() const { return passwordHash_; }

    /** 获取余额。 */
    double balance() const { return balance_; }

    /** 获取地址。 */
    const std::string& address() const { return address_; }

    /** 判断账号是否被冻结。 */
    bool frozen() const { return frozen_; }

    /** 更新密码随机盐和哈希。 */
    void updatePassword(const std::string& salt, const std::string& passwordHash) {
        salt_ = salt;
        passwordHash_ = passwordHash;
    }

    /** 账户充值。 */
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

    /** 序列化为文件行。 */
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

    /** 从文件行反序列化用户。 */
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

// [代码块说明] Admin 表示管理员和平台账户，保存管理员密码哈希以及平台余额。
// [设计说明] 用户寄件时费用进入管理员账户，快递员揽收时再从管理员账户支付 50% 提成。
// [实体模型]：管理员代表平台账户，承担全局管理权限和平台余额归集。
class Admin {
private:
    std::string username_;      // 管理员用户名。
    std::string name_;          // 管理员姓名。
    std::string salt_;          // 密码随机盐。
    std::string passwordHash_;  // 加盐后的密码哈希。
    double balance_;            // 物流公司账户余额。

public:
    /** 构造默认管理员对象。 */
    Admin() : username_("admin"), name_("SystemAdmin"), balance_(0.0) {
        salt_ = PasswordHasher::makeSalt(username_);
        passwordHash_ = PasswordHasher::hashPassword(salt_, PasswordHasher::defaultAdminPassword());
    }

    /** 获取管理员用户名。 */
    const std::string& username() const { return username_; }

    /** 获取管理员姓名。 */
    const std::string& name() const { return name_; }

    /** 获取密码随机盐。 */
    const std::string& salt() const { return salt_; }

    /** 获取密码哈希。 */
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

    /** 序列化管理员数据。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(username_) << '|'
            << StringUtil::escape(name_) << '|'
            << StringUtil::escape(salt_) << '|'
            << StringUtil::escape(passwordHash_) << '|'
            << StringUtil::moneyToString(balance_);
        return out.str();
    }

    /** 从文件行反序列化管理员。 */
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

// [代码块说明] Courier 表示快递员，除了账号信息，还保存 income、frozen、removed 三个业务状态。
// [设计说明] 自动分配会过滤 frozen/removed，并用 income 做公平性排序。
// [实体模型]：快递员聚合收入、冻结和逻辑删除状态，支撑任务派发与绩效统计。
class Courier {
private:
    std::string username_;      // 快递员唯一用户名。
    std::string name_;          // 快递员姓名。
    std::string phone_;         // 快递员电话。
    std::string salt_;          // 密码随机盐。
    std::string passwordHash_;  // 加盐后的密码哈希。
    double income_;             // 快递员账户余额/累计收入。
    bool frozen_;               // 是否被冻结。
    bool removed_;              // 是否已离职或被逻辑删除。

public:
    /** 构造空快递员，便于反序列化。 */
    Courier() : income_(0.0), frozen_(false), removed_(false) {}

    /** 构造完整快递员对象。 */
    Courier(const std::string& username, const std::string& name, const std::string& phone,
            const std::string& salt, const std::string& passwordHash, double income,
            bool frozen, bool removed)
        : username_(username), name_(name), phone_(phone), salt_(salt),
          passwordHash_(passwordHash), income_(income), frozen_(frozen), removed_(removed) {}

    /** 获取快递员用户名。 */
    const std::string& username() const { return username_; }

    /** 获取快递员姓名。 */
    const std::string& name() const { return name_; }

    /** 获取快递员电话。 */
    const std::string& phone() const { return phone_; }

    /** 获取密码随机盐。 */
    const std::string& salt() const { return salt_; }

    /** 获取密码哈希。 */
    const std::string& passwordHash() const { return passwordHash_; }

    /** 获取账户余额/累计收入。 */
    double income() const { return income_; }

    /** 判断账号是否被冻结。 */
    bool frozen() const { return frozen_; }

    /** 判断账号是否已逻辑删除。 */
    bool removed() const { return removed_; }

    /** 验证快递员密码。 */
    bool verifyPassword(const std::string& password) const {
        return PasswordHasher::verify(salt_, password, passwordHash_);
    }

    /** 增加快递员收入。 */
    void addIncome(double amount) {
        income_ += amount;
    }

    /** 设置冻结状态。 */
    void setFrozen(bool frozen) {
        frozen_ = frozen;
    }

    /** 标记为离职/逻辑删除。 */
    void markRemoved() {
        removed_ = true;
        frozen_ = true;
    }

    /** 序列化为 couriers.txt 文件行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(username_) << '|'
            << StringUtil::escape(name_) << '|'
            << StringUtil::escape(phone_) << '|'
            << StringUtil::escape(salt_) << '|'
            << StringUtil::escape(passwordHash_) << '|'
            << StringUtil::moneyToString(income_) << '|'
            << (frozen_ ? "1" : "0") << '|'
            << (removed_ ? "1" : "0");
        return out.str();
    }

    /** 从 couriers.txt 文件行反序列化快递员。 */
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
        if (!StringUtil::tryParseDouble(parts[5], income_) || income_ < 0.0) {
            return false;
        }
        if ((parts[6] != "0" && parts[6] != "1") || (parts[7] != "0" && parts[7] != "1")) {
            return false;
        }
        frozen_ = parts[6] == "1";
        removed_ = parts[7] == "1";
        return true;
    }
};

// [代码块说明] Express 是快递单实体，记录发件人、收件人、快递员、费用、物品类型和三状态。
// [设计说明] assignCourier、pickup、sign 分别对应分配、揽收、签收三个业务动作。
// [实体模型]：快递单聚合三状态流转、物品类型、费用和责任快递员。
class Express {
private:
    std::string id_;           // 快递单号。
    std::string sender_;       // 发件用户名。
    std::string receiver_;     // 收件用户名。
    std::string courier_;      // 负责该单的快递员用户名，未分配时为空。
    std::string sendTime_;     // 寄送时间。
    std::string pickupTime_;   // 快递员揽收时间，未揽收时为空。
    std::string receiveTime_;  // 签收时间。
    ExpressStatus status_;     // 当前快递状态。
    std::string itemType_;     // 物品类型代码，Phase 2 会接入多态计费。
    double itemAmount_;        // 计费权值，普通/易碎品为重量，图书为册数。
    std::string description_;  // 物品描述。
    double fee_;               // 本次快递费用。

public:
    /** 构造空快递对象，便于反序列化。 */
    Express() : status_(ExpressStatus::WaitingPickup), itemType_("Normal"), itemAmount_(0.0), fee_(15.0) {}

    /** 构造完整快递对象。 */
    Express(const std::string& id, const std::string& sender, const std::string& receiver,
            const std::string& sendTime, const std::string& description, double fee,
            const std::string& itemType = "Normal", double itemAmount = 0.0)
        : id_(id), sender_(sender), receiver_(receiver), courier_(""), sendTime_(sendTime),
          pickupTime_(""), receiveTime_(""), status_(ExpressStatus::WaitingPickup),
          itemType_(itemType), itemAmount_(itemAmount), description_(description), fee_(fee) {}

    /** 获取快递单号。 */
    const std::string& id() const { return id_; }

    /** 获取发件人用户名。 */
    const std::string& sender() const { return sender_; }

    /** 获取收件人用户名。 */
    const std::string& receiver() const { return receiver_; }

    /** 获取负责快递员用户名。 */
    const std::string& courier() const { return courier_; }

    /** 获取寄送时间。 */
    const std::string& sendTime() const { return sendTime_; }

    /** 获取揽收时间。 */
    const std::string& pickupTime() const { return pickupTime_; }

    /** 获取签收时间。 */
    const std::string& receiveTime() const { return receiveTime_; }

    /** 获取快递状态。 */
    ExpressStatus status() const { return status_; }

    /** 获取物品类型代码。 */
    const std::string& itemType() const { return itemType_; }

    /** 获取计费权值。 */
    double itemAmount() const { return itemAmount_; }

    /** 获取物品描述。 */
    const std::string& description() const { return description_; }

    /** 获取快递费用。 */
    double fee() const { return fee_; }

    /** 判断指定用户是否有权查看本快递。 */
    bool belongsTo(const std::string& username) const {
        return belongsToUser(username);
    }

    /** 判断指定普通用户是否与本快递有关。 */
    bool belongsToUser(const std::string& username) const {
        return sender_ == username || receiver_ == username;
    }

    /** 判断指定快递员是否负责本快递。 */
    bool belongsToCourier(const std::string& username) const {
        return !courier_.empty() && courier_ == username;
    }

    /** 判断本快递是否处于待揽收状态。 */
    bool isWaitingPickup() const {
        return status_ == ExpressStatus::WaitingPickup;
    }

    /** 判断本快递是否处于待签收状态。 */
    bool isWaitingSign() const {
        return status_ == ExpressStatus::WaitingSign;
    }

    /** 判断本快递是否已经签收。 */
    bool isSigned() const {
        return status_ == ExpressStatus::Signed;
    }

    /** 分配负责快递员，状态仍保持待揽收。 */
    void assignCourier(const std::string& courierUsername) {
        courier_ = courierUsername;
    }

    /** 快递员完成揽收，推进到待签收并记录揽收时间。 */
    void pickup() {
        status_ = ExpressStatus::WaitingSign;
        pickupTime_ = StringUtil::now();
    }

    /** 将快递设置为已签收状态。 */
    void sign() {
        status_ = ExpressStatus::Signed;
        receiveTime_ = StringUtil::now();
    }

    /** 获取状态中文名。 */
    std::string statusName() const {
        if (status_ == ExpressStatus::WaitingPickup) {
            return "待揽收";
        }
        if (status_ == ExpressStatus::WaitingSign) {
            return "待签收";
        }
        return "已签收";
    }

    /** 序列化为 V2 的 12 字段文件行。 */
    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(id_) << '|'
            << StringUtil::escape(sender_) << '|'
            << StringUtil::escape(receiver_) << '|'
            << StringUtil::escape(courier_) << '|'
            << StringUtil::escape(sendTime_) << '|'
            << StringUtil::escape(pickupTime_) << '|'
            << StringUtil::escape(receiveTime_) << '|'
            << static_cast<int>(status_) << '|'
            << StringUtil::escape(itemType_) << '|'
            << StringUtil::moneyToString(itemAmount_) << '|'
            << StringUtil::escape(description_) << '|'
            << StringUtil::moneyToString(fee_);
        return out.str();
    }

    /** 从文件行反序列化快递，兼容 V1 的 8 字段旧格式。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 8 && parts.size() != 12) {
            return false;
        }
        if (parts.size() == 8) {
            id_ = StringUtil::unescape(parts[0]);
            sender_ = StringUtil::unescape(parts[1]);
            receiver_ = StringUtil::unescape(parts[2]);
            courier_.clear();
            sendTime_ = StringUtil::unescape(parts[3]);
            pickupTime_.clear();
            receiveTime_ = StringUtil::unescape(parts[4]);

            int oldStatusValue = 0;
            std::istringstream statusInput(parts[5]);
            statusInput >> oldStatusValue;
            if (!statusInput || (oldStatusValue != 0 && oldStatusValue != 1)) {
                return false;
            }
            status_ = oldStatusValue == 0 ? ExpressStatus::WaitingSign : ExpressStatus::Signed;
            itemType_ = "Normal";
            description_ = StringUtil::unescape(parts[6]);
            if (!StringUtil::tryParseDouble(parts[7], fee_) || fee_ <= 0.0) {
                return false;
            }
            itemAmount_ = fee_ / 5.0;
            return true;
        }

        id_ = StringUtil::unescape(parts[0]);
        sender_ = StringUtil::unescape(parts[1]);
        receiver_ = StringUtil::unescape(parts[2]);
        courier_ = StringUtil::unescape(parts[3]);
        sendTime_ = StringUtil::unescape(parts[4]);
        pickupTime_ = StringUtil::unescape(parts[5]);
        receiveTime_ = StringUtil::unescape(parts[6]);

        int statusValue = 0;
        std::istringstream statusInput(parts[7]);
        statusInput >> statusValue;
        if (!statusInput || (statusValue < 0 || statusValue > 2)) {
            return false;
        }
        status_ = static_cast<ExpressStatus>(statusValue);
        itemType_ = StringUtil::unescape(parts[8]);
        if (itemType_.empty()) {
            return false;
        }
        if (!StringUtil::tryParseDouble(parts[9], itemAmount_) || itemAmount_ < 0.0) {
            return false;
        }
        description_ = StringUtil::unescape(parts[10]);
        return StringUtil::tryParseDouble(parts[11], fee_) && fee_ > 0.0;
    }
};

class LogEntry {
private:
    int sequence_;           // 哈希链序号；旧格式日志为 0。
    std::string time_;       // 操作时间。
    std::string actorType_;  // 操作人类型。
    std::string actor_;      // 操作人用户名。
    std::string action_;     // 操作名称。
    std::string result_;     // 操作结果。
    std::string detail_;     // 操作详情。
    std::string prevHash_;   // 上一条日志哈希。
    std::string currentHash_;  // 当前日志哈希。

public:
    /** 构造空日志记录，便于反序列化。 */
    LogEntry() : sequence_(0) {}

    /** 构造完整日志记录。 */
    LogEntry(int sequence, const std::string& time, const std::string& actorType, const std::string& actor,
             const std::string& action, const std::string& result, const std::string& detail)
        : sequence_(sequence), time_(time), actorType_(actorType), actor_(actor), action_(action),
          result_(result), detail_(detail), prevHash_(""), currentHash_("") {}

    /** 获取哈希链序号。 */
    int sequence() const { return sequence_; }

    /** 获取操作时间。 */
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

    /** 获取上一条日志哈希。 */
    const std::string& prevHash() const { return prevHash_; }

    /** 获取当前日志哈希。 */
    const std::string& currentHash() const { return currentHash_; }

    /** 判断是否为哈希链新格式日志。 */
    bool hasHashChain() const { return sequence_ > 0 && !currentHash_.empty(); }

    /** 设置哈希字段。 */
    void setHashes(const std::string& prevHash, const std::string& currentHash) {
        prevHash_ = prevHash;
        currentHash_ = currentHash;
    }

    /** 生成参与哈希计算的规范文本。 */
    std::string hashPayload() const {
        std::ostringstream out;
        out << sequence_ << '|'
            << time_ << '|'
            << actorType_ << '|'
            << actor_ << '|'
            << action_ << '|'
            << result_ << '|'
            << detail_ << '|'
            << prevHash_;
        return out.str();
    }

    /** 序列化日志记录。 */
    std::string serialize() const {
        std::ostringstream out;
        out << sequence_ << '|'
            << StringUtil::escape(time_) << '|'
            << StringUtil::escape(actorType_) << '|'
            << StringUtil::escape(actor_) << '|'
            << StringUtil::escape(action_) << '|'
            << StringUtil::escape(result_) << '|'
            << StringUtil::escape(detail_) << '|'
            << StringUtil::escape(prevHash_) << '|'
            << StringUtil::escape(currentHash_);
        return out.str();
    }

    /** 从文件行反序列化日志记录；兼容 V1/V2 旧 6 字段日志。 */
    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 6 && parts.size() != 9) {
            return false;
        }
        if (parts.size() == 6) {
            sequence_ = 0;
            time_ = StringUtil::unescape(parts[0]);
            actorType_ = StringUtil::unescape(parts[1]);
            actor_ = StringUtil::unescape(parts[2]);
            action_ = StringUtil::unescape(parts[3]);
            result_ = StringUtil::unescape(parts[4]);
            detail_ = StringUtil::unescape(parts[5]);
            prevHash_.clear();
            currentHash_.clear();
            return true;
        }
        if (!StringUtil::isDigits(parts[0])) {
            return false;
        }
        std::istringstream seqInput(parts[0]);
        seqInput >> sequence_;
        if (!seqInput || sequence_ <= 0) {
            return false;
        }
        time_ = StringUtil::unescape(parts[1]);
        actorType_ = StringUtil::unescape(parts[2]);
        actor_ = StringUtil::unescape(parts[3]);
        action_ = StringUtil::unescape(parts[4]);
        result_ = StringUtil::unescape(parts[5]);
        detail_ = StringUtil::unescape(parts[6]);
        prevHash_ = StringUtil::unescape(parts[7]);
        currentHash_ = StringUtil::unescape(parts[8]);
        if (prevHash_.empty() || currentHash_.empty()) {
            return false;
        }
        return true;
    }
};

// [代码块说明] AuthState 单独记录某角色某账号的连续登录失败次数和最后失败时间。
// [设计说明] 三次失败冻结不直接扩展 users.txt/couriers.txt 字段，降低实体协议污染。
// [安全状态模型]：独立记录登录失败次数，避免污染 User/Courier 实体文件协议。
class AuthState {
private:
    std::string role_;
    std::string username_;
    int failedCount_;
    std::string lastFailedTime_;

public:
    AuthState() : failedCount_(0) {}

    AuthState(const std::string& role, const std::string& username, int failedCount,
              const std::string& lastFailedTime)
        : role_(role), username_(username), failedCount_(failedCount), lastFailedTime_(lastFailedTime) {}

    const std::string& role() const { return role_; }

    const std::string& username() const { return username_; }

    int failedCount() const { return failedCount_; }

    const std::string& lastFailedTime() const { return lastFailedTime_; }

    void recordFailure() {
        ++failedCount_;
        lastFailedTime_ = StringUtil::now();
    }

    void resetFailures() {
        failedCount_ = 0;
        lastFailedTime_.clear();
    }

    std::string key() const {
        return role_ + "|" + username_;
    }

    std::string serialize() const {
        std::ostringstream out;
        out << StringUtil::escape(role_) << '|'
            << StringUtil::escape(username_) << '|'
            << failedCount_ << '|'
            << StringUtil::escape(lastFailedTime_);
        return out.str();
    }

    bool deserialize(const std::string& line) {
        std::vector<std::string> parts = StringUtil::split(line, '|');
        if (parts.size() != 4 || !StringUtil::isDigits(parts[2])) {
            return false;
        }
        role_ = StringUtil::unescape(parts[0]);
        username_ = StringUtil::unescape(parts[1]);
        std::istringstream input(parts[2]);
        input >> failedCount_;
        if (!input || failedCount_ < 0 || role_.empty() || username_.empty()) {
            return false;
        }
        lastFailedTime_ = StringUtil::unescape(parts[3]);
        return true;
    }
};

// [代码块说明] UserRepository 负责 users.txt 的读写，把文件行转换为 User 对象。
// [设计说明] Repository 只处理持久化，不判断用户是否能寄件，业务判断统一放在 LogisticsSystem。
// [仓储组件]：负责 users.txt 的加载、异常行过滤和原子化保存。
//我没有直接覆盖正式数据文件，而是先写 .tmp 临时文件，确认写入成功后再把旧文件轮转成 .bak 备份，
//最后把 .tmp 提交为正式文件。这样可以减少断电或保存失败导致数据文件损坏的风险。
class UserRepository {
private:
    std::string filePath_;  // 用户数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时跳过的异常行数。

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
        std::vector<std::string> lines;
        for (const User& user : users) {
            lines.push_back(user.serialize());
        }
        return StorageManager::saveLinesAtomically(filePath_, lines);
    }
};

// [仓储组件]：负责 admin.txt 的加载与保存，文件异常时回退默认管理员。
class AdminRepository {
private:
    std::string filePath_;  // 管理员数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时发现的异常行数。

public:
    /** 构造管理员仓储。 */
    explicit AdminRepository(const std::string& filePath) : filePath_(filePath), lastInvalidLineCount_(0) {}

    /** 从文件加载管理员；文件不存在时返回默认管理员。 */
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

    /** 获取最近一次加载时发现的异常行数。 */
    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    /** 保存管理员到文件。 */
    bool save(const Admin& admin) const {
        return StorageManager::saveLinesAtomically(filePath_, std::vector<std::string>{admin.serialize()});
    }
};

// [仓储组件]：负责 couriers.txt 的持久化，保留 frozen/removed 等管理状态。
class CourierRepository {
private:
    std::string filePath_;  // 快递员数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时跳过的异常行数。

public:
    /** 构造快递员仓储。 */
    explicit CourierRepository(const std::string& filePath) : filePath_(filePath), lastInvalidLineCount_(0) {}

    /** 从文件加载快递员列表，异常行会被跳过。 */
    std::vector<Courier> load() const {
        std::vector<Courier> couriers;
        lastInvalidLineCount_ = 0;
        std::ifstream in(filePath_);
        std::string line;
        while (std::getline(in, line)) {
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            Courier courier;
            if (courier.deserialize(line)) {
                couriers.push_back(courier);
            } else {
                ++lastInvalidLineCount_;
            }
        }
        return couriers;
    }

    /** 获取最近一次加载时跳过的异常行数。 */
    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    /** 保存快递员列表到文件。 */
    bool save(const std::vector<Courier>& couriers) const {
        std::vector<std::string> lines;
        for (const Courier& courier : couriers) {
            lines.push_back(courier.serialize());
        }
        return StorageManager::saveLinesAtomically(filePath_, lines);
    }
};

// [仓储组件]：负责 expresses.txt 的持久化，并兼容 V1 旧格式快递记录。
class ExpressRepository {
private:
    std::string filePath_;  // 快递数据文件路径。
    mutable int lastInvalidLineCount_;  // 最近一次加载时跳过的异常行数。

public:
    /** 构造快递仓储。 */
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
        std::vector<std::string> lines;
        for (const Express& express : expresses) {
            lines.push_back(express.serialize());
        }
        return StorageManager::saveLinesAtomically(filePath_, lines);
    }
};

// [仓储组件]：负责 auth_state.txt 的独立认证状态持久化。
class AuthStateRepository {
private:
    std::string filePath_;
    mutable int lastInvalidLineCount_;

public:
    explicit AuthStateRepository(const std::string& filePath)
        : filePath_(filePath), lastInvalidLineCount_(0) {}

    std::vector<AuthState> load() const {
        std::vector<AuthState> states;
        lastInvalidLineCount_ = 0;
        std::ifstream in(filePath_);
        std::string line;
        while (std::getline(in, line)) {
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            AuthState state;
            if (state.deserialize(line)) {
                states.push_back(state);
            } else {
                ++lastInvalidLineCount_;
            }
        }
        return states;
    }

    int lastInvalidLineCount() const {
        return lastInvalidLineCount_;
    }

    bool save(const std::vector<AuthState>& states) const {
        std::vector<std::string> lines;
        for (const AuthState& state : states) {
            lines.push_back(state.serialize());
        }
        return StorageManager::saveLinesAtomically(filePath_, lines);
    }
};

// [日志防篡改哈希链]：每条新日志引用上一条 currentHash，形成可审计的 SHA-256 链。
// [代码定位]：这里可以展示 operations.log 的 9 字段结构，重点解释 prevHash/currentHash 如何形成防篡改链。
// [代码块说明] Logger 负责 operations.log 的追加、读取和哈希链校验。
// [设计说明] 每条日志保存 prevHash/currentHash，任何中间日志被手动修改都会导致链式校验失败。
// [审计组件]：统一写入操作日志，并提供链式完整性校验入口。
class Logger {
private:
    std::string filePath_;  // 操作日志文件路径。

    /** 读取最后一条有效日志的序号和哈希，用于继续追加哈希链。 */
    bool readLastChainState(int& nextSeq, std::string& prevHash) const {
        nextSeq = 1;
        prevHash = "GENESIS";
        std::ifstream in(filePath_);
        std::string line;
        int legacyCount = 0;
        // 从尾部有效日志继承 prevHash，保证追加日志能接续历史链条。
        while (std::getline(in, line)) {
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            LogEntry entry;
            if (!entry.deserialize(line)) {
                continue;
            }
            if (entry.hasHashChain()) {
                nextSeq = entry.sequence() + 1;
                prevHash = entry.currentHash();
            } else {
                ++legacyCount;
                if (prevHash == "GENESIS") {
                    prevHash = "LEGACY";
                }
                nextSeq = legacyCount + 1;
            }
        }
        return true;
    }

public:
    /** 构造日志记录器。 */
    explicit Logger(const std::string& filePath) : filePath_(filePath) {}

    /** 追加一条带哈希链的操作日志。 */
    void write(const std::string& actorType, const std::string& actor, const std::string& action,
               const std::string& result, const std::string& detail) const {
        int sequence = 1;
        std::string prevHash = "GENESIS";
        readLastChainState(sequence, prevHash);
        std::ofstream out(filePath_, std::ios::app);
        if (!out) {
            return;
        }
        LogEntry entry(sequence, StringUtil::now(), actorType, actor, action, result, detail);
        entry.setHashes(prevHash, "");
        // [验收亮点：日志防篡改哈希链]：哈希载荷包含 prevHash，任意中间日志被改都会导致后续断链。
        std::string currentHash = SHA256::hash(entry.hashPayload());
        entry.setHashes(prevHash, currentHash);
        out << entry.serialize() << '\n';
    }

    /** 加载全部日志。 */
    std::vector<LogEntry> load() const {
        std::vector<LogEntry> entries;
        std::ifstream in(filePath_);
        std::string line;
        // [验收亮点：日志防篡改哈希链]：逐行重算并比对 prevHash/currentHash，可精准定位篡改行。
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

    /** 校验 9 字段日志哈希链完整性；旧 6 字段日志会被识别为历史记录并跳过链校验。 */
    bool verifyHashChain(std::string& message) const {
        std::ifstream in(filePath_);
        if (!in) {
            message = "日志文件不存在或无法打开。";
            return false;
        }
        std::string line;
        std::string expectedPrev = "GENESIS";
        bool seenChained = false;
        int lineNo = 0;
        int legacyCount = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            if (StringUtil::trim(line).empty()) {
                continue;
            }
            LogEntry entry;
            if (!entry.deserialize(line)) {
                message = "日志第 " + std::to_string(lineNo) + " 行格式损坏，无法解析。";
                return false;
            }
            if (!entry.hasHashChain()) {
                ++legacyCount;
                if (!seenChained) {
                    expectedPrev = "LEGACY";
                    continue;
                }
                message = "日志第 " + std::to_string(lineNo) + " 行是旧格式，出现在哈希链之后。";
                return false;
            }
            seenChained = true;
            if (entry.prevHash() != expectedPrev) {
                message = "日志第 " + std::to_string(lineNo) + " 行 prevHash 断链。";
                return false;
            }
            std::string recomputed = SHA256::hash(entry.hashPayload());
            if (recomputed != entry.currentHash()) {
                message = "日志第 " + std::to_string(lineNo) + " 行 currentHash 不匹配，疑似被篡改。";
                return false;
            }
            expectedPrev = entry.currentHash();
        }
        if (!seenChained) {
            message = "日志仅包含旧格式记录，尚未形成可校验哈希链。";
            return true;
        }
        message = legacyCount > 0
                      ? "日志哈希链完整；前 " + std::to_string(legacyCount) + " 条为升级前旧格式记录。"
                      : "日志哈希链完整，未发现篡改。";
        return true;
    }
};

// [代码块说明] TablePrinter 封装表格打印，并用 displayWidth 处理中文宽度。
// [设计说明] 中文按宽度 2、英文按宽度 1 计算，避免控制台表格边框错位。
// [渲染工具]：计算中英文混合显示宽度，修复 std::setw 在中文表格中的错列问题。
class TablePrinter {
private:
    static void printSeparator(const std::vector<int>& widths) {
        int total = 1;
        for (int width : widths) {
            total += width + 3;
        }
        std::cout << std::string(static_cast<std::size_t>(total), '-') << '\n';
    }

public:
    /**
     * 估算 Windows 中文控制台中的显示宽度：ASCII 计 1，GBK/UTF-8 中文等高位字节序列计 2。
     * 当前源码按 UTF-8 编写、运行字面量按 GBK 输出，两种场景都可通过高位字节归并避免错列。
     */
    static int displayWidth(const std::string& text) {
        int width = 0;
        for (std::size_t i = 0; i < text.size();) {
            unsigned char ch = static_cast<unsigned char>(text[i]);
            if (ch < 0x80) {
                ++width;
                ++i;
            } else if ((ch & 0xE0) == 0xC0 && i + 1 < text.size()) {
                width += 2;
                i += 2;
            } else if ((ch & 0xF0) == 0xE0 && i + 2 < text.size()) {
                width += 2;
                i += 3;
            } else if ((ch & 0xF8) == 0xF0 && i + 3 < text.size()) {
                width += 2;
                i += 4;
            } else {
                width += 2;
                i += (i + 1 < text.size()) ? 2 : 1;
            }
        }
        return width;
    }

    static std::string padRight(const std::string& text, int width) {
        int padding = width - displayWidth(text);
        if (padding <= 0) {
            return text;
        }
        return text + std::string(static_cast<std::size_t>(padding), ' ');
    }

    static void printRow(const std::vector<std::string>& cells, const std::vector<int>& widths) {
        std::cout << '|';
        for (std::size_t i = 0; i < cells.size() && i < widths.size(); ++i) {
            std::cout << ' ' << padRight(cells[i], widths[i]) << " |";
        }
        std::cout << '\n';
    }

    static void printTable(const std::vector<std::string>& headers,
                           const std::vector<std::vector<std::string> >& rows,
                           const std::vector<int>& widths) {
        printSeparator(widths);
        printRow(headers, widths);
        printSeparator(widths);
        for (const std::vector<std::string>& row : rows) {
            printRow(row, widths);
        }
        printSeparator(widths);
    }
};

// [代码块说明] ConsoleUI 是控制台交互工具类，负责读字符串、读数字、读密码和打印表格。
// [设计说明] 密码输入隐藏、输入范围检查都在这里统一处理，避免菜单代码重复。
// [UI 工具层]：集中处理控制台输入、表格展示和密码读取，业务层不直接依赖 std::cin/std::cout。
class ConsoleUI {
private:
    /** 密码输入是否启用隐藏模式；由 main() 统一设置，便于验收演示时切换。 */
    static bool& passwordHiddenEnabled() {
        static bool enabled = true;
        return enabled;
    }

public:
    /**
     * 控制台交互类只负责显示、读取和表格输出。
     * 验收说明：菜单显示与业务逻辑分离，main 函数不直接处理具体业务，
     * 这能体现面向对象分层，也方便后续 v2/v3 复用业务层。
     */

    /** 清屏：使用多行空白避免依赖平台命令。 */
    static void clear() {
        for (int i = 0; i < 30; ++i) {
            std::cout << '\n';
        }
    }

    /** 打印页面标题。 */
    static void title(const std::string& text) {
        std::cout << "\n============================================================\n";
        std::cout << "  " << text << '\n';
        std::cout << "============================================================\n";
    }

    /** 打印提示信息。 */
    static void message(const std::string& text) {
        std::cout << text << '\n';
    }

    /** 设置全局密码输入模式：true 表示星号隐藏，false 表示明文回显。 */
    static void setPasswordHiddenEnabled(bool enabled) {
        passwordHiddenEnabled() = enabled;
    }

    /** 展示当前密码输入模式，便于验收时说明系统已统一切换。 */
    static void printPasswordInputMode() {
        std::cout << "密码输入模式：" << (passwordHiddenEnabled() ? "隐藏星号模式" : "明文演示模式") << '\n';
        std::cout << "------------------------------------------------------------\n";
    }

    /** 等待用户按回车继续。 */
    static void pause() {
        std::cout << "\n按回车键继续...";
        std::string ignored;
        std::getline(std::cin, ignored);
    }

    /**
     * 读取一行文本并去掉首尾空白。
     * 当输入流结束时返回统一标记，调用方据此取消当前流程，避免空输入被误当成有效数据。
     */
    static std::string readLine(const std::string& prompt) {
        std::cout << prompt;
        std::string value;
        if (!std::getline(std::cin, value)) {
            return "__EOF__";
        }
        return StringUtil::trim(value);
    }

    /**
     * 隐藏读取密码。
     *
     * Windows 控制台下使用 _getch() 逐字符接管输入：字符不会被系统自动回显，
     * 程序只输出 '*' 作为掩码；用户按 Backspace 时，先删除内部字符串末尾字符，
     * 再输出 "\b \b"：第一个 \b 把光标退回到上一个星号位置，中间空格覆盖原星号，
     * 最后一个 \b 再把光标退回覆盖后的位置，从视觉上完成干净擦除。
     *
     * 非 Windows 环境保留 getline 兜底，方便后续移植；当前课程验收环境为 Windows，
     * 因此主路径不会使用明文回显。
     */
    // [验收亮点：安全防线]：密码输入使用 _getch() 接管字符读取，避免明文回显被旁观。
    static std::string readPasswordHidden(const std::string& prompt) {
        if (!passwordHiddenEnabled()) {
            std::cout << prompt;
            std::string password;
            if (!std::getline(std::cin, password)) {
                return "__EOF__";
            }
            return password;
        }
        std::cout << prompt;
#ifdef _WIN32
        std::string password;
        while (true) {
            // 字符级读取，不经过 getline/std::cin 的默认回显路径。
            int ch = _getch();
            if (ch == 13 || ch == '\r') {
                std::cout << '\n';
                return password;
            }
            if (ch == 8 || ch == '\b') {
                if (!password.empty()) {
                    password.pop_back();
                    // 退格、空格覆盖、再次退格：视觉上擦除上一个星号。
                    std::cout << "\b \b";
                }
                continue;
            }
            if (ch == 0 || ch == 224) {
                _getch();  // 吞掉方向键、功能键等扩展按键的第二个字节。
                continue;
            }
            if (ch >= 32 && ch <= 126) {
                password.push_back(static_cast<char>(ch));
                std::cout << '*';
            }
        }
#else
        std::string password;
        if (!std::getline(std::cin, password)) {
            return "__EOF__";
        }
        return password;
#endif
    }

    /** 判断 readLine 返回值是否表示输入流结束。 */
    static bool isEofValue(const std::string& value) {
        return value == "__EOF__";
    }

    /**
     * 读取范围内整数。
     * 返回 false 表示输入流结束，调用方应主动返回上级菜单；返回 true 时 value 一定合法。
     */
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

    /**
     * 读取正数金额。
     * 金额输入统一经过小数格式、正数范围和两位小数限制，防止负数、字母和畸形金额进入业务层。
     */
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

    /** 读取确认选项。 */
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
        std::vector<std::string> headers;
        std::vector<int> widths;
        if (withIndex) {
            headers.push_back("序号");
            widths.push_back(6);
        }
        headers.push_back("单号");
        headers.push_back("发件人");
        headers.push_back("收件人");
        headers.push_back("状态");
        headers.push_back("寄送时间");
        headers.push_back("费用");
        headers.push_back("描述");
        widths.push_back(20);
        widths.push_back(14);
        widths.push_back(14);
        widths.push_back(10);
        widths.push_back(20);
        widths.push_back(10);
        widths.push_back(18);
        std::vector<std::vector<std::string> > rows;
        for (std::size_t i = 0; i < expresses.size(); ++i) {
            std::vector<std::string> row;
            if (withIndex) {
                row.push_back(std::to_string(i + 1));
            }
            row.push_back(expresses[i].id());
            row.push_back(expresses[i].sender());
            row.push_back(expresses[i].receiver());
            row.push_back(expresses[i].statusName());
            row.push_back(expresses[i].sendTime());
            row.push_back(StringUtil::moneyToString(expresses[i].fee()));
            row.push_back(expresses[i].description());
            rows.push_back(row);
        }
        TablePrinter::printTable(headers, rows, widths);
    }

    /** 打印用户表格。 */
    static void printUserTable(const std::vector<User>& users) {
        if (users.empty()) {
            std::cout << "暂无用户。\n";
            return;
        }
        std::vector<std::vector<std::string> > rows;
        for (const User& user : users) {
            rows.push_back(std::vector<std::string>{
                user.username(), user.name(), user.phone(), StringUtil::moneyToString(user.balance()),
                user.frozen() ? "冻结" : "正常", user.address()});
        }
        TablePrinter::printTable(std::vector<std::string>{"用户名", "姓名", "电话", "余额", "状态", "地址"},
                                 rows, std::vector<int>{16, 14, 14, 12, 10, 18});
    }

    /** 打印快递员表格。 */
    static void printCourierTable(const std::vector<Courier>& couriers) {
        if (couriers.empty()) {
            std::cout << "暂无快递员。\n";
            return;
        }
        std::vector<std::vector<std::string> > rows;
        for (const Courier& courier : couriers) {
            rows.push_back(std::vector<std::string>{
                courier.username(), courier.name(), courier.phone(), StringUtil::moneyToString(courier.income()),
                courier.frozen() ? "是" : "否", courier.removed() ? "已停用" : "在职"});
        }
        TablePrinter::printTable(std::vector<std::string>{"用户名", "姓名", "电话", "收入", "冻结", "状态"},
                                 rows, std::vector<int>{16, 14, 14, 12, 10, 10});
    }

    /** 打印日志表格。 */
    static void printLogTable(const std::vector<LogEntry>& entries) {
        if (entries.empty()) {
            std::cout << "暂无日志记录。\n";
            return;
        }
        std::vector<std::vector<std::string> > rows;
        for (const LogEntry& entry : entries) {
            rows.push_back(std::vector<std::string>{
                entry.time(), entry.actorType(), entry.actor(), entry.action(), entry.result(), entry.detail()});
        }
        TablePrinter::printTable(std::vector<std::string>{"时间", "身份", "账号", "操作", "结果", "详情"},
                                 rows, std::vector<int>{20, 8, 14, 18, 10, 28});
    }

    /** 打印快递员绩效表格。 */
    static void printCourierPerformanceTable(const std::vector<CourierPerformance>& performances) {
        if (performances.empty()) {
            std::cout << "暂无快递员绩效数据。\n";
            return;
        }
        std::vector<std::vector<std::string> > rows;
        for (const CourierPerformance& performance : performances) {
            rows.push_back(std::vector<std::string>{
                performance.username,
                performance.name,
                std::to_string(performance.waitingPickupCount),
                std::to_string(performance.waitingSignCount),
                std::to_string(performance.signedCount),
                std::to_string(performance.totalTaskCount),
                StringUtil::moneyToString(performance.completionRate * 100.0) + "%",
                StringUtil::moneyToString(performance.income)});
        }
        TablePrinter::printTable(
            std::vector<std::string>{"用户名", "姓名", "待揽收", "待签收", "已完成", "总任务", "完成率", "收入"},
            rows,
            std::vector<int>{16, 14, 8, 8, 8, 8, 10, 12});
    }
};

// [代码定位]：这里是 V2 的 Service 层，UI 只收集输入，所有权限校验、状态流转和资金修改都集中在本类。
// [代码块说明] LogisticsSystem 是 V2 的业务服务层，集中管理注册、登录、寄件、揽收、签收、分配和统计。
// [设计说明] UI 层不直接改余额或状态，而是调用 LogisticsSystem，由它统一做权限、状态和保存失败回滚。
//虽然这是文本文件持久化，不是真正数据库事务，但我在业务层模拟了事务一致性。像快递员揽收这种同时涉及状态
//  和资金的操作，会先保存旧快照，保存失败时恢复快递、管理员和快递员三个对象，避免账实不一致。
// [核心组件]：负责全局业务流转、权限拦截、状态机推进、资金一致性和审计日志。
class LogisticsSystem {
private:
    struct CourierScheduleState {
        std::size_t courierIndex;
        int unfinishedCount;

        CourierScheduleState(std::size_t index, int count)
            : courierIndex(index), unfinishedCount(count) {}
    };

    std::string dataDir_;                          // 数据目录。
    UserRepository userRepo_;                      // 用户仓储。
    AdminRepository adminRepo_;                    // 管理员仓储。
    CourierRepository courierRepo_;                // 快递员仓储。
    ExpressRepository expressRepo_;                // 快递仓储。
    AuthStateRepository authStateRepo_;            // 登录失败状态仓储。
    Logger logger_;                                // 操作日志。
    std::vector<User> users_;                      // 内存中的用户列表。
    std::vector<Courier> couriers_;                // 内存中的快递员列表。
    std::vector<Express> expresses_;               // 内存中的快递列表。
    std::vector<AuthState> authStates_;            // 登录失败次数状态。
    Admin admin_;                                  // 管理员对象。

    /** 查找用户下标，不存在时返回 users_.size()。 */
    std::size_t findUserIndex(const std::string& username) const {
        for (std::size_t i = 0; i < users_.size(); ++i) {
            if (users_[i].username() == username) {
                return i;
            }
        }
        return users_.size();
    }

    /** 查找快递员下标，不存在时返回 couriers_.size()。 */
    std::size_t findCourierIndex(const std::string& username) const {
        for (std::size_t i = 0; i < couriers_.size(); ++i) {
            if (couriers_[i].username() == username) {
                return i;
            }
        }
        return couriers_.size();
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

    /** 查找认证状态下标。 */
    std::size_t findAuthStateIndex(const std::string& role, const std::string& username) const {
        for (std::size_t i = 0; i < authStates_.size(); ++i) {
            if (authStates_[i].role() == role && authStates_[i].username() == username) {
                return i;
            }
        }
        return authStates_.size();
    }

    /** 生成不会重复的快递单号。 */
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

    /** 保存所有核心数据。 */
    bool saveAll() const {
        bool ok = userRepo_.save(users_) && adminRepo_.save(admin_) &&
                  courierRepo_.save(couriers_) && expressRepo_.save(expresses_);
        if (ok) {
            mirrorCoreDataFiles();
        }
        return ok;
    }

    /** 保存认证状态。 */
    bool saveAuthStates() const {
        return authStateRepo_.save(authStates_);
    }

    /** 登录成功后清零失败次数。 */
    bool resetAuthFailures(const std::string& role, const std::string& username) {
        std::size_t index = findAuthStateIndex(role, username);
        if (index == authStates_.size()) {
            return true;
        }
        authStates_[index].resetFailures();
        return saveAuthStates();
    }

    /**
     * [输入]：登录身份 role 和账号 username。
     * [输出]：最新连续失败次数；保存失败返回 -1。
     * [核心业务目标]：把认证失败次数落盘，为三次冻结提供可恢复状态。
     */
    int recordAuthFailure(const std::string& role, const std::string& username) {
        std::size_t index = findAuthStateIndex(role, username);
        if (index == authStates_.size()) {
            authStates_.push_back(AuthState(role, username, 0, ""));
            index = authStates_.size() - 1;
        }
        authStates_[index].recordFailure();
        int count = authStates_[index].failedCount();
        if (!saveAuthStates()) {
            return -1;
        }
        return count;
    }

    /** 统一处理保存失败提示和日志。 */
    void reportSaveFailure(const std::string& actorType, const std::string& actor,
                           const std::string& action, std::string& message) const {
        message = "数据保存失败，请检查 data 目录权限，操作已回滚。";
        logger_.write(actorType, actor, action, "FAILED", message);
    }

    /** 将核心数据镜像到 V2 的 bin/data 和 src/data，避免手动点错目录时看到旧数据。 */
    void mirrorCoreDataFiles() const {
        std::vector<std::string> mirrorDirs;
        if (DirectoryUtil::exists("src") && DirectoryUtil::exists("bin")) {
            mirrorDirs.push_back(DirectoryUtil::join("src", ProjectConfig::DataDirectoryName));
            mirrorDirs.push_back(DirectoryUtil::join("bin", ProjectConfig::DataDirectoryName));
        } else if (DirectoryUtil::exists("../src") && DirectoryUtil::exists("../bin")) {
            mirrorDirs.push_back(DirectoryUtil::join("../src", ProjectConfig::DataDirectoryName));
            mirrorDirs.push_back(DirectoryUtil::join("../bin", ProjectConfig::DataDirectoryName));
        }
        for (const std::string& dir : mirrorDirs) {
            DirectoryUtil::createDirectory(dir);
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, ProjectConfig::AdminFileName),
                                        DirectoryUtil::join(dir, ProjectConfig::AdminFileName));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, ProjectConfig::UserFileName),
                                        DirectoryUtil::join(dir, ProjectConfig::UserFileName));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, ProjectConfig::CourierFileName),
                                        DirectoryUtil::join(dir, ProjectConfig::CourierFileName));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, ProjectConfig::ExpressFileName),
                                        DirectoryUtil::join(dir, ProjectConfig::ExpressFileName));
            DirectoryUtil::copyTextFile(DirectoryUtil::join(dataDir_, ProjectConfig::AuthStateFileName),
                                        DirectoryUtil::join(dir, ProjectConfig::AuthStateFileName));
        }
    }

    /** 记录启动时发现的数据文件异常，便于管理员验收时追踪。 */
    void logDataLoadWarnings() const {
        int badUsers = userRepo_.lastInvalidLineCount();
        int badAdmin = adminRepo_.lastInvalidLineCount();
        int badCouriers = courierRepo_.lastInvalidLineCount();
        int badExpresses = expressRepo_.lastInvalidLineCount();
        int badAuthStates = authStateRepo_.lastInvalidLineCount();
        if (badUsers == 0 && badAdmin == 0 && badCouriers == 0 && badExpresses == 0 && badAuthStates == 0) {
            return;
        }
        std::ostringstream detail;
        detail << "加载数据时跳过异常行：users=" << badUsers
               << ", admin=" << badAdmin
               << ", couriers=" << badCouriers
               << ", expresses=" << badExpresses
               << ", auth_state=" << badAuthStates
               << "；程序继续运行并保留可恢复数据";
        logger_.write("SYSTEM", "system", "DATA_VALIDATE", "FAILED", detail.str());
    }

    /** 判断快递是否满足对象化查询条件。 */
    bool matchExpress(const Express& express, const ExpressQueryCondition& condition) const {
        if (!condition.sender.empty() && express.sender() != condition.sender) {
            return false;
        }
        if (!condition.receiver.empty() && express.receiver() != condition.receiver) {
            return false;
        }
        if (!condition.courier.empty() && express.courier() != condition.courier) {
            return false;
        }
        if (!condition.expressId.empty() && express.id() != condition.expressId) {
            return false;
        }
        if (!condition.startTime.empty() && express.sendTime() < condition.startTime) {
            return false;
        }
        if (!condition.endTime.empty() && express.sendTime() > condition.endTime) {
            return false;
        }
        if (condition.statusFilter >= 0 && static_cast<int>(express.status()) != condition.statusFilter) {
            return false;
        }
        if (!condition.itemType.empty() && express.itemType() != condition.itemType) {
            return false;
        }
        return true;
    }

    /**
     * [输入]：无，直接读取当前快递员和快递内存状态。
     * [输出]：可用快递员的调度快照。
     * [核心业务目标]：过滤冻结/停用快递员，并缓存未完成任务数，支撑自动分配。
     */
    std::vector<CourierScheduleState> buildCourierScheduleStates() const {
        std::vector<CourierScheduleState> states;
        for (std::size_t i = 0; i < couriers_.size(); ++i) {
            // [验收亮点：智能调度算法]：第一层可用性过滤，冻结和停用快递员不进入候选池。
            if (!couriers_[i].frozen() && !couriers_[i].removed()) {
                states.push_back(CourierScheduleState(i, 0));
            }
        }
        for (const Express& express : expresses_) {
            if (express.courier().empty() || express.isSigned()) {
                continue;
            }
            for (CourierScheduleState& state : states) {
                if (couriers_[state.courierIndex].username() == express.courier()) {
                    ++state.unfinishedCount;
                    break;
                }
            }
        }
        return states;
    }

    /**
     * [输入]：buildCourierScheduleStates() 生成的候选池。
     * [输出]：最优快递员在 couriers_ 中的下标。
     * [核心业务目标]：按负载均衡、收入公平性和字典序稳定性完成调度决策。
     */
    std::size_t selectBestCourierIndex(const std::vector<CourierScheduleState>& states) const {
        if (states.empty()) {
            return couriers_.size();
        }
        std::size_t bestStateIndex = 0;
        for (std::size_t i = 1; i < states.size(); ++i) {
            const Courier& current = couriers_[states[i].courierIndex];
            const Courier& best = couriers_[states[bestStateIndex].courierIndex];
            // [验收亮点：智能调度算法]：负载最少优先；负载相同看收入公平；仍相同用用户名保证稳定结果。
            if (states[i].unfinishedCount < states[bestStateIndex].unfinishedCount ||
                (states[i].unfinishedCount == states[bestStateIndex].unfinishedCount &&
                 current.income() < best.income()) ||
                (states[i].unfinishedCount == states[bestStateIndex].unfinishedCount &&
                 current.income() == best.income() &&
                 current.username() < best.username())) {
                bestStateIndex = i;
            }
        }
        return states[bestStateIndex].courierIndex;
    }

    /** 分配成功后增量更新调度快照，避免批量分配时反复全量统计。 */
    void increaseCourierLoad(std::vector<CourierScheduleState>& states, std::size_t courierIndex) const {
        for (CourierScheduleState& state : states) {
            if (state.courierIndex == courierIndex) {
                ++state.unfinishedCount;
                return;
            }
        }
    }

public:
    /**
     * LogisticsSystem 是业务核心类。
     * 它负责注册、登录、充值、寄件、签收、查询、冻结和日志等业务规则，
     * App 只负责菜单流程，Repository 只负责文件读写，职责边界比较清晰。
     */

    /** 构造物流系统并绑定数据目录。 */
    explicit LogisticsSystem(const std::string& dataDir)
        : dataDir_(dataDir),
          userRepo_(DirectoryUtil::join(dataDir_, ProjectConfig::UserFileName)),
          adminRepo_(DirectoryUtil::join(dataDir_, ProjectConfig::AdminFileName)),
          courierRepo_(DirectoryUtil::join(dataDir_, ProjectConfig::CourierFileName)),
          expressRepo_(DirectoryUtil::join(dataDir_, ProjectConfig::ExpressFileName)),
          authStateRepo_(DirectoryUtil::join(dataDir_, ProjectConfig::AuthStateFileName)),
          logger_(DirectoryUtil::join(dataDir_, ProjectConfig::LogFileName)) {}

    /** 初始化目录并加载数据。 */
    void initialize() {
        DirectoryUtil::createDirectory(dataDir_);
        users_ = userRepo_.load();
        admin_ = adminRepo_.load();
        couriers_ = courierRepo_.load();
        expresses_ = expressRepo_.load();
        authStates_ = authStateRepo_.load();
        logDataLoadWarnings();
        if (!saveAll() || !saveAuthStates()) {
            logger_.write("SYSTEM", "system", "SAVE_DATA", "FAILED", "数据文件保存失败，请检查目录权限");
        }
        logger_.write("SYSTEM", "system", "INIT", "SUCCESS", "系统启动并加载数据");
    }

    /** 获取全部用户。 */
    const std::vector<User>& users() const {
        return users_;
    }

    /** 获取管理员。 */
    const Admin& admin() const {
        return admin_;
    }

    /** 获取全部快递员。 */
    const std::vector<Courier>& couriers() const {
        return couriers_;
    }

    /** 汇总管理员统计看板数据。 */
    SystemStatistics statistics() const {
        SystemStatistics stats;
        stats.platformNetIncome = admin_.balance();
        for (const Courier& courier : couriers_) {
            stats.courierPayout += courier.income();
        }
        for (const Express& express : expresses_) {
            if (express.status() == ExpressStatus::WaitingPickup) {
                ++stats.waitingPickupCount;
            } else if (express.status() == ExpressStatus::WaitingSign) {
                ++stats.waitingSignCount;
            } else {
                ++stats.signedCount;
            }

            if (express.itemType() == "Normal") {
                ++stats.normalCount;
                stats.normalFee += express.fee();
            } else if (express.itemType() == "Fragile") {
                ++stats.fragileCount;
                stats.fragileFee += express.fee();
            } else if (express.itemType() == "Book") {
                ++stats.bookCount;
                stats.bookFee += express.fee();
            }
        }
        return stats;
    }

    /** 计算全体快递员绩效，并按完成率、总任务数、收入、用户名排序。 */
    std::vector<CourierPerformance> courierPerformances() const {
        std::vector<CourierPerformance> performances;
        for (const Courier& courier : couriers_) {
            CourierPerformance performance;
            performance.username = courier.username();
            performance.name = courier.name();
            performance.income = courier.income();
            for (const Express& express : expresses_) {
                if (!express.belongsToCourier(courier.username())) {
                    continue;
                }
                ++performance.totalTaskCount;
                if (express.isWaitingPickup()) {
                    ++performance.waitingPickupCount;
                } else if (express.isWaitingSign()) {
                    ++performance.waitingSignCount;
                } else if (express.isSigned()) {
                    ++performance.signedCount;
                }
            }
            if (performance.totalTaskCount > 0) {
                performance.completionRate =
                    static_cast<double>(performance.signedCount) / performance.totalTaskCount;
            }
            performances.push_back(performance);
        }
        std::sort(performances.begin(), performances.end(),
                  [](const CourierPerformance& left, const CourierPerformance& right) {
                      if (left.completionRate != right.completionRate) {
                          return left.completionRate > right.completionRate;
                      }
                      if (left.totalTaskCount != right.totalTaskCount) {
                          return left.totalTaskCount > right.totalTaskCount;
                      }
                      if (left.income != right.income) {
                          return left.income > right.income;
                      }
                      return left.username < right.username;
                  });
        return performances;
    }

    /** 查询单个快递员绩效，供快递员个人看板复用全局统计口径。 */
    bool courierPerformanceFor(const std::string& username, CourierPerformance& performance) const {
        std::vector<CourierPerformance> performances = courierPerformances();
        for (const CourierPerformance& item : performances) {
            if (item.username == username) {
                performance = item;
                return true;
            }
        }
        return false;
    }

    /** 管理员校验操作日志哈希链完整性。 */
    bool verifyLogHashChain(std::string& message) const {
        bool ok = logger_.verifyHashChain(message);
        logger_.write("ADMIN", admin_.username(), "VERIFY_LOG_CHAIN", ok ? "SUCCESS" : "FAILED", message);
        return ok;
    }

    /** 根据用户名获取用户指针。 */
    const User* getUser(const std::string& username) const {
        std::size_t index = findUserIndex(username);
        if (index == users_.size()) {
            return nullptr;
        }
        return &users_[index];
    }

    /** 根据用户名获取快递员指针。 */
    const Courier* getCourier(const std::string& username) const {
        std::size_t index = findCourierIndex(username);
        if (index == couriers_.size()) {
            return nullptr;
        }
        return &couriers_[index];
    }

    /** 注册新用户。 */
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
        if (findUserIndex(username) != users_.size() ||
            findCourierIndex(username) != couriers_.size() ||
            username == admin_.username()) {
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
            users_.pop_back();
            saveAll();
            reportSaveFailure("GUEST", username, "REGISTER", message);
            return false;
        }
        message = "注册成功。";
        logger_.write("GUEST", username, "REGISTER", "SUCCESS", "新用户注册");
        return true;
    }

    /**
     * [输入]：用户名、明文密码和输出消息。
     * [输出]：登录是否成功。
     * [核心业务目标]：校验密码哈希并执行三次失败自动冻结。
     */
    bool loginUser(const std::string& username, const std::string& password, std::string& message) {
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
            int failedCount = recordAuthFailure("USER", username);
            if (failedCount < 0) {
                message = "认证状态保存失败，已拒绝本次登录。";
                logger_.write("USER", username, "LOGIN", "FAILED", message);
                return false;
            }
            if (failedCount >= 3) {
                // [验收亮点：安全防线]：连续三次密码错误触发自动冻结，防止暴力试探。
                users_[index].setFrozen(true);
                if (!saveAll()) {
                    logger_.write("USER", username, "LOGIN", "FAILED", "自动冻结保存失败");
                    message = "密码错误次数已达 3 次，但账号冻结保存失败，请联系管理员。";
                    return false;
                }
                message = "密码错误次数已达 3 次，账号已自动冻结。";
                logger_.write("USER", username, "AUTO_FREEZE", "SUCCESS", "连续登录失败 3 次");
                return false;
            }
            message = "密码错误，连续失败次数：" + std::to_string(failedCount) + "。";
            logger_.write("USER", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!resetAuthFailures("USER", username)) {
            message = "认证状态保存失败，已拒绝本次登录。";
            logger_.write("USER", username, "LOGIN", "FAILED", message);
            return false;
        }
        message = "登录成功。";
        logger_.write("USER", username, "LOGIN", "SUCCESS", message);
        return true;
    }

    /** 管理员登录校验。 */
    bool loginAdmin(const std::string& username, const std::string& password, std::string& message) {
        if (username != admin_.username()) {
            message = "管理员账号不存在。";
            logger_.write("ADMIN", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!PasswordHasher::verify(admin_.salt(), password, admin_.passwordHash())) {
            int failedCount = recordAuthFailure("ADMIN", username);
            if (failedCount < 0) {
                message = "认证状态保存失败，已拒绝本次登录。";
                logger_.write("ADMIN", username, "LOGIN", "FAILED", message);
                return false;
            }
            message = "管理员密码错误，连续失败次数：" + std::to_string(failedCount) + "。";
            logger_.write("ADMIN", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!resetAuthFailures("ADMIN", username)) {
            message = "认证状态保存失败，已拒绝本次登录。";
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
            users_[index].recharge(oldBalance - users_[index].balance());
            saveAll();
            reportSaveFailure("USER", username, "RECHARGE", message);
            return false;
        }
        message = "充值成功，当前余额：" + StringUtil::moneyToString(users_[index].balance()) + " 元。";
        logger_.write("USER", username, "RECHARGE", "SUCCESS", "充值 " + StringUtil::moneyToString(amount));
        return true;
    }

    /**
     * [输入]：发件人、收件人、物品描述、物品类型和计费数量。
     * [输出]：快递单号和业务消息。
     * [核心业务目标]：通过多态计费完成扣款、平台入账和待揽收快递创建。
     * [实现说明]：指向 item->getPrice()，说明这里不是 if-else 计费，而是运行期多态派发。
     */
    bool sendExpress(const std::string& sender, const std::string& receiver,
                     const std::string& description, const std::string& itemType, double itemAmount,
                     std::string& expressId, std::string& message) {
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

        std::unique_ptr<ExpressItem> item = ExpressItemFactory::createItem(itemType, itemAmount);
        if (!item) {
            message = "物品类型或计费数量无效。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        // [验收亮点：多态与继承]：通过基类指针调用 getPrice()，运行期派发到具体物品子类计费。
        double fee = item->getPrice();
        if (fee <= 0.0) {
            message = "快递费用计算异常，已取消发送。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }

        if (!users_[senderIndex].deduct(fee)) {
            message = "余额不足，当前余额：" + StringUtil::moneyToString(users_[senderIndex].balance()) +
                      " 元，本次需要：" + StringUtil::moneyToString(fee) + " 元。";
            logger_.write("USER", sender, "SEND_EXPRESS", "FAILED", message);
            return false;
        }
        admin_.addBalance(fee);
        expressId = makeExpressId();
        expresses_.push_back(Express(expressId, sender, receiver, StringUtil::now(), description, fee,
                                     item->typeCode(), itemAmount));
        if (!saveAll()) {
            expresses_.pop_back();
            admin_.addBalance(-fee);
            users_[senderIndex].recharge(fee);
            expressId.clear();
            saveAll();
            reportSaveFailure("USER", sender, "SEND_EXPRESS", message);
            return false;
        }
        message = "发送成功，快递单号：" + expressId + "，当前状态：待揽收。";
        logger_.write("USER", sender, "SEND_EXPRESS", "SUCCESS",
                      "单号 " + expressId + "，收件人 " + receiver + "，类型 " + item->typeName() +
                          "，数量 " + item->amountText() + "，费用 " + StringUtil::moneyToString(fee) +
                          "，状态 待揽收");
        return true;
    }

    /**
     * [输入]：快递员账号、明文密码和输出消息。
     * [输出]：登录是否成功。
     * [核心业务目标]：校验快递员状态、密码哈希和三次失败冻结策略。
     */
    bool loginCourier(const std::string& username, const std::string& password, std::string& message) {
        std::size_t index = findCourierIndex(username);
        if (index == couriers_.size()) {
            message = "快递员账号不存在。";
            logger_.write("COURIER", username, "LOGIN", "FAILED", message);
            return false;
        }
        const Courier& courier = couriers_[index];
        if (courier.removed()) {
            message = "快递员账号已停用，不能登录。";
            logger_.write("COURIER", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (courier.frozen()) {
            message = "快递员账号已被冻结，请联系管理员。";
            logger_.write("COURIER", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!courier.verifyPassword(password)) {
            int failedCount = recordAuthFailure("COURIER", username);
            if (failedCount < 0) {
                message = "认证状态保存失败，已拒绝本次登录。";
                logger_.write("COURIER", username, "LOGIN", "FAILED", message);
                return false;
            }
            if (failedCount >= 3) {
                // [验收亮点：安全防线]：快递员连续失败三次也会冻结，权限账号同样受保护。
                couriers_[index].setFrozen(true);
                if (!saveAll()) {
                    logger_.write("COURIER", username, "LOGIN", "FAILED", "自动冻结保存失败");
                    message = "快递员密码错误次数已达 3 次，但账号冻结保存失败，请联系管理员。";
                    return false;
                }
                message = "快递员密码错误次数已达 3 次，账号已自动冻结。";
                logger_.write("COURIER", username, "AUTO_FREEZE", "SUCCESS", "连续登录失败 3 次");
                return false;
            }
            message = "快递员密码错误，连续失败次数：" + std::to_string(failedCount) + "。";
            logger_.write("COURIER", username, "LOGIN", "FAILED", message);
            return false;
        }
        if (!resetAuthFailures("COURIER", username)) {
            message = "认证状态保存失败，已拒绝本次登录。";
            logger_.write("COURIER", username, "LOGIN", "FAILED", message);
            return false;
        }
        message = "快递员登录成功。";
        logger_.write("COURIER", username, "LOGIN", "SUCCESS", message);
        return true;
    }

    /**
     * [输入]：当前用户 username 和快递单号 expressId。
     * [输出]：签收是否成功和提示消息。
     * [核心业务目标]：执行收件人权限隔离与三状态签收拦截。
     * [实现说明]：先讲 receiver 权限，再讲 WaitingPickup/WaitingSign/Signed 三状态拦截。
     */
    bool signExpress(const std::string& username, const std::string& expressId, std::string& message) {
        std::size_t index = findExpressIndex(expressId);
        if (index == expresses_.size()) {
            message = "快递单号不存在。";
            logger_.write("USER", username, "SIGN_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        Express& express = expresses_[index];
        if (express.receiver() != username) {
            // [验收亮点：安全防线]：普通用户只能签收自己的收件，越权签收直接拦截并审计。
            message = "无权签收该快递。";
            logger_.write("USER", username, "SIGN_EXPRESS", "DENIED", "试图签收 " + expressId);
            return false;
        }
        if (express.isWaitingPickup()) {
            message = "该快递仍处于待揽收状态，暂不能签收。";
            logger_.write("USER", username, "SIGN_EXPRESS", "DENIED",
                          "试图签收待揽收快递 " + expressId);
            return false;
        }
        if (express.isSigned()) {
            message = "该快递已经签收，不能重复签收。";
            logger_.write("USER", username, "SIGN_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        if (!express.isWaitingSign()) {
            message = "快递状态异常，不能签收。";
            logger_.write("USER", username, "SIGN_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        Express oldExpress = express;
        express.sign();
        if (!saveAll()) {
            express = oldExpress;
            saveAll();
            reportSaveFailure("USER", username, "SIGN_EXPRESS", message);
            return false;
        }
        message = "签收成功：" + expressId;
        logger_.write("USER", username, "SIGN_EXPRESS", "SUCCESS", expressId);
        return true;
    }

    /** 获取用户未签收快递。 */
    std::vector<Express> waitingExpressesFor(const std::string& username) const {
        std::vector<Express> result;
        for (const Express& express : expresses_) {
            if (express.receiver() == username && express.isWaitingSign()) {
                result.push_back(express);
            }
        }
        return result;
    }

    /** 用户查询快递，业务层强制限制为本人相关快递。 */
    std::vector<Express> queryUserExpresses(const std::string& username,
                                            const ExpressQueryCondition& condition) const {
        std::vector<Express> result;
        bool denied = false;
        for (const Express& express : expresses_) {
            if (!condition.expressId.empty() && express.id() == condition.expressId &&
                !express.belongsToUser(username)) {
                denied = true;
            }
            if (!express.belongsToUser(username)) {
                continue;
            }
            if (matchExpress(express, condition)) {
                result.push_back(express);
            }
        }
        logger_.write("USER", username, "QUERY_EXPRESS", denied ? "DENIED" : "SUCCESS",
                      denied ? "存在越权查询尝试" : "查询快递记录");
        return result;
    }

    /** 快递员查询自己的任务，业务层强制限制 courier 为本人。 */
    std::vector<Express> queryCourierExpresses(const std::string& username,
                                               const ExpressQueryCondition& condition) const {
        std::vector<Express> result;
        bool denied = false;
        if (!condition.courier.empty() && condition.courier != username) {
            denied = true;
        }
        for (const Express& express : expresses_) {
            if (!condition.expressId.empty() && express.id() == condition.expressId &&
                !express.belongsToCourier(username)) {
                denied = true;
            }
            if (!express.belongsToCourier(username)) {
                continue;
            }
            ExpressQueryCondition scoped = condition;
            scoped.courier = username;
            if (matchExpress(express, scoped)) {
                result.push_back(express);
            }
        }
        logger_.write("COURIER", username, "QUERY_EXPRESS", denied ? "DENIED" : "SUCCESS",
                      denied ? "存在越权查询其他快递员任务尝试" : "查询本人任务");
        return result;
    }

    /** 管理员查询全部快递，可按快递员、物品类型等条件全局筛选。 */
    std::vector<Express> queryAllExpresses(const ExpressQueryCondition& condition) const {
        std::vector<Express> result;
        for (const Express& express : expresses_) {
            if (matchExpress(express, condition)) {
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
            users_[index].setFrozen(oldFrozen);
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "SET_USER_STATUS", message);
            return false;
        }
        if (!frozen && !resetAuthFailures("USER", username)) {
            logger_.write("ADMIN", admin_.username(), "SET_USER_STATUS", "FAILED",
                          "用户解冻后认证状态清零失败 " + username);
        }
        message = frozen ? "用户已冻结。" : "用户已解冻。";
        logger_.write("ADMIN", admin_.username(), "SET_USER_STATUS", "SUCCESS", username + " " + message);
        return true;
    }

    /** 管理员新增快递员。 */
    bool createCourier(const std::string& username, const std::string& name, const std::string& phone,
                       const std::string& password, std::string& message) {
        std::string reason;
        if (!InputValidator::isValidUsername(username, reason) ||
            !InputValidator::isValidName(name, reason) ||
            !InputValidator::isValidPhone(phone, reason) ||
            !InputValidator::isValidPassword(password, reason)) {
            message = reason;
            logger_.write("ADMIN", admin_.username(), "CREATE_COURIER", "FAILED", reason);
            return false;
        }
        if (username == admin_.username() ||
            findUserIndex(username) != users_.size() ||
            findCourierIndex(username) != couriers_.size()) {
            message = "用户名已存在。";
            logger_.write("ADMIN", admin_.username(), "CREATE_COURIER", "FAILED", message + " " + username);
            return false;
        }
        for (const Courier& courier : couriers_) {
            if (courier.phone() == phone && !courier.removed()) {
                message = "该手机号已被其他在职快递员使用。";
                logger_.write("ADMIN", admin_.username(), "CREATE_COURIER", "FAILED", message);
                return false;
            }
        }
        std::string salt = PasswordHasher::makeSalt(username);
        std::string hash = PasswordHasher::hashPassword(salt, password);
        couriers_.push_back(Courier(username, name, phone, salt, hash, 0.0, false, false));
        if (!saveAll()) {
            couriers_.pop_back();
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "CREATE_COURIER", message);
            return false;
        }
        message = "快递员新增成功。";
        logger_.write("ADMIN", admin_.username(), "CREATE_COURIER", "SUCCESS", username);
        return true;
    }

    /** 管理员逻辑删除快递员，保留历史记录。 */
    bool removeCourier(const std::string& username, std::string& message) {
        std::vector<Express> ignored;
        return removeCourier(username, message, ignored);
    }

    /**
     * [输入]：待停用快递员 username。
     * [输出]：操作消息和未完成任务清单。
     * [核心业务目标]：防止停用仍承担任务的快递员，保留可解释的冲突上下文。
     */
    bool removeCourier(const std::string& username, std::string& message, std::vector<Express>& unfinishedTasks) {
        unfinishedTasks.clear();
        std::size_t index = findCourierIndex(username);
        if (index == couriers_.size()) {
            message = "快递员不存在。";
            logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "FAILED", message + " " + username);
            return false;
        }
        if (couriers_[index].removed()) {
            message = "快递员已经停用。";
            logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "FAILED", message + " " + username);
            return false;
        }
        for (const Express& express : expresses_) {
            if (express.belongsToCourier(username) && !express.isSigned()) {
                // 未完成任务会阻塞停用，避免历史责任人突然失效造成业务断链。
                unfinishedTasks.push_back(express);
            }
        }
        if (!unfinishedTasks.empty()) {
            message = "该快递员仍有 " + std::to_string(unfinishedTasks.size()) +
                      " 个未完成任务，不能停用。请先完成、重分配或转移这些任务。";
            logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "FAILED", message + " " + username);
            return false;
        }
        Courier oldCourier = couriers_[index];
        couriers_[index].markRemoved();
        if (!saveAll()) {
            couriers_[index] = oldCourier;
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "REMOVE_COURIER", message);
            return false;
        }
        message = "快递员已停用。";
        logger_.write("ADMIN", admin_.username(), "REMOVE_COURIER", "SUCCESS", username);
        return true;
    }

    /** 管理员冻结或解冻快递员。 */
    bool setCourierFrozen(const std::string& username, bool frozen, std::string& message) {
        std::size_t index = findCourierIndex(username);
        if (index == couriers_.size()) {
            message = "快递员不存在。";
            logger_.write("ADMIN", admin_.username(), "SET_COURIER_STATUS", "FAILED", message + " " + username);
            return false;
        }
        if (couriers_[index].removed()) {
            message = "快递员已停用，不能修改冻结状态。";
            logger_.write("ADMIN", admin_.username(), "SET_COURIER_STATUS", "FAILED", message + " " + username);
            return false;
        }
        bool oldFrozen = couriers_[index].frozen();
        couriers_[index].setFrozen(frozen);
        if (!saveAll()) {
            couriers_[index].setFrozen(oldFrozen);
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "SET_COURIER_STATUS", message);
            return false;
        }
        if (!frozen && !resetAuthFailures("COURIER", username)) {
            logger_.write("ADMIN", admin_.username(), "SET_COURIER_STATUS", "FAILED",
                          "快递员解冻后认证状态清零失败 " + username);
        }
        message = frozen ? "快递员已冻结。" : "快递员已解冻。";
        logger_.write("ADMIN", admin_.username(), "SET_COURIER_STATUS", "SUCCESS", username + " " + message);
        return true;
    }

    /** 管理员为待揽收快递分配负责快递员。 */
    bool assignCourier(const std::string& expressId, const std::string& courierUsername, std::string& message) {
        std::size_t expressIndex = findExpressIndex(expressId);
        if (expressIndex == expresses_.size()) {
            message = "快递单号不存在。";
            logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "FAILED", message + " " + expressId);
            return false;
        }
        Express& express = expresses_[expressIndex];
        if (!express.isWaitingPickup()) {
            message = "只有待揽收快递可以分配快递员。";
            logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "FAILED", message + " " + expressId);
            return false;
        }

        std::size_t courierIndex = findCourierIndex(courierUsername);
        if (courierIndex == couriers_.size()) {
            message = "快递员不存在。";
            logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "FAILED", message + " " + courierUsername);
            return false;
        }
        const Courier& courier = couriers_[courierIndex];
        if (courier.removed()) {
            message = "快递员已停用，不能分配任务。";
            logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "FAILED", message + " " + courierUsername);
            return false;
        }
        if (courier.frozen()) {
            message = "快递员已冻结，不能分配任务。";
            logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "FAILED", message + " " + courierUsername);
            return false;
        }

        Express oldExpress = express;
        express.assignCourier(courierUsername);
        if (!saveAll()) {
            express = oldExpress;
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "ASSIGN_COURIER", message);
            return false;
        }
        message = "快递员分配成功。";
        logger_.write("ADMIN", admin_.username(), "ASSIGN_COURIER", "SUCCESS",
                      "单号 " + expressId + " 分配给 " + courierUsername);
        return true;
    }

    /**
     * [输入]：待揽收快递单号。
     * [输出]：被分配快递员和业务消息。
     * [核心业务目标]：按可用性、负载、收入和用户名稳定规则完成自动派单。
     * [实现说明]：按过滤冻结停用、任务数最少、收入最低、用户名兜底四层顺序解释调度。
     */
    bool autoAssignCourier(const std::string& expressId, std::string& assignedCourier, std::string& message) {
        assignedCourier.clear();
        std::size_t expressIndex = findExpressIndex(expressId);
        if (expressIndex == expresses_.size()) {
            message = "快递单号不存在。";
            logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "FAILED", message + " " + expressId);
            return false;
        }
        Express& express = expresses_[expressIndex];
        if (!express.isWaitingPickup()) {
            message = "只有待揽收快递可以自动分配快递员。";
            logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "FAILED", message + " " + expressId);
            return false;
        }

        // [验收亮点：智能调度算法]：此处基于可用性、负载均衡、收入公平性和字典序进行多维度调度。
        std::vector<CourierScheduleState> states = buildCourierScheduleStates();
        std::size_t courierIndex = selectBestCourierIndex(states);
        if (courierIndex == couriers_.size()) {
            message = "没有可用快递员：所有快递员均不存在、被冻结或已停用。";
            logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "FAILED", message + " " + expressId);
            return false;
        }

        Express oldExpress = express;
        assignedCourier = couriers_[courierIndex].username();
        express.assignCourier(assignedCourier);
        if (!saveAll()) {
            express = oldExpress;
            assignedCourier.clear();
            saveAll();
            reportSaveFailure("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", message);
            return false;
        }

        message = "自动分配成功：快递 " + expressId + " -> 快递员 " + assignedCourier + "。";
        logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "SUCCESS",
                      "单号 " + expressId + " 自动分配给 " + assignedCourier);
        return true;
    }

    /**
     * [输入]：输出消息列表。
     * [输出]：每一单分配结果。
     * [核心业务目标]：批量自动派单，允许部分成功并逐条记录审计日志。
     */
    void autoAssignAllWaitingPickup(std::vector<std::string>& messages) {
        messages.clear();
        // 批量场景只构建一次调度快照，后续用 increaseCourierLoad 增量维护负载。
        std::vector<CourierScheduleState> states = buildCourierScheduleStates();
        if (states.empty()) {
            std::string message = "没有可用快递员：所有快递员均不存在、被冻结或已停用。";
            messages.push_back(message);
            logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_ALL", "FAILED", message);
            return;
        }

        int successCount = 0;
        int skippedCount = 0;
        for (Express& express : expresses_) {
            if (!express.isWaitingPickup() || !express.courier().empty()) {
                continue;
            }
            std::size_t courierIndex = selectBestCourierIndex(states);
            if (courierIndex == couriers_.size()) {
                std::string message = "快递 " + express.id() + " 自动分配失败：没有可用快递员。";
                messages.push_back(message);
                logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "FAILED", message);
                ++skippedCount;
                continue;
            }

            std::string assignedCourier = couriers_[courierIndex].username();
            Express oldExpress = express;
            express.assignCourier(assignedCourier);
            if (!saveAll()) {
                express = oldExpress;
                saveAll();
                std::string message = "快递 " + express.id() + " 自动分配失败：数据保存失败，已回滚。";
                messages.push_back(message);
                logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "FAILED", message);
                ++skippedCount;
                continue;
            }

            increaseCourierLoad(states, courierIndex);
            std::string message = "快递 " + express.id() + " 自动分配给 " + assignedCourier + "。";
            messages.push_back(message);
            logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_COURIER", "SUCCESS", message);
            ++successCount;
        }

        if (messages.empty()) {
            messages.push_back("没有需要自动分配的待揽收快递。");
        }
        logger_.write("ADMIN", admin_.username(), "AUTO_ASSIGN_ALL", "SUCCESS",
                      "成功 " + std::to_string(successCount) + " 单，失败/跳过 " + std::to_string(skippedCount) + " 单");
    }

    /**
     * [输入]：快递单号和当前快递员账号。
     * [输出]：揽收是否成功和提示消息。
     * [核心业务目标]：推进待揽收到待签收，并模拟平台到快递员的 50% 提成分账。
     * [实现说明]：重点展示 oldExpress/oldAdmin/oldCourier 快照，说明保存失败时整体回滚。
     */
    bool pickupExpress(const std::string& expressId, const std::string& courierUsername, std::string& message) {
        std::size_t expressIndex = findExpressIndex(expressId);
        if (expressIndex == expresses_.size()) {
            message = "快递单号不存在。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }
        std::size_t courierIndex = findCourierIndex(courierUsername);
        if (courierIndex == couriers_.size()) {
            message = "快递员账号不存在。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message);
            return false;
        }
        if (couriers_[courierIndex].removed()) {
            message = "快递员账号已停用，不能揽收。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message);
            return false;
        }
        if (couriers_[courierIndex].frozen()) {
            message = "快递员账号已冻结，不能揽收。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message);
            return false;
        }

        Express& express = expresses_[expressIndex];
        if (!express.belongsToCourier(courierUsername)) {
            // 快递员只能揽收自己名下任务，防止跨账号抢单。
            message = "无权揽收该快递。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "DENIED",
                          "试图揽收非本人任务 " + expressId);
            return false;
        }
        if (!express.isWaitingPickup()) {
            message = express.isWaitingSign() ? "该快递已揽收，不能重复揽收。" : "该快递状态不允许揽收。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }

        double commission = express.fee() * 0.5;
        if (admin_.balance() + 0.0001 < commission) {
            message = "平台账户余额不足，无法支付揽收提成。";
            logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "FAILED", message + " " + expressId);
            return false;
        }

        Express oldExpress = express;
        Admin oldAdmin = admin_;
        Courier oldCourier = couriers_[courierIndex];
        // [验收亮点：资金事务一致性]：管理员扣 50% 与快递员加 50% 同步修改，保存失败时整体回滚。
        admin_.addBalance(-commission);
        couriers_[courierIndex].addIncome(commission);
        express.pickup();
        if (!saveAll()) {
            express = oldExpress;
            admin_ = oldAdmin;
            couriers_[courierIndex] = oldCourier;
            saveAll();
            reportSaveFailure("COURIER", courierUsername, "PICKUP_EXPRESS", message);
            return false;
        }

        message = "揽收成功：" + expressId + "，提成 " + StringUtil::moneyToString(commission) + " 元。";
        logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "SUCCESS",
                      "单号 " + expressId + "，提成 " + StringUtil::moneyToString(commission) +
                          "，状态 待签收");
        return true;
    }

    /** 查询快递员名下仍待揽收的任务。 */
    std::vector<Express> waitingPickupExpressesForCourier(const std::string& courierUsername) const {
        std::vector<Express> result;
        for (const Express& express : expresses_) {
            if (express.belongsToCourier(courierUsername) && express.isWaitingPickup()) {
                result.push_back(express);
            }
        }
        return result;
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

// [代码定位]：这里可以强调 UI 层只负责交互，不直接改余额、状态或文件，体现职责分离。
// [代码块说明] App 是菜单流程层，负责把用户输入整理成业务参数，再调用 LogisticsSystem。
// [设计说明] 发送快递流程里，App 收集收件人和物品信息，真正扣款和建单在 sendExpress 中完成。
// [流程控制层]：负责菜单导航和输入收集，所有业务规则下沉到 LogisticsSystem。
class App {
private:
    LogisticsSystem system_;  // 物流系统核心对象。

    /**
     * App 是程序流程控制层。
     * 这里所有 readXXX 函数都先做输入合法性检查，再把合法数据交给 LogisticsSystem。
     * 验收时可以说明：非法输入不会进入业务层，因此能减少异常状态和重复判断。
     */

    /** 判断输入流是否已经结束。 */
    bool isInputEnded(const std::string& value) const {
        return ConsoleUI::isEofValue(value);
    }

    /**
     * 读取菜单选项的统一入口。
     * 所有菜单都通过这里调用 ConsoleUI::readInt，保证越界、字母、空串等情况有一致提示。
     */
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

    /** 读取地址并即时校验。 */
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
            std::string password = ConsoleUI::readPasswordHidden(prompt);
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

    /** 读取两次密码并确保一致。 */
    std::string readConfirmedPassword(const std::string& prompt, const std::string& confirmPrompt) const {
        while (true) {
            std::string password = readPassword(prompt);
            if (password.empty()) {
                return "";
            }
            std::string confirm = ConsoleUI::readPasswordHidden(confirmPrompt);
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

    /** 读取可选用户名查询条件。 */
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

    /** 读取可选快递单号查询条件。 */
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

    /** 读取日志结果筛选条件。 */
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

    /** 打印主菜单。 */
    void showMainMenu() const {
        ConsoleUI::title(ProjectConfig::DisplayTitle);
        ConsoleUI::printPasswordInputMode();
        std::cout << "1. 用户注册\n";
        std::cout << "2. 用户登录\n";
        std::cout << "3. 快递员登录\n";
        std::cout << "4. 管理员登录\n";
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
            std::string password = ConsoleUI::readPasswordHidden("密码: ");
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
            std::string password = ConsoleUI::readPasswordHidden("管理员密码: ");
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

    /** 执行快递员登录流程。 */
    void courierLoginFlow() {
        ConsoleUI::title("快递员登录");
        for (int attempt = 1; attempt <= 3; ++attempt) {
            std::string username = readUsername("快递员账号: ");
            std::string password = ConsoleUI::readPasswordHidden("快递员密码: ");
            if (username.empty()) {
                ConsoleUI::message("快递员账号输入不完整，已返回上级菜单。");
                ConsoleUI::pause();
                return;
            }
            if (password.empty() || isInputEnded(password)) {
                ConsoleUI::message("快递员密码不能为空。");
                --attempt;
                continue;
            }
            std::string message;
            if (system_.loginCourier(username, password, message)) {
                ConsoleUI::message(message);
                courierMenu(username);
                return;
            }
            ConsoleUI::message(message + " 剩余尝试次数：" + std::to_string(3 - attempt));
        }
        ConsoleUI::pause();
    }

    /** 打印用户菜单顶部状态。 */
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

    /** 修改密码流程。 */
    void changePasswordFlow(const std::string& username) {
        ConsoleUI::title("修改账户密码");
        std::string oldPassword = ConsoleUI::readPasswordHidden("旧密码: ");
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

    /** 展示余额流程。 */
    void showBalanceFlow(const std::string& username) const {
        ConsoleUI::title("账户余额");
        const User* user = system_.getUser(username);
        if (user != nullptr) {
            std::cout << "当前余额：" << StringUtil::moneyToString(user->balance()) << " 元\n";
        }
        ConsoleUI::pause();
    }

    /** 账户充值流程。 */
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

    /** 快递员登录后的功能菜单。 */
    void courierMenu(const std::string& username) {
        while (true) {
            ConsoleUI::title("快递员功能菜单");
            const Courier* courier = system_.getCourier(username);
            if (courier != nullptr) {
                std::cout << "当前快递员：" << courier->name() << "(" << courier->username() << ")"
                          << " | 收入：" << StringUtil::moneyToString(courier->income()) << " 元\n";
                std::cout << "------------------------------------------------------------\n";
            }
            std::cout << "1. 查看待揽收任务\n";
            std::cout << "2. 揽收快递\n";
            std::cout << "3. 查询我的任务\n";
            std::cout << "4. 查看个人绩效\n";
            std::cout << "0. 退出登录\n";
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 4, choice)) {
                return;
            }
            if (choice == 0) {
                return;
            }
            if (choice == 1) {
                ConsoleUI::title("待揽收任务");
                ConsoleUI::printExpressTable(system_.waitingPickupExpressesForCourier(username), true);
                ConsoleUI::pause();
            } else if (choice == 2) {
                pickupExpressFlow(username);
            } else if (choice == 3) {
                courierQueryExpressFlow(username);
            } else if (choice == 4) {
                courierPerformanceFlow(username);
            }
        }
    }

    /** 快递员揽收任务流程，支持单个、批量和全部揽收。 */
    void pickupExpressFlow(const std::string& username) {
        while (true) {
            ConsoleUI::title("揽收快递");
            std::vector<Express> waiting = system_.waitingPickupExpressesForCourier(username);
            ConsoleUI::printExpressTable(waiting, true);
            if (waiting.empty()) {
                ConsoleUI::pause();
                return;
            }
            std::cout << "\n1. 揽收单个\n";
            std::cout << "2. 批量揽收\n";
            std::cout << "3. 全部揽收\n";
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
                system_.pickupExpress(waiting[static_cast<std::size_t>(index - 1)].id(), username, message);
                ConsoleUI::message(message);
                ConsoleUI::pause();
            } else if (choice == 2) {
                std::string text = ConsoleUI::readLine("请输入序号，使用空格或逗号分隔: ");
                pickupByIndexText(username, waiting, text);
                ConsoleUI::pause();
            } else if (choice == 3) {
                if (ConsoleUI::confirm("确认揽收全部待揽收任务")) {
                    for (const Express& express : waiting) {
                        std::string message;
                        system_.pickupExpress(express.id(), username, message);
                        std::cout << message << '\n';
                    }
                }
                ConsoleUI::pause();
            }
        }
    }

    /** 根据编号文本批量揽收。 */
    void pickupByIndexText(const std::string& username, const std::vector<Express>& waiting,
                           const std::string& text) {
        std::string normalized = text;
        std::replace(normalized.begin(), normalized.end(), ',', ' ');
        std::istringstream in(normalized);
        std::vector<int> pickedIndexes;
        std::string token;
        bool pickedAny = false;
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
            if (std::find(pickedIndexes.begin(), pickedIndexes.end(), index) != pickedIndexes.end()) {
                std::cout << "序号 " << index << " 重复，已跳过。\n";
                continue;
            }
            std::string message;
            system_.pickupExpress(waiting[static_cast<std::size_t>(index - 1)].id(), username, message);
            std::cout << message << '\n';
            pickedIndexes.push_back(index);
            pickedAny = true;
        }
        if (!pickedAny) {
            std::cout << "没有有效序号。\n";
        }
    }

    /** 选择快递物品类型，返回持久化类型代码。 */
    std::string chooseItemType() const {
        ConsoleUI::title("快递类型选择");
        std::cout << "1. 普通快递（5 元/kg）\n";
        std::cout << "2. 易碎品（8 元/kg）\n";
        std::cout << "3. 图书（2 元/本）\n";
        int choice = 0;
        if (!readMenuChoice("请选择快递类型: ", 1, 3, choice)) {
            return "";
        }
        if (choice == 1) {
            return "Normal";
        }
        if (choice == 2) {
            return "Fragile";
        }
        return "Book";
    }

    /** 读取重量或册数，拒绝 0、负数、字母和过多小数位。 */
    bool readItemAmount(const std::string& itemType, double& amount) const {
        while (true) {
            if (itemType == "Book") {
                std::string text = ConsoleUI::readLine("请输入图书册数(正整数): ");
                if (isInputEnded(text)) {
                    return false;
                }
                if (!StringUtil::isDigits(text)) {
                    std::cout << "册数必须是正整数，不能包含小数、负号或字母。\n";
                    continue;
                }
                std::istringstream input(text);
                int count = 0;
                input >> count;
                if (!input || count <= 0) {
                    std::cout << "册数必须大于 0。\n";
                    continue;
                }
                amount = static_cast<double>(count);
                return true;
            }

            std::string prompt = itemType == "Fragile" ? "请输入易碎品重量kg(正数，最多两位小数): "
                                                       : "请输入普通快递重量kg(正数，最多两位小数): ";
            std::string text = ConsoleUI::readLine(prompt);
            if (isInputEnded(text)) {
                return false;
            }
            double weight = 0.0;
            if (!StringUtil::tryParsePositiveMoney(text, weight)) {
                std::cout << "重量必须是大于 0 的数字，最多保留两位小数，不能包含字母或负号。\n";
                continue;
            }
            amount = weight;
            return true;
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
        if (description.empty()) {
            ConsoleUI::message("物品描述输入无效，已取消发送。");
            ConsoleUI::pause();
            return;
        }
        std::string itemType = chooseItemType();
        if (itemType.empty()) {
            ConsoleUI::message("快递类型输入无效，已取消发送。");
            ConsoleUI::pause();
            return;
        }
        double itemAmount = 0.0;
        if (!readItemAmount(itemType, itemAmount)) {
            ConsoleUI::message("计费数量输入无效，已取消发送。");
            ConsoleUI::pause();
            return;
        }
        std::unique_ptr<ExpressItem> item = ExpressItemFactory::createItem(itemType, itemAmount);
        if (!item) {
            ConsoleUI::message("快递类型或计费数量无效，已取消发送。");
            ConsoleUI::pause();
            return;
        }
        double fee = item->getPrice();
        const User* user = system_.getUser(username);
        if (user != nullptr && user->balance() < fee) {
            std::cout << "当前余额：" << StringUtil::moneyToString(user->balance())
                      << " 元，本次需要 " << StringUtil::moneyToString(fee) << " 元。\n";
            if (ConsoleUI::confirm("余额不足，是否立即充值")) {
                rechargeFlow(username);
            }
            return;
        }
        std::cout << "快递类型：" << item->typeName()
                  << " | 计费数量：" << item->amountText()
                  << " | 本次费用：" << StringUtil::moneyToString(fee) << " 元\n";
        if (!ConsoleUI::confirm("确认发送")) {
            ConsoleUI::message("已取消发送。");
            ConsoleUI::pause();
            return;
        }
        std::string expressId;
        std::string message;
        system_.sendExpress(username, receiver, description, itemType, itemAmount, expressId, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 接收快递流程，支持编号选择、批量签收和全部签收。 */
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

    /** 根据编号文本批量签收。 */
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

    /** 读取快递查询条件对象。 */
    ExpressQueryCondition readExpressCriteria(bool allowCourierFilter, bool allowItemTypeFilter) const {
        ExpressQueryCondition condition;
        while (true) {
            condition.sender = readOptionalUsername("发件人用户名(可留空): ", "发件人用户名");
            condition.receiver = readOptionalUsername("收件人用户名(可留空): ", "收件人用户名");
            if (allowCourierFilter) {
                condition.courier = readOptionalUsername("快递员用户名(可留空): ", "快递员用户名");
            }
            condition.expressId = readOptionalExpressId();
            condition.startTime = ConsoleUI::readLine("开始时间 YYYY-MM-DD HH:MM:SS(可留空): ");
            if (isInputEnded(condition.startTime)) {
                condition.startTime.clear();
                return condition;
            }
            condition.endTime = ConsoleUI::readLine("结束时间 YYYY-MM-DD HH:MM:SS(可留空): ");
            if (isInputEnded(condition.endTime)) {
                condition.endTime.clear();
                return condition;
            }
            std::string reason;
            if (!InputValidator::isValidDateTimeOrEmpty(condition.startTime, "开始时间", reason) ||
                !InputValidator::isValidDateTimeOrEmpty(condition.endTime, "结束时间", reason)) {
                std::cout << reason << '\n';
                continue;
            }
            if (!condition.startTime.empty() && !condition.endTime.empty() &&
                condition.startTime > condition.endTime) {
                std::cout << "开始时间不能晚于结束时间，请重新输入。\n";
                continue;
            }
            std::cout << "状态筛选：0. 全部  1. 待揽收  2. 待签收  3. 已签收\n";
            int statusChoice = 0;
            if (!readMenuChoice("请选择: ", 0, 3, statusChoice)) {
                condition.statusFilter = -1;
                return condition;
            }
            if (statusChoice == 0) {
                condition.statusFilter = -1;
            } else if (statusChoice == 1) {
                condition.statusFilter = static_cast<int>(ExpressStatus::WaitingPickup);
            } else if (statusChoice == 2) {
                condition.statusFilter = static_cast<int>(ExpressStatus::WaitingSign);
            } else {
                condition.statusFilter = static_cast<int>(ExpressStatus::Signed);
            }
            if (allowItemTypeFilter) {
                std::cout << "物品类型筛选：0. 全部  1. 普通快递  2. 易碎品  3. 图书\n";
                int itemChoice = 0;
                if (!readMenuChoice("请选择: ", 0, 3, itemChoice)) {
                    condition.itemType.clear();
                    return condition;
                }
                if (itemChoice == 1) {
                    condition.itemType = "Normal";
                } else if (itemChoice == 2) {
                    condition.itemType = "Fragile";
                } else if (itemChoice == 3) {
                    condition.itemType = "Book";
                }
            }
            return condition;
        }
    }

    /** 用户查询快递流程。 */
    void userQueryExpressFlow(const std::string& username) const {
        ConsoleUI::title("查询快递");
        ExpressQueryCondition condition = readExpressCriteria(false, true);
        std::vector<Express> result = system_.queryUserExpresses(username, condition);
        ConsoleUI::printExpressTable(result, false);
        ConsoleUI::pause();
    }

    /** 快递员查询本人任务流程。 */
    void courierQueryExpressFlow(const std::string& username) const {
        ConsoleUI::title("查询我的任务");
        ExpressQueryCondition condition = readExpressCriteria(false, true);
        std::vector<Express> result = system_.queryCourierExpresses(username, condition);
        ConsoleUI::printExpressTable(result, false);
        ConsoleUI::pause();
    }

    /** 快递员个人绩效看板。 */
    void courierPerformanceFlow(const std::string& username) const {
        ConsoleUI::title("个人绩效看板");
        CourierPerformance performance;
        if (!system_.courierPerformanceFor(username, performance)) {
            ConsoleUI::message("未找到当前快递员绩效数据。");
            ConsoleUI::pause();
            return;
        }
        ConsoleUI::printCourierPerformanceTable(std::vector<CourierPerformance>{performance});
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
            std::cout << "5. 查看快递员列表\n";
            std::cout << "6. 新增快递员\n";
            std::cout << "7. 删除/停用快递员\n";
            std::cout << "8. 冻结/解冻快递员\n";
            std::cout << "9. 分配快递员\n";
            std::cout << "10. 单个自动分配\n";
            std::cout << "11. 一键分配所有待揽收快递\n";
            std::cout << "12. 查看任务统计\n";
            std::cout << "13. 校验日志完整性\n";
            std::cout << "0. 退出登录\n";
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 13, choice)) {
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
            } else if (choice == 5) {
                ConsoleUI::title("快递员列表");
                ConsoleUI::printCourierTable(system_.couriers());
                ConsoleUI::pause();
            } else if (choice == 6) {
                createCourierFlow();
            } else if (choice == 7) {
                removeCourierFlow();
            } else if (choice == 8) {
                setCourierStatusFlow();
            } else if (choice == 9) {
                assignCourierFlow();
            } else if (choice == 10) {
                autoAssignCourierFlow();
            } else if (choice == 11) {
                autoAssignAllWaitingPickupFlow();
            } else if (choice == 12) {
                statisticsDashboardFlow();
            } else if (choice == 13) {
                verifyLogChainFlow();
            }
        }
    }

    /** 管理员查询快递流程。 */
    void adminQueryExpressFlow() const {
        ConsoleUI::title("管理员查询快递");
        ExpressQueryCondition condition = readExpressCriteria(true, true);
        std::vector<Express> result = system_.queryAllExpresses(condition);
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

    /** 管理员新增快递员流程。 */
    void createCourierFlow() {
        ConsoleUI::title("新增快递员");
        std::string username = readUsername("快递员用户名: ");
        std::string name = readName();
        std::string phone = readPhone();
        std::string password = readConfirmedPassword("初始密码(6-30位，必须含字母和数字): ", "确认初始密码: ");
        if (username.empty() || name.empty() || phone.empty() || password.empty()) {
            ConsoleUI::message("输入不完整，已取消新增。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.createCourier(username, name, phone, password, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 管理员停用快递员流程。 */
    void removeCourierFlow() {
        ConsoleUI::title("删除/停用快递员");
        std::string username = readUsername("快递员用户名: ");
        if (username.empty()) {
            ConsoleUI::message("快递员用户名输入不完整，已取消操作。");
            ConsoleUI::pause();
            return;
        }
        const Courier* courier = system_.getCourier(username);
        if (courier == nullptr) {
            ConsoleUI::message("快递员不存在。");
            ConsoleUI::pause();
            return;
        }
        if (!ConsoleUI::confirm("确认停用该快递员账号")) {
            ConsoleUI::message("已取消操作。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        std::vector<Express> unfinishedTasks;
        system_.removeCourier(username, message, unfinishedTasks);
        ConsoleUI::message(message);
        if (!unfinishedTasks.empty()) {
            std::cout << "未完成任务清单如下，可先完成、重新分配或转移后再停用：\n";
            ConsoleUI::printExpressTable(unfinishedTasks, false);
        }
        ConsoleUI::pause();
    }

    /** 管理员冻结或解冻快递员流程。 */
    void setCourierStatusFlow() {
        ConsoleUI::title("冻结/解冻快递员");
        std::string username = readUsername("快递员用户名: ");
        if (username.empty()) {
            ConsoleUI::message("快递员用户名输入不完整，已取消操作。");
            ConsoleUI::pause();
            return;
        }
        const Courier* courier = system_.getCourier(username);
        if (courier == nullptr) {
            ConsoleUI::message("快递员不存在。");
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
        system_.setCourierFrozen(username, choice == 1, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 管理员为待揽收快递分配快递员流程。 */
    void assignCourierFlow() {
        ConsoleUI::title("分配快递员");
        std::cout << "可先在查询全部快递中按状态筛选待揽收快递，再复制单号。\n";
        std::string expressId = readOptionalExpressId();
        if (expressId.empty()) {
            ConsoleUI::message("快递单号输入不完整，已取消分配。");
            ConsoleUI::pause();
            return;
        }
        std::string courierUsername = readUsername("快递员用户名: ");
        if (courierUsername.empty()) {
            ConsoleUI::message("快递员用户名输入不完整，已取消分配。");
            ConsoleUI::pause();
            return;
        }
        std::string message;
        system_.assignCourier(expressId, courierUsername, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 管理员对单个快递执行智能自动分配。 */
    void autoAssignCourierFlow() {
        ConsoleUI::title("单个自动分配");
        std::cout << "系统将按未完成任务数、累计收入、用户名字典序自动选择快递员。\n";
        std::string expressId = readOptionalExpressId();
        if (expressId.empty()) {
            ConsoleUI::message("快递单号输入不完整，已取消自动分配。");
            ConsoleUI::pause();
            return;
        }
        std::string assignedCourier;
        std::string message;
        system_.autoAssignCourier(expressId, assignedCourier, message);
        ConsoleUI::message(message);
        ConsoleUI::pause();
    }

    /** 管理员一键自动分配所有尚未指定快递员的待揽收快递。 */
    void autoAssignAllWaitingPickupFlow() {
        ConsoleUI::title("一键分配所有待揽收快递");
        if (!ConsoleUI::confirm("确认自动分配所有未指定快递员的待揽收快递")) {
            ConsoleUI::message("已取消自动分配。");
            ConsoleUI::pause();
            return;
        }
        std::vector<std::string> messages;
        system_.autoAssignAllWaitingPickup(messages);
        for (const std::string& message : messages) {
            std::cout << message << '\n';
        }
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

    /** 管理员统计看板。 */
    void statisticsDashboardFlow() const {
        ConsoleUI::title("任务统计看板");
        SystemStatistics stats = system_.statistics();
        double totalItemFee = stats.normalFee + stats.fragileFee + stats.bookFee;
        std::cout << "平台净收入：" << StringUtil::moneyToString(stats.platformNetIncome) << " 元\n";
        std::cout << "快递员支出：" << StringUtil::moneyToString(stats.courierPayout) << " 元\n\n";

        TablePrinter::printTable(
            std::vector<std::string>{"状态", "数量"},
            std::vector<std::vector<std::string> >{
                std::vector<std::string>{"待揽收", std::to_string(stats.waitingPickupCount)},
                std::vector<std::string>{"待签收", std::to_string(stats.waitingSignCount)},
                std::vector<std::string>{"已签收", std::to_string(stats.signedCount)}},
            std::vector<int>{12, 10});

        TablePrinter::printTable(
            std::vector<std::string>{"物品类型", "订单数", "费用合计", "收入占比"},
            std::vector<std::vector<std::string> >{
                std::vector<std::string>{"普通快递", std::to_string(stats.normalCount),
                                         StringUtil::moneyToString(stats.normalFee),
                                         StringUtil::moneyToString(totalItemFee > 0.0 ? stats.normalFee * 100.0 / totalItemFee : 0.0) + "%"},
                std::vector<std::string>{"易碎品", std::to_string(stats.fragileCount),
                                         StringUtil::moneyToString(stats.fragileFee),
                                         StringUtil::moneyToString(totalItemFee > 0.0 ? stats.fragileFee * 100.0 / totalItemFee : 0.0) + "%"},
                std::vector<std::string>{"图书", std::to_string(stats.bookCount),
                                         StringUtil::moneyToString(stats.bookFee),
                                         StringUtil::moneyToString(totalItemFee > 0.0 ? stats.bookFee * 100.0 / totalItemFee : 0.0) + "%"}},
            std::vector<int>{12, 10, 12, 10});

        std::cout << "\n全体快递员绩效排行：\n";
        ConsoleUI::printCourierPerformanceTable(system_.courierPerformances());
        ConsoleUI::pause();
    }

    /** 管理员校验日志哈希链。 */
    void verifyLogChainFlow() const {
        ConsoleUI::title("校验日志完整性");
        std::string message;
        bool ok = system_.verifyLogHashChain(message);
        ConsoleUI::message(ok ? "校验通过：" + message : "校验失败：" + message);
        ConsoleUI::pause();
    }

public:
    /** 构造应用对象。 */
    App() : system_(DirectoryUtil::projectDataDirectory()) {}

    /** 运行主循环。 */
    void run() {
        system_.initialize();
        while (true) {
            showMainMenu();
            int choice = 0;
            if (!readMenuChoice("请选择: ", 0, 4, choice)) {
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
                courierLoginFlow();
            } else if (choice == 4) {
                adminLoginFlow();
            }
        }
    }
};

// [代码块说明] main 函数是最外层入口，只初始化控制台、设置密码输入模式并启动 App。
// [设计说明] 入口层保持极简，能说明业务逻辑没有散落在 main 中。
// [程序入口]：只做环境初始化、密码模式开关设置和 App 启动，保持入口层零业务逻辑。
int main() {
    ConsoleEnvironment::initialize();
    /*
     * mima隐藏开关：
     * true  = 密码输入显示为星号，适合正式安全演示；
     * false = 密码输入明文回显，适合向助教展示输入内容与校验流程。
     * 该开关由 main() 统一设置，所有登录、注册、改密和新增快递员密码输入都会同步生效。
     */
    bool hidePasswordInput = true;
    ConsoleUI::setPasswordHiddenEnabled(hidePasswordInput);
    App app;
    app.run();
    return 0;
}
