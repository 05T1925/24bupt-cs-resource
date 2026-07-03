# v3_LOG07：Phase 6 管理员网络业务完成记录

## 1. 本次目标

在 Phase 5 普通用户网络业务基础上，接入管理员后台能力：

```text
QUERY_USERS
CREATE_COURIER
QUERY_COURIERS
REMOVE_COURIER
SET_COURIER_FROZEN
QUERY_ALL_EXPRESS
ASSIGN_COURIER
AUTO_ASSIGN_COURIER
AUTO_ASSIGN_ALL
VIEW_DASHBOARD
VIEW_COURIER_PERFORMANCE
VERIFY_LOG_CHAIN
```

## 2. 零信任管理员鉴权

所有管理员命令统一进入：

```text
ServerController::handleAdminCommand
ServerController::requireAdminSession
```

鉴权规则：

```text
SessionManager::getSession(token)
session.role == ADMIN
```

如果普通用户或快递员 token 调用管理员命令：

```text
ERR|PERMISSION_DENIED|当前身份无权执行管理员业务
```

同时写入安全审计日志：

```text
ADMIN_PERMISSION_DENIED
```

## 3. 服务层新增能力

`LogisticsSystem` 新增：

```text
queryUsers
queryCouriers
removeCourier
setCourierFrozen
autoAssignCourier
autoAssignAllWaitingPickup
viewDashboard
viewCourierPerformance
auditSecurityEvent
```

自动分配策略：

```text
1. 跳过 frozen / removed 快递员
2. 未完成任务数少者优先
3. 收入低者优先
4. 用户名字典序优先
```

停用快递员冲突：

```text
若快递员名下存在 WaitingPickup / WaitingSign 任务
返回 ERR|COURIER_HAS_UNFINISHED_TASKS
records 携带冲突任务清单
```

## 4. 客户端后台菜单

`client/main.cpp` 新增管理员菜单：

```text
查看用户
查看快递员
新增快递员
停用快递员
分配快递员
一键自动分配
查询全部快递
统计看板
快递员绩效
```

查询类响应统一使用 `TablePrinter::printRecords` 展示。

## 5. 自动联调

命令：

```text
build_all.bat
bin/logistics_v3_server.exe --once
bin/logistics_v3_client.exe --selftest-admin
```

自测流程：

```text
普通用户 token 调管理员看板 -> PERMISSION_DENIED
管理员登录
查询全部快递
分配快递给 phase6_courier
停用 phase6_courier -> 返回冲突任务清单
AUTO_ASSIGN_ALL
VIEW_DASHBOARD
VIEW_COURIER_PERFORMANCE
```

关键结果：

```text
remove courier conflict -> ERR|COURIER_HAS_UNFINISHED_TASKS
records 中包含冲突任务
auto assign all -> SUCCESS
dashboard -> SUCCESS
performance -> SUCCESS
```

日志核验：

```text
ADMIN_PERMISSION_DENIED
ASSIGN_COURIER
REMOVE_COURIER DENIED
AUTO_ASSIGN_ALL
```

## 6. Phase 0-5 回归检查

- Phase 0：`build_all.bat` 仍可生成 server/client 双进程。
- Phase 1：`common` 无 `iostream/std::cin/std::cout/ConsoleUI`。
- Phase 2：协议转义与 records 仍统一走 `ProtocolCodec`。
- Phase 3：PING/PONG 回归通过。
- Phase 4：Session token 仍是所有权限判断入口。
- Phase 5：用户业务仍保留，管理员业务没有绕过用户权限边界。

