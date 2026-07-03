# v1_LOG_SUM：Logistics_v1 全局总结与 V2 启动上下文

## 1. V1 核心架构与类图快照 (Architecture Snapshot)

### 1.1 当前工程状态

项目路径：

```text
C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v1
```

当前 V1 是一个基于 C++ 面向对象思想实现的单机控制台物流管理系统。代码主入口集中在：

```text
src/main.cpp
```

构建和运行脚本：

```text
build.bat
run.bat
```

当前默认管理员：

```text
账号：admin
密码：Admin0219
```

密码不明文保存，使用随机 salt + SHA-256 hash。

### 1.2 当前已实现类快照

#### `enum class ExpressStatus`

当前 V1 快递状态：

```cpp
WaitingSign = 0
Signed = 1
```

含义：

- `WaitingSign`：待签收。
- `Signed`：已签收。

V2 需要扩展为：

```text
待揽收 -> 待签收 -> 已签收
```

当前枚举需要重构或扩展。

#### `StringUtil`

职责：字符串、时间、金额、序列化辅助工具。

关键接口：

- `trim()`：去除首尾空白。
- `contains()`：判断字符串包含。
- `split()`：按分隔符拆分。
- `escape()` / `unescape()`：持久化文本字段转义。
- `moneyToString()`：金额格式化为两位小数。
- `now()`：获取 `YYYY-MM-DD HH:MM:SS` 时间。
- `compactNow()`：生成单号用紧凑时间戳。
- `isDigits()`：判断纯数字。
- `tryParseDouble()`：严格解析完整 double，拒绝 `12abc`。
- `tryParsePositiveMoney()`：解析正数金额，最多两位小数。

#### `DirectoryUtil`

职责：目录和路径处理。

关键接口：

- `exists()`：判断路径存在。
- `createDirectory()`：创建目录。
- `join()`：拼接目录和文件名。
- `projectDataDirectory()`：识别主数据目录。
- `copyTextFile()`：复制核心数据文件到镜像目录。

当前数据目录识别逻辑基于相对路径判断根目录、`src`、`bin`。一般运行没问题，但 V2 可考虑根据可执行文件路径定位项目根目录。

#### `WindowsConsoleOutputBuffer`

职责：Windows 中文控制台输出兼容。

背景：开发中曾出现中文菜单空白、乱码、只显示 ASCII 分隔线等问题。最终方案为：

- 源码 UTF-8 保存。
- 编译时 `-finput-charset=UTF-8 -fexec-charset=GBK`。
- 运行时使用系统中文代码页读取输入。
- 输出时将 GBK 字节转换为 UTF-16。
- 使用 `WriteConsoleW` 写入控制台。

后续红线：不要轻易改动此层。修改后必须重编译 `bin/logistics_v1.exe` 和 `src/main.exe`，并验证中文主菜单。

#### `ConsoleEnvironment`

职责：程序启动时初始化控制台环境。

关键接口：

- `initialize()`：设置 Windows 控制台代码页，挂接 `WindowsConsoleOutputBuffer`。

#### `SHA256`

职责：自实现 SHA-256 摘要算法。

关键接口：

- `hash()`：返回输入字符串的 SHA-256 十六进制摘要。

#### `PasswordHasher`

职责：密码安全封装。

关键接口：

- `defaultAdminPassword()`：返回默认管理员密码 `Admin0219`。
- `makeSalt()`：生成随机 salt。
- `hashPassword()`：根据 salt 和明文密码计算 hash。
- `verify()`：验证输入密码和存储 hash 是否一致。

当前密码策略：

- 密码长度 6-30。
- 必须同时包含字母和数字。
- 不能包含空白字符。
- 数据文件只保存 salt 和 hash，不保存明文。

#### `InputValidator`

职责：集中处理所有输入合法性规则。

关键接口：

- `isValidUsername()`：3-20 位，只允许字母、数字、下划线。
- `isNotBlank()`：非空且无控制字符。
- `isValidName()`：姓名非空、无控制字符、长度不超过 30。
- `isValidAddress()`：地址非空、长度 2-80、无控制字符。
- `isValidPhone()`：11 位数字。
- `isValidPassword()`：6-30 位，必须包含字母和数字，不能有空白。
- `isValidDescription()`：物品描述非空，长度不超过 60。
- `isValidOptionalUsername()`：查询条件中的可选用户名。
- `isValidOptionalExpressId()`：可选快递单号，以 `EX` 开头，只含字母数字。
- `isValidDateTimeOrEmpty()`：可选时间，格式 `YYYY-MM-DD HH:MM:SS`，检查真实年月日。
- `isValidOptionalKeyword()`：日志关键字筛选。

