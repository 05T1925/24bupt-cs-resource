# V2 验收步骤与复习说明

项目：物流管理系统 V2 单机版  
适用场景：助实验收、课设复习、现场代码讲解  
核心源码：`logistics_v2/src/main.cpp`

---

## 0. 验收前准备

### 0.1 编译运行

建议先在项目目录执行：

```bat
cd /d C:\Users\28641\Desktop\课设C++\物流管理平台设计\logistics_v2
build.bat
run.bat
```

当前项目使用 GBK 控制台方案：

```bat
-finput-charset=GBK -fexec-charset=GBK
chcp 936
```

说明时可以强调：这是为了保证 Windows 控制台下中文菜单、中文表格和中文日志能稳定显示。

### 0.2 建议提前准备的账号

建议准备以下角色账号：

```text
管理员：admin / Admin0219
普通用户 A：用于发件
普通用户 B：用于收件
快递员：用于分配和揽收
```

如果现场没有现成账号，可以按流程现场注册用户、由管理员新增快递员。

### 0.3 演示主线

推荐验收主线：

```text
管理员登录
-> 查看/新增快递员
-> 用户注册和充值
-> 用户寄件
-> 管理员手动分配或自动分配快递员
-> 快递员揽收并获得 50% 提成
-> 收件用户签收
-> 管理员查看统计看板、日志哈希链和绩效排行
-> 演示异常处理和安全机制
```

---

## 1. 第一部分：整体结构讲解

### 1.1 打开源码位置

打开：

```text
logistics_v2/src/main.cpp
```

建议先讲整体分层，而不是直接从菜单讲起。

### 1.2 代码分层说明

可以这样介绍：

```text
这个 V2 虽然集中在一个 main.cpp 中实现，但内部按工程分层组织。
我把代码分为工具层、实体层、仓储层、业务服务层和 UI 流程层。
```

对应代码结构：

```text
工具层：StringUtil、InputValidator、PasswordHasher、SHA256、StorageManager、TablePrinter
实体层：User、Admin、Courier、Express、ExpressItem
仓储层：UserRepository、CourierRepository、ExpressRepository、AuthStateRepository、Logger
业务层：LogisticsSystem
UI 层：ConsoleUI、App
入口层：main
```

### 1.3 重点解释：UI 与业务解耦

打开：

```text
class App
class LogisticsSystem
```

说明：

```text
App 负责菜单和输入，ConsoleUI 负责控制台交互。
真正的业务规则，比如寄件扣款、签收权限、快递员揽收、自动分配，都集中在 LogisticsSystem。
这样 UI 层不会直接修改余额、快递状态或数据文件。
```

可举例：

```cpp
system_.sendExpress(...);
system_.signExpress(...);
system_.pickupExpress(...);
```

解释重点：

```text
UI 层只是把合法输入传给业务层。
业务层才负责权限校验、状态流转和保存失败回滚。
```

---

## 2. 第二部分：基础功能验收流程

## 2.1 用户注册

菜单路径：

```text
主菜单 -> 用户注册
```

输入内容：

```text
用户名
姓名
手机号
密码
地址
```

讲解点：

```text
注册时会检查用户名格式、手机号格式、密码强度和重复账号。
密码不会明文保存，而是保存 salt + SHA-256 哈希。
```

源码定位：

```text
InputValidator
PasswordHasher
LogisticsSystem::registerUser
UserRepository
```

异常演示建议：

```text
输入过短用户名
输入非法手机号
重复注册同一用户名
输入弱密码
```

---

## 2.2 用户登录与充值

菜单路径：

```text
主菜单 -> 用户登录 -> 充值
```

讲解点：

```text
充值不会直接写文件，而是先修改内存对象，再调用 saveAll 保存。
如果保存失败，会回滚余额。
```

源码定位：

```text
LogisticsSystem::loginUser
LogisticsSystem::recharge
```

安全说明：

```text
登录时通过 PasswordHasher::verify 校验哈希，不比较明文密码。
登录成功后会清空 auth_state.txt 中的失败次数。
```

---

## 2.3 用户寄件

菜单路径：

```text
用户登录 -> 发送快递
```

建议演示：

```text
发件人：用户 A
收件人：用户 B
物品类型：普通 / 易碎 / 图书任选一种
重量或册数：输入合法正数
物品描述：任意非空文本
```

讲解点：

```text
寄件成功后，快递进入 WaitingPickup，也就是待揽收状态。
系统会扣除发件用户余额，并把费用加入管理员平台账户。
```

源码定位：

```text
LogisticsSystem::sendExpress
ExpressItemFactory
ExpressItem::getPrice
Express
```

