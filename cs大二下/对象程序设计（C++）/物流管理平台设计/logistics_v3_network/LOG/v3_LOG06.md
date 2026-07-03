# v3_LOG06：Phase 5 用户网络业务完成记录

## 1. 本次目标

在 Phase 4 token 会话基础上，把普通用户核心业务通过 socket 协议暴露出来：

```text
REGISTER_USER
QUERY_BALANCE
RECHARGE
SEND_EXPRESS
QUERY_MY_EXPRESS
QUERY_WAITING_SIGN
SIGN_EXPRESS
```

## 2. 零信任路由

`REGISTER_USER` 不需要 token。

以下命令必须携带 token：

```text
QUERY_BALANCE
RECHARGE
SEND_EXPRESS
QUERY_MY_EXPRESS
QUERY_WAITING_SIGN
SIGN_EXPRESS
```

服务端执行：

```text
SessionManager::getSession(token)
检查 session.role == USER
使用 session.username 调用 LogisticsSystem
```

服务端不信任客户端传来的当前用户名，普通用户业务请求中也不再传当前用户名。

## 3. 服务端路由落点

文件：

```text
server/ServerController.cpp
```

新增：

```text
handleRegisterUser
handleUserCommand
requireUserSession
```

金额和物品数量使用 `StringUtil::parseDoubleStrict` 严格解析。

## 4. 客户端业务菜单

文件：

```text
client/main.cpp
```

新增：

```text
注册普通用户
登录后用户菜单
查询余额
充值
寄件
查询我的快递
查询待签收
签收快递
TablePrinter 简易表格渲染
--selftest-user 自动联调脚本
```

客户端只收集基础参数并自动附带 token，不计算快递费用。

## 5. 已验证联调

命令：

```text
build_all.bat
bin/logistics_v3_server.exe --once
bin/logistics_v3_client.exe --selftest-user
```

自动联调流程：

```text
新用户注册
收件用户注册
发件用户登录
充值 100
寄送 Fragile 2kg
使用发件人 token 恶意签收该快递
查询我的快递
```

关键结果：

```text
Fragile 2kg fee = 16.00
remaining balance = 84.00
malicious SIGN_EXPRESS -> PERMISSION_DENIED
query my express -> SUCCESS
```

持久化核验：

```text
expresses.txt:
EX000001|sender|receiver||...|0|Fragile|2.00|Fragile selftest package|16.00

users.txt:
sender balance = 84.00
```

PING 回归：

```text
OK|PONG|Server is alive
```

## 6. Phase 0-4 回归检查

- Phase 0：双进程构建仍通过。
- Phase 1：common 业务核心仍独立，无 UI 污染。
- Phase 2：ProtocolCodec 仍用于所有请求/响应。
- Phase 3：PING/PONG 回归通过。
- Phase 4：登录 token 仍可用，用户业务复用 session.username。

## 7. 下一阶段入口

Phase 6 建议接入管理员网络业务：

```text
CREATE_COURIER
QUERY_COURIERS
ASSIGN_COURIER
AUTO_ASSIGN_COURIER
QUERY_ALL_EXPRESS
VIEW_DASHBOARD
```

