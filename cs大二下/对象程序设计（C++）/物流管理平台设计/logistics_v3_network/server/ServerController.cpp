// =============================================================================
// ServerController.cpp — 服务端命令路由实现
// =============================================================================
// 文件用途：实现全部 46 个网络命令的路由分发和权限校验。
// 所属模块：server（服务端网络层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 路由架构：
//   handle() 主入口
//     ├── PING — 心跳检测（无 token 要求）
//     ├── LOGIN_USER/COURIER/ADMIN — handleLogin（三身份登录）
//     ├── REGISTER_USER — handleRegisterUser（游客注册）
//     ├── CHECK_USERNAME_AVAILABLE / CHECK_PHONE_AVAILABLE — 即时校验
//     ├── LOGOUT — handleLogout（登出）
//     ├── QUERY_MY_NOTIFICATIONS / MARK_NOTIFICATION_READ / ... → handleNotificationCommand
//     ├── QUERY_BALANCE / RECHARGE / SEND_EXPRESS / ... → handleUserCommand
//     ├── isAdminCommand → handleAdminCommand
//     ├── isCourierCommand → handleCourierCommand
//     └── UNKNOWN_COMMAND（未知命令）
//
// 零信任权限校验流程：
//   1. require*Session: token 为空 → AUTH_REQUIRED
//   2. SessionManager::getSession(token) → 失败 → AUTH_REQUIRED
//   3. session.role != 要求角色 → PERMISSION_DENIED
//   4. requireAdminSession / requireCourierSession → 写审计日志
//   5. sessions_.touch(token) → 刷新活跃时间
// =============================================================================

#include "ServerController.h"

#include "../common/security/StringUtil.h"

#include <sstream>

ServerController::ServerController(LogisticsSystem& system, SessionManager& sessions)
    : system_(system), sessions_(sessions) {}

void ServerController::closeSession(const std::string& token) {
    if (!token.empty()) {
        sessions_.removeSession(token);
    }
}

Response ServerController::handle(const Request& request) {
    // 先处理游客级命令，再按身份命令集合分流；身份检查仍在各 handler 入口执行。
    if (request.command == "PING") {
        Response response;
        response.ok = true;
        response.code = "PONG";
        response.message = "Server is alive";
        return response;
    }
    if (request.command == "LOGIN_USER") {
        return handleLogin(request, "USER");
    }
    if (request.command == "LOGIN_COURIER") {
        return handleLogin(request, "COURIER");
    }
    if (request.command == "LOGIN_ADMIN") {
        return handleLogin(request, "ADMIN");
    }
    if (request.command == "REGISTER_USER") {
        return handleRegisterUser(request);
    }
    if (request.command == "CHECK_USERNAME_AVAILABLE") {
        return handleCheckUsernameAvailable(request);
    }
    if (request.command == "CHECK_PHONE_AVAILABLE") {
        return handleCheckPhoneAvailable(request);
    }
    if (request.command == "LOGOUT") {
        return handleLogout(request);
    }
    if (request.command == "QUERY_MY_NOTIFICATIONS" ||
        request.command == "MARK_NOTIFICATION_READ" ||
        request.command == "QUERY_UNREAD_NOTIFICATION_COUNT") {
        return handleNotificationCommand(request);
    }
    // CHANGE_PASSWORD 与 UPDATE_EXPRESS_NOTE 是多角色共享命令，必须先根据
    // 服务端 Session 中的真实角色分派，不能仅凭主路由的固定顺序决定身份。
    if (request.command == "CHANGE_PASSWORD" || request.command == "UPDATE_EXPRESS_NOTE") {
        Session session;
        if (request.token.empty() || !sessions_.getSession(request.token, session)) {
            Response response;
            response.ok = false;
            response.code = "AUTH_REQUIRED";
            response.message = "请先登录。";
            return response;
        }
        if (session.role == "USER") {
            return handleUserCommand(request);
        }
        if (session.role == "ADMIN") {
            return handleAdminCommand(request);
        }
        if (session.role == "COURIER" && request.command == "CHANGE_PASSWORD") {
            return handleCourierCommand(request);
        }
        system_.auditSecurityEvent(session.role, session.username, "EXPRESS_NOTE_PERMISSION_DENIED",
                                   "command=" + request.command);
        Response response;
        response.ok = false;
        response.code = "PERMISSION_DENIED";
        response.message = "当前身份无权修改快递备注。";
        return response;
    }
    if (request.command == "QUERY_BALANCE" || request.command == "RECHARGE" ||
        request.command == "SEND_EXPRESS" || request.command == "QUERY_MY_EXPRESS" ||
        request.command == "QUERY_WAITING_SIGN" || request.command == "SIGN_EXPRESS" ||
        request.command == "SIGN_BATCH" || request.command == "UPDATE_MY_PROFILE" ||
        request.command == "RATE_EXPRESS") {
        return handleUserCommand(request);
    }
    if (isAdminCommand(request.command)) {
        return handleAdminCommand(request);
    }
    if (isCourierCommand(request.command)) {
        return handleCourierCommand(request);
    }

    Response response;
    response.ok = false;
    response.code = "UNKNOWN_COMMAND";
    response.message = "Unknown command: " + request.command;
    return response;
}

