# v3_LOG08：Phase 7 快递员网络业务验收与加固记录

## 1. 本次目标

在 Phase 6 管理员网络后台基础上，完成 Phase 7 快递员网络业务的验收、加固与演示闭环：

```text
QUERY_MY_PICKUP_TASKS
PICKUP_EXPRESS
PICKUP_BATCH
QUERY_MY_TASKS
VIEW_MY_PERFORMANCE
```

## 2. 零信任快递员路由

所有快递员命令统一进入：

```text
ServerController::handleCourierCommand
ServerController::requireCourierSession
```

鉴权只使用：

```text
SessionManager::getSession(token)
session.role == COURIER
session.username
```

客户端请求参数中不允许携带“当前快递员账号”作为操作者身份。普通用户或管理员 token 调用快递员命令时返回：

```text
ERR|PERMISSION_DENIED|当前身份无权执行快递员业务。
```

并写入：

```text
COURIER_PERMISSION_DENIED
```

## 3. 揽收事务一致性

`LogisticsSystem::pickupExpress` 在同一把 `CoreMutex` 内完成：

```text
1. 校验 expressId 存在
2. 校验 courier 存在且未冻结/停用
3. 校验 express.courier == session.username
4. 校验状态仍为 WaitingPickup
5. 计算 commission = fee * 0.5
6. 管理员余额扣除 commission
7. 快递员收入增加 commission
8. 快递状态变为 WaitingSign
9. 保存所有数据文件
10. 写 PICKUP_EXPRESS SUCCESS 日志
```

保存失败时恢复旧的 `Express`、`Courier`、`Admin` 快照，避免状态变更和资金分账脱节。

## 4. 越权与重复揽收审计

新增更明确的揽收安全审计：

```text
COURIER_PICKUP_PERMISSION_DENIED
COURIER_PICKUP_STATE_CONFLICT
```

覆盖：

```text
快递员 A 揽收快递员 B 的任务 -> PERMISSION_DENIED
已揽收任务再次揽收 -> STATE_CONFLICT
```

## 5. 客户端菜单与批量组包

`client/main.cpp` 已接入快递员菜单：

```text
查看待揽收任务
揽收单个快递
批量揽收
查询我的任务
查看个人绩效
```

`PICKUP_BATCH` 支持输入多个快递单号，使用空格或逗号分隔，客户端拆分为 `args` 数组后发送给服务端。

查询任务、批量结果和个人绩效统一走 `TablePrinter` 自适应表格渲染；个人绩效额外绘制 ASCII 完成率进度条。

## 6. 自动联调增强

`--selftest-courier` 覆盖：

```text
管理员创建 courierA / courierB
注册 sender / receiver
sender 充值并寄三单
管理员分配两单给 courierA，一单给 courierB
courierA 登录
courierA 查询待揽收任务，确认 2 单
courierA 越权揽收 courierB 任务，确认 PERMISSION_DENIED
courierA 批量揽收本人 2 单，确认成功
courierA 重复揽收已处理任务，确认 STATE_CONFLICT
courierA 再查待揽收任务，确认清零
绩效检查 waitingPickup -2，waitingSign +2
收入检查精确增加两单费用 50%，本轮为 18.00
```

## 7. 回归结论

Phase 0-6 的目录边界、双进程构建、协议编解码、PING/PONG、Session token、普通用户业务和管理员业务仍保持原有设计。Phase 7 已完成快递员网络业务闭环，下一阶段进入 Phase 8 多客户端并发与异常测试。
