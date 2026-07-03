# v2_LOG_SUM：Logistics V2 最终状态序列化与 V3 C/S 网络版演进上下文

## 0. 文档定位

本文件是 `logistics_v2` 单机版最终里程碑总结，用作后续 `logistics_v3_network` 新对话、新工程、新阶段开发的第一核心上下文。它承接：

```text
LOG/v2_LOG00.md
LOG/v2_LOG01.md
LOG/v2_LOG02.md
src/main.cpp
实验(综合-物流管理平台设计)-面向对象程序设计与实践-2026春 发布版v3.docx
```

当前 V2 已完成实验二要求，并额外完成多项 P0/P1 加分增强：

```text
快递员管理
三状态物流流转
物品多态计费
手动分配快递员
智能自动分配快递员
快递员揽收与 50% 提成分账
快递员任务查询
管理员统计看板
快递员绩效看板
任务冲突兜底
密码隐藏输入
三次失败自动冻结
原子化保存
日志哈希链
中英文混合表格宽度优化
```

当前项目路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v2
```

当前核心源码：

```text
src/main.cpp
```

构建脚本：

```text
build.bat
```

运行脚本：

```text
run.bat
```

默认管理员：

```text
账号：admin
密码：Admin0219
```

注意：源码按 UTF-8 保存，构建时使用：

```bat
-finput-charset=UTF-8 -fexec-charset=GBK
```

运行脚本使用：

```bat
chcp 936
```

不要轻易改动中文控制台编码层。

---

## 1. V2 最终架构与核心资产快照 (Final Architecture & Assets)

### 1.1 总体分层

当前 `logistics_v2` 仍是单文件实现，所有类集中在：

```text
src/main.cpp
```

但逻辑上已经形成稳定分层：

```text
配置层：
  ProjectConfig

工具层：
  StringUtil
  DirectoryUtil
  InputValidator
  SHA256
  PasswordHasher
  StorageManager
  TablePrinter

控制台兼容层：
  WindowsConsoleOutputBuffer
  ConsoleEnvironment

实体层：
  User
  Admin
  Courier
  Express
  ExpressItem / NormalItem / FragileItem / BookItem
  LogEntry
  AuthState
  CourierPerformance
  SystemStatistics

仓储层：
  UserRepository
  AdminRepository
  CourierRepository
  ExpressRepository
  AuthStateRepository
  Logger

业务服务层：
  LogisticsSystem

UI 流程层：
  ConsoleUI
  App

入口层：
  main()
```

`main()` 保持简洁：

```cpp
int main() {
    ConsoleEnvironment::initialize();
    bool hidePasswordInput = true;
    ConsoleUI::setPasswordHiddenEnabled(hidePasswordInput);
    App app;
    app.run();
    return 0;
}
```

其中：

```cpp
bool hidePasswordInput = true;
```

是验收演示开关：

```text
true  -> 密码输入显示星号，适合正式安全演示。
false -> 密码输入明文回显，适合向助教展示输入内容与校验流程。
```

### 1.2 配置层 ProjectConfig

`ProjectConfig` 集中保存 V2 程序级常量：

```cpp
DisplayTitle        = "【物流管理平台 V2 - 快递员任务管理系统】"
WindowTitle         = L"【物流管理平台 V2 - 快递员任务管理系统】"
DataDirectoryName   = "data"
UserFileName        = "users.txt"
AdminFileName       = "admin.txt"
ExpressFileName     = "expresses.txt"
CourierFileName     = "couriers.txt"
LogFileName         = "operations.log"
AuthStateFileName   = "auth_state.txt"
PasswordHashScope   = "|logistics_v2|"
```

设计意义：

- V2 与 V1 数据隔离。
- 文件名不散落硬编码。
- 密码哈希域独立于 V1。
- 为 V3 抽离共享配置打基础。

### 1.3 用户、管理员、快递员实体体系

#### User

普通用户字段：

```text
username_      唯一用户名
name_          姓名
phone_         手机号
salt_          密码随机盐
passwordHash_  密码哈希
balance_       账户余额
address_       默认地址
frozen_        是否冻结
```

核心行为：

```text
updatePassword()
recharge()
deduct()
setFrozen()
serialize()
deserialize()
```

普通用户能力：

- 注册。
- 登录。
- 修改密码。
- 充值。
- 寄件。
- 查询自己相关快递。
- 签收自己接收且处于待签收状态的快递。

#### Admin

管理员字段：

```text
username_      默认 admin
name_          默认 SystemAdmin
salt_          密码随机盐
passwordHash_  密码哈希
balance_       平台/公司账户余额
```

核心行为：

```text
addBalance()
resetPassword()
serialize()
deserialize()
```

管理员能力：

- 登录。
- 查看所有用户。
- 查询全部快递。
- 冻结/解冻用户。
- 管理快递员。
- 手动分配快递员。
- 智能自动分配快递员。
- 查看日志。
- 校验日志哈希链。
- 查看任务统计看板。
- 查看全体快递员绩效排行。

#### Courier

快递员字段：

```text
username_      快递员唯一用户名
name_          姓名
phone_         手机号
salt_          密码随机盐
passwordHash_  密码哈希
income_        累计收入 / 账户余额
frozen_        是否冻结
removed_       是否逻辑删除 / 停用
```

核心行为：

```text
verifyPassword()
addIncome()
setFrozen()
markRemoved()
serialize()
deserialize()
```

快递员能力：

- 登录。
- 查看本人待揽收任务。
- 单个/批量/全部揽收本人任务。
- 查询本人任务。
- 查看个人绩效。
- 查看累计收入。

重要设计：

```text
删除快递员不是物理删除，而是 markRemoved() 逻辑停用。
这样历史快递记录中的 courier 字段仍可追溯。
```

### 1.4 Express 快递实体与三状态机

V2 快递状态：

```cpp
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};
```

中文语义：

```text
WaitingPickup -> 待揽收
WaitingSign   -> 待签收
Signed        -> 已签收
```

`Express` 字段：

```text
id_           快递单号
sender_       发件用户名
receiver_     收件用户名
courier_      负责快递员用户名，未分配时为空
sendTime_     寄件时间
pickupTime_   揽收时间，未揽收时为空
receiveTime_  签收时间，未签收时为空
status_       当前状态
itemType_     Normal / Fragile / Book
itemAmount_   普通/易碎品为 kg，图书为册数
description_  物品描述
fee_          本单费用
```

核心行为：

```text
belongsToUser()
belongsToCourier()
isWaitingPickup()
isWaitingSign()
isSigned()
assignCourier()
pickup()
sign()
statusName()
serialize()
deserialize()
```

状态流转：

```text
用户寄件成功
  -> WaitingPickup