void ServerController::ensureDemoAccounts() {
    // 初始化接口具备重复检查，多次启动服务端不会重复插入同名演示账号。
    system_.registerUser("demo_user", "Demo User", "13800001001", "User1234", "Demo Address");
    system_.registerUser("login_user", "Login User", "13800001003", "User1234", "Login Demo Address");
    system_.registerUser("freeze_user", "Freeze User", "13800001004", "User1234", "Freeze Demo Address");
    const ServiceResult senderResult = system_.registerUser("phase6_sender", "Phase6 Sender", "13800001005", "User1234", "Phase6 Sender Address");
    system_.registerUser("phase6_receiver", "Phase6 Receiver", "13800001006", "User1234", "Phase6 Receiver Address");
    system_.createCourier("demo_courier", "Demo Courier", "13800001002", "Courier1234");
    system_.createCourier("phase6_courier", "Phase6 Courier", "13800001007", "Courier1234");
    if (senderResult.ok) {
        system_.recharge("phase6_sender", 100.0);
        system_.sendExpress("phase6_sender", "phase6_receiver", "Phase6 admin seed package", "Fragile", 1.0);
    }
}

// ===========================================================================
// handleLogin — 三身份登录处理
// ===========================================================================
// 流程：1. 校验参数数量(2) → 2. 调用 system_ 业务登录 → 3. 业务失败返回错误
//       4. hasActiveSession 检查重复登录 → ALREADY_LOGGED_IN
//       5. createSession 创建 token → 6. 返回 LOGIN_SUCCESS + token + role + username
// 重复登录拦截：同一 username+role 已有活跃 session 时拒绝新登录
// 返回格式：Response.records = [token, role, username]
Response ServerController::handleLogin(const Request& request, const std::string& role) {
    if (request.args.size() != 2U) {
        Response response;
        response.ok = false;
        response.code = "PROTOCOL_ERROR";
        response.message = "登录请求需要 username 和 password 两个参数。";
        return response;
    }

    const std::string username = request.args[0];
    const std::string password = request.args[1];
    ServiceResult result;
    if (role == "USER") {
        result = system_.loginUser(username, password);
    } else if (role == "COURIER") {
        result = system_.loginCourier(username, password);
    } else {
        result = system_.loginAdmin(username, password);
    }

    if (!result.ok) {
        return fromServiceResult(result);
    }

    // Duplicate login check: same username+role cannot have multiple active sessions
    if (sessions_.hasActiveSession(username, role)) {
        Response response;
        response.ok = false;
        response.code = "ALREADY_LOGGED_IN";
        response.message = "该账号已在其他客户端登录，请先退出或等待断线清理。";
        return response;
    }

    const std::string token = sessions_.createSession(username, role);
    Response response;
    response.ok = true;
    response.code = "LOGIN_SUCCESS";
    response.message = result.message;
    response.records.push_back(token);
    response.records.push_back(role);
    response.records.push_back(username);
    return response;
}

Response ServerController::handleRegisterUser(const Request& request) {
    if (request.args.size() != 5U) {
        return protocolError("注册请求需要 username、name、phone、password、address 五个参数。");
    }
    return fromServiceResult(system_.registerUser(request.args[0], request.args[1], request.args[2],
                                                  request.args[3], request.args[4]));
}

