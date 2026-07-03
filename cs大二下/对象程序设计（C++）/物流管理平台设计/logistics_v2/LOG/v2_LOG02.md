# v2_LOG02：Phase 6 验收加分复盘、V2 后续优化与 V3 启动任务日志

## 0. 日志定位

本日志用于承接 `v2_LOG00.md` 和 `v2_LOG01.md`，记录 `logistics_v2` 在完成 Phase 0-6 后的真实能力、助教验收标记对应情况、仍需补强的功能点，以及面向 `logistics_v3_network` 网络版的初步架构准备。

当前项目路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v2
```

当前核心源码：

```text
src/main.cpp
```

当前日志目录：

```text
LOG/
```

当前阶段目标：

```text
在不破坏 V2 单机版稳定业务闭环的前提下，补齐助教关注的功能检查与异常处理加分点；
同时把可复用的业务层、安全层、持久化层能力沉淀为 V3 传统 C/S 网络版的服务端基础。
```

---

## 1. 当前已完成能力总览

### 1.1 Phase 0-5 核心业务闭环

当前 V2 已完成实验二的主体要求：

- 独立 `logistics_v2` 程序与数据目录，未混用 V1 数据。
- 主菜单支持三种身份：普通用户、快递员、管理员。
- 用户注册、登录、改密、充值、寄件、签收、查询。
- 管理员登录、查看用户、查询全部快递、冻结/解冻用户、查看日志。
- 快递员实体 `Courier` 与 `CourierRepository` 已落地。
- 管理员可新增、停用、冻结/解冻快递员。
- 快递员可登录、查看待揽收任务、单个/批量/全部揽收、查询本人任务、查看收入。
- 快递状态扩展为：

```text
待揽收 -> 待签收 -> 已签收
```

- 用户寄件后进入待揽收，不再直接待签收。
- 管理员为待揽收快递分配快递员。
- 快递员只能揽收自己名下待揽收任务。
- 揽收后快递状态进入待签收，并记录揽收时间。
- 收件用户只能签收待签收且属于自己的快递。
- 普通用户、快递员、管理员三类查询权限已在业务层隔离。

### 1.2 多态计费体系

当前已实现物品基类与子类：

```cpp
class ExpressItem;
class NormalItem;
class FragileItem;
class BookItem;
```

计费规则：

```text
普通快递：5 元/kg
易碎品：8 元/kg
图书：2 元/本
```

核心亮点：

- `ExpressItem::getPrice()` 是虚函数。
- 寄件流程中通过 `ExpressItemFactory` 创建子类对象。
- 业务层实际调用 `item->getPrice()` 参与费用计算。
- 费用会影响用户扣款、管理员入账、快递员 50% 提成。

验收解释重点：

```text
多态不是只定义类，而是实际进入了寄件扣费和资金流转。
```

### 1.3 权限隔离与异常处理

当前已实现的权限与异常防线：

- 用户只能查询自己发出或接收的快递。
- 用户不能签收他人快递。
- 用户不能签收待揽收快递。
- 快递员只能查询自己名下任务。
- 快递员不能揽收其他快递员任务。
- 快递员不能重复揽收已揽收任务。
- 管理员分配快递员时检查快递状态、快递员存在性、冻结状态、停用状态。
- 停用快递员时，如果仍有未完成任务，会拒绝停用。
- 保存失败会向上返回失败，并尽量回滚内存状态。
- 数据文件异常行会跳过并写日志。

当前明确写入 `DENIED` 的场景：

```text
普通用户查询无关快递
普通用户签收他人快递
普通用户签收待揽收快递
快递员查询他人任务
快递员揽收他人任务
```

### 1.4 密码保护与认证安全

当前密码安全能力：

- 用户、管理员、快递员密码均采用 salt + SHA-256 哈希保存。
- 数据文件中不出现明文密码。
- 密码规则要求长度 6-30 位，必须同时包含字母和数字，不允许空白字符。
- 修改密码时必须输入旧密码。
- 修改密码时新旧密码不能一致。
- 登录失败会写日志。
- 用户和快递员连续 3 次密码错误后自动冻结。

当前认证状态文件：

```text
data/auth_state.txt
```

格式：

```text
role|username|failedCount|lastFailedTime
```

设计价值：

- 不污染 `users.txt` 和 `couriers.txt` 的实体格式。
- 为 V3 服务端统一认证状态管理做准备。
- 认证失败次数可跨程序重启保留。

### 1.5 Phase 6 工程增强

当前已完成的 Phase 6 能力：

- `StorageManager` 原子化保存。
- 核心数据文件保存从直接覆盖升级为：

```text
file.tmp -> file.bak -> file
```

- 日志升级为 9 字段哈希链格式：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

- `Logger::verifyHashChain()` 可校验日志完整性并定位断链行。
- 管理员菜单已接入“校验日志完整性”。
- 新增 `TablePrinter::displayWidth()`，用于中英文混合表格宽度估算。
- 快递表格、用户表格、快递员表格、日志表格已改为宽度感知输出。
- 管理员菜单新增“任务统计看板”。

当前统计看板包含：

```text
平台净收入
快递员支出
待揽收数量
待签收数量
已签收数量
普通快递数量与费用
易碎品数量与费用
图书数量与费用
```

---

## 2. 助教标记逐项对照

### 2.1 功能检查

#### 有快递员的增删功能

状态：已具备。

当前实现：

- 管理员新增快递员。
- 管理员删除/停用快递员。
- 停用采用逻辑删除，保留历史任务和审计关系。
- 若快递员存在未完成任务，当前已拒绝停用。

后续可增强：

- 停用失败时列出未完成任务单号。
- 增加“转移该快递员未完成任务”流程。

#### 分配快递员出错的处理机制

状态：已具备。

当前覆盖：

- 快递单号不存在。
- 快递不是待揽收状态。
- 快递员不存在。
- 快递员已冻结。
- 快递员已停用。
- 保存失败回滚分配状态。

后续可增强：

- 自动推荐可用快递员。
- 分配失败时给出可选快递员列表。

#### 快递员管理功能实现很好

状态：主体已具备。

当前能力：

- 查看快递员列表。
- 新增快递员。
- 停用快递员。
- 冻结/解冻快递员。
- 快递员登录。
- 快递员收入持久化。
- 快递员任务查询。

后续可增强：

- 快递员详情页。
- 快递员绩效排行。
- 快递员任务重分配。
- 快递员离职原因记录。

#### 额外增加快递员绩效功能

状态：部分具备，仍需补强。

当前已有：

- 快递员收入字段。
- 快递员可查看个人收入。
- 管理员统计看板包含快递员总支出。

当前缺少：

- 按快递员维度统计任务数。
- 每个快递员待揽收、待签收、已完成数量。
- 每个快递员累计提成、平均单价、完成率。
- 快递员绩效排行。

建议补强为：

```text
管理员 -> 快递员绩效看板
  用户名
  姓名
  待揽收数
  待签收数
  已完成数
  总任务数
  完成率
  累计收入