管理员手动分配 / 自动分配快递员
  -> 状态仍为 WaitingPickup，只写 courier_

快递员揽收
  -> WaitingSign，写 pickupTime_

收件用户签收
  -> Signed，写 receiveTime_
```

状态防线：

- 未分配快递员的快递不能被快递员揽收。
- 快递员不能揽收非本人任务。
- 已经待签收或已签收的快递不能重复揽收。
- 待揽收快递不能被收件用户签收。
- 已签收快递不能重复签收。
- 用户不能签收他人快递。

### 1.5 ExpressItem 多态计费体系

V2 已落地物品分类继承体系：

```cpp
class ExpressItem {
public:
    virtual ~ExpressItem() = default;
    virtual std::string typeCode() const = 0;
    virtual std::string typeName() const = 0;
    virtual std::string amountText() const = 0;
    virtual double getPrice() const = 0;
};

class NormalItem : public ExpressItem;
class FragileItem : public ExpressItem;
class BookItem : public ExpressItem;
```

计费规则：

```text
NormalItem   普通快递：5 元/kg
FragileItem  易碎品：8 元/kg
BookItem     图书：2 元/本
```

`ExpressItemFactory`：

```cpp
static std::unique_ptr<ExpressItem> createItem(const std::string& type, double amount);
```

当前策略：

```text
Express 内部不直接保存 unique_ptr<ExpressItem>。
Express 只保存 itemType_ 和 itemAmount_。
寄件时临时通过 ExpressItemFactory 创建多态对象并调用 getPrice()。
```

这么做的原因：

- 当前查询接口大量返回 `std::vector<Express>`。
- 如果 `Express` 内部保存 `std::unique_ptr<ExpressItem>`，需要额外实现 clone、拷贝构造、拷贝赋值。
- 当前方案稳定、简单、可解释，并且多态确实参与业务计费。

寄件费用流：

```text
用户选择物品类型和重量/册数
-> ExpressItemFactory 创建子类对象
-> 通过基类指针调用 getPrice()
-> 校验用户余额
-> 扣用户余额
-> 加管理员余额
-> 创建 Express
-> 保存数据
```

验收重点：

```text
getPrice() 不是摆设，真实参与扣款、平台入账、快递员提成。
```

### 1.6 业务资金流转

#### 用户寄件

流程：

```text
校验发件用户存在且未冻结
校验收件用户存在且未冻结
禁止给自己寄件
校验物品描述
创建 ExpressItem 子类对象
调用 getPrice() 计算费用
校验余额
用户余额 -= fee
管理员余额 += fee
创建快递，状态 WaitingPickup
保存 users/admin/couriers/expresses
写 SEND_EXPRESS 日志
```

保存失败回滚：

```text
删除新增快递
管理员余额 -= fee
用户余额 += fee
清空 expressId
写 SAVE_DATA / SEND_EXPRESS FAILED 日志
```

#### 快递员揽收

流程：

```text
校验快递存在
校验快递员存在、未冻结、未停用
校验快递属于当前快递员
校验快递状态为 WaitingPickup
commission = fee * 0.5
管理员余额 -= commission
快递员 income += commission
快递状态 -> WaitingSign
写 pickupTime
保存数据
写 PICKUP_EXPRESS 日志
```

保存失败回滚：

```text
恢复 Express
恢复 Admin
恢复 Courier
```

#### 收件用户签收

流程：

```text
校验快递存在
校验 receiver == 当前用户
拒绝 WaitingPickup
拒绝 Signed
只允许 WaitingSign
状态 -> Signed
写 receiveTime
保存数据
写 SIGN_EXPRESS 日志
```

### 1.7 Phase 6 决胜优化成果

#### 1.7.1 智能自动分配快递员

已实现接口：

```cpp
bool autoAssignCourier(const std::string& expressId,
                       std::string& assignedCourier,
                       std::string& message);

