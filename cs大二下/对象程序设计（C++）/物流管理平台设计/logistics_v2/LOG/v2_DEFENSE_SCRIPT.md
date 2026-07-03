# V2 项目架构与源码剖析演示讲稿

答辩人：北京邮电大学计算机学院 2024 级领航班 刘文涛  
项目：物流管理系统 V2 单机版  
建议演示文件：`logistics_v2/src/main.cpp`

---

## 0. 开场白

[切换到 PPT 第 1 页：项目总览]

各位老师、助教好，我是北京邮电大学计算机学院 2024 级领航班的刘文涛。我这次展示的是《物流管理系统》V2 单机版。相比基础版，我的 V2 不只是完成用户寄件、快递员揽收、收件人签收这些基本功能，而是围绕“面向对象建模、业务状态机、防御性编程、可审计安全、自动调度”做了一套完整的工程化设计。

我会从三个角度讲：第一是整体架构，说明系统如何分层；第二是核心业务流转，说明快递生命周期和资金分账如何保持一致；第三是加分项，包括多态计费、日志哈希链和智能调度。

---

## 1. 开场与系统结构剖析（Architecture & Structure）

[切换到 PPT 第 2 页：分层架构]

我这个项目虽然最终集中在一个 `main.cpp` 中实现，但代码结构并不是过程式堆叠，而是按职责分层组织的。整体可以分为五层：

```text
Entity 层：User / Admin / Courier / Express / ExpressItem
Repository 层：UserRepository / CourierRepository / ExpressRepository / Logger
Service 层：LogisticsSystem
UI 层：ConsoleUI / App
Tool 层：StorageManager / PasswordHasher / TablePrinter / SHA256
```

这里最核心的设计思想是职责分离：UI 层只负责输入输出，业务规则全部收拢到 `LogisticsSystem`，仓储层只负责文件读写，不判断业务合法性。

### 样例 1：UI 层与业务层解耦

[打开 main.cpp，定位到 `App::sendExpressFlow`]

以“用户寄件”为例，UI 层会询问收件人、物品类型、重量或册数、物品描述，但它并不判断余额是否足够，也不直接修改用户余额、管理员余额或快递列表。UI 最终只是调用业务服务层：

```cpp
system_.sendExpress(username, receiver, description,
                    itemType, itemAmount, expressId, message);
```

[打开 main.cpp，定位到 `LogisticsSystem::sendExpress`]

真正的规则都在 `LogisticsSystem::sendExpress` 里，包括：发件人是否存在、是否被冻结，收件人是否存在，是否给自己寄件，物品类型是否合法，余额是否足够，以及保存失败时如何回滚。

我这样设计的好处是：如果后续做 V3 网络版，只需要把 `App` 和 `ConsoleUI` 换成 Socket 客户端，`LogisticsSystem` 这一套业务规则仍然可以直接复用到服务端。

### 样例 2：签收权限不放在 UI 层

[打开 main.cpp，定位到 `LogisticsSystem::signExpress`]

再看“签收”功能。UI 可以展示用户待签收列表，但真正的安全边界仍然在业务层。比如这里会检查：

```cpp
if (express.receiver() != username) {
    message = "无权签收该快递。";
    return false;
}
```

这说明即使用户通过异常输入绕过了菜单展示，也不能签收别人的快递。这个点体现的是服务层权限隔离，而不是依赖前端菜单隐藏。

---

## 2. 核心业务逻辑走读（Core Business Logic）

[切换到 PPT 第 3 页：快递生命周期]

V2 的业务核心是快递从“待揽收”到“待签收”再到“已签收”的完整生命周期。我用一个强类型枚举表达三状态：

```cpp
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};
```

### 样例 1：三状态机防止非法越级

[打开 main.cpp，定位到 `LogisticsSystem::signExpress`]

签收接口里，我没有直接把状态改成 `Signed`，而是先做状态拦截：

```cpp
if (express.isWaitingPickup()) {
    message = "该快递仍处于待揽收状态，暂不能签收。";
    return false;
}
if (express.isSigned()) {
    message = "该快递已经签收，不能重复签收。";
    return false;
}
if (!express.isWaitingSign()) {
    message = "快递状态异常，不能签收。";
    return false;
}
express.sign();
```

这段代码体现了状态机防线：待揽收不能越级签收，已签收不能重复签收，只有待签收状态才能进入已签收。

同理，快递员揽收时也会检查 `express.isWaitingPickup()`，所以已揽收或已签收的快递不能重复揽收。

### 样例 2：快递员揽收与资金分账

[切换到 PPT 第 4 页：账实一致性]

[打开 main.cpp，定位到 `LogisticsSystem::pickupExpress`]

快递员揽收是 V2 的核心接口，因为它同时改变三类状态：快递状态、管理员余额、快递员收入。