核心代码骨架：

```cpp
std::unique_ptr<ExpressItem> item = ExpressItemFactory::createItem(itemType, itemAmount);
double fee = item->getPrice();
users_[senderIndex].deduct(fee);
admin_.addBalance(fee);
expresses_.push_back(Express(...));
```

解释重点：

```text
这里体现了多态计费和资金流转。
费用不是在业务层用 if-else 硬编码，而是通过 item->getPrice() 动态派发到具体物品子类。
```

异常演示建议：

```text
余额不足
给自己寄件
收件人不存在
物品描述为空
物品数量非法
```

---

## 3. 第三部分：快递员任务管理验收

## 3.1 管理员登录

菜单路径：

```text
主菜单 -> 管理员登录
```

讲解点：

```text
管理员拥有全局查询、用户冻结、快递员管理、分配快递员、查看日志和统计看板权限。
普通用户和快递员无法执行这些全局操作。
```

源码定位：

```text
LogisticsSystem::loginAdmin
App::adminMenu
```

---

## 3.2 新增快递员

菜单路径：

```text
管理员菜单 -> 新增快递员
```

输入内容：

```text
快递员用户名
姓名
手机号
初始密码
```

讲解点：

```text
快递员是独立实体 Courier，包含 income、frozen、removed 状态。
删除快递员不是物理删除，而是 removed 逻辑停用，保证历史快递记录可追溯。
```

源码定位：

```text
Courier
CourierRepository
LogisticsSystem::createCourier
LogisticsSystem::removeCourier
```

异常演示建议：

```text
重复新增快递员
手机号格式错误
弱密码
停用仍有未完成任务的快递员
```

---

## 3.3 手动分配快递员

菜单路径：

```text
管理员菜单 -> 分配快递员
```

讲解点：

```text
只有 WaitingPickup 状态的快递可以分配快递员。
被冻结或停用的快递员不能接收新任务。
```

源码定位：

```text
LogisticsSystem::assignCourier
Express::assignCourier
```

异常演示建议：

```text
分配不存在的快递单号
分配不存在的快递员
给已签收快递分配快递员
给冻结或停用快递员分配任务
```

---

## 3.4 智能自动分配

菜单路径：

```text
管理员菜单 -> 单个自动分配
管理员菜单 -> 一键分配所有待揽收快递
```

讲解点：

```text
自动分配不是随机分配，而是按多维度策略选择最合适的快递员。
```

调度优先级：

```text
1. 可用性：过滤 frozen 和 removed 快递员
2. 负载均衡：未完成任务数少的优先
3. 收入公平：收入低的优先
4. 字典序兜底：用户名升序，保证结果稳定
```

源码定位：

```text
LogisticsSystem::buildCourierScheduleStates
LogisticsSystem::selectBestCourierIndex
LogisticsSystem::autoAssignCourier
LogisticsSystem::autoAssignAllWaitingPickup
```

核心代码骨架：

```cpp
if (currentLoad < bestLoad ||
    (currentLoad == bestLoad && currentIncome < bestIncome) ||
    (currentLoad == bestLoad && currentIncome == bestIncome && currentUsername < bestUsername)) {
    best = current;
}
```

解释重点：

```text
这里体现了调度算法和公平性设计。
批量分配时先构建快递员负载快照，再增量更新任务数，避免反复全量统计。
```

---

## 4. 第四部分：三状态流转与资金分账

## 4.1 快递员揽收

菜单路径：

```text
快递员登录 -> 查看/揽收任务
```

讲解点：

```text
快递员只能揽收分配给自己的 WaitingPickup 快递。
揽收成功后，快递状态从 WaitingPickup 变为 WaitingSign。
同时管理员账户扣除快递费 50%，快递员收入增加 50%。
```

源码定位：

```text
LogisticsSystem::pickupExpress
Express::pickup
Courier::addIncome
Admin::addBalance
```

核心代码骨架：

```cpp
Express oldExpress = express;
Admin oldAdmin = admin_;
Courier oldCourier = couriers_[courierIndex];

admin_.addBalance(-commission);
couriers_[courierIndex].addIncome(commission);
express.pickup();

if (!saveAll()) {
    express = oldExpress;
    admin_ = oldAdmin;
    couriers_[courierIndex] = oldCourier;
}
```

解释重点：

```text
这段代码模拟了事务一致性。
状态修改和资金修改绑定在一起，如果保存失败，就恢复快递、管理员和快递员三个对象。
```

异常演示建议：

```text
快递员揽收非本人任务
重复揽收已揽收快递
冻结快递员尝试揽收
平台余额不足无法支付提成
```

---

## 4.2 用户签收

