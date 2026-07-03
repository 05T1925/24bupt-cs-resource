# Logistics V2 快递员任务管理系统

本目录是《物流管理平台设计》第二阶段单机版最终工程。V2 在 V1 用户寄件、签收、查询、持久化、安全登录和日志审计基础上，完成快递员任务管理、多态计费、三状态物流流转，并额外加入智能调度、安全加固和运营看板能力。

当前核心源码集中在：

```text
src/main.cpp
```

构建与运行入口：

```text
build.bat
run.bat
```

> 注意：源码按 UTF-8 保存，构建脚本使用 `-finput-charset=UTF-8 -fexec-charset=GBK`，运行脚本执行 `chcp 936`。不要轻易改动中文控制台编码层。

---

## 1. V2 最终核心能力

### 1.1 三类身份与权限隔离

```text
User    普通用户：注册、登录、充值、寄件、查询本人相关快递、签收本人收件。
Admin   管理员：用户管理、快递员管理、全局查询、手动/自动分配、日志审计、统计看板。
Courier 快递员：查看本人任务、揽收任务、查询任务、查看个人绩效。
```

关键安全边界：

- 普通用户只能访问自己作为发件人或收件人的快递。
- 快递员只能访问和揽收分配给自己的任务。
- 管理员拥有全局管理权限，但停用快递员时会被未完成任务拦截。
- 登录失败状态独立保存在 `auth_state.txt`，普通用户和快递员连续失败 3 次自动冻结。

### 1.2 快递状态机与资金流转

快递状态：

```text
WaitingPickup 待揽收
WaitingSign   待签收
Signed        已签收
```

流转链路：

```text
用户寄件 -> WaitingPickup
管理员手动或自动分配快递员 -> 仍为 WaitingPickup，但写入 courier
快递员揽收 -> WaitingSign，并写 pickupTime
收件用户签收 -> Signed，并写 receiveTime
```

资金链路：

```text
寄件时：用户余额扣除 fee，管理员账户增加 fee。
揽收时：管理员账户向快递员支付 fee * 50% 提成，快递员 income 增加。
签收时：只改变物流状态，不再发生资金转移。
```

### 1.3 ExpressItem 多态计费

V2 已落地基类虚函数计费：

```text
ExpressItem
  NormalItem   普通快递：5 元/kg
  FragileItem  易碎品：8 元/kg
  BookItem     图书：2 元/本
```

寄件时通过 `ExpressItemFactory` 创建多态对象，并经 `getPrice()` 真实参与扣款、管理员入账和后续快递员提成计算。

---

## 2. Phase 6 / P0 / P1 加分增强

### 2.1 智能自动分配快递员

业务接口：

```cpp
bool autoAssignCourier(const std::string& expressId,
                       std::string& assignedCourier,
                       std::string& message);

void autoAssignAllWaitingPickup(std::vector<std::string>& messages);
```

调度优先级：

```text
1. 可用性过滤：剔除 frozen == true 或 removed == true 的快递员。
2. 负载均衡：未完成任务数（待揽收 + 待签收）最少优先。
3. 收入公平：负载相同时 income 更低者优先。
4. 稳定兜底：仍相同时 username 字典序升序。
```

批量分配会缓存快递员负载快照，每成功分配一单后增量更新负载，避免每次全量重复统计。

### 2.2 停用快递员冲突兜底

快递员停用接口已支持返回未完成任务清单：

```cpp
bool removeCourier(const std::string& username,
                   std::string& message,
                   std::vector<Express>& unfinishedTasks);
```

当快递员名下存在 `WaitingPickup` 或 `WaitingSign` 任务时：

```text
底层拒绝停用
message 返回原因和处理建议
unfinishedTasks 返回阻塞任务列表
UI 使用表格打印单号、状态、发件人、收件人等上下文
```

### 2.3 快递员绩效与管理员数据大屏

绩效结构：

```cpp
struct CourierPerformance {
    std::string username;
    std::string name;
    int waitingPickupCount;
    int waitingSignCount;
    int signedCount;
    int totalTaskCount;
    double completionRate;
    double income;
};
```

统计接口：

```cpp
std::vector<CourierPerformance> courierPerformances() const;
bool courierPerformanceFor(const std::string& username, CourierPerformance& performance) const;
```

管理员统计看板展示：