```

#### 管理员功能做得很完备

状态：基本具备。

当前能力：

- 查看用户。
- 查询全部快递。
- 冻结/解冻用户。
- 查看日志。
- 管理快递员。
- 分配快递员。
- 查看任务统计。
- 校验日志完整性。

后续可增强：

- 自动分配快递员。
- 快递员绩效排行。
- 账号安全中心。
- 数据完整性检查中心。
- 异常操作审计中心。

#### 实现快递自动分配

状态：缺少。

当前只有手动分配。

建议实现：

```text
管理员选择单个待揽收快递 -> 自动分配
管理员一键自动分配全部待揽收快递
```

推荐分配策略：

```text
候选快递员 = 未停用 && 未冻结
优先级 = 未完成任务数少 -> 已完成任务数少 -> 收入低 -> 用户名字典序
```

解释亮点：

```text
自动分配不是随机，而是基于负载均衡和公平性。
```

#### 各类输入检查完整

状态：大部分具备。

当前覆盖：

- 用户名。
- 姓名。
- 电话。
- 地址。
- 密码。
- 金额。
- 菜单选项。
- 快递单号。
- 查询时间。
- 状态筛选。
- 日志筛选。
- 快递重量。
- 图书册数。

后续可增强：

- 输入超长文本截断或拒绝。
- 网络版协议字段长度限制。
- 所有外部输入统一返回错误码。

#### 密码输入做了优化，密码做了保护

状态：保护已具备，输入优化仍可增强。

已具备：

- salt + SHA-256。
- 不保存明文。
- 密码复杂度校验。
- 三次失败冻结。

待增强：

- 控制台输入密码时隐藏明文或显示 `*`。

建议实现：

```text
Windows 使用 _getch() 读取密码字符
退格键可删除
普通字符显示 *
回车结束输入
```

#### 修改密码时，新旧密码不能一致

状态：已具备。

当前业务层已经拦截：

```text
oldPassword == newPassword
```

---

## 3. 当前缺少或偏弱功能清单

以下内容建议作为 V2 下一轮优化优先级。

### P0：应优先补齐

1. 自动分配快递员。
2. 快递员绩效看板。
3. 快递员任务详情页。
4. 密码输入隐藏。
5. 停用快递员失败时列出未完成任务。

### P1：增强验收说服力

1. 自动分配失败原因细分。
2. 自动分配批处理结果展示。
3. 管理员账号安全中心。
4. 日志完整性校验结果写入审计日志。
5. 数据文件完整性检查页面。
6. 统计看板增加快递员排行和物品类型收入占比。

### P2：工程质量升级

1. 跨文件事务保存。
2. 查询接口返回结构化状态码。
3. 将单文件 `main.cpp` 拆分为多文件模块。
4. 增加自动化测试脚本。
5. 文档同步更新 Phase 6 状态。

---

## 4. V2 后续优化路线

### 4.1 自动分配快递员

目标：

```text
让管理员不必手动输入快递员用户名，也能把待揽收任务分配给合适快递员。
```

建议新增业务接口：

```cpp
bool autoAssignCourier(const std::string& expressId, std::string& assignedCourier, std::string& message);
bool autoAssignAllWaitingPickup(std::vector<std::string>& messages);
```

候选规则：

```text
快递员未停用
快递员未冻结
```

排序规则：

```text
1. 未完成任务数更少
2. 待揽收任务数更少
3. 累计收入更低
4. 用户名字典序更小
```

异常处理：

- 无可用快递员。
- 快递不存在。
- 快递不是待揽收状态。
- 保存失败。
- 批量自动分配中部分成功、部分失败。

验收讲解：

```text
自动分配体现任务调度和负载均衡，不只是简单菜单功能。
```

### 4.2 快递员绩效看板

目标：

```text
将快递员管理从“账号管理”提升为“运营绩效管理”。
```

建议新增结构：

```cpp
struct CourierPerformance {
    std::string username;
    std::string name;
    int waitingPickupCount;
    int waitingSignCount;
    int signedCount;
    int totalTaskCount;
    double income;
    double completionRate;
};
```

管理员菜单新增：

```text
查看快递员绩效
```

快递员个人菜单新增：

```text
查看我的绩效
```

统计字段：

- 待揽收任务数。
- 待签收任务数。
- 已完成任务数。
- 总任务数。
- 完成率。
- 累计收入。
- 平均单票提成。

异常处理：

- 没有快递员。
- 快递员无任务。
- 停用快递员是否参与历史绩效统计，需要明确说明。

### 4.3 密码输入隐藏

目标：

```text
避免用户输入密码时直接显示明文。
```

建议新增：

```cpp
ConsoleUI::readPasswordHidden()
```

Windows 实现思路：

```text
使用 _getch()
普通字符显示 *
Backspace 删除
Enter 结束
Ctrl+C / EOF 做取消处理
```

注意事项：

- 不要改变密码哈希逻辑。
- 不要把明文密码写日志。
- 登录失败日志只记录失败，不记录密码内容。

### 4.4 停用快递员前任务冲突检查

当前已能拒绝有未完成任务的停用操作。

建议继续增强：

```text
如果停用失败，列出该快递员名下未完成任务：
  单号
  状态
  发件人
  收件人