当前设计原则：UI 层做即时校验，业务层也对关键规则兜底，避免绕过 UI 写入非法数据。

#### `User`

职责：普通用户实体。

核心数据成员：

- `username_`：唯一用户名。
- `name_`：真实姓名。
- `phone_`：手机号。
- `salt_`：密码盐。
- `passwordHash_`：密码哈希。
- `balance_`：用户余额。
- `address_`：默认地址。
- `frozen_`：冻结状态。

关键接口：

- getter：`username()`、`name()`、`phone()`、`salt()`、`passwordHash()`、`balance()`、`address()`、`frozen()`。
- `updatePassword()`：更新 salt 和 hash。
- `recharge()`：充值或余额调整。
- `deduct()`：扣费，余额不足返回 false。
- `setFrozen()`：冻结/解冻。
- `serialize()`：序列化为文件行。
- `deserialize()`：从文件行反序列化，当前已严格检查余额和冻结字段。

#### `Admin`

职责：管理员实体和平台账户。

核心数据成员：

- `username_`：管理员账号，默认 `admin`。
- `name_`：管理员姓名，默认 `SystemAdmin`。
- `salt_`：密码盐。
- `passwordHash_`：密码哈希。
- `balance_`：平台/公司账户余额。

关键接口：

- getter：`username()`、`name()`、`salt()`、`passwordHash()`、`balance()`。
- `addBalance()`：增加或调整平台账户余额。
- `resetPassword()`：重置管理员密码。
- `serialize()` / `deserialize()`：持久化，管理员余额必须完整解析且非负。

#### `Express`

职责：快递实体。

核心数据成员：

- `id_`：快递单号。
- `sender_`：发件用户名。
- `receiver_`：收件用户名。
- `sendTime_`：寄送时间。
- `receiveTime_`：签收时间。
- `status_`：当前状态，V1 只有待签收/已签收。
- `description_`：物品描述。
- `fee_`：本次费用，V1 固定 15.00 元。

关键接口：

- getter：`id()`、`sender()`、`receiver()`、`sendTime()`、`receiveTime()`、`status()`、`description()`、`fee()`。
- `belongsTo()`：判断用户是否与快递相关。
- `isWaitingSign()`：是否待签收。
- `sign()`：设置为已签收并写入签收时间。
- `statusName()`：状态中文名。
- `serialize()` / `deserialize()`：持久化，状态和费用已严格校验。

V2 重构重点：`Express` 需要新增快递员字段、物品类别字段、状态流转字段，并从固定费用过渡到 `ExpressItem::getPrice()` 多态计费。

#### `LogEntry`

职责：操作日志实体。

核心数据成员：

- `time_`：操作时间。
- `actorType_`：操作人身份，如 `USER`、`ADMIN`、`SYSTEM`、`GUEST`。
- `actor_`：操作人账号。
- `action_`：操作类型。
- `result_`：结果，如 `SUCCESS`、`FAILED`、`DENIED`。
- `detail_`：详情。

关键接口：

- getter：`time()`、`actorType()`、`actor()`、`action()`、`result()`、`detail()`。
- `serialize()` / `deserialize()`。

#### `UserRepository`

职责：普通用户文件仓储。

核心数据成员：

- `filePath_`：用户数据文件路径。
- `lastInvalidLineCount_`：最近一次加载跳过的异常行数量。

关键接口：

- `load()`：读取用户列表，跳过异常行。
- `save()`：保存用户列表。
- `lastInvalidLineCount()`：返回异常行数量。

#### `AdminRepository`

职责：管理员文件仓储。

关键接口：

- `load()`：读取管理员数据，文件不存在时返回默认管理员。
- `save()`：保存管理员数据。
- `lastInvalidLineCount()`：返回异常行数量。

#### `ExpressRepository`

职责：快递文件仓储。

关键接口：

- `load()`：读取快递列表，跳过异常行。
- `save()`：保存快递列表。
- `lastInvalidLineCount()`：返回异常行数量。

