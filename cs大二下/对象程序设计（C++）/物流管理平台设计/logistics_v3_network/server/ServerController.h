// =============================================================================
// ServerController.h — 服务端命令路由与权限控制器
// =============================================================================
// 文件用途：接收解析后的 Request，校验权限，路由到 LogisticsSystem 业务方法。
// 所属模块：server（服务端网络层，调用 common/service 业务层）
// 作者：Logistics V3 Network Team
// 日期：2026-06-10（Phase 9 终态验收版）
//
// 命令路由架构：
//   handle() 主入口 → 按 command 分发到 4 个处理器
//     ├── GUEST 命令：PING / REGISTER_USER / LOGIN_* / CHECK_* / LOGOUT
//     ├── handleUserCommand：USER 角色命令（需 requireUserSession）
//     ├── handleAdminCommand：ADMIN 角色命令（需 requireAdminSession + 审计日志）
//     ├── handleCourierCommand：COURIER 角色命令（需 requireCourierSession + 审计日志）
//     └── handleNotificationCommand：通知中心命令（需任一有效 session）
//
// 零信任架构：
//   - 业务身份严格从 SessionManager::getSession(token) 解析
//   - 客户端传来的 username 只能作为操作目标或查询条件
//   - 越权访问写入安全审计日志（ADMIN_PERMISSION_DENIED / COURIER_PERMISSION_DENIED）
//
// 权限校验方法：
//   requireUserSession   → session.role == "USER"
//   requireAdminSession  → session.role == "ADMIN"（非 ADMIN 记录审计日志）
//   requireCourierSession → session.role == "COURIER"（非 COURIER 记录审计日志）
//   touch 刷新 session.lastActiveTime
//
// 命令分类方法：
//   isAdminCommand(command)   → 19 个管理员命令
//   isCourierCommand(command)  → 7 个快递员命令
//   其余 USER 命令在 handle() 中直接匹配
//
// 与 SocketServer 的边界：
//   SocketServer 只保证拿到一个格式合法的 Request。
//   ServerController 决定命令属于哪个角色、参数个数是否正确。
//   真正的余额、订单状态和对象归属仍由 LogisticsSystem 判断。
// =============================================================================

#ifndef LOGISTICS_V3_SERVER_SERVER_CONTROLLER_H
#define LOGISTICS_V3_SERVER_SERVER_CONTROLLER_H

#include "SessionManager.h"
#include "../common/protocol/ProtocolCodec.h"
#include "../common/service/LogisticsSystem.h"

class ServerController {
public:
    // 构造函数：绑定业务系统和会话管理器
    ServerController(LogisticsSystem& system, SessionManager& sessions);

    // handle：主路由入口 — 根据 Request.command 分发到对应的处理器
    // 返回值：结构化 Response（由 SocketServer 编码为 RES 帧发送）
    Response handle(const Request& request);

    // closeSession：清理指定 token 的会话（客户端断线时由 SocketServer 调用）
    void closeSession(const std::string& token);

    // ensureDemoAccounts：确保演示种子数据存在（幂等操作）
    // 创建 6 个用户 + 2 个快递员 + 1 个种子快递
    void ensureDemoAccounts();

private:
    // 引用意味着对象由 server/main.cpp 创建并保证生命周期，本类不负责 delete。
    LogisticsSystem& system_;       // 业务核心引用
    SessionManager& sessions_;      // 会话管理器引用

    // --- GUEST 命令处理器 ---
    Response handleLogin(const Request& request, const std::string& role);  // 三身份登录（含重复登录拦截）
    Response handleRegisterUser(const Request& request);
    Response handleLogout(const Request& request);

    // --- 通知中心处理器（三身份通用） ---
    Response handleNotificationCommand(const Request& request);

    // --- 即时查重/校验处理器（GUEST 命令） ---
    Response handleCheckUsernameAvailable(const Request& request);
    Response handleCheckPhoneAvailable(const Request& request);

    // --- 三身份命令处理器 ---
    Response handleUserCommand(const Request& request);     // USER 角色命令（8个）
    Response handleAdminCommand(const Request& request);    // ADMIN 角色命令（19个）
    Response handleCourierCommand(const Request& request);  // COURIER 角色命令（7个）

    // --- 密码修改处理器（每个身份独立的密码处理方法） ---
    Response handleChangePasswordForUser(const Request& request, const Session& session);
    Response handleChangePasswordForCourier(const Request& request, const Session& session);
    Response handleChangePasswordForAdmin(const Request& request, const Session& session);

    // --- 权限校验（零信任实现） ---
    // 三个 require 方法从 token 解析 session，校验 role 匹配
    // 失败时填充 response 并返回 false（含审计日志写入）
    bool requireUserSession(const Request& request, Session& session, Response& response);
    bool requireAdminSession(const Request& request, Session& session, Response& response);
    bool requireCourierSession(const Request& request, Session& session, Response& response);

    // --- 命令分类 ---
    static bool isAdminCommand(const std::string& command);   // 判断是否为管理员命令
    static bool isCourierCommand(const std::string& command); // 判断是否为快递员命令

    // --- 响应转换 ---
    // fromServiceResult：将 ServiceResult 转换为 Response（records ← data）
    static Response fromServiceResult(const ServiceResult& result);
    // protocolError：生成协议错误响应（code=PROTOCOL_ERROR）
    static Response protocolError(const std::string& message);
};

#endif