Response ServerController::handleCheckUsernameAvailable(const Request& request) {
    if (request.args.size() != 1U) {
        return protocolError("CHECK_USERNAME_AVAILABLE 需要 username 参数。");
    }
    return fromServiceResult(system_.checkUsernameAvailable(request.args[0]));
}

Response ServerController::handleCheckPhoneAvailable(const Request& request) {
    if (request.args.size() != 1U) {
        return protocolError("CHECK_PHONE_AVAILABLE 需要 phone 参数。");
    }
    return fromServiceResult(system_.checkPhoneAvailable(request.args[0]));
}

Response ServerController::handleLogout(const Request& request) {
    if (request.token.empty()) {
        Response response;
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "未登录，无需登出。";
        return response;
    }
    Session session;
    if (!sessions_.getSession(request.token, session)) {
        Response response;
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "会话已失效或未登录。";
        return response;
    }
    sessions_.removeSession(request.token);
    Response response;
    response.ok = true;
    response.code = "SUCCESS";
    response.message = "已登出，会话已清理。";
    return response;
}

Response ServerController::handleNotificationCommand(const Request& request) {
    Session session;
    // 通知属于三种登录身份的公共能力，因此这里只校验会话有效性，不限定角色。
    if (request.token.empty() || !sessions_.getSession(request.token, session)) {
        Response response;
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "请先登录。";
        return response;
    }
    sessions_.touch(request.token);

    if (request.command == "QUERY_MY_NOTIFICATIONS") {
        bool unreadOnly = false;
        if (!request.args.empty() && request.args[0] == "1") {
            unreadOnly = true;
        }
        if (request.args.size() > 1U) {
            return protocolError("QUERY_MY_NOTIFICATIONS 最多接受一个参数 unreadOnly(0/1)。");
        }
        return fromServiceResult(system_.queryMyNotifications(session.role, session.username, unreadOnly));
    }
    if (request.command == "MARK_NOTIFICATION_READ") {
        if (request.args.size() != 1U) {
            return protocolError("MARK_NOTIFICATION_READ 需要 notificationId 参数。");
        }
        return fromServiceResult(system_.markNotificationRead(session.role, session.username, request.args[0]));
    }
    if (request.command == "QUERY_UNREAD_NOTIFICATION_COUNT") {
        if (!request.args.empty()) {
            return protocolError("QUERY_UNREAD_NOTIFICATION_COUNT 不需要参数。");
        }
        return fromServiceResult(system_.countUnreadNotifications(session.role, session.username));
    }
    Response response;
    response.ok = false;
    response.code = "UNKNOWN_COMMAND";
    response.message = "Unknown notification command: " + request.command;
    return response;
}

// ============================================================
// User command handler
// ============================================================