void autoAssignAllWaitingPickup(std::vector<std::string>& messages);
```

调度辅助结构：

```cpp
struct CourierScheduleState {
    std::size_t courierIndex;
    int unfinishedCount;
};
```

候选池：

```text
所有存在的快递员
剔除 frozen == true
剔除 removed == true
```

排序优先级：

```text
第一优先级：未完成任务数最少
  未完成任务 = WaitingPickup + WaitingSign

第二优先级：累计收入 income 更低

第三优先级：username 字典序升序
```

核心辅助函数：

```cpp
std::vector<CourierScheduleState> buildCourierScheduleStates() const;
std::size_t selectBestCourierIndex(const std::vector<CourierScheduleState>& states) const;
void increaseCourierLoad(std::vector<CourierScheduleState>& states, std::size_t courierIndex) const;
```

性能设计：

```text
单个自动分配：构建一次快照，选择最优快递员。
批量自动分配：构建一次快照，每成功分配一单后增量更新该快递员未完成任务数。
避免每分配一单都全量重新统计。
```

单个自动分配防线：

- 快递单号不存在 -> 失败。
- 快递不是待揽收 -> 失败。
- 无可用快递员 -> 失败。
- 保存失败 -> 回滚 courier 字段。
- 成功写 `AUTO_ASSIGN_COURIER` 日志。

批量自动分配策略：

```text
遍历所有 WaitingPickup 且 courier 为空的快递
逐单选择当前最优快递员
每单保存一次
允许部分成功、部分失败
messages 返回每单结果
最终写 AUTO_ASSIGN_ALL 汇总日志
```

管理员菜单：

```text
10. 单个自动分配
11. 一键分配所有待揽收快递
```

#### 1.7.2 停用快递员任务冲突兜底

已重构：

```cpp
bool removeCourier(const std::string& username, std::string& message);
bool removeCourier(const std::string& username,
                   std::string& message,
                   std::vector<Express>& unfinishedTasks);
```

逻辑：

```text
如果快递员不存在 -> 失败
如果快递员已停用 -> 失败
扫描该快递员名下任务
如果存在未完成任务，即 WaitingPickup 或 WaitingSign
  -> 拒绝停用
  -> message 返回数量和处理建议
  -> unfinishedTasks 返回单号、状态等完整任务列表
  -> UI 打印任务表格
否则 markRemoved()
```

验收亮点：

```text
系统不是简单拒绝，而是明确告诉管理员哪些任务阻塞了停用操作。
```

#### 1.7.3 CourierPerformance 快递员绩效看板

结构：

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

业务接口：

```cpp
std::vector<CourierPerformance> courierPerformances() const;
bool courierPerformanceFor(const std::string& username, CourierPerformance& performance) const;
```

统计口径：

```text
waitingPickupCount = 该快递员名下待揽收任务数
waitingSignCount   = 该快递员名下待签收任务数
signedCount        = 该快递员名下已签收任务数
totalTaskCount     = 三类任务总和
completionRate     = signedCount / totalTaskCount
income             = courier.income()
```

全局排序：

```text
完成率降序
总任务数降序
收入降序
用户名升序
```

接入点：

```text
管理员任务统计看板：
  显示全体快递员绩效排行

快递员功能菜单：
  4. 查看个人绩效
```

#### 1.7.4 管理员业务数据大屏

`SystemStatistics` 字段：

```cpp
double platformNetIncome;
double courierPayout;
int waitingPickupCount;
int waitingSignCount;
int signedCount;
int normalCount;
int fragileCount;
int bookCount;
double normalFee;
double fragileFee;
double bookFee;
```

管理员看板展示：

```text
平台净收入
快递员支出
各状态数量：待揽收 / 待签收 / 已签收
各物品类型订单数与费用合计
各物品类型收入占比
全体快递员绩效排行
```

#### 1.7.5 密码底层隐藏输入组件

新增：

```cpp
ConsoleUI::readPasswordHidden(const std::string& prompt)
```

Windows 下使用：

```cpp
_getch()
```

行为：

```text
普通可见字符 -> 内部追加字符，屏幕只输出 *
Backspace -> password.pop_back()，输出 "\b \b" 完成视觉擦除
Enter -> 结束输入，输出换行
方向键 / 功能键 -> 吞掉扩展字节，不进入密码
```

跨平台处理：

```text
#ifdef _WIN32
  使用 <conio.h> 和 _getch()
