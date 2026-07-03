# v2_LOG01：Logistics_v2 Phase 0-5 全局状态序列化

## 0. 当前工程基线

项目路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v2
```

当前源码入口：

```text
src/main.cpp
```

构建与运行：

```text
build.bat
run.bat
bin/logistics_v2.exe
```

当前系统标题：

```text
【物流管理平台 V2 - 快递员任务管理系统】
```

当前数据目录：

```text
data/
```

当前核心数据文件：

```text
data/users.txt
data/admin.txt
data/couriers.txt
data/expresses.txt
data/operations.log
```

当前默认管理员：

```text
账号：admin
密码：Admin0219
```

当前已编译验证：

```text
cmd /c build.bat
```

构建通过，目标产物为：

```text
bin/logistics_v2.exe
```

Phase 0-5 已完成：

- Phase 0：独立项目、V2 二进制名、V2 数据目录隔离、标题更新、中文控制台兼容保留。
- Phase 1：快递三状态、12 字段快递持久化、待揽收初始状态、签收状态拦截。
- Phase 2：`ExpressItem` 多态计费真实落地。
- Phase 3：`Courier` 快递员实体、`CourierRepository`、快递员登录、管理员管理快递员。
- Phase 4：管理员分配快递员、快递员揽收、50% 提成分账、状态推进到待签收。
- Phase 5：`ExpressQueryCondition` 查询条件对象化，三类身份查询权限隔离。

---

## 1. V2 核心架构与多态落地快照 (Architecture & Polymorphism)

### 1.1 当前架构分层

当前仍为单文件实现，所有类都位于 `src/main.cpp`，但逻辑上保持 V1/V2 的分层：

```text
配置层：ProjectConfig
工具层：StringUtil / DirectoryUtil / InputValidator / SHA256 / PasswordHasher
控制台兼容层：WindowsConsoleOutputBuffer / ConsoleEnvironment
实体层：User / Admin / Courier / Express / LogEntry / ExpressItem 体系
仓储层：UserRepository / AdminRepository / CourierRepository / ExpressRepository / Logger
业务层：LogisticsSystem
UI 流程层：ConsoleUI / App
入口层：main()
```

`main()` 当前保持精简：

```cpp
int main() {
    ConsoleEnvironment::initialize();
    App app;
    app.run();
    return 0;
}
```

### 1.2 ProjectConfig

`ProjectConfig` 是 Phase 0 引入的 V2 配置集中点，当前包含：

```cpp
class ProjectConfig {
public:
    static constexpr const char* DisplayTitle = "【物流管理平台 V2 - 快递员任务管理系统】";
    static constexpr const wchar_t* WindowTitle = L"【物流管理平台 V2 - 快递员任务管理系统】";
    static constexpr const char* DataDirectoryName = "data";
    static constexpr const char* UserFileName = "users.txt";
    static constexpr const char* AdminFileName = "admin.txt";
    static constexpr const char* ExpressFileName = "expresses.txt";
    static constexpr const char* CourierFileName = "couriers.txt";
    static constexpr const char* LogFileName = "operations.log";
    static constexpr const char* PasswordHashScope = "|logistics_v2|";
};
```

意义：

- 避免 V2 读写 `logistics_v1` 的数据。
- 避免文件名散落硬编码。
- 密码哈希域从 V1 的 `|logistics_v1|` 切换到 `|logistics_v2|`。

### 1.3 角色与继承体系

当前尚未抽取 `Person` 基类。原因：

- V2 当前优先保证业务闭环稳定。
- `User`、`Admin`、`Courier` 虽有用户名、姓名、salt、hash 等共性，但余额、电话、地址、冻结/删除语义不同。
- 后续若做代码拆分和报告美化，可再抽取 `Person` 或 `AccountBase`，但目前不是必要路径。

当前角色：

```text
User      普通用户
Admin     物流公司管理员
Courier   快递员
```

#### Courier 实体字段

`Courier` 当前字段：

```cpp
std::string username_;      // 快递员唯一用户名
std::string name_;          // 快递员姓名
std::string phone_;         // 快递员电话
std::string salt_;          // 密码随机盐
std::string passwordHash_;  // 加盐后的密码哈希
double income_;             // 快递员账户余额/累计收入
bool frozen_;               // 是否被冻结
bool removed_;              // 是否已离职或逻辑删除
```

关键行为：

```cpp
bool verifyPassword(const std::string& password) const;
void addIncome(double amount);
void setFrozen(bool frozen);
void markRemoved();
std::string serialize() const;
bool deserialize(const std::string& line);
```

安全要点：

- 快递员密码不明文保存。
- 新增快递员时生成 `salt`，再保存 `SHA256(salt + "|logistics_v2|" + password)`。
- `removed == true` 表示逻辑删除/停用，登录时拒绝。
- `frozen == true` 表示冻结，登录时拒绝。
- 删除快递员不物理移除行，保留历史任务审计关系。

### 1.4 多态计费体系

Phase 2 已真实落地物品基类和三种子类。

抽象基类：

```cpp
class ExpressItem {
public:
    virtual ~ExpressItem() = default;
    virtual std::string typeCode() const = 0;
    virtual std::string typeName() const = 0;
    virtual std::string amountText() const = 0;
    virtual double getPrice() const = 0;
};
```

三种子类：

```text
NormalItem   普通快递：5 元/kg
FragileItem  易碎品：8 元/kg
BookItem     图书：2 元/本
```

计费规则：

```cpp
NormalItem::getPrice()  = weight * 5.0;
FragileItem::getPrice() = weight * 8.0;
BookItem::getPrice()    = count * 2.0;
```

类型代码：

```text
Normal
Fragile
Book
```

展示名称：

```text
普通快递
易碎品
图书
```

### 1.5 工厂模式与多态调用方式

当前没有在 `Express` 内部保存 `std::unique_ptr<ExpressItem>`，避免 `std::vector<Express>` 查询返回时触发拷贝构造和 `clone()` 重构。

当前策略：

```text
Express 只持久化 itemType_ 和 itemAmount_
需要计费或展示时由 ExpressItemFactory 临时创建多态对象
业务层通过基类指针调用 getPrice()
```

工厂接口：

```cpp
class ExpressItemFactory {
public:
    static std::unique_ptr<ExpressItem> createItem(const std::string& type, double amount);
};
```

`LogisticsSystem::sendExpress()` 中真实调用：

```cpp
std::unique_ptr<ExpressItem> item = ExpressItemFactory::createItem(itemType, itemAmount);
double fee = item->getPrice();
```

因此实验要求中的虚函数 `getPrice()` 不是摆设，实际参与扣费、余额不足判断、管理员账户入账、快递费用持久化。

### 1.6 查询条件对象化

Phase 5 新增 `ExpressQueryCondition`：

```cpp
struct ExpressQueryCondition {
    std::string sender;      // 发件人用户名，可为空
    std::string receiver;    // 收件人用户名，可为空
    std::string courier;     // 快递员用户名，可为空
    std::string expressId;   // 快递单号，可为空
    std::string startTime;   // 寄送起始时间，可为空
    std::string endTime;     // 寄送结束时间，可为空
    int statusFilter = -1;   // -1 表示全部状态
    std::string itemType;    // 物品类型代码，可为空
};
```

替代旧接口：

```cpp
sender, receiver, id, startTime, endTime, statusFilter
```

当前业务层查询接口：

```cpp
std::vector<Express> queryUserExpresses(const std::string& username,
                                        const ExpressQueryCondition& condition) const;