#### `Logger`

职责：系统日志读写。

核心数据成员：

- `filePath_`：日志文件路径。

关键接口：

- `write()`：追加日志。
- `load()`：读取日志。

日志只通过管理员菜单查看，普通用户没有日志入口。

#### `ConsoleUI`

职责：控制台 UI 通用能力。

关键接口：

- `clear()`：输出空行模拟清屏。
- `title()`：统一页面标题。
- `message()`：消息输出。
- `pause()`：等待回车。
- `readLine()`：读取并 trim。
- `readInt()`：读取范围内整数。
- `readMoney()`：读取合法金额。
- `confirm()`：Y/N 确认。
- `printExpressTable()`：快递表格。
- `printUserTable()`：用户表格。
- `printLogTable()`：日志表格。

#### `LogisticsSystem`

职责：核心业务服务类。

核心数据成员：

- `BaseFee`：V1 固定运费 15.00。
- `dataDir_`：主数据目录。
- `userRepo_`、`adminRepo_`、`expressRepo_`：仓储对象。
- `logger_`：日志对象。
- `users_`：内存用户列表。
- `expresses_`：内存快递列表。
- `admin_`：管理员对象。

关键私有接口：

- `findUserIndex()`：查找用户。
- `findExpressIndex()`：查找快递。
- `makeExpressId()`：生成唯一快递单号，格式 `EX + 时间 + 四位序号`。
- `saveAll()`：保存用户、管理员、快递核心数据。
- `reportSaveFailure()`：统一保存失败提示和日志。
- `mirrorCoreDataFiles()`：镜像核心数据到 `src/data`、`bin/data`。
- `logDataLoadWarnings()`：启动时记录异常数据行。
- `matchExpress()`：快递查询条件匹配。

关键公共接口：

- `initialize()`：创建目录、加载数据、记录启动日志。
- `users()`：返回用户列表。
- `admin()`：返回管理员。
- `getUser()`：查询用户指针。
- `registerUser()`：注册用户，业务层兜底校验姓名/地址/密码/手机号。
- `loginUser()`：普通用户登录，冻结用户不可登录。
- `loginAdmin()`：管理员登录。
- `changeUserPassword()`：改密，保存失败回滚。
- `recharge()`：充值，保存失败回滚。
- `sendExpress()`：寄件，扣费、平台收入、生成单号，保存失败回滚。
- `signExpress()`：签收，权限校验，保存失败回滚。
- `waitingExpressesFor()`：查询用户未签收快递。
- `queryUserExpresses()`：普通用户查询，先做权限过滤。
- `queryAllExpresses()`：管理员查询所有快递。
- `setUserFrozen()`：冻结/解冻用户，保存失败回滚。
- `queryLogs()`：管理员筛选日志。

#### `App`

职责：顶层应用流程控制。

核心数据成员：

- `system_`：`LogisticsSystem` 实例。

关键接口：

- 输入读取：`readRegisterUsername()`、`readUsername()`、`readName()`、`readPhone()`、`readAddress()`、`readPassword()`、`readConfirmedPassword()`、`readReceiverUsername()`、`readOptionalUsername()`、`readOptionalExpressId()`、`readLogResultFilter()`、`readOptionalKeyword()`。
- 菜单流程：`showMainMenu()`、`registerFlow()`、`userLoginFlow()`、`adminLoginFlow()`、`userMenu()`、`adminMenu()`。
- 用户功能：`changePasswordFlow()`、`showBalanceFlow()`、`rechargeFlow()`、`chooseDescription()`、`sendExpressFlow()`、`receiveExpressFlow()`、`signByIndexText()`、`userQueryExpressFlow()。
- 管理员功能：`adminQueryExpressFlow()`、`setUserStatusFlow()`、`viewLogFlow()`。
- `run()`：主循环。

### 1.3 文件持久化实现方案

主数据目录：

```text
data/
```

核心数据文件：

```text
data/users.txt
data/admin.txt
data/expresses.txt
data/operations.log
```

镜像目录：

```text
src/data/
bin/data/
```

说明：

- 业务真实读写以项目根目录 `data/` 为准。
- 保存成功后，核心数据文件 `admin.txt`、`users.txt`、`expresses.txt` 会镜像到 `src/data` 和 `bin/data`，方便查看。
- 日志 `operations.log` 不作为核心业务数据镜像，主要以主数据目录为准。

文件格式：

- 每个对象一行。
- 字段使用 `|` 分隔。
- 文本字段通过 `%25`、`%7C`、`%0A`、`%0D` 转义特殊字符。
- 用户、管理员、快递、日志各自有 `serialize()` 和 `deserialize()`。

当前数据格式概念：

```text
users.txt:
username|name|phone|salt|passwordHash|balance|address|frozen