#else
  使用 getline 兜底
#endif
```

全局演示开关：

```cpp
bool hidePasswordInput = true;
ConsoleUI::setPasswordHiddenEnabled(hidePasswordInput);
```

所有密码输入统一接入：

- 用户登录。
- 管理员登录。
- 快递员登录。
- 用户注册。
- 修改密码旧密码。
- 修改密码新密码与确认。
- 管理员新增快递员初始密码与确认。

重要红线：

```text
明文密码不写日志。
明文密码不落文件。
```

### 1.8 安全与审计增强

#### 密码哈希

密码保存形式：

```text
salt + SHA-256
```

哈希域：

```text
|logistics_v2|
```

计算逻辑：

```cpp
SHA256::hash(salt + ProjectConfig::PasswordHashScope + password)
```

#### 登录失败三次自动冻结

认证状态独立保存于：

```text
auth_state.txt
```

用户和快递员：

```text
密码错误 -> failedCount + 1
failedCount >= 3 -> 自动冻结实体 frozen_ = true
登录成功 -> failedCount 清零
```

管理员：

```text
记录失败次数，但不自动冻结，避免系统锁死。
```

#### 日志哈希链

日志格式升级为 9 字段：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

计算公式：

```text
currentHash = SHA256(seq|time|actorType|actor|action|result|detail|prevHash)
```

管理员可执行：

```text
校验日志完整性
```

若日志被外部文本编辑器篡改，可定位断链行。

#### 原子化保存

`StorageManager` 已实现单文件原子保存：

```text
写 file.tmp
flush/close 成功
旧 file -> file.bak
file.tmp -> file
失败时尽量恢复旧 file
```

当前仍不是严格多文件事务，见后文 V3 风险。

---

## 2. 底层协议与持久化规范 (Protocols & Persistence)

### 2.1 通用文本转义协议

所有数据文件采用文本行协议：

```text
一条记录一行
字段使用 | 分隔
字符串字段通过 StringUtil::escape() 转义
```

当前转义：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

读取时：

```text
StringUtil::unescape()
```

注意：

```text
V3 网络协议也应复用类似转义思想，不能直接相信 | 分隔输入。
```

### 2.2 users.txt

路径：

```text
data/users.txt
```

字段顺序：

```text
username|name|phone|salt|passwordHash|balance|address|frozen
```

字段含义：

```text
username      唯一用户名
name          姓名
phone         11 位手机号
salt          密码随机盐
passwordHash  SHA-256 密码哈希
balance       用户余额，两位小数字符串
address       默认地址
frozen        0/1，账号是否冻结
```

反序列化约束：

- 字段数必须为 8。
- balance 必须完整解析为非负数。
- frozen 必须为 `0` 或 `1`。
- 异常行跳过并计入仓储异常计数。

### 2.3 admin.txt

路径：

```text
data/admin.txt
```

字段顺序：

```text
username|name|salt|passwordHash|balance
```

字段含义：

```text
username      管理员用户名，默认 admin
name          管理员姓名，默认 SystemAdmin
salt          密码随机盐
passwordHash  SHA-256 密码哈希
balance       平台账户余额
```

反序列化约束：

- 字段数必须为 5。
- balance 必须完整解析为非负数。
- 文件不存在或异常时使用默认管理员对象。

### 2.4 couriers.txt

路径：

```text
data/couriers.txt
```

字段顺序：

```text
username|name|phone|salt|passwordHash|income|frozen|removed
```

字段含义：

```text
username      快递员用户名
name          姓名
phone         手机号
salt          密码随机盐
passwordHash  SHA-256 密码哈希
income        累计收入 / 账户余额
frozen        0/1，是否冻结
removed       0/1，是否停用 / 逻辑删除
```

反序列化约束：

- 字段数必须为 8。
- income 必须完整解析为非负数。
- frozen 和 removed 必须为 `0` 或 `1`。
- 异常行跳过并计入仓储异常计数。

### 2.5 expresses.txt

路径：

```text
data/expresses.txt
```

V2 新格式字段顺序：

```text
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee
```

字段含义：

```text
id           快递单号
sender       发件用户名
receiver     收件用户名
courier      负责快递员用户名，可为空
sendTime     寄件时间
pickupTime   揽收时间，可为空
receiveTime  签收时间，可为空
status       0/1/2，对应待揽收/待签收/已签收
itemType     Normal / Fragile / Book
itemAmount   普通/易碎品为 kg，图书为册数
description  物品描述
fee          快递费用
```

兼容旧 V1 8 字段：

```text
id|sender|receiver|sendTime|receiveTime|status|description|fee
```

兼容映射：

```text
courier    = 空
pickupTime = 空
itemType   = Normal
itemAmount = fee / 5.0
V1 status 0 -> V2 WaitingSign
V1 status 1 -> V2 Signed
```

注意：

```text
V1 的 status=0 是待签收，不是 V2 的待揽收。
```

### 2.6 auth_state.txt

路径：

```text
data/auth_state.txt
```

字段顺序：

```text
role|username|failedCount|lastFailedTime
```

字段含义：

```text
role            USER / COURIER / ADMIN
username        账号
failedCount     连续登录失败次数
lastFailedTime  最近一次失败时间
```

设计原因：

```text
不污染 users.txt 和 couriers.txt 的实体结构。
便于 V3 服务端统一管理认证状态。
```

### 2.7 operations.log

路径：

```text
data/operations.log
```

新 9 字段哈希链格式：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

字段含义：

```text
seq          日志序号
time         操作时间
actorType    USER / ADMIN / COURIER / SYSTEM / GUEST
actor        操作账号
action       操作类型
result       SUCCESS / FAILED / DENIED
detail       详情
prevHash     上一条链式日志 currentHash
currentHash  当前日志哈希
```

哈希输入规范：

```text
seq|time|actorType|actor|action|result|detail|prevHash
```

哈希算法：

```text
SHA-256
```

兼容旧格式：

```text
time|actorType|actor|action|result|detail
```

旧 6 字段日志可读取，但不参与新链校验。新链从 `LEGACY` 或 `GENESIS` 继续。

### 2.8 原子化保存 StorageManager

核心接口：

```cpp
static bool saveLinesAtomically(const std::string& filePath,
                                const std::vector<std::string>& lines);