菜单路径：

```text
收件用户登录 -> 签收快递
```

讲解点：

```text
签收是三状态机的最后一步。
只有收件人本人、且快递处于 WaitingSign 状态时，才能签收。
```

源码定位：

```text
LogisticsSystem::signExpress
Express::sign
```

核心拦截：

```cpp
if (express.receiver() != username) return false;
if (express.isWaitingPickup()) return false;
if (express.isSigned()) return false;
if (!express.isWaitingSign()) return false;
express.sign();
```

解释重点：

```text
这里体现了权限隔离和状态机控制。
用户不能签收别人的快递，待揽收快递也不能越级签收。
```

---

## 5. 第五部分：工程加分项验收

## 5.1 多态计费

源码定位：

```text
ExpressItem
NormalItem
FragileItem
BookItem
ExpressItemFactory
LogisticsSystem::sendExpress
```

讲解点：

```text
ExpressItem 定义 getPrice 纯虚函数。
不同物品子类重写 getPrice。
业务层统一通过基类指针调用 item->getPrice()。
```

演示说明：

```text
普通快递按重量 5 元/kg
易碎品按重量 8 元/kg
图书按册数 2 元/本
```

价值：

```text
体现继承、多态和开闭原则。
新增物品类型时只需要新增子类，不需要大改业务流程。
```

---

## 5.2 原子化保存

源码定位：

```text
StorageManager::saveLinesAtomically
各 Repository::save
```

讲解点：

```text
保存数据时不是直接覆盖正式文件，而是先写 tmp，再轮转 bak，最后提交正式文件。
```

流程：

```text
file.tmp 写入完整数据
旧 file 改名为 file.bak
file.tmp 改名为 file
失败时尽量恢复旧文件
```

价值：

```text
降低断电、崩溃或写入失败造成正式数据损坏的风险。
业务层根据 false 返回值执行回滚，不提示假成功。
```

---

## 5.3 日志哈希链

源码定位：

```text
Logger::write
Logger::verifyHashChain
LogEntry
operations.log
```

日志格式：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

讲解点：

```text
每条新日志都会读取上一条日志的 currentHash 作为 prevHash。
然后把当前日志内容和 prevHash 一起做 SHA-256，得到 currentHash。
```

价值：

```text
如果有人手动修改中间某条日志，后续哈希校验会断链。
管理员可以通过日志完整性校验发现篡改位置。
```

演示建议：

```text
先正常校验日志完整性。
如果时间允许，可复制备份后手动改 operations.log 中一行，再校验，展示断链提示。
注意不要破坏正式演示数据。
```

---

## 5.4 密码安全

源码定位：

```text
PasswordHasher
SHA256
ConsoleUI::readPasswordHidden
AuthState
AuthStateRepository
LogisticsSystem::loginUser
LogisticsSystem::loginCourier
```

讲解点：

```text
密码不明文保存，而是 salt + SHA-256。
输入密码时使用 _getch 隐藏回显。
用户和快递员连续三次密码错误后自动冻结。
```

演示建议：

```text
展示登录密码输入为星号。
展示数据文件中没有明文密码。
说明 auth_state.txt 独立记录失败次数。
```

---

## 5.5 表格显示优化

源码定位：

```text
TablePrinter::displayWidth
ConsoleUI::printExpressTable
ConsoleUI::printCourierPerformanceTable
```

讲解点：

```text
Windows 控制台中中文字符通常占两个显示宽度，英文占一个显示宽度。
普通 std::setw 对中文表格容易错位。
TablePrinter 会计算显示宽度，让中英文混合表格更整齐。
```

---

## 6. 第六部分：统计看板与绩效排行

## 6.1 管理员统计看板

菜单路径：

```text
管理员菜单 -> 任务统计
```

展示内容：

```text
平台净收入
快递员总收入
待揽收、待签收、已签收数量
普通、易碎、图书类型数量和收入占比
全体快递员绩效排行
```

源码定位：

```text
SystemStatistics
CourierPerformance
LogisticsSystem::statistics
LogisticsSystem::courierPerformances
```

讲解点：

```text
这部分把系统从简单的数据记录工具提升为运营管理工具。
管理员可以看到平台整体收入、任务状态分布和快递员绩效。
```

---

## 6.2 快递员个人绩效

菜单路径：

```text
快递员登录 -> 查看个人绩效
```

展示内容：

```text
待揽收数量
待签收数量
已完成数量
总任务数
完成率
累计收入
```

讲解点：

```text
个人绩效复用全局统计接口，只是过滤到当前登录快递员。
这体现了业务统计逻辑复用。
```

---

## 7. 异常处理验收清单