admin.txt:
username|name|salt|passwordHash|balance

expresses.txt:
id|sender|receiver|sendTime|receiveTime|status|description|fee

operations.log:
time|actorType|actor|action|result|detail
```

读写控制：

- 仓储类负责读写文件。
- `load()` 跳过空行和异常行。
- 异常行数量保存在 `lastInvalidLineCount_`。
- 系统启动后通过 `logDataLoadWarnings()` 写入数据异常日志。
- 保存使用 `std::ofstream(filePath_, std::ios::trunc)` 直接覆盖。
- `LogisticsSystem::saveAll()` 同时保存用户、管理员、快递。
- 业务层已检查 `saveAll()` 返回值，并在注册、改密、充值、寄件、签收、冻结/解冻失败时尝试回滚内存状态。

当前持久化技术债：

- 保存不是原子化写入。
- 如果写文件过程中断电或系统崩溃，仍有文件损坏风险。
- V2 建议实现 `tmp -> bak -> rename` 原子保存机制。

## 2. 加分项与核心功能落实情况 (Features & Innovations)

### 2.1 终端 UI 交互逻辑

当前 V1 采用页面式 CLI：

- 主菜单。
- 用户注册页。
- 用户登录页。
- 管理员登录页。
- 用户功能菜单。
- 管理员功能菜单。
- 快递查询结果表。
- 用户信息表。
- 操作日志表。

统一 UI 机制：

- `ConsoleUI::title()` 输出统一标题和分隔线。
- `ConsoleUI::readInt()` 统一读取菜单整数并限制范围。
- `ConsoleUI::readMoney()` 统一读取金额并限制格式。
- `ConsoleUI::confirm()` 用于充值、寄件、全部签收等确认操作。
- 用户登录后顶部显示当前用户、余额、未签收数量。

快捷功能：

- 寄件时提供物品描述快捷选项：文件资料、衣物鞋帽、数码配件、食品零食、日用品、自定义。
- 收件时支持单个签收、批量签收、全部签收。
- 批量签收支持空格或逗号分隔序号，并跳过非法、越界、重复序号。

中文显示最终方案：

- 编译字符串常量为 GBK。
- 运行脚本使用 `chcp 936`。
- Windows 下输出转换为 UTF-16 并调用 `WriteConsoleW`。
- 不要回退到单纯 `chcp 65001` 方案，历史上该方案在用户本机造成中文空白或乱码。

### 2.2 密码哈希存储与安全验证机制

当前密码安全方案：

```text
salt = PasswordHasher::makeSalt(username)
hash = SHA256(salt + "|logistics_v1|" + password)
```

保存：

- 用户文件和管理员文件只保存 salt 与 passwordHash。
- 不保存明文密码。

验证：

- 登录时读取 salt。
- 将用户输入密码和 salt 重新计算 hash。
- 与文件中 passwordHash 比较。

密码规则：

- 长度 6-30。
- 必须同时包含字母和数字。
- 不能包含空白字符。
- 注册和改密均要求二次确认。
- 改密需要旧密码。
- 新密码不能和旧密码相同。

登录控制：

- 用户登录连续 3 次失败后返回上级菜单。
- 管理员登录连续 3 次失败后返回上级菜单。
- 用户被冻结后不能登录。

注意：当前“三次失败”不是永久账号锁定，而是当前登录流程限制。若 V2 需要更强安全性，可将失败次数持久化到用户对象或单独认证记录中。

### 2.3 权限控制与日志系统

普通用户权限：

- 只能查询自己发出的快递。
- 只能查询自己接收的快递。
- 不能签收别人的快递。
- 查询他人单号时不返回结果，并写入 `DENIED` 日志。

管理员权限：

- 查看所有用户。
- 查询全部快递。
- 冻结/解冻用户。
- 查看操作日志。

关键设计：权限控制在 `LogisticsSystem` 业务层完成，而不是只靠 UI 菜单隐藏。即使 UI 层传入非法条件，业务层仍会过滤。

日志系统：

```text
data/operations.log
```

日志字段：

```text
时间 | 操作人身份 | 操作人账号 | 操作类型 | 操作结果 | 详情
```

当前记录场景：

- 系统启动。
- 数据异常行。
- 保存失败。
- 注册成功/失败。
- 用户登录成功/失败。
- 管理员登录成功/失败。
- 修改密码成功/失败。
- 充值。
- 寄件成功/失败。
- 签收成功/失败/越权。
- 查询快递。
- 越权查询。
- 管理员查询快递。
- 管理员查看日志。
- 管理员冻结/解冻用户。

管理员可按操作人、操作类型关键字、结果筛选日志。

### 2.4 输入检查与异常场景覆盖

当前已覆盖输入：

- 用户名：3-20 位，字母/数字/下划线。
- 姓名：非空、无控制字符、长度不超过 30。
- 电话：11 位数字，注册时检查重复。
- 地址：非空，长度 2-80，无控制字符。
- 密码：6-30，必须含字母和数字。
- 金额：正数，最多两位小数；拒绝 `1.234`、`.5`、`1.`、`1e3`、字母、负数。
- 菜单：范围内整数。
- 物品描述：非空，长度不超过 60。
- 快递单号：可空；非空以 `EX` 开头，仅字母数字。
- 查询时间：可空；非空符合 `YYYY-MM-DD HH:MM:SS` 且日期真实。
- 状态筛选：全部、待签收、已签收。
- 日志筛选：结果仅 `SUCCESS`、`FAILED`、`DENIED` 或空。
- 批量签收序号：数字、范围内、不重复。

异常工况：

- 余额不足：寄件失败，提示当前余额和所需费用。
- 收件人不存在：寄件失败。
- 给自己寄件：禁止。
- 收件人被冻结：禁止寄件给该用户。
- 发件人被冻结：禁止寄件。
- 重复签收：拒绝。
- 签收他人快递：拒绝并记 `DENIED`。
- 文件异常行：跳过并记日志。
- 保存失败：提示失败并回滚内存状态。
- 快递单号冲突：生成单号时循环递增序号直到唯一。

### 2.5 已讨论并采纳或预留的 C++ 创新优化点

已落地：

- 自实现 SHA-256。
- `const` 成员函数广泛使用。
- 实体/仓储/业务/UI/工具分层。
- Windows 控制台输出缓冲类 `WindowsConsoleOutputBuffer`。
- 业务层保存失败回滚。
- 数据文件异常行统计与日志记录。

已讨论但 V1 尚未完全落地，适合 V2 或重构阶段加入：

- `ExpressQueryCondition` 查询条件对象，替代超长参数列表。
- `LogisticsException`、`InputException`、`AuthException`、`PermissionException`、`BalanceException`、`StorageException` 异常类体系。
- 原子化保存：写 `.tmp`，备份 `.bak`，再 rename 替换。
- 审计日志哈希链：`prevHash + currentHash` 检测日志篡改。
- `Person` 基类 + `User/Admin/Courier` 继承体系。
- `ExpressItem` 抽象基类 + `FragileItem/BookItem/NormalItem` 子类，使用虚函数 `getPrice()` 多态计费。
- 可考虑使用 `std::unique_ptr<ExpressItem>` 存储快递物品，体现智能指针和多态所有权。

## 3. 已知问题与待优化技术债 (Tech Debt)

### 3.1 文件保存尚未原子化

当前 `Repository::save()` 直接使用 `std::ofstream(..., std::ios::trunc)` 覆盖写入。

已改善：

- 业务层会检查 `saveAll()`。
- 保存失败时回滚内存状态。

仍存在：

- 如果文件写到一半断电或程序崩溃，正式文件可能损坏。
- `saveAll()` 需要同时保存多个文件，当前不是事务型保存。

V2 建议：

- 引入 `StorageManager`。
- 每个文件先写 `.tmp`。
- 保存成功后备份旧文件为 `.bak`。
- 最后 rename 替换。
- 保存多文件时尽量设计事务补偿。

### 3.2 数据目录识别依赖相对路径

当前 `DirectoryUtil::projectDataDirectory()` 根据当前目录是否存在 `src/bin` 或上级目录是否存在 `src/bin` 来决定数据目录。

优点：

- 从项目根目录、`src`、`bin` 启动时基本可用。
- 能减少多份 data 混乱。

风险：

- 从项目内更深层目录运行可能误判。
- V2 代码目录变复杂后，路径识别可能变脆弱。

V2 建议：

- 根据可执行文件路径定位项目根目录。
- 或在启动脚本中强制切到项目根目录。
- 或提供配置文件指定数据目录。

### 3.3 表格中文对齐依赖 `std::setw`

当前表格使用 `std::setw`。它对中文宽度并不真正感知，不同终端字体下可能轻微错位。

V2 建议：

- 实现中英文混合显示宽度估算。
- 中文按宽度 2，ASCII 按宽度 1。
- 或缩短表格字段，把详情改为逐条详情页。

### 3.4 登录失败不是持久化锁定

当前用户和管理员登录最多尝试 3 次，失败后返回上级菜单。它不是永久锁定。

如果 V2 或老师要求“输错 3 次锁定账号”，应增加：

- 用户失败次数。
- 失败次数持久化。
- 达到阈值自动冻结。
- 管理员解冻。
- 日志记录锁定原因。

### 3.5 Express 类承载过多未来职责

V1 中 `Express` 同时保存：

- 单号。
- 用户关系。
- 时间。
- 状态。
- 描述。
- 费用。

V2 要引入快递员、物品分类、多态计费、待揽收状态。若继续把所有字段堆进 `Express`，类会膨胀。

V2 建议：

- `Express` 保存订单/运单公共信息。
- `ExpressItem` 负责物品类型和计费。
- `Courier` 负责配送角色。
- 状态流转逻辑放入业务服务层或状态流转函数，不要散落 UI。

### 3.6 查询函数参数较长

当前查询接口使用多个参数：

```cpp
sender, receiver, id, startTime, endTime, statusFilter
```

V2 增加快递员、物品类型、状态后，参数会继续膨胀。

建议 V2 先引入：

```cpp
struct ExpressQueryCondition;
```

统一承载查询条件。

### 3.7 当前未使用智能指针

V1 不需要动态多态对象，因此没有使用智能指针。

V2 要引入物品基类和子类后，建议使用：

```cpp
std::unique_ptr<ExpressItem>
```

或在仓储加载时使用工厂函数：

```cpp
std::unique_ptr<ExpressItem> createItemByType(...)
```

这样可以体现 C++ 资源管理和多态所有权。

### 3.8 历史中文显示错误必须避免复发

历史上曾经出现：

- 只显示 `====`，中文完全空白。
- 中文变成乱码。
- PowerShell 和 cmd 命令混淆。
- 直接运行旧 exe，误以为改动无效。

V2 开发红线：

- 不要改动编码方案，除非有明确替代设计。
- 改完必须执行 `cmd /c build.bat`。
- 用户如果直接运行 `src/main.exe`，必须确认这个 exe 是最新编译产物。
- 不要把 `chcp 65001` 和 GBK 输出混用。

## 4. V2 演进预审 (Bridge to V2)

### 4.1 V2 背景需求注入

第二阶段题目：快递员任务管理子系统。

V2 强制需求：

- 引入新角色：快递员 `Courier`。
- 引入三种快递分类：
  - 易碎品。
  - 图书。
  - 普通物品。
- 强制设计继承体系：
  - 物品基类。
  - 物品子类。
  - 基类包含虚函数 `getPrice()` 用于计算价格。
- 快递状态扩充为：

```text
待揽收 -> 待签收 -> 已签收
```

V2 推荐展示闭环：

```text
用户寄件
快递进入待揽收
管理员或系统分配快递员
快递员登录查看待揽收任务
快递员揽收
快递状态变为待签收
收件人签收
快递状态变为已签收
平台和快递员收入记录更新
管理员查询任务与统计
```

### 4.2 身份鉴权如何扩展

V1 当前身份：

- 普通用户 `User`。
- 管理员 `Admin`。

V2 新增：

- 快递员 `Courier`。

推荐重构方向：

```cpp
class Person {
protected:
    std::string username_;
    std::string name_;
    std::string salt_;
    std::string passwordHash_;
public:
    virtual std::string roleName() const = 0;
    virtual ~Person() = default;
};