std::vector<Express> queryCourierExpresses(const std::string& username,
                                           const ExpressQueryCondition& condition) const;

std::vector<Express> queryAllExpresses(const ExpressQueryCondition& condition) const;
```

当前匹配函数：

```cpp
bool matchExpress(const Express& express, const ExpressQueryCondition& condition) const;
```

价值：

- 查询接口不再继续膨胀参数列表。
- 管理员、普通用户、快递员复用同一套筛选对象。
- 后续 Phase 6/V3 增加费用区间、揽收时间区间、签收时间区间时，只扩展对象字段。

---

## 2. 数据持久化与状态机确立 (Persistence & State Machine)

### 2.1 数据目录识别

`DirectoryUtil::projectDataDirectory()` 当前根据运行目录识别：

```text
从项目根目录运行 -> data
从 src 或 bin 运行 -> ../data
其他情况 -> data
```

主数据目录为 `logistics_v2/data`。

保存成功后会镜像核心数据到：

```text
src/data
bin/data
```

镜像文件包括：

```text
admin.txt
users.txt
couriers.txt
expresses.txt
```

日志 `operations.log` 主要以主数据目录为准。

### 2.2 文件存储协议

所有文件均为文本行协议：

- 一条记录一行。
- 字段使用 `|` 分隔。
- 字符串字段通过 `StringUtil::escape()` 转义。
- 支持 `%25`、`%7C`、`%0A`、`%0D` 转义。
- 读取时使用 `StringUtil::unescape()` 还原。

#### users.txt

字段顺序必须严格为：

```text
username|name|phone|salt|passwordHash|balance|address|frozen
```

含义：

```text
username      普通用户唯一用户名
name          姓名
phone         11 位电话
salt          密码随机盐
passwordHash  SHA-256 哈希
balance       用户余额，两位小数文本
address       默认地址
frozen        0/1，是否冻结
```

反序列化要求：

- 字段数必须为 8。
- `balance` 必须完整解析为非负数。
- `frozen` 必须为 `0` 或 `1`。
- 异常行跳过，计入 `UserRepository::lastInvalidLineCount_`。

#### admin.txt

字段顺序：

```text
username|name|salt|passwordHash|balance
```

含义：

```text
username      管理员账号，默认 admin
name          管理员姓名，默认 SystemAdmin
salt          密码随机盐
passwordHash  SHA-256 哈希
balance       公司/平台账户余额
```

反序列化要求：

- 字段数必须为 5。
- `balance` 必须完整解析为非负数。
- 文件不存在或异常时使用默认管理员对象。

#### couriers.txt

字段顺序必须严格为：

```text
username|name|phone|salt|passwordHash|income|frozen|removed
```

含义：

```text
username      快递员唯一用户名
name          快递员姓名
phone         快递员电话
salt          密码随机盐
passwordHash  SHA-256 哈希
income        快递员收入/余额
frozen        0/1，是否冻结
removed       0/1，是否逻辑删除/停用
```

反序列化要求：

- 字段数必须为 8。
- `income` 必须完整解析为非负数。
- `frozen` 和 `removed` 必须为 `0` 或 `1`。
- 异常行跳过，计入 `CourierRepository::lastInvalidLineCount_`。

#### expresses.txt

V2 新格式字段顺序必须严格为 12 字段：

```text
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee
```

含义：

```text
id           快递单号，格式 EX + 时间戳 + 序号
sender       发件用户名
receiver     收件用户名
courier      负责快递员用户名，未分配时为空
sendTime     寄件时间，YYYY-MM-DD HH:MM:SS
pickupTime   揽收时间，未揽收时为空
receiveTime  签收时间，未签收时为空
status       0/1/2，对应 WaitingPickup/WaitingSign/Signed
itemType     Normal/Fragile/Book
itemAmount   计费权值，普通/易碎品为 kg，图书为册数
description  物品描述
fee          本单总费用
```

`Express::deserialize()` 当前兼容 V1 旧格式 8 字段：

```text
id|sender|receiver|sendTime|receiveTime|status|description|fee
```

兼容映射：

- 旧字段 `courier` 补为空。
- 旧字段 `pickupTime` 补为空。
- 旧字段 `itemType` 补为 `Normal`。
- 旧字段 `itemAmount` 补为 `fee / 5.0`。
- V1 旧状态 `0` 原语义是待签收，迁移为 V2 `WaitingSign`。
- V1 旧状态 `1` 迁移为 V2 `Signed`。

这点非常重要：旧数据的 `0` 不能误判为 V2 的 `WaitingPickup`。

#### operations.log

当前日志字段：

```text
time|actorType|actor|action|result|detail
```

含义：

```text
time       操作时间
actorType  USER / ADMIN / COURIER / SYSTEM / GUEST
actor      操作账号
action     操作类型
result     SUCCESS / FAILED / DENIED
detail     详情
```

Phase 6 计划升级为哈希链时，可扩展为：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

### 2.3 三状态机

当前快递状态枚举：

```cpp
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};
```

中文映射：

```text
WaitingPickup -> 待揽收
WaitingSign   -> 待签收
Signed        -> 已签收
```

状态流转：

```text
用户寄件成功 -> WaitingPickup
管理员分配快递员 -> WaitingPickup（状态不变，只填 courier）
快递员揽收 -> WaitingSign，并写 pickupTime
收件用户签收 -> Signed，并写 receiveTime
```

禁止流转：

- 未分配快递员的快递不能被快递员揽收。
- 快递员不能揽收不属于自己的快递。
- 已经 `WaitingSign` 的快递不能重复揽收。
- `WaitingPickup` 的快递不能被收件人签收。
- `Signed` 的快递不能重复签收。

### 2.4 资金流转

#### 用户寄件

`LogisticsSystem::sendExpress()` 流程：

```text
校验发件用户存在且未冻结
校验收件用户存在且未冻结
校验不能给自己寄件
校验物品描述
通过 ExpressItemFactory 创建 ExpressItem
通过 item->getPrice() 多态计算 fee
校验用户余额 >= fee
扣除用户余额 fee
管理员余额增加 fee
创建 Express，状态为 WaitingPickup
保存 users/admin/couriers/expresses
保存失败则回滚用户余额、管理员余额和新增快递
记录日志
```

#### 管理员分配快递员

`LogisticsSystem::assignCourier()` 流程：

```text
校验快递单号存在
校验快递状态必须为 WaitingPickup
校验快递员存在
校验快递员 removed == false
校验快递员 frozen == false
设置 express.courier = courierUsername
状态保持 WaitingPickup
保存失败则回滚快递对象
记录 ASSIGN_COURIER 日志
```

#### 快递员揽收

`LogisticsSystem::pickupExpress()` 流程：

```text
校验快递单号存在
校验快递员账号存在、未冻结、未停用
校验 express.courier == 当前快递员
校验 express.status == WaitingPickup
commission = express.fee * 0.5
校验 admin.balance >= commission
admin.balance -= commission
courier.income += commission
express.status = WaitingSign
express.pickupTime = now()
保存 users/admin/couriers/expresses
保存失败则回滚 express/admin/courier
记录 PICKUP_EXPRESS 日志
```

#### 收件用户签收

`LogisticsSystem::signExpress()` 流程：

```text
校验快递单号存在
校验 express.receiver == 当前用户
如果 WaitingPickup：拒绝签收，写 DENIED
如果 Signed：拒绝重复签收
只允许 WaitingSign 签收
express.status = Signed
express.receiveTime = now()
保存失败则回滚 express
记录 SIGN_EXPRESS 日志
```

---

## 3. 权限隔离边界落实情况 (Permission Isolation Boundaries)

### 3.1 总原则

权限边界已经放在 `LogisticsSystem`，不是只靠 UI 菜单隐藏。

UI 负责：

- 收集输入。
- 做基础格式校验。
- 展示结果。

业务层负责：

- 角色数据可见性过滤。
- 越权操作拦截。
- 状态机合法性。
- 审计日志。

### 3.2 普通用户边界

普通用户只能看到：

```text
express.sender == username
或
express.receiver == username
```

当前实现：

```cpp
std::vector<Express> queryUserExpresses(const std::string& username,
                                        const ExpressQueryCondition& condition) const;