建议向助教展示其中 3 到 5 个即可。

```text
1. 用户余额不足不能寄件。
2. 用户不能给自己寄件。
3. 用户不能签收别人的快递。
4. 待揽收快递不能越级签收。
5. 快递员不能揽收非本人任务。
6. 已揽收快递不能重复揽收。
7. 冻结或停用快递员不能分配任务。
8. 停用有未完成任务的快递员会返回任务清单。
9. 密码错误三次自动冻结。
10. 日志哈希链能检测篡改。
11. 保存失败时业务层有回滚逻辑。
```

---

## 8. 推荐现场演示顺序

完整演示可以按下面走：




```text
1. 编译运行 启动程序，展示密码输入模式。   展示主菜单和密码输入隐藏模式。
2. 管理员登录。
3. 查看快递员列表，新增快递员。
4. 用户寄普通/易碎/图书快递，展示多态计费。
   注册两个普通用户。
   用户 A 充值。  改密码不能原密码
   用户 A 给用户 B 寄件，选择普通/易碎/图书物品。
5. 管理员查看所有快递，说明状态为待揽收。  
6. 管理员手动分配一单  ； 管理员自动分配一单，展示负载均衡与公平性策略。
7. 快递员登录，查看本人待揽收任务。
8. 快递员揽收任务，展示 50% 提成和状态变为待签收。
9. 收件用户签收，用户 B 登录并签收。
10. 管理员查看任务统计看板和快递员绩效排行。
11. 快递员查看统计看板和个人绩效排行。
12. 尝试停用有未完成任务的快递员，展示冲突清单。
13. 演示密码错误三次自动冻结。
14. 管理员查看日志 并 演示日志哈希链完整性校验。
15. 演示 1 到 2 个异常场景，例如越权签收或停用有任务快递员

用户余额不足不能寄件；不能给自己寄件；不能签收别人的快递；
待揽收快递不能越级签收；快递员不能揽收非本人任务；已揽收快递不能重复揽收；
冻结或停用快递员不能分配任务；停用有未完成任务的快递员会返回任务清单；
密码错误三次自动冻结；保存失败时业务层有回滚逻辑；
日志哈希链能检测篡改；保存失败时业务层有回滚逻辑。

16. 简要说明 `.tmp` / `.bak` 原子保存机制。
```



如果时间较紧，可以压缩为：

```text
管理员登录 -> 用户寄件 -> 自动分配 -> 快递员揽收 -> 用户签收 -> 统计看板 -> 日志校验
```

---

## 9. 助教可能追问与回答要点

### Q1：为什么要把业务放在 LogisticsSystem，而不是直接写在菜单里？

回答要点：

```text
这样可以让 UI 和业务解耦。
菜单只负责输入输出，业务层统一处理权限、状态和回滚。
以后升级 V3 网络版时，可以复用 LogisticsSystem，把 ConsoleUI 换成 Socket 服务端控制器。
```

### Q2：多态计费体现在哪里？

回答要点：

```text
ExpressItem 是抽象基类，getPrice 是纯虚函数。
NormalItem、FragileItem、BookItem 分别重写 getPrice。
寄件时只通过基类指针调用 item->getPrice，实现运行期多态派发。
```

### Q3：如果揽收时保存失败怎么办？

回答要点：

```text
pickupExpress 修改前保存 oldExpress、oldAdmin、oldCourier。
如果 saveAll 失败，就恢复三个对象，避免状态和资金不一致。
```

### Q4：日志哈希链有什么用？

回答要点：

```text
普通日志可以被手动改。
哈希链把上一条日志 hash 放进下一条日志的计算中。
一旦中间日志被改，后续 prevHash/currentHash 校验就会失败。
```

### Q5：自动分配为什么公平？

回答要点：

```text
先过滤不可用快递员，再比较未完成任务数。
任务数相同则比较累计收入，收入低者优先。
仍相同按用户名排序，保证稳定性。
```

### Q6：为什么删除快递员用 removed 而不是直接删？

回答要点：

```text
因为历史快递记录中可能保存了 courier 用户名。
如果物理删除快递员，历史责任链会断。
逻辑删除可以停用账号，同时保留历史可追溯性。
```

---

## 10. 最后总结话术

可以这样收尾：

```text
我的 V2 完成了题目二要求的三角色管理、快递员任务流转、多态计费和持久化。
在此基础上，我额外加入了密码哈希、隐藏输入、三次失败冻结、原子化保存、日志哈希链、智能自动分配、绩效统计和中英文表格对齐。
整体设计上，我重点体现了面向对象、多态派发、职责分离、状态机拦截、事务式回滚和防御性编程。
```