class User : public Person { ... };
class Admin : public Person { ... };
class Courier : public Person { ... };
```

可抽出的共性：

- 用户名。
- 姓名。
- salt。
- passwordHash。
- 密码校验。
- 登录认证。

不要过度抽象的部分：

- `User` 有余额、地址、电话。
- `Admin` 有平台余额。
- `Courier` 可能有电话、负责区域、收入、任务数、是否在岗。

推荐新增：

- `CourierRepository`
- `Courier` 实体。
- `loginCourier()`。
- `courierMenu()`。
- 管理员创建/冻结/查看快递员。

认证逻辑可从 `loginUser()`、`loginAdmin()` 抽出公共函数，例如：

```cpp
bool verifyPassword(const std::string& salt,
                    const std::string& hash,
                    const std::string& inputPassword);
```

也可设计：

```cpp
class AuthService;
```

但 V2 若时间紧，可以先保持 `LogisticsSystem` 内部扩展，避免重构过大。

### 4.3 Express 如何过渡到多态物品体系

V1 当前 `Express` 中只有：

```cpp
description_
fee_
```

V2 需要物品分类和多态计费。建议新增抽象基类：

```cpp
class ExpressItem {
public:
    virtual ~ExpressItem() = default;
    virtual std::string typeName() const = 0;
    virtual double getPrice() const = 0;
    virtual std::string serializeExtra() const = 0;
};
```

三种子类示例：

```cpp
class NormalItem : public ExpressItem {
public:
    double getPrice() const override;
};