```text
平台净收入
快递员支出
待揽收 / 待签收 / 已签收数量
普通 / 易碎 / 图书数量和收入占比
全体快递员绩效排行
```

快递员菜单接入个人绩效查询。

### 2.4 密码隐藏输入与演示开关

`ConsoleUI::readPasswordHidden()` 在 Windows 下使用 `_getch()` 逐字符读取密码，并以 `*` 掩码回显。Backspace 使用 `\b \b` 完成视觉擦除。

`main()` 中统一提供验收演示开关：

```cpp
bool hidePasswordInput = true;
ConsoleUI::setPasswordHiddenEnabled(hidePasswordInput);
```

含义：

```text
true  -> 星号隐藏模式，适合正式安全演示。
false -> 明文演示模式，适合向助教展示输入与校验链路。
```

所有登录、注册、修改密码、管理员新增快递员初始密码均走该统一入口。

### 2.5 原子化保存与日志哈希链

`StorageManager` 保存策略：

```text
写 file.tmp
flush/close 成功
旧 file -> file.bak
file.tmp -> file
失败时清理 tmp，并尽量恢复 bak
```

日志格式升级为 9 字段：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

哈希计算：

```text
currentHash = SHA256(seq|time|actorType|actor|action|result|detail|prevHash)
```

管理员菜单已接入日志完整性校验，可定位断链位置。

---

## 3. 持久化文件协议

所有数据文件均采用一行一条记录，字段以 `|` 分隔，字符串字段通过 `StringUtil::escape()` 转义。

```text
users.txt:
username|name|phone|salt|passwordHash|balance|address|frozen

admin.txt:
username|name|salt|passwordHash|balance

couriers.txt:
username|name|phone|salt|passwordHash|income|frozen|removed

expresses.txt:
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee

auth_state.txt:
role|username|failedCount|lastFailedTime

operations.log:
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

转义规则：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

---

## 4. 目录结构

```text
logistics_v2/
  README.md
  build.bat
  run.bat
  LOG/
    v2_LOG00.md
    v2_LOG01.md
    v2_LOG02.md
    v2_LOG_SUM.md
  docs/
    V2_REQUIREMENTS_ANALYSIS.md
    V2_ARCHITECTURE_PLAN.md
    V2_DEVELOPMENT_ROADMAP.md
    V2_ACCEPTANCE_AND_BONUS.md
  src/
    main.cpp
  data/
    users.txt
    admin.txt
    couriers.txt
    expresses.txt
    auth_state.txt
    operations.log
  bin/
  tests/
```

---

## 5. 推荐验收演示链路

```text
1. 启动程序，展示密码输入模式。
2. 管理员登录。
3. 查看快递员列表，新增快递员。
4. 用户寄普通/易碎/图书快递，展示多态计费。
5. 管理员手动分配一单。
6. 管理员自动分配一单，展示负载均衡与公平性策略。
7. 快递员登录，查看本人待揽收任务。
8. 快递员揽收任务，展示 50% 提成。
9. 收件用户签收。
10. 管理员查看任务统计看板和快递员绩效排行。
11. 快递员查看个人绩效。
12. 尝试停用有未完成任务的快递员，展示冲突清单。
13. 演示密码错误三次自动冻结。
14. 演示日志哈希链完整性校验。
15. 简要说明 `.tmp` / `.bak` 原子保存机制。
```

---

## 6. V3 网络版演进红线

V2 的 `Entity`、`Repository`、`LogisticsSystem`、`StorageManager`、`Logger` 等模块可作为 V3 服务端核心资产迁移。

V3 必须遵守题目三硬性要求：

```text
传统 C/S 架构
client 与 server 是不同进程
必须使用 socket 通信
不能使用 RPC 框架
```

服务端剥离红线：

```text
server 中绝对不能残留 ConsoleUI、App、std::cin、std::cout、readPasswordHidden。
UI、密码隐藏输入、菜单和表格展示只能属于 client 进程。
server 只负责 socket 收发、协议解析、Session 鉴权、权限路由和业务调用。
```

V3 首要新增模块：

```text
SocketServer
SocketClient
ProtocolCodec
SessionManager
ServerController
```

安全原则：

```text
服务端不能信任客户端传来的 username。
登录成功后用 token 绑定 Session。
后续业务身份必须从 token 解析。
修改型接口需要加 std::mutex 并发锁。
日志不能记录明文密码或完整 token。
```