```

保存流程：

```text
1. 写入 file.tmp
2. flush 并关闭文件流
3. 如果旧 file 存在，删除旧 bak，再 rename file -> file.bak
4. rename file.tmp -> file
5. 任一步失败则删除 tmp，并尽量把 bak 恢复为 file
6. 返回 false 给业务层，业务层不能提示假成功
```

当前接入：

```text
UserRepository::save()
AdminRepository::save()
CourierRepository::save()
ExpressRepository::save()
AuthStateRepository::save()
```

注意：

```text
当前是单文件原子保存。
saveAll() 跨多个文件保存时，还不是严格事务型提交。
V3 如果支持多客户端并发，应进一步增强。
```

---

## 3. V3 架构转型规划与复用边界 (V3 Architecture Blueprint)

### 3.1 题目三硬性红线预警

实验题目三要求：

```text
物流管理系统（网络版）
传统 C/S 架构
客户端与服务器端为不同进程
使用 socket 通信
不能使用 RPC 框架
网络版功能要求与单机版一致
支持一定错误场景处理能力
```

红线：

```text
V3 不能是单进程模拟网络。
V3 不能把 V2 控制台菜单直接当服务端。
V3 不能使用 RPC 框架。
V3 必须有 server 进程和 client 进程。
V3 必须通过 socket 通信。
```

### 3.2 V3 推荐工程目录

建议新建独立目录：

```text
logistics_v3_network/
  common/
    models/
    storage/
    service/
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
    ConsoleClientUI
  data/
  LOG/
  build_server.bat
  build_client.bat
  run_server.bat
  run_client.bat