Response ServerController::handleUserCommand(const Request& request) {
    Session session;
    Response authResponse;
    if (!requireUserSession(request, session, authResponse)) {
        return authResponse;
    }

    // 路由层只解析协议参数，实体归属、余额和状态机仍由 LogisticsSystem 二次校验。
    if (request.command == "QUERY_BALANCE") {
        if (!request.args.empty()) {
            return protocolError("QUERY_BALANCE 不需要参数。");
        }
        return fromServiceResult(system_.queryBalance(session.username));
    }
    if (request.command == "CHANGE_PASSWORD") {
        return handleChangePasswordForUser(request, session);
    }
    if (request.command == "UPDATE_MY_PROFILE") {
        if (request.args.size() != 3U) {
            return protocolError("UPDATE_MY_PROFILE 需要 name、phone、address 三个参数。");
        }
        return fromServiceResult(system_.updateMyProfile(session.username, request.args[0], request.args[1], request.args[2]));
    }
    if (request.command == "UPDATE_EXPRESS_NOTE") {
        if (request.args.size() != 2U) {
            return protocolError("UPDATE_EXPRESS_NOTE 需要 expressId 和 note 两个参数。");
        }
        return fromServiceResult(system_.updateExpressNote(session.username, session.role, request.args[0], request.args[1]));
    }
    if (request.command == "RATE_EXPRESS") {
        if (request.args.size() != 3U) {
            return protocolError("RATE_EXPRESS 需要 expressId、score、comment 三个参数。");
        }
        int score = 0;
        // 流结束检查保证评分是完整整数，而不是带非法后缀的数值前缀。
        std::istringstream scoreStream(request.args[1]);
        scoreStream >> score;
        if (scoreStream.fail() || !scoreStream.eof() || score < 1 || score > 5) {
            Response response;
            response.ok = false;
            response.code = "INVALID_ARGUMENT";
            response.message = "评分必须为 1 到 5 的整数。";
            return response;
        }
        return fromServiceResult(system_.rateExpress(session.username, request.args[0], score, request.args[2]));
    }
    if (request.command == "RECHARGE") {
        if (request.args.size() != 1U) {
            return protocolError("RECHARGE 需要 amount 参数。");
        }
        double amount = 0.0;
        if (!StringUtil::parseDoubleStrict(request.args[0], amount) || amount <= 0.0) {
            Response response;
            response.ok = false;
            response.code = "INVALID_ARGUMENT";
            response.message = "充值金额必须为正数。";
            return response;
        }
        return fromServiceResult(system_.recharge(session.username, amount));
    }
    if (request.command == "SEND_EXPRESS") {
        if (request.args.size() != 4U && request.args.size() != 5U) {
            return protocolError("SEND_EXPRESS 需要 receiver、description、itemType、itemAmount 四个参数，note 为可选第五参数。");
        }
        double itemAmount = 0.0;
        if (!StringUtil::parseDoubleStrict(request.args[3], itemAmount) || itemAmount <= 0.0) {
            Response response;
            response.ok = false;
            response.code = "INVALID_ARGUMENT";
            response.message = "物品重量或册数必须为正数。";
            return response;
        }
        const std::string note = request.args.size() >= 5U ? request.args[4] : "";
        return fromServiceResult(system_.sendExpress(session.username, request.args[0], request.args[1],
                                                     request.args[2], itemAmount, note));
    }
    if (request.command == "QUERY_MY_EXPRESS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_MY_EXPRESS 不需要参数。");
        }
        return fromServiceResult(system_.queryUserExpresses(session.username));
    }
    if (request.command == "QUERY_WAITING_SIGN") {
        if (!request.args.empty()) {
            return protocolError("QUERY_WAITING_SIGN 不需要参数。");
        }
        return fromServiceResult(system_.queryWaitingSignExpresses(session.username));
    }
    if (request.command == "SIGN_EXPRESS") {
        if (request.args.size() != 1U) {
            return protocolError("SIGN_EXPRESS 需要 expressId 参数。");
        }
        return fromServiceResult(system_.signExpress(session.username, request.args[0]));
    }
    if (request.command == "SIGN_BATCH") {
        if (request.args.empty()) {
            return protocolError("SIGN_BATCH 至少需要一个 expressId 参数。");
        }
        return fromServiceResult(system_.signBatchExpresses(request.args, session.username));
    }
    Response response;
    response.ok = false;
    response.code = "UNKNOWN_COMMAND";
    response.message = "Unknown user command: " + request.command;
    return response;
}

Response ServerController::handleChangePasswordForUser(const Request& request, const Session& session) {
    if (request.args.size() != 2U) {
        return protocolError("CHANGE_PASSWORD 需要 oldPassword 和 newPassword 两个参数。");
    }
    return fromServiceResult(system_.changePassword(session.username, request.args[0], request.args[1]));
}

bool ServerController::requireUserSession(const Request& request, Session& session, Response& response) {
    if (request.token.empty() || !sessions_.getSession(request.token, session)) {
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "请先登录。";
        return false;
    }
    if (session.role != "USER") {
        response.ok = false;
        response.code = "PERMISSION_DENIED";
        response.message = "当前身份无权执行普通用户业务。";
        return false;
    }
    // 仅在鉴权成功后刷新活跃时间，失败请求不会延长无效会话。
    sessions_.touch(request.token);
    return true;
}

// ============================================================
// Courier command handler
// ============================================================