```

进一步可增加：

```text
是否将待揽收任务转移给其他快递员
```

### 4.5 统计看板升级

当前看板已有全局数据。

建议新增：

- 快递员任务排行。
- 快递员收入排行。
- 物品类型收入占比。
- 待揽收积压列表。
- 高风险异常操作计数，例如 `FAILED` 和 `DENIED` 数量。

验收讲解：

```text
管理员不仅能操作系统，还能从数据层面观察系统运行状态。
```

---

## 5. V2 异常处理进一步优化

### 5.1 自动分配异常

新增自动分配后必须覆盖：

- 没有任何快递员。
- 所有快递员都冻结。
- 所有快递员都停用。
- 快递不处于待揽收状态。
- 快递已经分配给他人。
- 保存失败时回滚。

建议日志：

```text
AUTO_ASSIGN_COURIER SUCCESS / FAILED
```

### 5.2 登录冻结异常

当前已有：

- 用户三次失败冻结。
- 快递员三次失败冻结。
- 管理员记录失败次数，但不自动冻结，避免系统锁死。

建议补强：

- 管理员查看认证失败次数。
- 解冻时显示“是否清除失败次数”。
- 账号冻结日志记录最后失败时间。

### 5.3 数据完整性异常

建议新增管理员功能：

```text
数据完整性检查
```

检查内容：

- 快递发件人是否存在。
- 快递收件人是否存在。
- 快递员字段非空时快递员是否存在。
- 已停用快递员是否仍有未完成任务。
- 快递状态与时间字段是否一致：
  - 待揽收不应有 pickupTime/receiveTime。
  - 待签收应有 pickupTime，不应有 receiveTime。
  - 已签收应有 receiveTime。
- 费用是否与物品类型和数量重新计算一致。

价值：

```text
这属于业务数据自检，比单纯文件读取异常更高阶。
```

### 5.4 跨文件事务保存

当前 `StorageManager` 已完成单文件原子保存。

仍可升级：

```text
saveAll() 多文件事务
```

思路：

```text
1. users/admin/couriers/expresses/auth_state 全部先写 tmp
2. 全部 tmp 成功后统一备份 bak
3. 全部 rename 成正式文件
4. 任一步失败则恢复所有 bak
```

这可以作为强工程能力加分项，但实现复杂度较高，应在 V2 稳定后再做。

---

## 6. V3 网络版启动指导

### 6.1 V3 题目边界

实验要求：

```text
第三题是传统 C/S 网络版。
客户端与服务器端必须是不同进程。
使用 socket 通信。
不能使用 RPC 框架。
网络版功能要求与单机版一致。
```

注意：

```text
V3 不能替代 V1/V2 提交。
V3 应体现从 V2 演进而来，但必须是独立程序。
```

### 6.2 V2 可复用资产

V3 服务端可复用：

- `User`
- `Admin`
- `Courier`
- `Express`
- `ExpressItem` 体系
- `ExpressItemFactory`
- `InputValidator`
- `StringUtil`
- `PasswordHasher`
- `SHA256`
- `StorageManager`
- `Logger`
- `AuthStateRepository`
- `LogisticsSystem`

V3 不应复用：

- `ConsoleUI`
- `App`
- `WindowsConsoleOutputBuffer`
- 任何直接 `std::cin/std::cout` 的交互流程

### 6.3 V3 推荐目录结构

建议新建：

```text
logistics_v3_network/
  common/
    model/
    service/
    storage/
    security/
    protocol/
  server/
    main.cpp
    SocketServer
    ServerController
    SessionManager
  client/
    main.cpp
    SocketClient
    ClientApp
  data/
  LOG/
  build_server.bat
  build_client.bat
  run_server.bat
  run_client.bat
