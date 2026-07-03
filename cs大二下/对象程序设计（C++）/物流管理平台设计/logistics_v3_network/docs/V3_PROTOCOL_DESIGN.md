# V3 Socket 协议设计

## 1. 协议目标

V3 协议用于连接控制台客户端和服务端，不使用 RPC 框架。协议必须足够简单，便于 C++ 手写解析，也必须能处理半包、粘包、非法输入和字段转义。

## 2. 消息边界

每条请求和响应以 `\n` 结尾：

```text
COMMAND|token|arg1|arg2\n
```

服务端和客户端都维护接收缓冲区：

```text
recv bytes
append buffer
while buffer contains '\n':
  extract one line
  decode line
```

不能假设一次 `recv()` 等于一条完整消息。

## 3. 字段转义

沿用 V2 文件协议思路：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

所有文本字段发送前转义，接收后反转义。

## 4. 请求格式

### 4.1 未登录请求

```text
REGISTER_USER||username|name|phone|password|address
LOGIN_USER||username|password
LOGIN_COURIER||username|password
LOGIN_ADMIN||username|password
```

第二个字段固定为 token。未登录时 token 为空。

### 4.2 普通用户请求

```text
REGISTER_USER||username|name|phone|password|address
CHANGE_PASSWORD|token|oldPassword|newPassword
QUERY_BALANCE|token
RECHARGE|token|amount
SEND_EXPRESS|token|receiver|description|itemType|itemAmount
QUERY_MY_EXPRESS|token|sender|receiver|expressId|startTime|endTime|status|itemType
QUERY_WAITING_SIGN|token
SIGN_EXPRESS|token|expressId
SIGN_BATCH|token|expressId1,expressId2,expressId3
```

### 4.3 快递员请求

```text
QUERY_MY_PICKUP_TASKS|token
PICKUP_EXPRESS|token|expressId
PICKUP_BATCH|token|expressId1,expressId2,expressId3
QUERY_MY_TASKS|token|sender|receiver|expressId|startTime|endTime|status|itemType
VIEW_MY_PERFORMANCE|token
```

### 4.4 管理员请求

```text
QUERY_USERS|token
QUERY_USERS|token
SET_USER_FROZEN|token|username|0/1
CREATE_COURIER|token|username|name|phone|password
REMOVE_COURIER|token|courierUsername
SET_COURIER_FROZEN|token|courierUsername|0/1
QUERY_COURIERS|token
QUERY_ALL_EXPRESS|token|sender|receiver|courier|expressId|startTime|endTime|status|itemType
ASSIGN_COURIER|token|expressId|courierUsername
AUTO_ASSIGN_COURIER|token|expressId
AUTO_ASSIGN_ALL|token
QUERY_LOGS|token|actorType|actor|action|result|startTime|endTime
VERIFY_LOG_CHAIN|token
VIEW_DASHBOARD|token
VIEW_COURIER_PERFORMANCE|token
```

## 5. 响应格式

### 5.1 当前 Phase 2 实际帧格式

请求：

```text
REQ|command|token|argCount|arg1|arg2|...\n
```

响应：

```text
RES|1/0|code|message|recordCount|record1|record2|...\n
```

`argCount` 和 `recordCount` 用来检测参数缺失、多余字段和畸形帧。

### 5.2 逻辑成功响应

```text
OK|code|message
```

示例：

```text
OK|SEND_SUCCESS|寄件成功，快递单号：EX202605270001
```

### 5.3 逻辑失败响应

```text
ERR|code|message
```

示例：

```text
ERR|BALANCE_NOT_ENOUGH|余额不足，当前余额无法支付本次快递费用
```

### 5.4 登录成功

```text
OK|LOGIN_SUCCESS|token|role|username|message
```

客户端保存 token，后续请求都携带 token。

### 5.5 数据响应

```text
DATA|type|count|record1|record2|...
```

record 内部推荐继续使用转义后的字段串，例如：

```text
DATA|EXPRESS|2|id=EX1;sender=u1;receiver=u2;status=WaitingPickup|id=EX2;sender=u1;receiver=u3;status=Signed
```

第一版也可直接复用 `|` 分隔 record，但必须经过二级转义，避免解析歧义。

## 6. 错误码

推荐错误码：

```text
OK
PROTOCOL_ERROR
UNKNOWN_COMMAND
INVALID_ARGUMENT
AUTH_REQUIRED
AUTH_FAILED
TOKEN_EXPIRED
PERMISSION_DENIED
ACCOUNT_FROZEN
NOT_FOUND
DUPLICATE
BALANCE_NOT_ENOUGH
STATE_CONFLICT
STORAGE_FAILED
SERVER_ERROR
```

## 7. 安全限制

建议限制：

```text
单条消息最大长度：8192 字节
单字段最大长度：512 字节
单次 DATA 记录数：分页返回，默认 20 条
token 长度：至少 32 字节随机文本
token 过期：可设置 2 小时，演示版可只支持手动退出
```

## 8. 命令处理原则

- 登录和注册属于 GUEST 命令。
- 修改型命令必须加锁。
- 查询型命令第一版也建议加锁。
- 服务端对所有字段再次校验。
- 客户端不能提交 `fee`、`currentUsername`、`role` 这类敏感结果字段。
- 所有权限判断在 `ServerController` 和 `LogisticsSystem` 双层兜底。