```

如果时间紧，可先采用较少文件，但逻辑必须分清：

```text
server 只负责网络收发、会话、权限、业务调用。
client 只负责输入输出、菜单、展示。
common 保存可复用实体、仓储、协议和业务服务。
```

### 3.3 Server 端可复用资产

以下 V2 模块适合直接迁移到 V3 服务端：

#### 实体模型

```text
User
Admin
Courier
Express
ExpressStatus
ExpressQueryCondition
CourierPerformance
SystemStatistics
AuthState
LogEntry
```

#### 多态计费

```text
ExpressItem
NormalItem
FragileItem
BookItem
ExpressItemFactory
```

#### 工具与安全

```text
StringUtil
InputValidator
SHA256
PasswordHasher
StorageManager
```

#### 仓储

```text
UserRepository
AdminRepository
CourierRepository
ExpressRepository
AuthStateRepository
Logger
```

#### 业务服务

```text
LogisticsSystem
```

特别适合复用的业务函数：

```text
registerUser()
loginUser()
loginCourier()
loginAdmin()
changeUserPassword()
recharge()
sendExpress()
signExpress()
createCourier()
removeCourier()
setCourierFrozen()
assignCourier()
autoAssignCourier()
autoAssignAllWaitingPickup()
pickupExpress()
queryUserExpresses()
queryCourierExpresses()
queryAllExpresses()
queryLogs()
statistics()
courierPerformances()
courierPerformanceFor()
verifyLogHashChain()
```

### 3.4 Server 端必须剥离的 UI 资产

V3 服务端绝对禁止残留：

```text
ConsoleUI
App
WindowsConsoleOutputBuffer
ConsoleEnvironment
std::cin
std::cout
ConsoleUI::readLine()
ConsoleUI::readPasswordHidden()
ConsoleUI::printTable()
ConsoleUI::pause()
```

原因：

```text
服务端不能等待人工输入。
服务端不能直接打印业务菜单。
服务端应该接收 socket 请求，返回协议响应。
```

V3 客户端可以复用或重写：

```text
ConsoleUI 的菜单展示思想
TablePrinter 的表格展示思想
readPasswordHidden 的隐藏密码输入
```

但这些只能出现在 client 进程，不能进入 server。

### 3.5 LogisticsSystem 的 V3 适配方向

当前 `LogisticsSystem` 已经基本没有 UI 依赖，这是 V3 的最大优势。

V3 可逐步把接口返回从：

```cpp
bool + std::string message
```

升级为：

```cpp
struct ServiceResult {
    bool ok;
    std::string code;
    std::string message;
};
```

推荐错误码：

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

为了降低 V3 首轮开发风险，可以先复用 `bool + message`，等网络闭环跑通后再引入 `ServiceResult`。

---

## 4. V3 网络核心技术栈筹备 (V3 Network Preparations)

### 4.1 Socket 技术选择

Windows 下建议使用 Winsock：

```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
```

链接：

```bat
-lws2_32
```

基础流程：

Server：

```text
WSAStartup
socket
bind
listen
accept
recv
send
closesocket
WSACleanup
```

Client：

```text
WSAStartup
socket
connect
send
recv
closesocket
WSACleanup
```

### 4.2 文本命令协议草案

建议请求格式：

```text
COMMAND|token|arg1|arg2|...
```

登录类请求尚无 token：

```text
LOGIN_USER|username|password
LOGIN_COURIER|username|password
LOGIN_ADMIN|username|password
```

登录成功响应：

```text
OK|LOGIN_SUCCESS|token|role|username|message
```

登录后请求：

```text
REGISTER_USER|token|username|name|phone|password|address
CHANGE_PASSWORD|token|oldPassword|newPassword
RECHARGE|token|amount
SEND_EXPRESS|token|receiver|description|itemType|itemAmount
SIGN_EXPRESS|token|expressId

CREATE_COURIER|token|username|name|phone|password
REMOVE_COURIER|token|courierUsername
SET_COURIER_FROZEN|token|courierUsername|0/1
ASSIGN_COURIER|token|expressId|courierUsername
AUTO_ASSIGN_COURIER|token|expressId
AUTO_ASSIGN_ALL|token
VERIFY_LOG_CHAIN|token
VIEW_DASHBOARD|token
VIEW_COURIER_PERFORMANCE|token

PICKUP_EXPRESS|token|expressId
QUERY_MY_TASKS|token|sender|receiver|expressId|start|end|status|itemType
QUERY_MY_EXPRESS|token|sender|receiver|expressId|start|end|status|itemType
QUERY_ALL_EXPRESS|token|sender|receiver|courier|expressId|start|end|status|itemType
```

响应格式：

```text
OK|code|message
ERR|code|message
DATA|type|count|payload...
```

示例：

```text
OK|SEND_SUCCESS|发送成功，快递单号：EX...
ERR|BALANCE_NOT_ENOUGH|余额不足...
DATA|EXPRESS|2|record1|record2
DATA|COURIER_PERFORMANCE|3|record1|record2|record3
```

### 4.3 协议编码与转义

V3 不能直接把用户输入拼进 `|` 协议。

建议复用：

```text
StringUtil::escape()
StringUtil::unescape()
```

所有文本字段发送前 escape，服务端解析后 unescape。

必须限制：

```text
单条请求最大长度
单字段最大长度
参数个数
命令白名单
```

防止：

- 协议注入。
- 超长请求占用内存。
- 字段缺失导致越界。
- 非法命令绕过权限。

### 4.4 SessionManager 零信任安全模型

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

核心规则：

```text
客户端传来的 username 不可信。
服务端只相信 token 对应的 Session。
```

登录成功：

```text
服务端生成随机 token
保存 token -> Session
返回 token 给客户端
```

后续请求：

```text
客户端带 token
服务端查 Session
根据 Session.role 和 Session.username 调用业务函数
```

禁止：

```text
SIGN_EXPRESS|token|任意username|expressId
PICKUP_EXPRESS|token|任意courierUsername|expressId
```

正确：

```text
SIGN_EXPRESS|token|expressId
服务端从 token 解析真实 username

PICKUP_EXPRESS|token|expressId
服务端从 token 解析真实 courierUsername
```

### 4.5 ServerController 权限路由

建议新增：

```cpp
class ServerController;
```

职责：

```text
接收 Request
校验 token
校验 role 权限
转换参数
调用 LogisticsSystem
返回 Response
```

权限矩阵：

```text
GUEST:
  LOGIN_USER
  LOGIN_COURIER
  LOGIN_ADMIN
  REGISTER_USER

