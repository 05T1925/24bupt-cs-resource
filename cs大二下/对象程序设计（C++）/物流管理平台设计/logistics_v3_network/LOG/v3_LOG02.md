# v3_LOG02：Phase 1 业务核心迁移记录

## 1. 本次目标

本阶段将 V2 单机版核心业务能力迁移到 V3 `common/` 模块，暂不引入 Socket。目标是验证拆分后的业务核心可以脱离 UI 独立编译和运行。

## 2. 文件拆分结果

```text
common/models/
  Entities.h/.cpp       User、Admin、Courier、Express、ExpressStatus
  ExpressItem.h/.cpp    ExpressItem、NormalItem、FragileItem、BookItem、ExpressItemFactory

common/security/
  StringUtil.h/.cpp     trim、split、escape、unescape、金额格式化、严格 double 解析
  HashUtil.h/.cpp       SHA-256
  PasswordHasher.h/.cpp salt 与密码哈希
  InputValidator.h/.cpp 用户名、手机号、密码、金额校验

common/storage/
  StorageManager.h/.cpp 文本行读取、目录创建、.tmp -> .bak -> 正式文件原子保存
  Repositories.h/.cpp   User/Admin/Courier/Express 仓储
  Logger.h/.cpp         9 字段日志哈希链与完整性校验

common/service/
  ServiceResult.h       统一返回结构
  LogisticsSystem.h/.cpp 注册、充值、寄件、分配、揽收、签收、查询、日志校验
```

## 3. 已保留的 V2 核心能力

- `ExpressItem::getPrice()` 多态计费。
- `Normal / Fragile / Book` 三类快递。
- 快递 12 字段文本格式。
- `.tmp -> .bak -> 正式文件` 原子保存。
- 9 字段日志哈希链。
- 待揽收 -> 待签收 -> 已签收状态机。
- 寄件扣用户余额并加管理员余额。
- 揽收时管理员向快递员支付 50% 提成。
- 用户、快递员、管理员权限边界在服务层兜底。

## 4. ServiceResult 标准化

服务层接口统一返回：

```text
bool ok
string code
string message
vector<string> data
```

这为 Phase 2 的协议序列化做准备，避免继续使用散落的 `bool + message + out 参数`。

## 5. UI 解耦检查

已检查 `common/**/*.h` 和 `common/**/*.cpp`：

```text
无 #include <iostream>
无 std::cin
无 std::cout
无 ConsoleUI
```

`common` 输入全部来自函数参数，输出全部通过 `ServiceResult` 或返回值传递。

## 6. 编译与自测

已执行：

```text
build_server.bat
bin/logistics_v3_server.exe
```

自测流程：

```text
initialize
register alice01
register bob02
recharge alice01 100
create courier01
send Fragile 2kg
assign courier01
pickup
sign
query all expresses
verify log hash chain
```

关键验证：

```text
Fragile 2kg fee = 16.00
pickup commission = 8.00
final status = 已签收
log hash chain = 完整
```

## 7. 并发预留说明

当前 MinGW 环境对 `std::shared_mutex` 支持不稳定，因此 Phase 1 使用 `CoreMutex` 自旋锁封装所有服务入口。它已经形成统一并发保护点，后续可在不改变业务接口的前提下升级为读写锁。