```

如果时间紧，可以先保持单文件服务端和单文件客户端，但逻辑上仍按以上模块划分。

### 6.4 V3 网络协议设计

建议使用文本协议，不使用 RPC。

请求格式：

```text
COMMAND|token|arg1|arg2|...
```

登录前：

```text
LOGIN_USER|username|password
LOGIN_COURIER|username|password
LOGIN_ADMIN|username|password
```

登录后：

```text
SEND_EXPRESS|token|receiver|description|itemType|itemAmount
ASSIGN_COURIER|token|expressId|courierUsername
AUTO_ASSIGN_COURIER|token|expressId
PICKUP_EXPRESS|token|expressId
SIGN_EXPRESS|token|expressId
QUERY_MY_EXPRESS|token|...
QUERY_ADMIN_EXPRESS|token|...
```

响应格式：

```text
OK|message|data...
ERR|code|message
DATA|type|count|...
```

建议错误码：

```text
OK
INVALID_INPUT
AUTH_FAILED
ACCOUNT_FROZEN
PERMISSION_DENIED
NOT_FOUND
STATE_CONFLICT
BALANCE_NOT_ENOUGH
STORAGE_FAILED
PROTOCOL_ERROR
SERVER_ERROR
```

### 6.5 V3 会话安全

V3 必须新增：

```cpp
class SessionManager;
```

会话结构：

```cpp
struct Session {
    std::string token;
    std::string role;
    std::string username;
    std::string loginTime;
    std::string clientAddress;
};
```

核心原则：

```text
服务端不相信客户端传来的 username。
客户端登录成功后只拿 token。
后续请求根据 token 查真实身份。
```

这是 V3 比 V2 更重要的安全边界。

### 6.6 V3 权限控制

建议新增：

```cpp
class ServerController;
```

职责：

```text
解析 token
识别角色
检查命令权限
调用 LogisticsSystem
返回协议响应
```

命令权限示例：

```text
USER:
  SEND_EXPRESS
  SIGN_EXPRESS
  QUERY_MY_EXPRESS
  RECHARGE