```

逻辑：

- 遍历全量 `expresses_`。
- 若单号条件命中某快递，但该快递不属于用户，则标记 `denied = true`。
- 不属于该用户的快递直接 `continue`。
- 只对用户相关快递执行 `matchExpress()`。
- 最后写日志：

```cpp
logger_.write("USER", username, "QUERY_EXPRESS",
              denied ? "DENIED" : "SUCCESS",
              denied ? "存在越权查询尝试" : "查询快递记录");
```

普通用户签收边界：

- 快递不存在：失败。
- `express.receiver() != username`：拒绝，写 `DENIED`。
- `express.isWaitingPickup()`：拒绝，写 `DENIED`，因为快递尚未被快递员揽收。
- `express.isSigned()`：拒绝重复签收。
- 只有 `WaitingSign` 允许签收。

已落实：

```text
普通用户无法查询无关快递。
普通用户无法签收他人快递。
普通用户无法签收待揽收快递。
越权签收和待揽收签收尝试会写 DENIED。
```

### 3.3 快递员边界

快递员只能看到：

```text
express.courier == courierUsername
```

当前实现：

```cpp
std::vector<Express> queryCourierExpresses(const std::string& username,
                                           const ExpressQueryCondition& condition) const;
```

防线：

- 若 `condition.courier` 非空且不是本人，标记 `denied = true`。
- 若单号条件命中某快递，但该快递不属于该快递员，标记 `denied = true`。
- 不属于该快递员的快递直接跳过。
- 匹配前强制覆盖：

```cpp
ExpressQueryCondition scoped = condition;
scoped.courier = username;
```

即使未来 UI 或 socket 客户端传入其他快递员用户名，业务层仍强制按本人过滤。

日志：

```cpp
logger_.write("COURIER", username, "QUERY_EXPRESS",
              denied ? "DENIED" : "SUCCESS",
              denied ? "存在越权查询其他快递员任务尝试" : "查询本人任务");