USER:
  CHANGE_PASSWORD
  RECHARGE
  SEND_EXPRESS
  SIGN_EXPRESS
  QUERY_MY_EXPRESS

COURIER:
  PICKUP_EXPRESS
  QUERY_MY_TASKS
  VIEW_MY_PERFORMANCE

ADMIN:
  CREATE_COURIER
  REMOVE_COURIER
  SET_COURIER_FROZEN
  SET_USER_FROZEN
  ASSIGN_COURIER
  AUTO_ASSIGN_COURIER
  AUTO_ASSIGN_ALL
  QUERY_ALL_EXPRESS
  QUERY_LOGS
  VERIFY_LOG_CHAIN
  VIEW_DASHBOARD
  VIEW_COURIER_PERFORMANCE
```

### 4.6 并发与数据一致性预警

V2 是单进程单用户交互，不存在并发写。

V3 Server 可能同时服务多个客户端，必须考虑：

```text
两个用户同时寄件
管理员自动分配时快递员同时揽收
两个快递员重复揽收同一单
收件用户重复签收
多个请求同时写 users/admin/couriers/expresses
多个请求同时写 operations.log
```

建议第一版使用一把互斥锁：

```cpp
std::mutex systemMutex;
```

所有会修改状态的业务调用加锁：

```text
registerUser
changeUserPassword
recharge
sendExpress
signExpress
createCourier
removeCourier
setCourierFrozen
setUserFrozen
assignCourier
autoAssignCourier
autoAssignAllWaitingPickup
pickupExpress
loginUser / loginCourier / loginAdmin
```

原因：

```text
登录失败会修改 auth_state 和 frozen 状态，也需要锁。
```

只读查询可以第一版也加同一把锁，保证简单稳定。后续再考虑读写锁。

### 4.7 V3 网络异常处理清单

必须覆盖：

- 空请求。
- 未知命令。
- 参数数量错误。
- 字段格式错误。
- 字段超长。
- token 缺失。
- token 无效。
- token 过期。
- 权限不足。
- 客户端断开。
- 半包 / 粘包。
- 保存失败。
- 日志写入失败。
- 并发状态冲突。

推荐响应：

```text
ERR|PROTOCOL_ERROR|参数数量错误
ERR|AUTH_REQUIRED|请先登录
ERR|PERMISSION_DENIED|当前身份无权执行该操作
ERR|STATE_CONFLICT|快递状态已变化，请刷新后重试
ERR|STORAGE_FAILED|数据保存失败
```

### 4.8 半包与粘包处理

不要假设一次 `recv()` 就得到完整命令。

建议协议以换行作为消息边界：

```text
COMMAND|token|arg1|arg2\n
```

服务端为每个连接维护接收缓冲区：

```text
recv bytes -> append buffer -> while buffer contains '\n' -> extract one command
```

响应同样以 `\n` 结束。

### 4.9 V3 日志审计升级

V2 日志字段：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

V3 可继续复用，也可新增网络日志：

```text
network_operations.log
```

建议 V3 网络日志字段：

```text
seq|time|clientAddress|actorType|actor|command|result|detail|prevHash|currentHash
```

注意：

```text
不要记录明文密码。
不要记录完整 token。
如果需要记录 token，只记录 tokenHash 前若干位。
```

### 4.10 V3 首轮开发顺序

推荐顺序：

```text
1. 新建 logistics_v3_network 独立目录。
2. 复制 V2 的实体、工具、仓储、业务服务层到 common。
3. 删除或隔离 ConsoleUI/App/ConsoleEnvironment。
4. 创建 server main，启动 Winsock 监听。
5. 创建 client main，连接 server。
6. 实现 LOGIN_ADMIN / LOGIN_USER / LOGIN_COURIER。
7. 实现 token SessionManager。
8. 实现 QUERY 类命令，先跑通只读。
9. 实现 SEND_EXPRESS。
10. 实现 ASSIGN_COURIER / AUTO_ASSIGN_COURIER / AUTO_ASSIGN_ALL。
11. 实现 PICKUP_EXPRESS。
12. 实现 SIGN_EXPRESS。
13. 实现 DASHBOARD / PERFORMANCE。
14. 实现日志校验与异常演示。
15. 增加并发锁。
```

第一版目标：

```text
能完整演示一个客户端用户寄件、管理员分配、快递员揽收、收件用户签收。
```

第二版目标：

```text
增加多客户端并发、错误处理、日志审计、自动分配和绩效看板。
```

---

## 5. V2 最终验收演示建议

如果需要展示 V2，可按以下路线：

```text
1. 启动程序，展示密码输入模式开关提示。
2. 管理员登录。
3. 查看快递员列表。
4. 新增快递员。
5. 注册/登录两个用户。
6. 用户选择普通/易碎/图书寄件，展示多态计费。
7. 管理员手动分配一单。
8. 管理员自动分配一单。
9. 快递员登录，展示个人待揽收任务。
10. 快递员批量揽收。
11. 收件用户签收。
12. 管理员查看任务统计看板和绩效排行。
13. 快递员查看个人绩效。
14. 尝试停用有未完成任务的快递员，展示任务冲突清单。
15. 演示密码错误三次自动冻结。
16. 演示日志哈希链校验。
17. 简要说明原子化保存产生 .bak。
```

---

## 6. V3 开发红线总表

必须遵守：

```text
V3 必须是独立程序。
V3 必须有 server/client 两个不同进程。
V3 必须使用 socket。
V3 不能使用 RPC 框架。
V3 server 不能包含 ConsoleUI/App/std::cin/std::cout/readPasswordHidden。
V3 client 不能直接改数据文件。
V3 client 不能绕过 server 调 LogisticsSystem。
V3 server 不能信任客户端传来的 username。
V3 必须使用 token/session 做身份绑定。
V3 修改型业务必须考虑并发锁。
V3 不记录明文密码。
V3 不记录完整 token。
V3 应复用 V2 的权限防线，不要把权限判断下放到 client。
```

---

## 7. 当前仍可优化但不阻塞 V3 的事项

V2 仍可继续增强：

```text
跨文件事务保存
数据完整性检查中心
快递员任务转移功能
自动分配策略可配置
密码隐藏输入支持 Linux termios
日志哈希链对旧 6 字段日志的迁移工具
单元测试脚本
```

V3 更优先：

```text
先跑通 C/S 闭环。
再把 V2 加分能力逐步网络化。
```

---

## 8. 上下文一致性核验记录

本节用于压缩上下文后的二次校准，避免后续 V3 开发时把早期规划、旧 README 或模型记忆误认为当前真实状态。

### 8.1 README 与 LOG_SUM 的职责划分

```text
README.md:
  作为 V2 工程当前入口说明，记录最终能力、运行方式、持久化协议和 V3 红线。