Response ServerController::handleCourierCommand(const Request& request) {
    Session session;
    Response authResponse;
    if (!requireCourierSession(request, session, authResponse)) {
        return authResponse;
    }

    // courierUsername 始终取自会话，不接受客户端自行声明身份。
    if (request.command == "QUERY_MY_PICKUP_TASKS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_MY_PICKUP_TASKS 不需要参数。");
        }
        return fromServiceResult(system_.queryCourierPickupTasks(session.username));
    }
    if (request.command == "PICKUP_EXPRESS") {
        if (request.args.size() != 1U) {
            return protocolError("PICKUP_EXPRESS 需要 expressId 参数。");
        }
        return fromServiceResult(system_.pickupExpress(request.args[0], session.username));
    }
    if (request.command == "PICKUP_BATCH") {
        if (request.args.empty()) {
            return protocolError("PICKUP_BATCH 至少需要一个 expressId 参数。");
        }
        return fromServiceResult(system_.pickupBatchExpresses(request.args, session.username));
    }
    if (request.command == "QUERY_MY_TASKS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_MY_TASKS 不需要参数。");
        }
        return fromServiceResult(system_.queryCourierExpresses(session.username));
    }
    if (request.command == "VIEW_MY_PERFORMANCE") {
        if (!request.args.empty()) {
            return protocolError("VIEW_MY_PERFORMANCE 不需要参数。");
        }
        return fromServiceResult(system_.viewMyCourierPerformance(session.username));
    }
    if (request.command == "CHANGE_PASSWORD") {
        return handleChangePasswordForCourier(request, session);
    }
    if (request.command == "UPDATE_COURIER_PROFILE") {
        if (request.args.size() != 2U) {
            return protocolError("UPDATE_COURIER_PROFILE 需要 name、phone 两个参数。");
        }
        return fromServiceResult(system_.updateCourierProfile(session.username, request.args[0], request.args[1]));
    }

    Response response;
    response.ok = false;
    response.code = "UNKNOWN_COMMAND";
    response.message = "Unknown courier command: " + request.command;
    return response;
}

Response ServerController::handleChangePasswordForCourier(const Request& request, const Session& session) {
    if (request.args.size() != 2U) {
        return protocolError("CHANGE_PASSWORD 需要 oldPassword 和 newPassword 两个参数。");
    }
    return fromServiceResult(system_.changeCourierPassword(session.username, request.args[0], request.args[1]));
}

// ============================================================
// Admin command handler
// ============================================================

