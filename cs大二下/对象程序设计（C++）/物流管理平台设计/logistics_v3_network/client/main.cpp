#include "SocketClient.h"

#include "../common/security/InputValidator.h"
#include "../common/security/StringUtil.h"

#include <atomic>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#endif

class TablePrinter {
public:
    static void printRecords(const std::vector<std::string>& records) {
        if (records.empty()) {
            std::cout << "[Table] 无记录。" << '\n';
            return;
        }
        std::size_t recordWidth = 20;
        // 控制台按显示宽度而非 UTF-8 字节数对齐，宽度上限避免超长记录撑破窗口。
        for (const std::string& record : records) {
            const std::size_t width = displayWidth(record);
            if (width > recordWidth) {
                recordWidth = width;
            }
        }
        if (recordWidth > 110U) {
            recordWidth = 110U;
        }
        const std::string border = "+------+-" + std::string(recordWidth, '-') + "-+";
        std::cout << border << '\n';
        std::cout << "| No.  | " << padRight("Record", recordWidth) << " |" << '\n';
        std::cout << border << '\n';
        for (std::size_t i = 0; i < records.size(); ++i) {
            std::cout << "| " << std::setw(4) << (i + 1U) << " | " << padRight(trimForCell(records[i], recordWidth), recordWidth) << " |" << '\n';
        }
        std::cout << border << '\n';
    }

private:
    static std::size_t displayWidth(const std::string& value) {
        std::size_t width = 0;
        // ASCII 按一列、非 ASCII UTF-8 码点按两列估算，适配当前中文控制台表格。
        for (std::size_t i = 0; i < value.size(); ++i) {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch < 0x80U) {
                ++width;
            } else {
                width += 2U;
                while (i + 1U < value.size() &&
                       (static_cast<unsigned char>(value[i + 1U]) & 0xC0U) == 0x80U) {
                    ++i;
                }
            }
        }
        return width;
    }

    static std::string trimForCell(const std::string& value, std::size_t maxWidth) {
        if (displayWidth(value) <= maxWidth) {
            return value;
        }
        std::string result;
        std::size_t width = 0;
        for (std::size_t i = 0; i < value.size() && width + 3U < maxWidth; ++i) {
            const unsigned char ch = static_cast<unsigned char>(value[i]);
            if (ch < 0x80U) {
                result.push_back(value[i]);
                ++width;
            } else {
                std::size_t end = i;
                while (end + 1U < value.size() &&
                       (static_cast<unsigned char>(value[end + 1U]) & 0xC0U) == 0x80U) {
                    ++end;
                }
                if (width + 2U + 3U >= maxWidth) {
                    break;
                }
                result.append(value.substr(i, end - i + 1U));
                width += 2U;
                i = end;
            }
        }
        return result + "...";
    }

    static std::string padRight(const std::string& value, std::size_t targetWidth) {
        const std::size_t width = displayWidth(value);
        if (width >= targetWidth) {
            return value;
        }
        return value + std::string(targetWidth - width, ' ');
    }
};

class ClientApp {
public:
    int run(int argc, char* argv[]) {
#ifdef _WIN32
        SetConsoleOutputCP(936);
#endif
        std::cout << "Logistics V3 Client started." << '\n';
        std::cout << "Phase 8: concurrency self-test capable client." << '\n';

        SocketClient client("127.0.0.1", 9000);
        if (!client.connectToServer()) {
            std::cerr << "[Client] failed to connect to 127.0.0.1:9000" << '\n';
            return 1;
        }

        // 命令行模式优先于交互菜单，供演示脚本和自动联调直接选择固定流程。
        if (argc == 2 && std::string(argv[1]) == "--selftest-user") {
            return runUserSelfTest(client) ? 0 : 2;
        }
        if (argc == 2 && std::string(argv[1]) == "--selftest-admin") {
            return runAdminSelfTest(client) ? 0 : 2;
        }
        if (argc == 2 && std::string(argv[1]) == "--selftest-courier") {
            return runCourierSelfTest(client) ? 0 : 2;
        }
        if (argc == 2 && std::string(argv[1]) == "--selftest-concurrency") {
            return runConcurrencySelfTest(client) ? 0 : 2;
        }
        if (argc == 4) {
            return loginOnce(client, argv[1], argv[2], argv[3]) ? 0 : 2;
        }
        return mainMenu(client) ? 0 : 2;
    }

private:
    std::string token_;
    std::string role_;
    std::string username_;

#ifdef _WIN32
    // 并发自测中的两个工作线程各持有独立 SocketClient，但共享同一合法会话 Token。
    struct PickupThreadContext {
        const ClientApp* app;
        SocketClient* client;
        std::string token;
        std::string expressId;
        std::atomic<bool>* start;
        Response* response;
    };

    static DWORD WINAPI pickupThreadEntry(LPVOID rawContext) {
        PickupThreadContext* context = static_cast<PickupThreadContext*>(rawContext);
        // 两个线程等待同一启动标志，尽量缩小请求到达服务端的时间差。
        while (!context->start->load()) {
        }
        *(context->response) = context->app->sendRequest(
            *(context->client), "PICKUP_EXPRESS", context->token,
            std::vector<std::string>{context->expressId});
        return 0;
    }
#endif

    bool mainMenu(SocketClient& client) {
        // 主菜单捕获单次网络异常，保持客户端进程可继续操作或正常退出。
        while (true) {
            std::cout << '\n' << "1. 注册普通用户" << '\n';
            std::cout << "2. 登录" << '\n';
            std::cout << "0. 退出" << '\n';
            std::cout << "请选择: ";
            std::string choice;
            std::getline(std::cin, choice);
            try {
            if (choice == "1") {
                registerUserFlow(client);
            } else if (choice == "2") {
                if (interactiveLogin(client)) {
                    if (role_ == "USER") {
                        userMenu(client);
                    } else if (role_ == "ADMIN") {
                        adminMenu(client);
                    } else if (role_ == "COURIER") {
                        courierMenu(client);
                    } else {
                        std::cout << "当前阶段暂未接入该身份菜单。" << '\n';
                    }
                }
            } else if (choice == "0") {
                return true;
            } else {
                std::cout << "无效选择。" << '\n';
            }
            } catch (const std::exception& error) {
                std::cerr << "[错误] " << error.what() << '\n';
                std::cout << "请检查服务端是否仍在运行。" << '\n';
            }
        }
    }