LOG/v2_LOG_SUM.md:
  作为 V2 -> V3 的高浓度迁移上下文，记录更细的类职责、接口名、状态机、协议草案和开发顺序。
```

两者现在已经统一到同一事实基线：

```text
项目不是智能旅游系统，而是物流管理平台。
V2 不是数据库项目，当前持久化采用文本文件 + StorageManager 原子保存。
V2 不是网络版，当前仍为单进程控制台程序。
V3 才进入传统 C/S + Socket 双进程架构。
```

### 8.2 与当前源码核对过的关键符号

以下符号已在 `src/main.cpp` 中存在，后续生成代码时应以这些名字为准：

```text
ProjectConfig
ExpressStatus
ExpressQueryCondition
SystemStatistics
CourierPerformance
StorageManager
Logger::verifyHashChain()
ConsoleUI::setPasswordHiddenEnabled()
ConsoleUI::readPasswordHidden()
LogisticsSystem::removeCourier(username, message, unfinishedTasks)
LogisticsSystem::autoAssignCourier(expressId, assignedCourier, message)
LogisticsSystem::autoAssignAllWaitingPickup(messages)
LogisticsSystem::courierPerformances()
LogisticsSystem::courierPerformanceFor(username, performance)
```

### 8.3 当前上下文风险提醒

```text
README 早期版本曾只记录到 v2_LOG00 和基础快递员任务管理，现已更新为最终状态。
V2 所有核心代码仍集中在单个 src/main.cpp，尚未拆分多文件。
当前没有 SQL 数据库、ORM、JSON API 或第三方网络库强依赖。
当前日志协议是文本 9 字段哈希链，不是 JSON 日志。
当前 V2 的原子保存是单文件级别，跨 users/admin/couriers/expresses 的 saveAll() 还不是严格多文件事务。
```

### 8.4 下一步唯一主线

```text
下一步不是继续改 V2 主流程，而是在 logistics_v3_network 中搭建 V3 网络版。
第一目标是 server/client 双进程 socket 最小闭环。
首批命令建议优先实现登录、Session token、查询、寄件、分配、揽收、签收。
```

---

## 9. 最终结论

`logistics_v2` 当前已经完成题目二的核心要求，并形成了比基础实验更高阶的工程能力：

```text
面向对象实体设计
继承与多态计费
三角色权限隔离
三状态业务状态机
资金流转与提成分账
手动分配与智能自动分配
快递员绩效统计
管理员业务大屏
密码哈希与隐藏输入
三次失败自动冻结
原子化文件保存
日志哈希链防篡改
保存失败回滚
中文控制台兼容
```

V3 应以 V2 的 `LogisticsSystem` 为服务端核心，以 socket 协议替换 `ConsoleUI/App`，并引入 `SessionManager`、`ServerController`、`ProtocolCodec`、`SocketServer`、`SocketClient` 等网络版专属模块。

下一阶段第一目标：

```text
在 logistics_v3_network 中搭建 server/client 双进程骨架，并通过 socket 完成登录、查询、寄件、分配、揽收、签收的最小闭环。
```
