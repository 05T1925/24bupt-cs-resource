# V2 架构设计规划

## 1. 分层结构

V2 继续使用 V1 的分层方式：

```text
Entity      实体对象：User、Admin、Courier、Express、LogEntry、ExpressItem
Repository  文件仓储：UserRepository、AdminRepository、CourierRepository、ExpressRepository
Service     业务服务：LogisticsSystem
UI          控制台流程：ConsoleUI、App
Utility     工具类：StringUtil、InputValidator、PasswordHasher、DirectoryUtil、StorageManager
```

目标是让 `main()` 仍只负责初始化环境和启动 `App`。

## 2. 核心类设计

### 2.1 Person 基类

建议抽取身份共性：

```cpp
class Person {
protected:
    std::string username_;
    std::string name_;
    std::string salt_;
    std::string passwordHash_;
    bool frozen_;
public:
    virtual ~Person() = default;
    virtual std::string roleName() const = 0;
};
```

是否第一轮就重构 `User/Admin/Courier` 继承 `Person`，取决于时间：

- 稳妥路线：先新增 `Courier`，保留 V1 `User/Admin`，只抽出认证辅助函数。
- 展示路线：重构 `Person` 基类，强调面向对象继承。

推荐：第一轮先稳妥跑通，第二轮再抽 `Person`，避免一次改动过大。

### 2.2 Courier

字段：

```text
username
name
phone
salt
passwordHash
income/balance
frozen
active/removed
```

关键函数：

- `verifyPassword()`
- `addIncome()`
- `setFrozen()`
- `markRemoved()`
- `serialize()`
- `deserialize()`

### 2.3 ExpressItem 继承体系

```cpp
class ExpressItem {
public:
    virtual ~ExpressItem() = default;
    virtual std::string typeCode() const = 0;
    virtual std::string typeName() const = 0;
    virtual double getPrice() const = 0;
    virtual std::string amountText() const = 0;
};
```

子类：

- `FragileItem`：重量 kg，`getPrice() = weight * 8`
- `BookItem`：册数，`getPrice() = count * 2`
- `NormalItem`：重量 kg，`getPrice() = weight * 5`

持久化建议：

```text
itemType|itemAmount|fee
```

`Express` 内部可以先保存 `itemType_` 和 `itemAmount_`，用工厂临时创建 `ExpressItem` 计算费用。这样避免 `std::unique_ptr` 导致 `vector<Express>` 拷贝查询结果大规模重构。

### 2.4 Express

V2 字段：

```text
id
sender
receiver
courier
sendTime
pickupTime
receiveTime
status
itemType
itemAmount
description
fee
```

状态：

```cpp
enum class ExpressStatus {
    WaitingPickup = 0,
    WaitingSign = 1,
    Signed = 2
};
```

关键行为：

- `belongsToUser(username)`
- `belongsToCourier(username)`
- `assignCourier(courier)`
- `pickup(time)`
- `sign(time)`
- `statusName() const`

状态修改仍建议由 `LogisticsSystem` 调用，避免 UI 直接改实体。

### 2.5 ExpressQueryCondition

字段：

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

价值：

- 用户、快递员、管理员查询复用同一套筛选对象。
- 后续增加费用区间、揽收时间等条件时不拉长参数列表。

### 2.6 LogisticsSystem 新增接口

建议新增：

```text
loginCourier()
createCourier()
removeCourier()
setCourierFrozen()
assignCourier()
pickupExpress()
pickupExpressBatch()
waitingPickupForCourier()
queryCourierExpresses()
queryAllCouriers()
queryCourierStatistics()
```

原有接口需调整：

- `sendExpress()`：费用由物品多态计算，状态改为待揽收。
- `signExpress()`：只允许签收待签收状态。
- `queryAllExpresses()`：支持三状态、快递员、物品类型。

## 3. 文件格式设计

### 3.1 新增 `couriers.txt`

```text
username|name|phone|salt|passwordHash|income|frozen|removed
```

### 3.2 扩展 `expresses.txt`

```text
id|sender|receiver|courier|sendTime|pickupTime|receiveTime|status|itemType|itemAmount|description|fee
```

兼容建议：

- V2 可以独立初始化新数据，不必读取 V1 旧数据。
- 若复制 V1 数据，应在 `deserialize()` 中识别旧字段数并补默认值。

## 4. 安全与可靠性增强

### 4.1 原子保存

仓储保存改为：

```text
写入 file.tmp
成功后把旧 file 备份为 file.bak
rename file.tmp -> file
失败时保留旧 file
```

### 4.2 日志哈希链

日志字段升级为：

```text
seq|time|actorType|actor|action|result|detail|prevHash|currentHash
```

管理员菜单新增：

- 查看日志。
- 按条件筛选日志。
- 校验日志完整性。

## 5. UI 菜单规划

主菜单：

```text
1. 用户注册
2. 用户登录
3. 快递员登录
4. 管理员登录
0. 退出系统
```

快递员菜单：

```text
1. 查看待揽收任务
2. 揽收快递
3. 查询我的任务
4. 查看收入统计
0. 退出登录
```

管理员菜单新增：

```text
查看快递员
新增快递员
删除/停用快递员
冻结/解冻快递员
分配快递员
查看任务统计
校验日志完整性
```