Response ServerController::handleAdminCommand(const Request& request) {
    Session session;
    Response authResponse;
    if (!requireAdminSession(request, session, authResponse)) {
        return authResponse;
    }

    // 管理员路由负责参数形状校验，核心服务仍负责目标存在性和状态冲突检查。
    if (request.command == "CREATE_COURIER") {
        if (request.args.size() != 4U) {
            return protocolError("CREATE_COURIER 需要 username、name、phone、password 四个参数。");
        }
        return fromServiceResult(system_.createCourier(request.args[0], request.args[1], request.args[2], request.args[3]));
    }
    if (request.command == "SET_USER_FROZEN") {
        if (request.args.size() != 2U || (request.args[1] != "0" && request.args[1] != "1")) {
            return protocolError("SET_USER_FROZEN 需要 username 和 0/1 参数。");
        }
        return fromServiceResult(system_.setUserFrozen(request.args[0], request.args[1] == "1"));
    }
    if (request.command == "QUERY_USERS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_USERS 不需要参数。");
        }
        return fromServiceResult(system_.queryUsers());
    }
    if (request.command == "QUERY_COURIERS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_COURIERS 不需要参数。");
        }
        return fromServiceResult(system_.queryCouriers());
    }
    if (request.command == "REMOVE_COURIER") {
        if (request.args.size() != 1U) {
            return protocolError("REMOVE_COURIER 需要 courierUsername 参数。");
        }
        return fromServiceResult(system_.removeCourier(request.args[0]));
    }
    if (request.command == "SET_COURIER_FROZEN") {
        if (request.args.size() != 2U || (request.args[1] != "0" && request.args[1] != "1")) {
            return protocolError("SET_COURIER_FROZEN 需要 courierUsername 和 0/1 参数。");
        }
        return fromServiceResult(system_.setCourierFrozen(request.args[0], request.args[1] == "1"));
    }
    if (request.command == "QUERY_ALL_EXPRESS") {
        if (!request.args.empty()) {
            return protocolError("QUERY_ALL_EXPRESS 不需要参数。");
        }
        return fromServiceResult(system_.queryAllExpresses());
    }
    if (request.command == "ASSIGN_COURIER") {
        if (request.args.size() != 2U) {
            return protocolError("ASSIGN_COURIER 需要 expressId 和 courierUsername 参数。");
        }
        return fromServiceResult(system_.assignCourier(request.args[0], request.args[1]));
    }
    if (request.command == "AUTO_ASSIGN_COURIER") {
        if (request.args.size() != 1U) {
            return protocolError("AUTO_ASSIGN_COURIER 需要 expressId 参数。");
        }
        return fromServiceResult(system_.autoAssignCourier(request.args[0]));
    }
    if (request.command == "AUTO_ASSIGN_ALL") {
        if (!request.args.empty()) {
            return protocolError("AUTO_ASSIGN_ALL 不需要参数。");
        }
        return fromServiceResult(system_.autoAssignAllWaitingPickup());
    }
    if (request.command == "VIEW_DASHBOARD") {
        if (!request.args.empty()) {
            return protocolError("VIEW_DASHBOARD 不需要参数。");
        }
        return fromServiceResult(system_.viewDashboard());
    }
    if (request.command == "VIEW_COURIER_PERFORMANCE") {
        if (!request.args.empty()) {
            return protocolError("VIEW_COURIER_PERFORMANCE 不需要参数。");
        }
        return fromServiceResult(system_.viewCourierPerformance());
    }
    if (request.command == "QUERY_LOGS") {
        if (request.args.size() > 4U) {
            return protocolError("QUERY_LOGS 最多接受 4 个筛选参数：actorType、actor、action、result。");
        }
        const std::string filterType = request.args.size() >= 1U ? request.args[0] : "";
        // 缺省筛选项使用空串，Logger 将其解释为“不限制该字段”。
        const std::string filterActor = request.args.size() >= 2U ? request.args[1] : "";
        const std::string filterAction = request.args.size() >= 3U ? request.args[2] : "";
        const std::string filterResult = request.args.size() >= 4U ? request.args[3] : "";
        return fromServiceResult(system_.queryLogs(filterType, filterActor, filterAction, filterResult));
    }
    if (request.command == "VERIFY_LOG_CHAIN") {
        if (!request.args.empty()) {
            return protocolError("VERIFY_LOG_CHAIN 不需要参数。");
        }
        return fromServiceResult(system_.verifyLogHashChain());
    }
    if (request.command == "CHANGE_PASSWORD") {
        return handleChangePasswordForAdmin(request, session);
    }
    if (request.command == "ADMIN_UPDATE_USER") {
        // args: targetUsername name phone address frozen(0/1)
        if (request.args.size() != 5U) {
            return protocolError("ADMIN_UPDATE_USER 需要 targetUsername、name、phone、address、frozen 五个参数。");
        }
        if (request.args[4] != "0" && request.args[4] != "1") {
            return protocolError("ADMIN_UPDATE_USER frozen 参数必须为 0 或 1。");
        }
        return fromServiceResult(system_.adminUpdateUser(request.args[0], request.args[1], request.args[2],
                                                         request.args[3], request.args[4] == "1" ? 1 : 0));
    }
    if (request.command == "ADMIN_UPDATE_COURIER") {
        // args: targetUsername name phone frozen(0/1)
        if (request.args.size() != 4U) {
            return protocolError("ADMIN_UPDATE_COURIER 需要 targetUsername、name、phone、frozen 四个参数。");
        }
        if (request.args[3] != "0" && request.args[3] != "1") {
            return protocolError("ADMIN_UPDATE_COURIER frozen 参数必须为 0 或 1。");
        }
        return fromServiceResult(system_.adminUpdateCourier(request.args[0], request.args[1], request.args[2],
                                                            request.args[3] == "1" ? 1 : 0));
    }
    if (request.command == "REASSIGN_COURIER") {
        // args: expressId newCourierUsername reason
        if (request.args.size() != 3U) {
            return protocolError("REASSIGN_COURIER 需要 expressId、newCourierUsername、reason 三个参数。");
        }
        return fromServiceResult(system_.reassignCourier(request.args[0], request.args[1], request.args[2]));
    }
    if (request.command == "UPDATE_EXPRESS_NOTE") {
        if (request.args.size() != 2U) {
            return protocolError("UPDATE_EXPRESS_NOTE 需要 expressId 和 note 两个参数。");
        }
        return fromServiceResult(system_.updateExpressNote(session.username, session.role, request.args[0], request.args[1]));
    }

    Response response;
    response.ok = false;
    response.code = "UNKNOWN_COMMAND";
    response.message = "Unknown admin command: " + request.command;
    return response;
}