class BookItem : public ExpressItem {
public:
    double getPrice() const override;
};

class FragileItem : public ExpressItem {
public:
    double getPrice() const override;
};
```

费用策略示例：

- 普通物品：基础价 12 或 15。
- 图书：基础价较低，可按重量/册数加价。
- 易碎品：基础价 + 包装保护费。

`Express` 推荐新增：

```cpp
std::string courier_;                  // 负责快递员用户名，可为空
ExpressStatus status_;                 // 待揽收/待签收/已签收
std::unique_ptr<ExpressItem> item_;     // 多态物品
double fee_;                           // 持久化实际费用
```

注意：如果使用 `std::unique_ptr<ExpressItem>`，`Express` 需要定义拷贝构造/拷贝赋值，或避免复制，改用移动语义。由于当前系统大量 `std::vector<Express>` 拷贝返回查询结果，直接引入 `unique_ptr` 会造成编译问题。可选方案：

方案 A：V2 初期不直接在 `Express` 内放 `unique_ptr`，先存 `itemType_` 和 `fee_`，用工厂临时创建对象计算价格。

方案 B：为 `ExpressItem` 增加 `clone()`：

```cpp
virtual std::unique_ptr<ExpressItem> clone() const = 0;
```

然后 `Express` 手写拷贝构造和赋值。

方案 C：查询结果返回指针或引用，不复制 `Express`。

如果目标是稳定完成课程要求，推荐方案 A 或 B。

### 4.4 状态流转如何重构

V1 状态：

```text
WaitingSign
Signed
```

V2 状态：

```text
WaitingPickup
WaitingSign
Signed
```

推荐枚举：

```cpp
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};
```

状态流转规则：

- 用户寄件后：`WaitingPickup`。
- 快递员揽收后：`WaitingSign`。
- 收件人签收后：`Signed`。

权限规则：

- 用户不能直接把待揽收快递签收。
- 快递员只能揽收分配给自己的或可抢单的待揽收快递。
- 收件人只能签收 `WaitingSign` 状态的快递。
- 管理员可以查询全部状态，可分配快递员。

建议新增业务函数：

```cpp
bool assignCourier(const std::string& admin,
                   const std::string& expressId,
                   const std::string& courier,
                   std::string& message);