```

快递员揽收边界：

```cpp
if (!express.belongsToCourier(courierUsername)) {
    message = "无权揽收该快递。";
    logger_.write("COURIER", courierUsername, "PICKUP_EXPRESS", "DENIED",
                  "试图揽收非本人任务 " + expressId);
    return false;
}
```

已落实：

```text
快递员无法查询其他快递员任务。
快递员无法揽收他人任务。
快递员无法重复揽收已变为待签收的任务。
揽收他人任务写 DENIED。
```

### 3.4 管理员边界

管理员拥有全局权限：

```cpp
std::vector<Express> queryAllExpresses(const ExpressQueryCondition& condition) const;
```

管理员可按以下维度筛选：

```text
sender
receiver
courier
expressId
startTime
endTime
statusFilter
itemType
```

管理员菜单已支持：

- 查看所有用户。
- 查询全部快递。
- 冻结/解冻用户。
- 查看操作日志。
- 查看快递员列表。
- 新增快递员。
- 删除/停用快递员。
- 冻结/解冻快递员。
- 分配快递员。

### 3.5 当前 DENIED 日志覆盖

当前明确写入 `DENIED` 的场景：

```text
普通用户签收他人快递
普通用户签收待揽收快递
普通用户按单号查询无关快递
快递员按单号查询非本人任务
快递员尝试传入其他 courier 条件
快递员揽收非本人任务
```

仍可增强：

- 管理员分配快递给冻结/停用快递员当前记为 `FAILED`，不是 `DENIED`。这是业务非法状态，不是越权，当前设计合理。
- 用户登录失败、密码错误、余额不足等记为 `FAILED`。

---

## 4. Phase 6 待办事项预留 (Pending Hooks for Phase 6)

### 4.1 原子化保存 `.tmp -> .bak -> 正式文件`

当前保存方式：

```cpp
std::ofstream out(filePath_, std::ios::trunc);
```

当前仓储类：

```text
UserRepository::save()
AdminRepository::save()
CourierRepository::save()
ExpressRepository::save()
```

当前问题：

- 直接覆盖正式文件。
- 如果写入过程中断电或程序崩溃，可能留下半文件。
- `saveAll()` 同时保存多个文件，但不是事务型提交。

Phase 6 切入点：

新增：

```cpp
class StorageManager
```

建议接口：

```cpp
static bool saveTextAtomically(const std::string& filePath, const std::vector<std::string>& lines);
static bool backupFile(const std::string& filePath);
static bool replaceFile(const std::string& tmpPath, const std::string& filePath);
```

替换所有 Repository 的 `save()` 内部写法：

```text
写 file.tmp
flush/close 成功
旧 file -> file.bak
file.tmp -> file
失败时保留旧 file
```

注意：

- Windows 下 `rename`/覆盖行为需谨慎。
- 如果目标文件存在，可能需要先删除或移动到 `.bak`。
- 不要用 shell 命令做保存逻辑，应使用 C++ 文件 API。

### 4.2 日志哈希链

当前日志类：

```cpp
class Logger {
private:
    std::string filePath_;
public:
    void write(...);
    std::vector<LogEntry> load() const;
};
```

当前日志格式：

```text
time|actorType|actor|action|result|detail
```

Phase 6 目标格式：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

建议新增字段：

```cpp
int sequence_;
std::string prevHash_;
std::string currentHash_;
```

哈希计算建议：

```text
currentHash = SHA256(seq + "|" + time + "|" + actorType + "|" + actor + "|" + action + "|" + result + "|" + detail + "|" + prevHash)
```

切入点：

- `Logger::write()`：读取最后一条日志 hash，生成下一条。
- `Logger::load()`：兼容旧 6 字段和新 9 字段。
- 新增：

```cpp
bool Logger::verifyHashChain(std::string& message) const;
```

管理员菜单新增：

```text
校验日志完整性
```

### 4.3 登录失败 3 次自动冻结

当前实现：

- 用户登录、管理员登录、快递员登录在 UI 流程中最多尝试 3 次。
- 失败 3 次后返回上级菜单。
- 不会持久化失败次数。
- 不会自动冻结账号。

切入点：

```cpp
loginUser()
loginCourier()
loginAdmin()
```

建议不要让 `App` 负责冻结逻辑，而应放在 `LogisticsSystem`。

可选设计：

#### 方案 A：扩展 users.txt / couriers.txt

新增字段：

```text
failedLoginCount
```

缺点：

- 改持久化协议，需要兼容旧字段数。

#### 方案 B：新增 auth_state.txt

格式：

```text
role|username|failedCount|lastFailedTime
```

优点：

- 不污染用户/快递员实体。
- 适合未来 V3 服务端统一认证。

推荐方案 B。

规则：

```text
密码错误 -> failedCount + 1
连续 3 次 -> 冻结用户/快递员
登录成功 -> failedCount 清零
管理员账号建议不自动冻结，可仅记录 FAILED，避免系统锁死
```

### 4.4 中英文混合表格对齐

当前表格输出：

```cpp
std::setw(...)
```

问题：

- `std::setw` 以字节/字符为单位，不感知中文显示宽度。
- GBK/中文控制台下可能轻微错列。

当前表格函数：

```cpp
ConsoleUI::printExpressTable()
ConsoleUI::printUserTable()
ConsoleUI::printCourierTable()
ConsoleUI::printLogTable()
```

Phase 6 切入点：

新增：

```cpp
class TablePrinter
```

建议能力：

```cpp
static int displayWidth(const std::string& text);
static std::string padRight(const std::string& text, int width);
static std::string truncateToWidth(const std::string& text, int width);
```

宽度估算策略：

- ASCII 字符宽度 1。
- 中文或高位字节按宽度 2。
- GBK 编译下需谨慎处理多字节，简单策略可按 `unsigned char >= 0x80` 合并估算。

### 4.5 查询结果减少拷贝

当前查询返回：

```cpp
std::vector<Express>
```

当前可接受，因为 `Express` 只保存普通字段，没有 `unique_ptr`。

若后续把 `ExpressItem` 真正放入 `Express`，会触发拷贝问题。

Phase 6/V3 可考虑：

```cpp
std::vector<std::size_t> queryExpressIndexes(...)
std::vector<const Express*> queryExpressRefs(...)
```

或保持当前字段化持久化策略，继续用 `ExpressItemFactory` 临时创建多态对象。

---

## 5. 桥接 V3 网络版的前瞻预审 (Bridge to V3 C/S Architecture)

### 5.1 V3 题目要求

题目三要求：

```text
传统 C/S 架构
客户端与服务器端为不同进程
使用 socket 通信
不能使用 RPC 框架
网络版功能要求与单机版一致
```

### 5.2 当前最重要的好消息

当前 `LogisticsSystem` 内部没有发现：

```cpp
std::cin
std::cout
ConsoleUI
readLine
```

这些 UI 耦合。

也就是说，当前核心业务层已经基本适合作为 V3 服务端的业务服务层复用。

`std::cin/std::cout` 主要存在于：

```text
ConsoleUI
App
ConsoleEnvironment
WindowsConsoleOutputBuffer
```

这些都属于控制台客户端/UI 层，不属于核心业务层。

### 5.3 当前业务接口对 V3 的适配度

当前 `LogisticsSystem` 大多数接口已是参数输入 + message 输出：

```cpp
bool registerUser(..., std::string& message);
bool loginUser(..., std::string& message) const;
bool loginCourier(..., std::string& message) const;
bool loginAdmin(..., std::string& message) const;
bool recharge(..., std::string& message);
bool sendExpress(..., std::string& expressId, std::string& message);
bool signExpress(..., std::string& message);
bool createCourier(..., std::string& message);
bool removeCourier(..., std::string& message);
bool setCourierFrozen(..., std::string& message);
bool assignCourier(..., std::string& message);
bool pickupExpress(..., std::string& message);
std::vector<Express> queryUserExpresses(...);
std::vector<Express> queryCourierExpresses(...);
std::vector<Express> queryAllExpresses(...);
```

这非常适合 V3 服务端：

```text
socket 收到命令字符串
解析为参数对象
调用 LogisticsSystem
把 bool/message/数据列表序列化回客户端
```

### 5.4 仍需为 V3 解耦的点

#### App 当前仍承担输入编排

`App` 中含有大量：

```cpp
ConsoleUI::readLine()
ConsoleUI::readInt()
ConsoleUI::confirm()
std::cout
ConsoleUI::pause()
```

V3 不应复用 `App` 作为服务端逻辑。

V3 应新增：

```cpp
class CommandParser;
class ServerController;
class ResponseBuilder;
```

服务端流程：

```text
Client socket request
-> CommandParser 解析命令与参数
-> ServerController 调用 LogisticsSystem
-> ResponseBuilder 序列化响应
-> socket send
```

#### ConsoleUI 表格输出不能进入服务端响应

当前 `ConsoleUI::printExpressTable()` 直接输出表格。

V3 应让业务层返回结构化数据：

```cpp
std::vector<Express>
std::vector<User>
std::vector<Courier>
std::vector<LogEntry>
```

然后服务端序列化为协议文本，例如：

```text
OK|count|...
ERR|message
DATA|EXPRESS|...
```

#### message 字符串可保留，但建议加状态码

当前业务层通过：

```cpp
bool + std::string message
```

返回结果。

V3 可升级为：

```cpp
struct ServiceResult {
    bool ok;
    std::string code;
    std::string message;
};
```

示例 code：

```text
OK
INVALID_INPUT
AUTH_FAILED
PERMISSION_DENIED
NOT_FOUND
BALANCE_NOT_ENOUGH
STATE_CONFLICT
STORAGE_FAILED
```

但为了不大改 V2，V3 可以先复用 `bool + message`。

### 5.5 V3 推荐接口层

建议新增命令协议，不使用 RPC：

```text
LOGIN_USER|username|password
LOGIN_COURIER|username|password
LOGIN_ADMIN|username|password
REGISTER_USER|username|name|phone|password|address
RECHARGE|username|amount
SEND_EXPRESS|sender|receiver|description|itemType|itemAmount
ASSIGN_COURIER|expressId|courierUsername
PICKUP_EXPRESS|expressId|courierUsername
SIGN_EXPRESS|username|expressId
QUERY_USER_EXPRESS|username|sender|receiver|courier|expressId|start|end|status|itemType
QUERY_COURIER_EXPRESS|courierUsername|sender|receiver|courier|expressId|start|end|status|itemType
QUERY_ADMIN_EXPRESS|sender|receiver|courier|expressId|start|end|status|itemType
```

与 `ExpressQueryCondition` 的映射很直接。

### 5.6 V3 数据安全预警

V3 比 V2 多一个安全问题：

```text
客户端传来的身份 username 不可信
```

V2 当前是单机菜单，登录后把 username 传给后续业务函数。

V3 服务端应维护会话：

```cpp
struct Session {
    std::string token;
    std::string role;
    std::string username;
};
```

客户端登录成功后拿到 token。

后续请求：

```text
token + command + params
```

服务端根据 token 查身份，而不是相信客户端传来的 username。

### 5.7 V3 最小迁移策略

推荐不要推翻当前 V2：

1. 保留 `LogisticsSystem`。
2. 保留所有实体、仓储、工具、日志类。
3. 新增 `ServerApp` 替代 `App`。
4. 新增 `SocketServer` 负责网络收发。
5. 新增 `CommandParser` 解析文本协议。
6. 新增 `ProtocolCodec` 序列化业务对象。
7. 客户端可以是控制台程序，负责输入输出；服务端不出现 `std::cin` 交互。

### 5.8 当前代码中 UI 耦合审查结论

审查结果：

```text
LogisticsSystem：未发现 std::cin/std::cout/ConsoleUI/readLine
Repository：未发现 std::cin/std::cout/ConsoleUI/readLine
Entity：未发现 std::cin/std::cout/ConsoleUI/readLine
Utility：除 WindowsConsoleOutputBuffer/ConsoleEnvironment 外无业务 UI 耦合
ConsoleUI/App：大量 std::cin/std::cout，属于 UI 层，V3 服务端不复用
```

因此当前 V2 对 V3 的最大优势是：

```text
核心业务服务层已经基本可直接被 socket 服务端调用。
```

---

## 6. 当前验收演示链路建议

当前 V2 最完整演示链路：

```text
1. 启动程序，展示 V2 主菜单：用户注册、用户登录、快递员登录、管理员登录。
2. 管理员登录 admin/Admin0219。
3. 管理员新增快递员 courier001。
4. 注册或登录两个普通用户。
5. 发件用户充值。
6. 发件用户选择普通/易碎品/图书，输入重量或册数，系统通过多态 getPrice() 计算费用。
7. 寄件成功后，快递状态为待揽收。
8. 管理员按状态查询待揽收快递，并分配给 courier001。
9. courier001 登录，查看待揽收任务。
10. courier001 单个/批量/全部揽收。
11. 揽收后状态变为待签收，pickupTime 写入，管理员余额减少 50%，快递员收入增加 50%。
12. 收件用户登录，查看待签收快递并签收。
13. 用户、快递员、管理员分别查询，展示权限边界。
14. 管理员查看日志，展示 SUCCESS/FAILED/DENIED 记录。
```

当前测试数据中已存在示例：

```text
courier001 / Cour123
```

以及至少一条已分配并揽收的快递：

```text
EX202605192307360002
状态：WaitingSign
快递员：courier001
费用：8.00
快递员提成：4.00
```

---

## 7. 后续修改红线

继续开发 Phase 6/V3 时必须遵守：

- 不要破坏 `ProjectConfig` 的 V2 数据隔离。
- 不要让任何密码明文进入 `users.txt`、`admin.txt`、`couriers.txt`。
- 不要绕过 `LogisticsSystem` 做权限过滤。
- 不要让 UI 直接遍历全量快递后过滤给用户或快递员。
- 不要让快递员揽收非本人任务。
- 不要让用户签收待揽收快递。
- 不要让 `ExpressItem::getPrice()` 变成未使用代码。
- 不要把业务逻辑重新堆入 `main()`。
- 不要在 `LogisticsSystem` 中加入 `std::cin/std::cout`，否则会伤害 V3 服务端复用。
- 每次修改后执行 `cmd /c build.bat`。
- 修改中文输出或脚本后，至少运行一次主菜单，确认中文显示方案没有被破坏。

