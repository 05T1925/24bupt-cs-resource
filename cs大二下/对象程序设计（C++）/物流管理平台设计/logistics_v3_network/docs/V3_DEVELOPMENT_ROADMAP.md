# V3 开发路线图

## Phase 0：独立工程初始化

目标：

- `logistics_v3_network` 与 V1/V2 分离。
- 建立 client/server/common 目录。
- 准备 README、规划文档、测试清单。

验收：

- 目录清晰。
- 文档说明 V3 硬性要求和迁移边界。

## Phase 1：迁移 V2 业务核心

目标：

- 将 V2 的实体、仓储、工具、安全、日志和 `LogisticsSystem` 迁移到 common。
- 暂不引入 socket，先保证服务端业务核心可编译。

任务：

- 拆出模型类。
- 拆出 `ExpressItem` 多态计费体系。
- 拆出仓储和原子保存。
- 拆出日志哈希链。
- 保留 V2 文本数据格式。
- 新增 `ServiceResult` 或继续使用 `bool + message`。

验收：

- common 不依赖 `ConsoleUI`。
- common 不包含 `std::cin` 菜单流程。
- 多态计费、状态机、资金流、权限边界仍存在。

## Phase 2：协议编解码

目标：

- 实现 `Request`、`Response`、`ProtocolCodec`。
- 支持换行分帧和字段转义。

任务：

- `encodeRequest()`。
- `decodeRequest()`。
- `encodeResponse()`。
- `decodeResponse()`。
- 参数数量校验。
- 消息长度限制。

验收：

- 包含 `|`、换行、百分号的文本字段能正确往返。
- 空请求、未知命令、参数缺失能返回协议错误。

## Phase 3：Server 最小网络闭环

目标：

- 服务端启动并监听端口。
- 客户端能连接、发送请求、收到响应。

任务：

- `SocketServer` 初始化 Winsock。
- bind/listen/accept。
- 处理单客户端请求。
- `PING` 命令返回 `PONG`。

验收：

- server 和 client 是不同进程。
- client 通过 socket 收到 `OK|PONG|...`。

## Phase 4：登录与 Session

目标：

- 实现用户、快递员、管理员登录。
- 登录成功返回 token。

任务：

- `SessionManager`。
- `LOGIN_USER`。
- `LOGIN_COURIER`。
- `LOGIN_ADMIN`。
- token 生成和校验。
- 登录失败写日志。

验收：

- 三种身份可通过客户端登录。
- token 无效时业务请求被拒绝。
- 密码错误三次冻结逻辑仍生效。

## Phase 5：用户网络业务

目标：

- 普通用户通过客户端完成寄件、查询、签收。

任务：

- `REGISTER_USER`。
- `QUERY_BALANCE`。
- `RECHARGE`。
- `SEND_EXPRESS`。
- `QUERY_MY_EXPRESS`。
- `QUERY_WAITING_SIGN`。
- `SIGN_EXPRESS`。

验收：

- 寄件费用由服务端根据 `ExpressItem::getPrice()` 计算。
- 用户不能查询或签收他人快递。
- 余额不足、收件人不存在、待揽收签收等错误返回清晰。

## Phase 6：管理员网络业务

目标：

- 管理员通过客户端完成快递员管理、分配和统计。

任务：

- `QUERY_USERS`。
- `SET_USER_FROZEN`。
- `CREATE_COURIER`。
- `REMOVE_COURIER`。
- `SET_COURIER_FROZEN`。
- `QUERY_COURIERS`。
- `QUERY_ALL_EXPRESS`。
- `ASSIGN_COURIER`。
- `AUTO_ASSIGN_COURIER`。
- `AUTO_ASSIGN_ALL`。
- `VIEW_DASHBOARD`。
- `VIEW_COURIER_PERFORMANCE`。

验收：

- 管理员可分配待揽收快递。
- 自动分配策略仍按负载、收入、用户名排序。
- 停用有未完成任务的快递员时返回冲突清单。

## Phase 7：快递员网络业务

目标：

- 快递员通过客户端查看、揽收和查询任务。

任务：

- `QUERY_MY_PICKUP_TASKS`。
- `PICKUP_EXPRESS`。
- `PICKUP_BATCH`。
- `QUERY_MY_TASKS`。
- `VIEW_MY_PERFORMANCE`。

验收：

- 快递员只能看到本人任务。
- 揽收后状态变为待签收。
- 管理员账户向快递员账户转 50%。
- 重复揽收和揽收他人任务被拒绝。

## Phase 8：多客户端并发与异常

目标：

- 支持多个客户端同时连接。
- 修改型请求具备基本并发安全。

任务：

- 每连接一个线程。
- `std::mutex systemMutex`。
- 客户端断开处理。
- 半包/粘包测试。
- 并发重复揽收/签收冲突测试。

验收：

- 两个 client 同时操作同一快递，只能成功一次。
- server 不崩溃。
- 日志记录冲突和拒绝原因。

## Phase 9：验收材料与报告支撑

目标：

- 为实验报告和助教演示准备证据。

任务：

- 更新 README。
- 完成 `tests/V3_TEST_CASES.md`。
- 记录 `LOG/v3_LOG01.md`。
- 准备演示数据和演示脚本。

验收：

- 能按固定脚本演示完整 C/S 业务链路。
- 能解释关键类、socket 协议、会话安全、并发处理和加分点。