核心代码骨架如下：

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
    return false;
}
```

我这里强调的是事务一致性。虽然这是单机文本文件系统，不是真正数据库事务，但我在内存层面先保存旧快照，然后把“管理员扣 50%”“快递员加 50%”“快递状态变为待签收”三个动作绑定在一起。如果持久化失败，就把快递、管理员、快递员全部恢复到旧状态，避免出现“快递已揽收但钱没到账”或者“钱划了但状态没变”的账实不一致问题。

---

## 3. 工程亮点与满分加分项（Highlights & Bonus Points）

[切换到 PPT 第 5 页：加分项总览]

基础功能之外，我重点做了三个工程增强：真实多态计费、日志哈希链防篡改、智能分配调度。

### 亮点 1：真实落地的多态计费

[打开 main.cpp，定位到 `ExpressItem`]

我定义了一个抽象基类 `ExpressItem`，其中 `getPrice()` 是纯虚函数：

```cpp
class ExpressItem {
public:
    virtual double getPrice() const = 0;
};

class FragileItem : public ExpressItem {
    double getPrice() const override { return weight_ * 8.0; }
};

class BookItem : public ExpressItem {
    double getPrice() const override { return count_ * 2.0; }
};

class NormalItem : public ExpressItem {
    double getPrice() const override { return weight_ * 5.0; }
};
```

[打开 main.cpp，定位到 `LogisticsSystem::sendExpress`]

在寄件时，业务层只持有基类指针：

```cpp
std::unique_ptr<ExpressItem> item =
    ExpressItemFactory::createItem(itemType, itemAmount);

double fee = item->getPrice();
```

这里的亮点是多态派发。业务层不需要写一堆 `if-else` 判断“如果是图书就乘 2，如果是易碎品就乘 8”。新增一种物品时，只需要新增一个子类并重写 `getPrice()`，符合开闭原则。

### 亮点 2：日志哈希链与防篡改审计

[切换到 PPT 第 6 页：日志哈希链]

[打开 main.cpp，定位到 `Logger::write` 和 `Logger::verifyHashChain`]

我的日志不是普通文本追加，而是 9 字段哈希链：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

写日志时，会先读取上一条日志的 `currentHash` 作为本条 `prevHash`，再把本条关键字段和 `prevHash` 一起做 SHA-256：

```cpp
readLastChainState(sequence, prevHash);
LogEntry entry(sequence, now, actorType, actor, action, result, detail);
entry.setHashes(prevHash, "");
currentHash = SHA256::hash(entry.hashPayload());
entry.setHashes(prevHash, currentHash);
```

校验时逐行重算，如果某一行的内容、上一行哈希或当前哈希被手动改过，校验函数就能定位到断链行。

这个设计的价值是防篡改审计。即使日志还是文本文件，也能让管理员发现外部文本编辑器的手动修改。

### 亮点 3：智能分配调度算法

[切换到 PPT 第 7 页：自动调度]

[打开 main.cpp，定位到 `buildCourierScheduleStates` 和 `selectBestCourierIndex`]

手动分配之外，我实现了智能自动分配。调度策略分三层：

```text
1. 可用性：过滤冻结和停用快递员
2. 负载均衡：未完成任务数少的优先
3. 公平性：收入低的优先
4. 字典序：用户名升序兜底，保证结果稳定
```

核心比较逻辑可以概括为：

```cpp
if (currentLoad < bestLoad ||
    (currentLoad == bestLoad && currentIncome < bestIncome) ||
    (currentLoad == bestLoad && currentIncome == bestIncome &&
     currentUsername < bestUsername)) {
    best = current;
}
```

[打开 main.cpp，定位到 `autoAssignAllWaitingPickup`]

批量分配时，我不是每分配一单就重新全量统计一次，而是先构建调度快照，每成功分配一单后只对该快递员的负载做增量更新：

```cpp
std::vector<CourierScheduleState> states = buildCourierScheduleStates();
...
increaseCourierLoad(states, courierIndex);
```

这体现了算法意识：在保证公平性的同时减少重复遍历，提高批量分配效率。

---

## 4. 结束总结

[切换到 PPT 第 8 页：总结]

总结一下，我的 V2 项目不只是完成题目二要求，而是围绕工程质量做了系统化增强：

```text
面向对象：User / Courier / Express / ExpressItem 清晰建模
职责分离：UI、Service、Repository、Tool 分层明确
状态机：待揽收 -> 待签收 -> 已签收，全链路拦截非法流转
事务一致性：揽收时状态和资金同步修改，保存失败整体回滚
安全性：密码哈希、隐藏输入、三次失败冻结
可审计性：operations.log 哈希链防篡改
智能化：自动分配综合考虑负载、公平和稳定性
```

如果进入 V3 网络版，我会直接复用 `Entity`、`Repository` 和 `LogisticsSystem`，把 `ConsoleUI/App` 替换为 Socket 客户端与服务端控制器。也就是说，V2 的业务层已经为 C/S 架构演进做好了准备。