    void registerUserFlow(SocketClient& client) {
        // Step 1: username with instant server-side availability check
        std::string username;
        while (true) {
            username = readLine("用户名: ");
            if (username.empty()) {
                std::cout << "用户名不能为空。" << '\n';
                continue;
            }
            // Local format check first
            if (username.size() < 3 || username.size() > 32) {
                std::cout << "用户名长度应为 3-32 个字符。" << '\n';
                continue;
            }
            // Server-side availability check
            Response checkResp = sendRequest(client, "CHECK_USERNAME_AVAILABLE", "", std::vector<std::string>{username});
            if (!checkResp.ok) {
                std::cout << "[提示] " << checkResp.message << '\n';
                continue;
            }
            break;
        }
        // Step 2: name
        std::string name = readLine("姓名: ");
        // Step 3: phone with format validation
        std::string phone;
        while (true) {
            phone = readLine("手机号: ");
            if (phone.size() != 11 || phone[0] != '1') {
                std::cout << "手机号必须为 11 位数字且以 1 开头。" << '\n';
                continue;
            }
            bool allDigit = true;
            for (char ch : phone) {
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    allDigit = false;
                    break;
                }
            }
            if (!allDigit) {
                std::cout << "手机号只能包含数字。" << '\n';
                continue;
            }
            // Optional: check phone availability with server
            Response phoneResp = sendRequest(client, "CHECK_PHONE_AVAILABLE", "", std::vector<std::string>{phone});
            if (!phoneResp.ok) {
                std::cout << "[提示] " << phoneResp.message << '\n';
                continue;
            }
            break;
        }
        // Step 4: password strength check
        std::string password;
        while (true) {
            password = readPasswordHidden("密码: ");
            if (password.size() < 8) {
                std::cout << "密码长度至少为 8 位。" << '\n';
                continue;
            }
            if (password.size() > 64) {
                std::cout << "密码长度不能超过 64 位。" << '\n';
                continue;
            }
            bool hasAlpha = false, hasDigit = false, allSpace = true;
            for (char ch : password) {
                const unsigned char value = static_cast<unsigned char>(ch);
                if (!std::isspace(value)) allSpace = false;
                if (std::isalpha(value)) hasAlpha = true;
                if (std::isdigit(value)) hasDigit = true;
            }
            if (allSpace) {
                std::cout << "密码不能全为空格。" << '\n';
                continue;
            }
            if (!hasAlpha || !hasDigit) {
                std::cout << "密码必须同时包含字母和数字。" << '\n';
                continue;
            }
            break;
        }
        // Step 5: address
        std::string address = readLine("地址: ");
        // Step 6: send registration (server re-validates everything)
        Response response = sendRequest(client, "REGISTER_USER", "", std::vector<std::string>{username, name, phone, password, address});
        printResponse(response);
    }

    bool interactiveLogin(SocketClient& client) {
        std::cout << "请选择登录身份：" << '\n';
        std::cout << "1. 普通用户 login_user / User1234" << '\n';
        std::cout << "2. 快递员 demo_courier / Courier1234" << '\n';
        std::cout << "3. 管理员 admin / Admin0219" << '\n';
        std::cout << "请输入序号: ";
        std::string choice;
        std::getline(std::cin, choice);

        std::string command;
        if (choice == "1") {
            command = "LOGIN_USER";
        } else if (choice == "2") {
            command = "LOGIN_COURIER";
        } else if (choice == "3") {
            command = "LOGIN_ADMIN";
        } else {
            std::cerr << "[Client] invalid choice." << '\n';
            return false;
        }
        const std::string username = readLine("账号: ");
        const std::string password = readPasswordHidden("密码: ");
        return loginOnce(client, command, username, password);
    }

    void userMenu(SocketClient& client) {
        // 身份菜单只发送当前角色允许的命令，服务端仍会对 Token 和角色做零信任校验。
        while (true) {
            std::cout << '\n' << "用户菜单 - " << username_ << '\n';
            std::cout << "1. 查询余额" << '\n';
            std::cout << "2. 充值" << '\n';
            std::cout << "3. 寄件" << '\n';
            std::cout << "4. 查询我的快递" << '\n';
            std::cout << "5. 查询待签收" << '\n';
            std::cout << "6. 签收快递" << '\n';
            std::cout << "7. 批量签收" << '\n';
            std::cout << "8. 修改密码" << '\n';
            std::cout << "9. 修改个人信息" << '\n';
            std::cout << "10. 修改快递备注" << '\n';
            std::cout << "11. 评价已签收快递" << '\n';
            std::cout << "12. 通知中心" << '\n';
            std::cout << "0. 退出登录" << '\n';
            std::cout << "请选择: ";
            std::string choice;
            std::getline(std::cin, choice);
            // 每次操作独立 try-catch，单次失败不影响继续使用
            try {
            if (choice == "1") {
                Response response = sendRequest(client, "QUERY_BALANCE", token_, std::vector<std::string>());
                printResponse(response);
                // 余额在 records[0] 中，必须显式展示给用户
                if (response.ok && !response.records.empty()) {
                    std::cout << "当前余额：" << response.records[0] << " 元" << '\n';
                }
            } else if (choice == "2") {
                rechargeFlow(client);
            } else if (choice == "3") {
                sendExpressFlow(client);
            } else if (choice == "4") {
                Response response = sendRequest(client, "QUERY_MY_EXPRESS", token_, std::vector<std::string>());
                printResponse(response);
                TablePrinter::printRecords(response.records);
            } else if (choice == "5") {
                Response response = sendRequest(client, "QUERY_WAITING_SIGN", token_, std::vector<std::string>());
                printResponse(response);
                TablePrinter::printRecords(response.records);
            } else if (choice == "6") {
                signExpressWithRatingPrompt(client);
            } else if (choice == "7") {
                signBatchFlow(client);
            } else if (choice == "8") {
                const std::string oldPassword = readPasswordHidden("原密码: ");
                const std::string newPassword = readPasswordHidden("新密码: ");
                printResponse(sendRequest(client, "CHANGE_PASSWORD", token_, std::vector<std::string>{oldPassword, newPassword}));
            } else if (choice == "9") {
                updateMyProfileFlow(client);
            } else if (choice == "10") {
                updateExpressNoteFlow(client);
            } else if (choice == "11") {
                rateExpressFlow(client);
            } else if (choice == "12") {
                notificationMenu(client);
            } else if (choice == "0") {
                sendLogout(client);
                token_.clear();
                role_.clear();
                username_.clear();
                return;
            } else {
                std::cout << "无效选择。" << '\n';
            }
            } catch (const std::exception& error) {
                std::cerr << "[错误] 操作失败：" << error.what() << '\n';
                std::cout << "请检查服务端是否仍在运行，然后重试。" << '\n';
            }
        }
    }

    void adminMenu(SocketClient& client) {
        while (true) {
            std::cout << '\n' << "管理员菜单 - " << username_ << '\n';
            std::cout << "1. 查看用户" << '\n';
            std::cout << "2. 冻结/解冻用户" << '\n';
            std::cout << "3. 查看快递员" << '\n';
            std::cout << "4. 新增快递员" << '\n';
            std::cout << "5. 冻结/解冻快递员" << '\n';
            std::cout << "6. 停用快递员" << '\n';
            std::cout << "7. 分配快递员" << '\n';
            std::cout << "8. 自动分配(单条)" << '\n';
            std::cout << "9. 一键自动分配(全部)" << '\n';
            std::cout << "10. 查询全部快递" << '\n';
            std::cout << "11. 统计看板" << '\n';
            std::cout << "12. 快递员绩效" << '\n';
            std::cout << "13. 查询操作日志" << '\n';
            std::cout << "14. 校验日志哈希链" << '\n';
            std::cout << "15. 修改密码" << '\n';
            std::cout << "16. 修改用户信息" << '\n';
            std::cout << "17. 修改快递员信息" << '\n';
            std::cout << "18. 快递改派" << '\n';
            std::cout << "19. 修改快递备注" << '\n';
            std::cout << "20. 通知中心" << '\n';
            std::cout << "0. 退出登录" << '\n';
            std::cout << "请选择: ";
            std::string choice;
            std::getline(std::cin, choice);
            try {
            if (choice == "1") {
                printRecordsResponse(sendRequest(client, "QUERY_USERS", token_, std::vector<std::string>()));
            } else if (choice == "2") {
                adminSetUserFrozenFlow(client);
            } else if (choice == "3") {
                printRecordsResponse(sendRequest(client, "QUERY_COURIERS", token_, std::vector<std::string>()));
            } else if (choice == "4") {
                createCourierFlow(client);
            } else if (choice == "5") {
                adminSetCourierFrozenFlow(client);
            } else if (choice == "6") {
                const std::string courier = readLine("快递员用户名: ");
                printRecordsResponse(sendRequest(client, "REMOVE_COURIER", token_, std::vector<std::string>{courier}));
            } else if (choice == "7") {
                const std::string expressId = readLine("快递单号: ");
                const std::string courier = readLine("快递员用户名: ");
                printResponse(sendRequest(client, "ASSIGN_COURIER", token_, std::vector<std::string>{expressId, courier}));
            } else if (choice == "8") {
                const std::string expressId = readLine("快递单号: ");
                printResponse(sendRequest(client, "AUTO_ASSIGN_COURIER", token_, std::vector<std::string>{expressId}));
            } else if (choice == "9") {
                printRecordsResponse(sendRequest(client, "AUTO_ASSIGN_ALL", token_, std::vector<std::string>()));
            } else if (choice == "10") {
                printRecordsResponse(sendRequest(client, "QUERY_ALL_EXPRESS", token_, std::vector<std::string>()));
            } else if (choice == "11") {
                printDashboard(sendRequest(client, "VIEW_DASHBOARD", token_, std::vector<std::string>()));
            } else if (choice == "12") {
                printRecordsResponse(sendRequest(client, "VIEW_COURIER_PERFORMANCE", token_, std::vector<std::string>()));
            } else if (choice == "13") {
                adminQueryLogsFlow(client);
            } else if (choice == "14") {
                printResponse(sendRequest(client, "VERIFY_LOG_CHAIN", token_, std::vector<std::string>()));
            } else if (choice == "15") {
                const std::string oldPassword = readPasswordHidden("原密码: ");
                const std::string newPassword = readPasswordHidden("新密码: ");
                printResponse(sendRequest(client, "CHANGE_PASSWORD", token_, std::vector<std::string>{oldPassword, newPassword}));
            } else if (choice == "16") {
                adminUpdateUserFlow(client);
            } else if (choice == "17") {
                adminUpdateCourierFlow(client);
            } else if (choice == "18") {
                reassignCourierFlow(client);
            } else if (choice == "19") {
                updateExpressNoteFlow(client);
            } else if (choice == "20") {
                notificationMenu(client);
            } else if (choice == "0") {
                sendLogout(client);
                token_.clear();
                role_.clear();
                username_.clear();
                return;
            } else {
                std::cout << "无效选择。" << '\n';
            }
            } catch (const std::exception& error) {
                std::cerr << "[错误] 操作失败：" << error.what() << '\n';
                std::cout << "请检查服务端是否仍在运行，然后重试。" << '\n';
            }
        }
    }

    void courierMenu(SocketClient& client) {
        while (true) {
            std::cout << '\n' << "快递员菜单 - " << username_ << '\n';
            std::cout << "1. 查看待揽收任务" << '\n';
            std::cout << "2. 揽收单个快递" << '\n';
            std::cout << "3. 批量揽收" << '\n';
            std::cout << "4. 查询我的任务" << '\n';
            std::cout << "5. 查看个人绩效" << '\n';
            std::cout << "6. 修改密码" << '\n';
            std::cout << "7. 修改个人信息" << '\n';
            std::cout << "8. 通知中心" << '\n';
            std::cout << "0. 退出登录" << '\n';
            std::cout << "请选择: ";
            std::string choice;
            std::getline(std::cin, choice);
            try {
            if (choice == "1") {
                printRecordsResponse(sendRequest(client, "QUERY_MY_PICKUP_TASKS", token_, std::vector<std::string>()));
            } else if (choice == "2") {
                const std::string expressId = readLine("快递单号: ");
                printResponse(sendRequest(client, "PICKUP_EXPRESS", token_, std::vector<std::string>{expressId}));
            } else if (choice == "3") {
                pickupBatchFlow(client);
            } else if (choice == "4") {
                printRecordsResponse(sendRequest(client, "QUERY_MY_TASKS", token_, std::vector<std::string>()));
            } else if (choice == "5") {
                printPerformance(sendRequest(client, "VIEW_MY_PERFORMANCE", token_, std::vector<std::string>()));
            } else if (choice == "6") {
                const std::string oldPassword = readPasswordHidden("原密码: ");
                const std::string newPassword = readPasswordHidden("新密码: ");
                printResponse(sendRequest(client, "CHANGE_PASSWORD", token_, std::vector<std::string>{oldPassword, newPassword}));
            } else if (choice == "7") {
                updateCourierProfileFlow(client);
            } else if (choice == "8") {
                notificationMenu(client);
            } else if (choice == "0") {
                sendLogout(client);
                token_.clear();
                role_.clear();
                username_.clear();
                return;
            } else {
                std::cout << "无效选择。" << '\n';
            }
            } catch (const std::exception& error) {
                std::cerr << "[错误] 操作失败：" << error.what() << '\n';
                std::cout << "请检查服务端是否仍在运行，然后重试。" << '\n';
            }
        }
    }

    void notificationMenu(SocketClient& client) {
        // 通知中心为三种登录身份共用的子菜单，复用当前 token_。
        while (true) {
            std::cout << '\n' << "通知中心" << '\n';
            std::cout << "1. 查看全部通知" << '\n';
            std::cout << "2. 只看未读通知" << '\n';
            std::cout << "3. 标记通知已读" << '\n';
            std::cout << "4. 刷新未读数量" << '\n';
            std::cout << "0. 返回上级" << '\n';
            std::cout << "请选择: ";
            std::string choice;
            std::getline(std::cin, choice);
            try {
            if (choice == "1") {
                queryNotificationsFlow(client);
            } else if (choice == "2") {
                Response response = sendRequest(client, "QUERY_MY_NOTIFICATIONS", token_, std::vector<std::string>{"1"});
                printResponse(response);
                TablePrinter::printRecords(response.records);
            } else if (choice == "3") {
                markNotificationReadFlow(client);
            } else if (choice == "4") {
                showUnreadNotificationCount(client);
            } else if (choice == "0") {
                return;
            } else {
                std::cout << "无效选择。" << '\n';
            }
            } catch (const std::exception& error) {
                std::cerr << "[错误] 操作失败：" << error.what() << '\n';
            }
        }
    }

    void pickupBatchFlow(SocketClient& client) {
        const std::string text = readLine("请输入多个快递单号，使用空格或逗号分隔: ");
        // 每个单号作为独立协议参数发送，服务端返回对应的逐单结果。
        const std::vector<std::string> ids = splitExpressIds(text);
        if (ids.empty()) {
            std::cout << "未输入有效单号。" << '\n';
            return;
        }
        Response response = sendRequest(client, "PICKUP_BATCH", token_, ids);
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void showUnreadNotificationCount(SocketClient& client) {
        if (token_.empty()) return;
        Response response = sendRequest(client, "QUERY_UNREAD_NOTIFICATION_COUNT", token_, std::vector<std::string>());
        if (response.ok && !response.records.empty()) {
            const int count = response.records[0] == "0" ? 0 :
                              (response.records[0][0] >= '1' && response.records[0][0] <= '9' ?
                               std::stoi(response.records[0]) : 0);
            if (count > 0) {
                std::cout << "[通知] 你有 " << count << " 条未读通知。" << '\n';
            }
        }
    }

    void queryNotificationsFlow(SocketClient& client) {
        std::cout << "查看通知（输入 q 取消）" << '\n';
        std::cout << "1. 全部通知  2. 只看未读: ";
        std::string filter;
        std::getline(std::cin, filter);
        if (filter == "q" || filter == "Q") return;
        const std::string unreadOnly = (filter == "2") ? "1" : "0";
        Response response = sendRequest(client, "QUERY_MY_NOTIFICATIONS", token_, std::vector<std::string>{unreadOnly});
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void markNotificationReadFlow(SocketClient& client) {
        const std::string notificationId = readLine("通知 ID（输入 q 取消）: ");
        if (notificationId == "q" || notificationId == "Q") return;
        Response response = sendRequest(client, "MARK_NOTIFICATION_READ", token_, std::vector<std::string>{notificationId});
        printResponse(response);
    }

    void updateMyProfileFlow(SocketClient& client) {
        std::cout << "修改个人信息（输入 q 取消当前操作）" << '\n';
        const std::string name = readLine("姓名: ");
        if (name == "q" || name == "Q") return;
        const std::string phone = readLine("手机号: ");
        if (phone == "q" || phone == "Q") return;
        const std::string address = readLine("地址: ");
        if (address == "q" || address == "Q") return;
        Response response = sendRequest(client, "UPDATE_MY_PROFILE", token_, std::vector<std::string>{name, phone, address});
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void updateCourierProfileFlow(SocketClient& client) {
        std::cout << "修改个人信息（输入 q 取消当前操作）" << '\n';
        const std::string name = readLine("姓名: ");
        if (name == "q" || name == "Q") return;
        const std::string phone = readLine("手机号: ");
        if (phone == "q" || phone == "Q") return;
        Response response = sendRequest(client, "UPDATE_COURIER_PROFILE", token_, std::vector<std::string>{name, phone});
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void adminUpdateUserFlow(SocketClient& client) {
        std::cout << "修改用户信息（输入 q 取消当前操作）" << '\n';
        const std::string target = readLine("目标用户名: ");
        if (target == "q" || target == "Q") return;
        const std::string name = readLine("姓名: ");
        if (name == "q" || name == "Q") return;
        const std::string phone = readLine("手机号: ");
        if (phone == "q" || phone == "Q") return;
        const std::string address = readLine("地址: ");
        if (address == "q" || address == "Q") return;
        std::cout << "冻结状态（0=正常 1=冻结）: ";
        std::string frozenChoice;
        std::getline(std::cin, frozenChoice);
        if (frozenChoice == "q" || frozenChoice == "Q") return;
        const std::string frozen = (frozenChoice == "1") ? "1" : "0";
        Response response = sendRequest(client, "ADMIN_UPDATE_USER", token_,
                                        std::vector<std::string>{target, name, phone, address, frozen});
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void adminUpdateCourierFlow(SocketClient& client) {
        std::cout << "修改快递员信息（输入 q 取消当前操作）" << '\n';
        const std::string target = readLine("目标快递员用户名: ");
        if (target == "q" || target == "Q") return;
        const std::string name = readLine("姓名: ");
        if (name == "q" || name == "Q") return;
        const std::string phone = readLine("手机号: ");
        if (phone == "q" || phone == "Q") return;
        std::cout << "冻结状态（0=正常 1=冻结）: ";
        std::string frozenChoice;
        std::getline(std::cin, frozenChoice);
        if (frozenChoice == "q" || frozenChoice == "Q") return;
        const std::string frozen = (frozenChoice == "1") ? "1" : "0";
        Response response = sendRequest(client, "ADMIN_UPDATE_COURIER", token_,
                                        std::vector<std::string>{target, name, phone, frozen});
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void reassignCourierFlow(SocketClient& client) {
        std::cout << "快递改派（输入 q 取消当前操作）" << '\n';
        const std::string expressId = readLine("快递单号: ");
        if (expressId == "q" || expressId == "Q") return;
        const std::string newCourier = readLine("新快递员用户名: ");
        if (newCourier == "q" || newCourier == "Q") return;
        const std::string reason = readLine("改派原因: ");
        if (reason == "q" || reason == "Q") return;
        Response response = sendRequest(client, "REASSIGN_COURIER", token_,
                                        std::vector<std::string>{expressId, newCourier, reason});
        printResponse(response);
    }

    void updateExpressNoteFlow(SocketClient& client) {
        std::cout << "修改快递备注（输入 q 取消）" << '\n';
        const std::string expressId = readLine("快递单号: ");
        if (expressId == "q" || expressId == "Q") return;
        const std::string note = readLine("备注内容: ");
        if (note == "q" || note == "Q") return;
        Response response = sendRequest(client, "UPDATE_EXPRESS_NOTE", token_,
                                        std::vector<std::string>{expressId, note});
        printResponse(response);
    }

    void rateExpressFlow(SocketClient& client) {
        std::cout << "评价已签收快递（输入 q 取消）" << '\n';
        const std::string expressId = readLine("快递单号: ");
        if (expressId == "q" || expressId == "Q") return;
        std::cout << "评分（1-5，5 为最优）: ";
        std::string scoreText;
        std::getline(std::cin, scoreText);
        if (scoreText == "q" || scoreText == "Q") return;
        const std::string comment = readLine("评价内容: ");
        if (comment == "q" || comment == "Q") return;
        Response response = sendRequest(client, "RATE_EXPRESS", token_,
                                        std::vector<std::string>{expressId, scoreText, comment});
        printResponse(response);
    }

    void signExpressWithRatingPrompt(SocketClient& client) {
        const std::string expressId = readLine("快递单号: ");
        Response response = sendRequest(client, "SIGN_EXPRESS", token_, std::vector<std::string>{expressId});
        printResponse(response);
        if (!response.ok) {
            return;
        }
        std::cout << "是否立即评价本次快递？(y/n): ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "y" || choice == "Y") {
            std::cout << "评分（1-5，5 为最优）: ";
            std::string scoreText;
            std::getline(std::cin, scoreText);
            if (scoreText == "q" || scoreText == "Q") return;
            const std::string comment = readLine("评价内容: ");
            if (comment == "q" || comment == "Q") return;
            Response rateResponse = sendRequest(client, "RATE_EXPRESS", token_,
                                                std::vector<std::string>{expressId, scoreText, comment});
            printResponse(rateResponse);
        }
    }

    void signBatchFlow(SocketClient& client) {
        const std::string text = readLine("请输入多个快递单号，使用空格或逗号分隔: ");
        const std::vector<std::string> ids = splitExpressIds(text);
        if (ids.empty()) {
            std::cout << "未输入有效单号。" << '\n';
            return;
        }
        Response response = sendRequest(client, "SIGN_BATCH", token_, ids);
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    void adminSetUserFrozenFlow(SocketClient& client) {
        const std::string username = readLine("用户名: ");
        std::cout << "1.冻结 2.解冻，请选择: ";
        std::string choice;
        std::getline(std::cin, choice);
        const std::string frozen = (choice == "2") ? "0" : "1";
        printResponse(sendRequest(client, "SET_USER_FROZEN", token_, std::vector<std::string>{username, frozen}));
    }

    void adminSetCourierFrozenFlow(SocketClient& client) {
        const std::string username = readLine("快递员用户名: ");
        std::cout << "1.冻结 2.解冻，请选择: ";
        std::string choice;
        std::getline(std::cin, choice);
        const std::string frozen = (choice == "2") ? "0" : "1";
        printResponse(sendRequest(client, "SET_COURIER_FROZEN", token_, std::vector<std::string>{username, frozen}));
    }

    void adminQueryLogsFlow(SocketClient& client) {
        std::cout << "筛选条件（回车跳过）：" << '\n';
        const std::string filterType = readLine("  角色类型 (USER/ADMIN/COURIER/GUEST/SYSTEM): ");
        const std::string filterActor = readLine("  操作者: ");
        const std::string filterAction = readLine("  操作动作: ");
        const std::string filterResult = readLine("  操作结果 (SUCCESS/FAILED/DENIED): ");
        Response response = sendRequest(client, "QUERY_LOGS", token_,
                                        std::vector<std::string>{filterType, filterActor, filterAction, filterResult});
        printResponse(response);
        if (!response.records.empty()) {
            const std::size_t show = response.records.size() > 50U ? 50U : response.records.size();
            std::cout << "[Logs] 共 " << response.records.size() << " 条，显示最近 " << show << " 条：" << '\n';
            std::vector<std::string> recent(response.records.end() - static_cast<std::ptrdiff_t>(show), response.records.end());
            TablePrinter::printRecords(recent);
        }
    }

    void createCourierFlow(SocketClient& client) {
        const std::string username = readLine("快递员用户名: ");
        const std::string name = readLine("姓名: ");
        const std::string phone = readLine("手机号: ");
        const std::string password = readPasswordHidden("初始密码: ");
        printResponse(sendRequest(client, "CREATE_COURIER", token_, std::vector<std::string>{username, name, phone, password}));
    }

    void rechargeFlow(SocketClient& client) {
        const std::string amountText = readLine("充值金额: ");
        double amount = 0.0;
        if (!StringUtil::parseDoubleStrict(amountText, amount) || !InputValidator::validPositiveAmount(amount)) {
            std::cout << "金额必须为正数。" << '\n';
            return;
        }
        printResponse(sendRequest(client, "RECHARGE", token_, std::vector<std::string>{amountText}));
    }

    void sendExpressFlow(SocketClient& client) {
        const std::string receiver = readLine("收件用户名: ");
        const std::string description = readLine("物品描述: ");
        const std::string note = readLine("备注（可回车跳过）: ");
        std::cout << "类型：1.普通 2.易碎品 3.图书，请选择: ";
        std::string typeChoice;
        std::getline(std::cin, typeChoice);
        std::string itemType = "Normal";
        if (typeChoice == "2") {
            itemType = "Fragile";
        } else if (typeChoice == "3") {
            itemType = "Book";
        }
        const std::string amountText = readLine(itemType == "Book" ? "册数: " : "重量 kg: ");
        double amount = 0.0;
        if (!StringUtil::parseDoubleStrict(amountText, amount) || !InputValidator::validPositiveAmount(amount)) {
            std::cout << "重量或册数必须为正数。" << '\n';
            return;
        }

        std::vector<std::string> expressArgs;
        // 保存本次寄件参数，余额不足并充值成功后可原样重试，费用仍由服务端计算。
        expressArgs.push_back(receiver);
        expressArgs.push_back(description);
        expressArgs.push_back(itemType);
        expressArgs.push_back(amountText);
        if (!note.empty()) {
            expressArgs.push_back(note);
        }
        Response response = sendRequest(client, "SEND_EXPRESS", token_, expressArgs);
        printResponse(response);
        if (response.ok && response.records.size() >= 3U) {
            std::cout << "单号: " << response.records[0] << "，费用: " << response.records[1]
                      << "，剩余余额: " << response.records[2] << '\n';
            return;
        }

        // Balance insufficient: offer immediate recharge and retry
        if (!response.ok && response.code == "BALANCE_NOT_ENOUGH") {
            std::cout << '\n' << "[提示] 余额不足，请充值后再寄件。" << '\n';
            // Parse balance/fee/shortage from records if available
            std::string balanceStr, feeStr, shortageStr;
            for (const std::string& record : response.records) {
                if (record.find("balance=") == 0) balanceStr = record.substr(8);
                else if (record.find("fee=") == 0) feeStr = record.substr(4);
                else if (record.find("shortage=") == 0) shortageStr = record.substr(9);
            }
            if (!balanceStr.empty()) std::cout << "  当前余额: " << balanceStr << '\n';
            if (!feeStr.empty()) std::cout << "  预计费用: " << feeStr << '\n';
            if (!shortageStr.empty()) std::cout << "  缺口金额: " << shortageStr << '\n';

            std::cout << "是否立即充值？(y/n): ";
            std::string rechargeChoice;
            std::getline(std::cin, rechargeChoice);
            if (rechargeChoice == "y" || rechargeChoice == "Y") {
                const std::string rechargeAmount = readLine("充值金额: ");
                double rcAmount = 0.0;
                if (!StringUtil::parseDoubleStrict(rechargeAmount, rcAmount) || rcAmount <= 0.0) {
                    std::cout << "金额无效，充值取消。" << '\n';
                    return;
                }
                Response rcResponse = sendRequest(client, "RECHARGE", token_, std::vector<std::string>{rechargeAmount});
                printResponse(rcResponse);
                if (!rcResponse.ok) {
                    std::cout << "充值失败，寄件已取消。" << '\n';
                    return;
                }
                std::cout << "是否使用刚才的寄件信息重新提交？(y/n): ";
                std::string retryChoice;
                std::getline(std::cin, retryChoice);
                if (retryChoice == "y" || retryChoice == "Y") {
                    Response retryResponse = sendRequest(client, "SEND_EXPRESS", token_, expressArgs);
                    printResponse(retryResponse);
                    if (retryResponse.ok && retryResponse.records.size() >= 3U) {
                        std::cout << "单号: " << retryResponse.records[0] << "，费用: " << retryResponse.records[1]
                                  << "，剩余余额: " << retryResponse.records[2] << '\n';
                    }
                }
            }
        }
    }

    bool runUserSelfTest(SocketClient& client) {
        // 验证主链路：注册双方、充值、服务端计费寄件，以及发件人越权签收拦截。
        const std::string suffix = uniqueSuffix();
        const std::string sender = "u" + suffix;
        const std::string receiver = "r" + suffix;

        if (!expectOk("register sender", sendRequest(client, "REGISTER_USER", "",
                                                      std::vector<std::string>{sender, "Sender", "139" + suffix.substr(0, 8), "User1234", "Sender Address"}))) {
            return false;
        }
        if (!expectOk("register receiver", sendRequest(client, "REGISTER_USER", "",
                                                        std::vector<std::string>{receiver, "Receiver", "138" + suffix.substr(0, 8), "User1234", "Receiver Address"}))) {
            return false;
        }
        if (!loginOnce(client, "LOGIN_USER", sender, "User1234")) {
            return false;
        }
        if (!expectOk("recharge 100", sendRequest(client, "RECHARGE", token_, std::vector<std::string>{"100"}))) {
            return false;
        }
        const Response sendResponse = sendRequest(client, "SEND_EXPRESS", token_,
                                                  std::vector<std::string>{receiver, "Fragile selftest package", "Fragile", "2"});
        if (!expectOk("send fragile 2kg", sendResponse) || sendResponse.records.size() < 3U ||
            sendResponse.records[1] != "16.00" || sendResponse.records[2] != "84.00") {
            std::cerr << "[SelfTest] send express fee/balance mismatch." << '\n';
            return false;
        }
        const std::string expressId = sendResponse.records[0];
        const Response maliciousSign = sendRequest(client, "SIGN_EXPRESS", token_, std::vector<std::string>{expressId});
        printNamedResponse("malicious sign", maliciousSign);
        if (maliciousSign.ok || maliciousSign.code != "PERMISSION_DENIED") {
            std::cerr << "[SelfTest] malicious sign was not blocked." << '\n';
            return false;
        }
        const Response queryResponse = sendRequest(client, "QUERY_MY_EXPRESS", token_, std::vector<std::string>());
        if (!expectOk("query my express", queryResponse)) {
            return false;
        }
        TablePrinter::printRecords(queryResponse.records);
        std::cout << "[SelfTest] Phase 5 user network business passed." << '\n';
        return true;
    }

    bool runAdminSelfTest(SocketClient& client) {
        // 先用普通用户会话验证管理员权限防线，再覆盖分配、停用冲突和统计能力。
        if (!loginOnce(client, "LOGIN_USER", "login_user", "User1234")) {
            return false;
        }
        const Response denied = sendRequest(client, "VIEW_DASHBOARD", token_, std::vector<std::string>());
        printNamedResponse("user token calls admin dashboard", denied);
        if (denied.ok || denied.code != "PERMISSION_DENIED") {
            std::cerr << "[AdminSelfTest] user token was not blocked." << '\n';
            return false;
        }

        if (!loginOnce(client, "LOGIN_ADMIN", "admin", "Admin0219")) {
            return false;
        }
        Response allExpress = sendRequest(client, "QUERY_ALL_EXPRESS", token_, std::vector<std::string>());
        if (!expectOk("query all express", allExpress) || allExpress.records.empty()) {
            return false;
        }
        const std::string expressId = firstAssignableExpressId(allExpress.records);
        if (expressId.empty()) {
            std::cerr << "[AdminSelfTest] cannot parse express id." << '\n';
            return false;
        }
        const Response assign = sendRequest(client, "ASSIGN_COURIER", token_, std::vector<std::string>{expressId, "phase6_courier"});
        printNamedResponse("assign phase6_courier", assign);

        const Response remove = sendRequest(client, "REMOVE_COURIER", token_, std::vector<std::string>{"phase6_courier"});
        printNamedResponse("remove courier conflict", remove);
        TablePrinter::printRecords(remove.records);
        if (remove.ok || remove.code != "COURIER_HAS_UNFINISHED_TASKS" || remove.records.empty()) {
            std::cerr << "[AdminSelfTest] conflict list was not returned." << '\n';
            return false;
        }

        if (!expectOk("auto assign all", sendRequest(client, "AUTO_ASSIGN_ALL", token_, std::vector<std::string>()))) {
            return false;
        }
        const Response dashboard = sendRequest(client, "VIEW_DASHBOARD", token_, std::vector<std::string>());
        if (!expectOk("view dashboard", dashboard)) {
            return false;
        }
        printDashboard(dashboard);

        const Response performance = sendRequest(client, "VIEW_COURIER_PERFORMANCE", token_, std::vector<std::string>());
        if (!expectOk("view courier performance", performance)) {
            return false;
        }
        TablePrinter::printRecords(performance.records);
        std::cout << "[SelfTest] Phase 6 admin network business passed." << '\n';
        return true;
    }

    bool runCourierSelfTest(SocketClient& client) {
        // 构造两个快递员和三笔任务，核对归属校验、批量状态变化与提成增量。
        const std::string suffix = uniqueSuffix();
        const std::string sender = "cs" + suffix.substr(0, 7);
        const std::string receiver = "cr" + suffix.substr(0, 7);
        const std::string courierA = "ca" + suffix.substr(0, 7);
        const std::string courierB = "cb" + suffix.substr(0, 7);

        if (!loginOnce(client, "LOGIN_ADMIN", "admin", "Admin0219")) {
            return false;
        }
        const std::string adminToken = token_;
        if (!expectOk("create courier A", sendRequest(client, "CREATE_COURIER", adminToken,
                                                       std::vector<std::string>{courierA, "Courier A", "137" + suffix.substr(0, 8), "Courier1234"}))) {
            return false;
        }
        if (!expectOk("create courier B", sendRequest(client, "CREATE_COURIER", adminToken,
                                                       std::vector<std::string>{courierB, "Courier B", "136" + suffix.substr(0, 8), "Courier1234"}))) {
            return false;
        }
        if (!expectOk("register sender", sendRequest(client, "REGISTER_USER", "",
                                                      std::vector<std::string>{sender, "Courier Sender", "135" + suffix.substr(0, 8), "User1234", "Sender Address"}))) {
            return false;
        }
        if (!expectOk("register receiver", sendRequest(client, "REGISTER_USER", "",
                                                        std::vector<std::string>{receiver, "Courier Receiver", "134" + suffix.substr(0, 8), "User1234", "Receiver Address"}))) {
            return false;
        }
        if (!loginOnce(client, "LOGIN_USER", sender, "User1234")) {
            return false;
        }
        if (!expectOk("recharge sender", sendRequest(client, "RECHARGE", token_, std::vector<std::string>{"100"}))) {
            return false;
        }
        const Response s1 = sendRequest(client, "SEND_EXPRESS", token_, std::vector<std::string>{receiver, "A task one", "Fragile", "2"});
        const Response s2 = sendRequest(client, "SEND_EXPRESS", token_, std::vector<std::string>{receiver, "A task two", "Normal", "4"});
        const Response s3 = sendRequest(client, "SEND_EXPRESS", token_, std::vector<std::string>{receiver, "B task", "Book", "5"});
        if (!expectOk("send task one", s1) || !expectOk("send task two", s2) || !expectOk("send B task", s3) ||
            s1.records.empty() || s2.records.empty() || s3.records.empty()) {
            return false;
        }
        const std::string aExpress1 = s1.records[0];
        const std::string aExpress2 = s2.records[0];
        const std::string bExpress = s3.records[0];
        if (!expectOk("assign A task one", sendRequest(client, "ASSIGN_COURIER", adminToken, std::vector<std::string>{aExpress1, courierA})) ||
            !expectOk("assign A task two", sendRequest(client, "ASSIGN_COURIER", adminToken, std::vector<std::string>{aExpress2, courierA})) ||
            !expectOk("assign B task", sendRequest(client, "ASSIGN_COURIER", adminToken, std::vector<std::string>{bExpress, courierB}))) {
            return false;
        }

        if (!loginOnce(client, "LOGIN_COURIER", courierA, "Courier1234")) {
            return false;
        }
        const Response pickupTasksBefore = sendRequest(client, "QUERY_MY_PICKUP_TASKS", token_, std::vector<std::string>());
        if (!expectOk("pickup tasks before", pickupTasksBefore) || pickupTasksBefore.records.size() != 2U) {
            std::cerr << "[CourierSelfTest] pickup task count before batch mismatch." << '\n';
            return false;
        }
        TablePrinter::printRecords(pickupTasksBefore.records);

        const Response before = sendRequest(client, "VIEW_MY_PERFORMANCE", token_, std::vector<std::string>());
        if (!expectOk("performance before", before)) {
            return false;
        }
        printPerformance(before);
        const int beforeWaitingPickup = extractIntValue(before.records.empty() ? "" : before.records[0], "waitingPickup=");
        const int beforeWaitingSign = extractIntValue(before.records.empty() ? "" : before.records[0], "waitingSign=");

        const Response denied = sendRequest(client, "PICKUP_EXPRESS", token_, std::vector<std::string>{bExpress});
        printNamedResponse("courier A pickup courier B task", denied);
        if (denied.ok || denied.code != "PERMISSION_DENIED") {
            std::cerr << "[CourierSelfTest] cross-courier pickup was not blocked." << '\n';
            return false;
        }

        const Response batch = sendRequest(client, "PICKUP_BATCH", token_, std::vector<std::string>{aExpress1, aExpress2});
        printNamedResponse("pickup batch own tasks", batch);
        TablePrinter::printRecords(batch.records);
        if (!batch.ok) {
            return false;
        }

        const Response repeated = sendRequest(client, "PICKUP_EXPRESS", token_, std::vector<std::string>{aExpress1});
        printNamedResponse("repeat pickup after batch", repeated);
        if (repeated.ok || repeated.code != "STATE_CONFLICT") {
            std::cerr << "[CourierSelfTest] repeat pickup was not blocked." << '\n';
            return false;
        }

        const Response pickupTasksAfter = sendRequest(client, "QUERY_MY_PICKUP_TASKS", token_, std::vector<std::string>());
        if (!expectOk("pickup tasks after", pickupTasksAfter) || !pickupTasksAfter.records.empty()) {
            std::cerr << "[CourierSelfTest] pickup task count after batch mismatch." << '\n';
            return false;
        }
        TablePrinter::printRecords(pickupTasksAfter.records);

        const Response after = sendRequest(client, "VIEW_MY_PERFORMANCE", token_, std::vector<std::string>());
        if (!expectOk("performance after", after)) {
            return false;
        }
        printPerformance(after);
        const double beforeIncome = extractMoneyValue(before.records.empty() ? "" : before.records[0], "income=");
        const double afterIncome = extractMoneyValue(after.records.empty() ? "" : after.records[0], "income=");
        const int afterWaitingPickup = extractIntValue(after.records.empty() ? "" : after.records[0], "waitingPickup=");
        const int afterWaitingSign = extractIntValue(after.records.empty() ? "" : after.records[0], "waitingSign=");
        const double expectedDelta = 18.0;
        if (afterIncome < beforeIncome + expectedDelta - 0.0001 ||
            afterIncome > beforeIncome + expectedDelta + 0.0001) {
            std::cerr << "[CourierSelfTest] income delta mismatch. before=" << beforeIncome
                      << " after=" << afterIncome << '\n';
            return false;
        }
        if (afterWaitingPickup != beforeWaitingPickup - 2 || afterWaitingSign != beforeWaitingSign + 2) {
            std::cerr << "[CourierSelfTest] pickup/sign task counters mismatch. beforePickup="
                      << beforeWaitingPickup << " afterPickup=" << afterWaitingPickup
                      << " beforeSign=" << beforeWaitingSign << " afterSign=" << afterWaitingSign << '\n';
            return false;
        }
        std::cout << "[SelfTest] Phase 7 courier network business passed." << '\n';
        return true;
    }

    bool runConcurrencySelfTest(SocketClient& setupClient) {
        // 先串行准备唯一待揽收订单，再用双连接制造对同一状态的并发竞争。
        const std::string suffix = uniqueSuffix();
        const std::string sender = "xs" + suffix.substr(0, 7);
        const std::string receiver = "xr" + suffix.substr(0, 7);
        const std::string courier = "xc" + suffix.substr(0, 7);

        if (!loginOnce(setupClient, "LOGIN_ADMIN", "admin", "Admin0219")) {
            return false;
        }
        const std::string adminToken = token_;
        if (!expectOk("create concurrency courier", sendRequest(setupClient, "CREATE_COURIER", adminToken,
                                                                 std::vector<std::string>{courier, "Concurrent Courier", "133" + suffix.substr(0, 8), "Courier1234"}))) {
            return false;
        }
        if (!expectOk("register concurrency sender", sendRequest(setupClient, "REGISTER_USER", "",
                                                                  std::vector<std::string>{sender, "Concurrent Sender", "132" + suffix.substr(0, 8), "User1234", "Sender Address"}))) {
            return false;
        }
        if (!expectOk("register concurrency receiver", sendRequest(setupClient, "REGISTER_USER", "",
                                                                    std::vector<std::string>{receiver, "Concurrent Receiver", "131" + suffix.substr(0, 8), "User1234", "Receiver Address"}))) {
            return false;
        }
        if (!loginOnce(setupClient, "LOGIN_USER", sender, "User1234")) {
            return false;
        }
        if (!expectOk("recharge concurrency sender", sendRequest(setupClient, "RECHARGE", token_, std::vector<std::string>{"50"}))) {
            return false;
        }
        const Response sendResponse = sendRequest(setupClient, "SEND_EXPRESS", token_,
                                                  std::vector<std::string>{receiver, "Concurrent pickup target", "Fragile", "2"});
        if (!expectOk("send concurrency target", sendResponse) || sendResponse.records.empty()) {
            return false;
        }
        const std::string expressId = sendResponse.records[0];
        if (!expectOk("assign concurrency target", sendRequest(setupClient, "ASSIGN_COURIER", adminToken,
                                                                std::vector<std::string>{expressId, courier}))) {
            return false;
        }

        SocketClient clientA("127.0.0.1", 9000);
        SocketClient clientB("127.0.0.1", 9000);
        if (!clientA.connectToServer() || !clientB.connectToServer()) {
            std::cerr << "[ConcurrencySelfTest] failed to open two extra client connections." << '\n';
            return false;
        }

        // Login once and share the token across both connections.
        // Duplicate login (same username+role) is now blocked by the server,
        // but both connections can share a valid token to test concurrent state conflicts.
        std::string sharedToken;
        if (!loginToken(clientA, "LOGIN_COURIER", courier, "Courier1234", sharedToken)) {
            return false;
        }

        std::atomic<bool> start{false};
        Response responseA;
        Response responseB;
#ifdef _WIN32
        PickupThreadContext contextA{this, &clientA, sharedToken, expressId, &start, &responseA};
        PickupThreadContext contextB{this, &clientB, sharedToken, expressId, &start, &responseB};
        HANDLE handles[2] = {
            CreateThread(nullptr, 0, &ClientApp::pickupThreadEntry, &contextA, 0, nullptr),
            CreateThread(nullptr, 0, &ClientApp::pickupThreadEntry, &contextB, 0, nullptr)
        };
        if (handles[0] == nullptr || handles[1] == nullptr) {
            std::cerr << "[ConcurrencySelfTest] failed to create pickup worker threads." << '\n';
            if (handles[0] != nullptr) {
                CloseHandle(handles[0]);
            }
            if (handles[1] != nullptr) {
                CloseHandle(handles[1]);
            }
            return false;
        }
        // 所有上下文均已就绪后同时放行，并等待两个响应全部返回再断言结果。
        start.store(true);
        WaitForMultipleObjects(2, handles, TRUE, INFINITE);
        CloseHandle(handles[0]);
        CloseHandle(handles[1]);
#else
        responseA = sendRequest(clientA, "PICKUP_EXPRESS", tokenA, std::vector<std::string>{expressId});
        responseB = sendRequest(clientB, "PICKUP_EXPRESS", tokenB, std::vector<std::string>{expressId});
#endif

        printNamedResponse("concurrent pickup A", responseA);
        printNamedResponse("concurrent pickup B", responseB);
        const int successCount = (responseA.ok ? 1 : 0) + (responseB.ok ? 1 : 0);
        const int conflictCount = ((!responseA.ok && responseA.code == "STATE_CONFLICT") ? 1 : 0) +
                                  ((!responseB.ok && responseB.code == "STATE_CONFLICT") ? 1 : 0);
        if (successCount != 1 || conflictCount != 1) {
            std::cerr << "[ConcurrencySelfTest] expected exactly one success and one STATE_CONFLICT." << '\n';
            return false;
        }

        std::cout << "[SelfTest] Phase 8 concurrent pickup conflict passed." << '\n';
        return true;
    }

    bool loginOnce(SocketClient& client, const std::string& command,
                   const std::string& username, const std::string& password) {
        const Response response = sendRequest(client, command, "", std::vector<std::string>{username, password});
        printResponse(response);
        if (!response.ok) {
            if (response.code == "ALREADY_LOGGED_IN") {
                std::cout << "[提示] 该账号已在其他客户端登录，请先退出或等待断线清理。" << '\n';
            }
            return false;
        }
        if (response.records.size() >= 3U) {
            // 登录响应约定 records 顺序为 token、role、username。
            token_ = response.records[0];
            role_ = response.records[1];
            username_ = response.records[2];
            std::cout << "[Client] token: " << token_ << '\n';
            std::cout << "[Client] role: " << role_ << ", username: " << username_ << '\n';
            showUnreadNotificationCount(client);
        }
        return true;
    }

    void sendLogout(SocketClient& client) {
        if (token_.empty()) {
            return;
        }
        Response response = sendRequest(client, "LOGOUT", token_, std::vector<std::string>());
        // Logout is best-effort; failure does not block exit
        if (!response.ok) {
            std::cout << "[提示] 登出请求失败：" << response.message << '\n';
        }
    }

    bool loginToken(SocketClient& client, const std::string& command,
                    const std::string& username, const std::string& password,
                    std::string& tokenOut) const {
        const Response response = sendRequest(client, command, "", std::vector<std::string>{username, password});
        printResponse(response);
        if (!response.ok || response.records.empty()) {
            return false;
        }
        tokenOut = response.records[0];
        return true;
    }

    Response sendRequest(SocketClient& client, const std::string& command,
                         const std::string& token, const std::vector<std::string>& args) const {
        // 所有界面和自测路径通过同一入口组装 Request，避免协议字段顺序分散。
        Request request;
        request.command = command;
        request.token = token;
        request.args = args;
        return client.sendCommand(request);
    }

    bool expectOk(const std::string& step, const Response& response) const {
        printNamedResponse(step, response);
        return response.ok;
    }

    void printNamedResponse(const std::string& step, const Response& response) const {
        std::cout << "[Client] " << step << ": ";
        printResponse(response);
    }

    static void printRecordsResponse(const Response& response) {
        printResponse(response);
        TablePrinter::printRecords(response.records);
    }

    static void printDashboard(const Response& response) {
        printResponse(response);
        if (!response.records.empty()) {
            std::cout << "[Dashboard]" << '\n';
            for (const std::string& record : response.records) {
                std::cout << "  " << record << '\n';
            }
        }
    }

    static void printResponse(const Response& response) {
        std::cout << (response.ok ? "OK" : "ERR") << '|'
                  << response.code << '|' << response.message << '\n';
    }

    static std::string readLine(const std::string& prompt) {
        std::cout << prompt;
        std::string value;
        std::getline(std::cin, value);
        return value;
    }

    static std::string uniqueSuffix() {
        // 取毫秒时间低八位生成本轮测试后缀，降低重复运行时账号冲突概率。
        const long long ticks = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream stream;
        stream << (ticks % 100000000LL);
        std::string value = stream.str();
        while (value.size() < 8U) {
            value = "0" + value;
        }
        return value;
    }

    static std::string firstAssignableExpressId(const std::vector<std::string>& records) {
        if (records.empty()) {
            return "";
        }
        for (const std::string& record : records) {
            if (record.find("courier=;") != std::string::npos && record.find("status=待揽收") != std::string::npos) {
                const std::size_t pos = record.find(';');
                return pos == std::string::npos ? record : record.substr(0, pos);
            }
        }
        const std::size_t pos = records.front().find(';');
        return pos == std::string::npos ? records.front() : records.front().substr(0, pos);
    }

    static std::vector<std::string> splitExpressIds(const std::string& text) {
        std::vector<std::string> result;
        std::string current;
        // 连续逗号或空白只作为分隔，不生成空单号参数。
        for (char ch : text) {
            if (ch == ',' || std::isspace(static_cast<unsigned char>(ch))) {
                if (!current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            result.push_back(current);
        }
        return result;
    }

    static void printPerformance(const Response& response) {
        printResponse(response);
        TablePrinter::printRecords(response.records);
        for (const std::string& record : response.records) {
            const double completion = extractMoneyValue(record, "completionRate=");
            const int bars = static_cast<int>(completion / 10.0);
            std::cout << "完成率: [";
            for (int i = 0; i < 10; ++i) {
                std::cout << (i < bars ? '#' : '.');
            }
            std::cout << "] " << completion << "%" << '\n';
        }
    }

    static double extractMoneyValue(const std::string& record, const std::string& key) {
        const std::size_t begin = record.find(key);
        if (begin == std::string::npos) {
            return 0.0;
        }
        std::size_t valueBegin = begin + key.size();
        std::size_t valueEnd = record.find(';', valueBegin);
        if (valueEnd == std::string::npos) {
            valueEnd = record.size();
        }
        std::string value = record.substr(valueBegin, valueEnd - valueBegin);
        if (!value.empty() && value.back() == '%') {
            value.pop_back();
        }
        double parsed = 0.0;
        StringUtil::parseDoubleStrict(value, parsed);
        return parsed;
    }

    static int extractIntValue(const std::string& record, const std::string& key) {
        const std::size_t begin = record.find(key);
        if (begin == std::string::npos) {
            return 0;
        }
        const std::size_t valueBegin = begin + key.size();
        std::size_t valueEnd = record.find(';', valueBegin);
        if (valueEnd == std::string::npos) {
            valueEnd = record.size();
        }
        std::istringstream stream(record.substr(valueBegin, valueEnd - valueBegin));
        int value = 0;
        stream >> value;
        return value;
    }

    static std::string readPasswordHidden(const std::string& prompt) {
        std::cout << prompt;
#ifdef _WIN32
        std::string password;
        // 退格同步删除内存字符和屏幕掩码；扩展按键的第二字节直接忽略。
        while (true) {
            const int ch = _getch();
            if (ch == '\r' || ch == '\n') {
                std::cout << '\n';
                break;
            }
            if (ch == '\b') {
                if (!password.empty()) {
                    password.pop_back();
                    std::cout << "\b \b";
                }
                continue;
            }
            if (ch == 0 || ch == 224) {
                _getch();
                continue;
            }
            password.push_back(static_cast<char>(ch));
            std::cout << '*';
        }
        return password;
#else
        std::string password;
        std::getline(std::cin, password);
        return password;
#endif
    }
};

int main(int argc, char* argv[]) {
    try {
        ClientApp app;
        return app.run(argc, argv);
    } catch (const ProtocolError& error) {
        std::cerr << "[Client] protocol error: " << error.code() << " | " << error.what() << '\n';
    } catch (const std::exception& error) {
        std::cerr << "[Client] error: " << error.what() << '\n';
    }
    return 9;
}