bool pickupExpress(const std::string& courier,
                   const std::string& expressId,
                   std::string& message);
```

原 `signExpress()` 继续用于收件人签收，但要检查状态必须为 `WaitingSign`。

### 4.5 仓储和文件格式如何扩展

V1 文件：

```text
users.txt
admin.txt
expresses.txt
operations.log
```

V2 建议新增：

```text
couriers.txt
```

`couriers.txt` 字段可设计为：

```text
username|name|phone|salt|passwordHash|income|frozen|taskCount
```

`expresses.txt` 字段需要扩展。为兼容 V1，可以新版本直接重写格式，也可以在 `deserialize()` 中支持旧字段数和新字段数。

建议 V2 新格式：

```text
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|description|fee
```

如果物品子类有额外字段，可追加：

```text
itemExtra
```

或按类型定义不同 extra 格式。

### 4.6 查询系统如何扩展

V1 查询条件：

- sender。
- receiver。
- id。
- startTime。
- endTime。
- status。

V2 需要新增：

- courier。
- itemType。
- pickupTime。
- fee range。
- task status。

建议先实现查询条件对象：

```cpp
struct ExpressQueryCondition {
    std::string sender;
    std::string receiver;
    std::string courier;
    std::string expressId;
    std::string itemType;
    std::string startTime;
    std::string endTime;
    int statusFilter = -1;
};
```

然后把：

```cpp
matchExpress(...)
queryUserExpresses(...)
queryAllExpresses(...)
```

重构为接收 `ExpressQueryCondition`。

### 4.7 UI 菜单如何扩展

V2 新增主菜单项：

```text
1. 用户注册
2. 用户登录
3. 管理员登录
4. 快递员登录
0. 退出系统
```

快递员菜单建议：

```text
1. 查看待揽收任务
2. 揽收快递
3. 查看已完成任务
4. 查询我的任务
5. 查看收入
0. 退出登录
```

管理员菜单新增：

```text
5. 查看所有快递员
6. 新增快递员
7. 冻结/解冻快递员
8. 分配快递员
9. 查看任务统计
```

用户寄件流程新增：

- 选择物品类型：普通、图书、易碎品。
- 根据物品类型调用 `getPrice()`。
- 余额不足时提示充值。
- 寄件成功后状态为“待揽收”，不是 V1 的“待签收”。

### 4.8 推荐 V2 实施顺序

为降低风险，V2 不建议一上来大重构全部代码。推荐顺序：

1. 备份 V1。
2. 新建 `logistics_v2` 或在 V1 基础上复制目录。
3. 扩展 `ExpressStatus` 为三状态。
4. 扩展 `Express` 增加 `courier_`、`pickupTime_`、`itemType_`。
5. 先用 `itemType_ + calculatePrice()` 跑通流程。
6. 再引入 `ExpressItem` 抽象基类和三个子类，把价格计算迁移到 `getPrice()`。
7. 新增 `Courier` 实体和 `CourierRepository`。
8. 新增快递员登录和菜单。
9. 修改寄件流程：寄件后待揽收。
10. 新增管理员分配快递员。
11. 新增快递员揽收流程。
12. 修改签收流程：只有待签收可签收。
13. 扩展查询条件对象。
14. 更新 README 和 LOG。
15. 最后考虑原子化保存和日志哈希链。

### 4.9 V2 开发红线

- 不要破坏 V1 已经解决的中文显示方案。
- 不要让密码明文进入任何新文件，包括 `couriers.txt`。
- 不要只在 UI 层判断快递员权限，业务层必须兜底。
- 不要让用户直接签收“待揽收”快递。
- 不要让快递员揽收不属于自己的任务，除非明确设计为抢单模式。
- 不要让多态物品只停留在类定义，要实际调用 `getPrice()` 参与费用计算。
- 不要让新增字段破坏旧数据读取而无提示。
- 每次改完都执行 `cmd /c build.bat`，并至少测试主菜单、用户寄件、快递员揽收、收件人签收、管理员日志。

## 5. V1 最终状态结论

`logistics_v1` 已完成第一阶段开发和调试，当前版本具备：

- 清晰 CLI。
- 用户/管理员双角色。
- 注册、登录、充值、寄件、签收、查询闭环。
- 管理员查看用户、查快递、冻结/解冻、查日志。
- salt + SHA-256 密码保护。
- 权限过滤与越权日志。
- 完整输入校验。
- 文件持久化。
- 异常行跳过。
- 保存失败回滚。
- 中文控制台兼容处理。

V1 的最佳验收定位：

```text
功能完整、安全意识明显、异常处理充分、日志可追踪、输入校验严密、架构可扩展的单机版物流管理系统。
```

V2 的核心目标：

```text
在 V1 稳定业务闭环基础上，引入 Courier 快递员角色、三类物品继承体系、多态 getPrice() 计费、三阶段物流状态流转，形成更完整的面向对象物流任务管理子系统。
```
