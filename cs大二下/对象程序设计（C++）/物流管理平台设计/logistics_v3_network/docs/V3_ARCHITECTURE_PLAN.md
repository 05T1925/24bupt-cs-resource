# V3 架构设计规划

## 1. 总体架构

V3 使用三层工程结构：

```text
client  ->  socket protocol  ->  server  ->  common business core
```

推荐逻辑分层：

```text
common/models     实体模型和值对象
common/storage    文本持久化、原子保存、日志
common/security   密码哈希、输入校验、会话 token 工具
common/service    LogisticsSystem 业务服务
common/protocol   Request/Response、协议编解码
server            SocketServer、SessionManager、ServerController
client            SocketClient、ClientApp、ConsoleClientUI
```

## 2. 关键类规划

### 2.1 ProtocolCodec

职责：

- 将一行文本解析成 `Request`。
- 将 `Response` 编码成一行文本。
- 对字段执行 `escape/unescape`。
- 校验命令名、字段数量、消息长度。

建议结构：

```cpp
class Request {
public:
    std::string command;
    std::string token;
    std::vector<std::string> args;
};

class Response {
public:
    bool ok;
    std::string code;
    std::string message;
    std::vector<std::string> records;
};
```

### 2.2 SessionManager

职责：

- 登录成功后生成 token。
- 保存 token 到 Session 的映射。
- 根据 token 返回当前角色和账号。
- 支持退出登录、过期清理。

建议结构：

```cpp
class Session {
public:
    std::string token;
    std::string role;
    std::string username;
    std::string loginTime;
    std::string clientAddress;
};
```

安全原则：

```text
业务身份只从 Session 读取。
客户端参数中的 username 只能作为查询条件，不能作为当前操作者身份。
```

### 2.3 ServerController

职责：

- 接收 `Request`。
- 校验 token。
- 校验角色权限。
- 转换参数类型。
- 调用 `LogisticsSystem`。
- 返回结构化 `Response`。

权限矩阵：

```text
GUEST:
  REGISTER_USER
  LOGIN_USER
  LOGIN_COURIER
  LOGIN_ADMIN

USER:
  CHANGE_PASSWORD
  QUERY_BALANCE
  RECHARGE
  SEND_EXPRESS
  QUERY_MY_EXPRESS
  QUERY_WAITING_SIGN
  SIGN_EXPRESS

COURIER:
  QUERY_MY_PICKUP_TASKS
  PICKUP_EXPRESS
  PICKUP_BATCH
  QUERY_MY_TASKS
  VIEW_MY_PERFORMANCE

ADMIN:
  QUERY_USERS
  SET_USER_FROZEN
  CREATE_COURIER
  REMOVE_COURIER
  SET_COURIER_FROZEN
  QUERY_COURIERS
  ASSIGN_COURIER
  AUTO_ASSIGN_COURIER
  AUTO_ASSIGN_ALL
  QUERY_ALL_EXPRESS
  QUERY_LOGS
  VERIFY_LOG_CHAIN
  VIEW_DASHBOARD
  VIEW_COURIER_PERFORMANCE
```

### 2.4 SocketServer

职责：

- 初始化 Winsock。
- bind/listen/accept。
- 为每个客户端连接创建处理循环。
- 维护每个连接的接收缓冲区。
- 按换行切分请求。
- 调用 `ServerController`。
- 发送响应。

第一版可以单线程顺序处理客户端；正式版建议每个连接一个线程，并用互斥锁保护业务核心。

### 2.5 SocketClient

职责：

- 初始化 Winsock。
- 连接 server。
- 发送一条请求并等待响应。
- 处理服务端断开和超时。

客户端只负责网络 I/O，不包含业务规则。

### 2.6 ConsoleClientUI / ClientApp

职责：

- 菜单展示。
- 输入合法性初筛。
- 密码隐藏输入。
- 调用 `SocketClient` 发送命令。
- 将 `Response.records` 打印成表格。

客户端可以做输入体验优化，但所有关键校验服务端必须再做一遍。

## 3. V2 迁移策略

### 3.1 先迁移业务核心

从 `logistics_v2/src/main.cpp` 中拆出：

- 工具类。
- 实体类。
- 仓储类。
- 业务服务类。
- 日志和认证状态。

暂时不改变文件协议，降低迁移风险。

### 3.2 再新增网络壳

网络壳包括：

- `ProtocolCodec`
- `SocketServer`
- `SocketClient`
- `SessionManager`
- `ServerController`

### 3.3 最后优化多文件结构

如果时间不足，可以保持少量大文件，但逻辑上必须清楚分离 client/server/common。若时间充足，建议每个核心类拆 `.h/.cpp`，便于报告说明。

## 4. 服务端禁止项

服务端不得包含：

- 控制台菜单。
- `std::cin`。
- 业务表格打印。
- 密码隐藏输入。
- 用户交互暂停。
- 客户端专用 UI 文案分支。

服务端只返回结构化响应，客户端决定如何展示。

## 5. 数据一致性设计

第一版：

```text
所有请求进入 ServerController 后统一加锁。
读请求和写请求都使用同一把锁。
```

原因：

- 实现简单。
- 避免多个客户端同时修改同一文件。
- 避免管理员分配和快递员揽收同时发生状态冲突。

增强版：

- 引入读写锁。
- 对日志、用户、快递、快递员仓储分锁。
- 增加跨文件事务保存。

## 6. 网络日志设计

保留 V2 的业务日志 `operations.log`，同时可新增：

```text
network_operations.log
```

字段建议：

```text
seq|time|clientAddress|actorType|actor|command|result|detail|prevHash|currentHash
```

注意：

- 密码永不入日志。
- token 只记录 hash 前缀或不记录。
- 协议错误也应写网络日志，方便演示错误场景。