Response ServerController::handleChangePasswordForAdmin(const Request& request, const Session& session) {
    if (request.args.size() != 2U) {
        return protocolError("CHANGE_PASSWORD 需要 oldPassword 和 newPassword 两个参数。");
    }
    return fromServiceResult(system_.changeAdminPassword(session.username, request.args[0], request.args[1]));
}

// ===========================================================================
// requireAdminSession — 管理员权限校验（零信任）
// 校验失败时写入 ADMIN_PERMISSION_DENIED 安全审计日志
// 审计日志记录越权者的 role+username 和试图执行的 command
// ===========================================================================
bool ServerController::requireAdminSession(const Request& request, Session& session, Response& response) {
    if (request.token.empty() || !sessions_.getSession(request.token, session)) {
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "请先以管理员身份登录。";
        return false;
    }
    if (session.role != "ADMIN") {
        system_.auditSecurityEvent(session.role, session.username, "ADMIN_PERMISSION_DENIED",
                                   "command=" + request.command);
        response.ok = false;
        response.code = "PERMISSION_DENIED";
        response.message = "当前身份无权执行管理员业务。";
        return false;
    }
    sessions_.touch(request.token);
    return true;
}

bool ServerController::requireCourierSession(const Request& request, Session& session, Response& response) {
    if (request.token.empty() || !sessions_.getSession(request.token, session)) {
        response.ok = false;
        response.code = "AUTH_REQUIRED";
        response.message = "请先以快递员身份登录。";
        return false;
    }
    if (session.role != "COURIER") {
        // 有效但身份错误的会话属于越权尝试，需写安全审计；无效 Token 无可信主体可记录。
        system_.auditSecurityEvent(session.role, session.username, "COURIER_PERMISSION_DENIED",
                                   "command=" + request.command);
        response.ok = false;
        response.code = "PERMISSION_DENIED";
        response.message = "当前身份无权执行快递员业务。";
        return false;
    }
    sessions_.touch(request.token);
    return true;
}

bool ServerController::isAdminCommand(const std::string& command) {
    // CHANGE_PASSWORD 和 UPDATE_EXPRESS_NOTE 为多角色命令，主路由顺序决定其进入正确处理器。
    return command == "CREATE_COURIER" ||
           command == "SET_USER_FROZEN" ||
           command == "QUERY_USERS" ||
           command == "QUERY_COURIERS" ||
           command == "REMOVE_COURIER" ||
           command == "SET_COURIER_FROZEN" ||
           command == "QUERY_ALL_EXPRESS" ||
           command == "ASSIGN_COURIER" ||
           command == "AUTO_ASSIGN_COURIER" ||
           command == "AUTO_ASSIGN_ALL" ||
           command == "VIEW_DASHBOARD" ||
           command == "VIEW_COURIER_PERFORMANCE" ||
           command == "VERIFY_LOG_CHAIN" ||
           command == "QUERY_LOGS" ||
           command == "CHANGE_PASSWORD" ||
           command == "ADMIN_UPDATE_USER" ||
           command == "ADMIN_UPDATE_COURIER" ||
           command == "REASSIGN_COURIER" ||
           command == "UPDATE_EXPRESS_NOTE";
}

bool ServerController::isCourierCommand(const std::string& command) {
    return command == "QUERY_MY_PICKUP_TASKS" ||
           command == "PICKUP_EXPRESS" ||
           command == "PICKUP_BATCH" ||
           command == "QUERY_MY_TASKS" ||
           command == "VIEW_MY_PERFORMANCE" ||
           command == "CHANGE_PASSWORD" ||
           command == "UPDATE_COURIER_PROFILE";
}

Response ServerController::fromServiceResult(const ServiceResult& result) {
    // 保持服务层 code/message/data 原样映射，网络层不重新解释业务结果。
    Response response;
    response.ok = result.ok;
    response.code = result.code;
    response.message = result.message;
    response.records = result.data;
    return response;
}

Response ServerController::protocolError(const std::string& message) {
    Response response;
    response.ok = false;
    response.code = "PROTOCOL_ERROR";
    response.message = message;
    return response;
}