COURIER:
  PICKUP_EXPRESS
  QUERY_MY_TASKS
  VIEW_MY_PERFORMANCE

ADMIN:
  CREATE_COURIER
  REMOVE_COURIER
  ASSIGN_COURIER
  AUTO_ASSIGN_COURIER
  QUERY_ALL_EXPRESS
  VERIFY_LOG_CHAIN
  VIEW_DASHBOARD
```

### 6.7 V3 网络异常处理

必须覆盖：

- 客户端断开连接。
- 空请求。
- 未知命令。
- 参数数量错误。
- 字段超长。
- 未登录访问。
- token 失效。
- 权限不足。
- 服务端保存失败。
- 多客户端同时操作同一快递。

建议新增：

```cpp
class ProtocolCodec;
class Request;
class Response;
```

### 6.8 V3 并发与数据一致性

如果服务端支持多个客户端同时连接，必须考虑：

```text
两个用户同时寄件
管理员分配时快递员同时揽收
快递员重复揽收
收件人重复签收
多个请求同时写文件
```

建议：

```cpp
std::mutex systemMutex;
```

所有会修改数据的 `LogisticsSystem` 调用应加锁。

只读查询可以先简单共用同一把锁，后续再优化读写锁。

### 6.9 V3 审计日志升级

V3 日志建议增加：

```text
clientAddress
command
tokenHash
```

但不要记录完整 token，不要记录明文密码。

日志仍使用哈希链：

```text
seq|time|actorType|actor|client|command|result|detail|prevHash|currentHash
```

如果不想改 V2 日志格式，可在 V3 新增 `network_operations.log`。

---

## 7. 推荐后续执行顺序

### Step 1：更新文档

任务：

- 更新 README，写明 Phase 6 已完成能力。
- 更新 `v2_LOG01.md` 或保留本日志作为 Phase 6 后续记录。
- 补充验收演示脚本。

验收价值：

```text
避免代码已实现但文档仍写“待办”的矛盾。
```

### Step 2：补 V2 自动分配

任务：

- 新增自动分配业务函数。
- 管理员菜单接入单个自动分配。
- 管理员菜单接入批量自动分配。
- 写入 `AUTO_ASSIGN_COURIER` 日志。

### Step 3：补 V2 快递员绩效

任务：

- 新增 `CourierPerformance` 结构。
- 新增绩效统计函数。
- 管理员查看全体绩效排行。
- 快递员查看个人绩效。

### Step 4：补 V2 密码隐藏输入

任务：

- 新增 `ConsoleUI::readPasswordHidden()`。
- 用户登录、管理员登录、快递员登录、注册、改密、新增快递员初始密码统一使用隐藏输入。

### Step 5：补 V2 数据完整性检查

任务：

- 新增管理员菜单“数据完整性检查”。
- 检查用户、快递员、快递之间的引用关系。
- 检查状态与时间字段一致性。
- 检查费用是否与类型和数量一致。

### Step 6：启动 V3

任务：

- 新建 `logistics_v3_network` 独立目录。
- 从 V2 复制实体、仓储、工具、安全、业务服务层。
- 新增 server/client 两个进程。
- 先实现登录、查询、退出。
- 再实现寄件、分配、揽收、签收。
- 最后接入自动分配、绩效、日志校验。

---

## 8. 验收展示建议

### V2 强化展示链路

建议演示顺序：

```text
1. 管理员登录。
2. 查看快递员列表。
3. 新增快递员。
4. 注册/登录两个用户。
5. 用户寄普通/易碎/图书快递，展示多态计费。
6. 管理员手动分配快递员。
7. 管理员自动分配另一个待揽收快递。
8. 快递员登录，批量揽收。
9. 收件用户签收。
10. 管理员查看任务统计看板。
11. 管理员查看快递员绩效排行。
12. 演示密码错误三次冻结。
13. 演示日志完整性校验。
14. 演示停用有未完成任务的快递员被拒绝。
```

### V3 初版展示链路

建议演示顺序：

```text
1. 启动 server。
2. 启动 client。
3. 用户通过 client 登录。
4. 用户发送快递，请求发送到 server。
5. 管理员 client 登录并分配快递员。
6. 快递员 client 登录并揽收。
7. 收件用户 client 登录并签收。
8. 管理员查询全局数据。
9. 演示未登录 token 被拒绝。
10. 演示越权命令被拒绝。
```

---

## 9. 当前风险与红线

后续修改必须遵守：

- 不要让密码明文进入任何数据文件或日志。
- 不要让客户端传来的 username 成为 V3 权限依据。
- 不要把 V3 服务端逻辑写进 `ConsoleUI` 或 `App`。
- 不要绕过 `LogisticsSystem` 做业务修改。
- 不要让自动分配分配到冻结或停用快递员。
- 不要让快递员停用破坏历史快递记录。
- 不要让用户签收待揽收快递。
- 不要让快递员揽收他人任务。
- 不要破坏 `StorageManager` 原子保存逻辑。
- 不要改动中文控制台编码方案，除非有完整替代方案并验证。
- 每次代码修改后执行：

```bat
cmd /c build.bat
```

至少验证：

```text
主菜单
管理员登录
用户寄件
管理员分配
快递员揽收
用户签收
日志查看
```

---

## 10. 总结

当前 `logistics_v2` 已经从题目二基础要求推进到具备工程加分特征的单机系统：

- 多态计费真实参与业务。
- 快递员任务流完整。
- 三角色权限边界清晰。
- 密码哈希保护。
- 三次失败自动冻结。
- 原子化保存。
- 日志哈希链。
- 管理员统计看板。
- 中英文表格宽度优化。

下一轮最值得补强的 V2 加分点：

```text
自动分配快递员
快递员绩效看板
密码隐藏输入
快递员任务详情页
数据完整性检查
```

V3 最关键的启动原则：

```text
复用 V2 的业务服务层，但用 socket server/client 替代 ConsoleUI/App；
服务端必须使用 token/session 判断身份，不能信任客户端传来的 username。
```

