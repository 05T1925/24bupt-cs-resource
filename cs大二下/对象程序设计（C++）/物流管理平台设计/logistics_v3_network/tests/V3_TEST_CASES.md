# V3 测试用例清单

## 1. 网络连接

- server 启动并监听指定端口。
- client 可连接 server。
- server 未启动时 client 给出连接失败提示。
- client 异常退出后 server 不崩溃。
- 多个 client 可先后连接。
- 多个 client 可同时连接，server 主线程继续 accept，新连接由 worker thread 处理。
- 一个 client 登录后直接关闭窗口，server 打印 client disconnected 或 cleaned session 日志，其他 client 仍可继续请求。
- 一个 client 发送协议帧后断开，server 不崩溃并关闭该 socket。

## 2. 协议解析

- `PING` 返回成功。
- 空请求返回 `PROTOCOL_ERROR`。
- 未知命令返回 `UNKNOWN_COMMAND`。
- 参数数量不足返回 `PROTOCOL_ERROR`。
- 超长字段返回 `INVALID_ARGUMENT`。
- 字段中包含 `|`、换行、百分号时能正确转义和还原。
- 连续发送两条命令，服务端能拆成两条处理。
- 一条命令拆成两次发送，服务端能合并处理。
- 一次 `send` 拼接两条完整请求帧，服务端能按两个 `\n` 分别处理并返回两条响应。
- 恶意客户端只发送半条帧后断开，服务端释放连接资源，不影响后续客户端。
- 恶意客户端持续发送不带 `\n` 的超长半包，超过缓冲限制后返回 `PROTOCOL_ERROR` 并断开。
- 请求字段中包含 `%7C`、`%0A`、`%25` 等转义序列时，业务层看到还原后的原始文本。

## 3. 登录与 Session

- 用户登录成功并返回 token。
- 快递员登录成功并返回 token。
- 管理员登录成功并返回 token。
- 密码错误返回 `AUTH_FAILED`。
- 普通用户或快递员连续三次失败后冻结。
- 无 token 请求业务命令返回 `AUTH_REQUIRED`。
- 错误 token 返回 `AUTH_REQUIRED` 或 `TOKEN_EXPIRED`。
- 用户 token 调管理员命令返回 `PERMISSION_DENIED`。

## 4. 普通用户业务

- 注册用户成功。
- 重复用户名注册失败。
- 用户充值成功。
- 未登录时查询余额返回 `AUTH_REQUIRED`。
- 使用非 USER token 调用用户业务返回 `PERMISSION_DENIED`。
- 普通快递 `3 kg` 费用为 `15.00`。
- 图书 `4 本` 费用为 `8.00`。
- 易碎品 `2 kg` 费用为 `16.00`。
- 网络版寄件请求不传费用，费用必须由服务端计算。
- 易碎品 `2 kg` 网络寄件后，余额从 `100.00` 变为 `84.00`。
- 余额不足时寄件失败。
- 收件人不存在时寄件失败。
- 向自己寄件失败。
- 用户只能查询自己相关快递。
- 用户不能签收待揽收快递。
- 用户不能签收他人快递。
- 发件人恶意签收寄给收件人的快递返回 `PERMISSION_DENIED`。
- 重复签收失败。

## 5. 管理员业务

- 管理员新增快递员成功。
- 重复快递员用户名新增失败。
- 普通用户 token 调用管理员命令返回 `PERMISSION_DENIED`。
- 越权管理员命令写入 `ADMIN_PERMISSION_DENIED` 安全审计日志。
- 冻结快递员后快递员登录失败。
- 给待揽收快递分配快递员成功。
- 分配给不存在快递员失败。
- 分配给被冻结快递员失败。
- 自动分配单个快递成功。
- 一键自动分配全部待揽收快递成功。
- 停用有未完成任务的快递员失败并返回冲突清单。
- 冲突清单通过 `Response.records` 返回，并由客户端表格展示。
- 管理员查询全部快递成功。
- 管理员查看统计看板成功。
- 管理员查看快递员绩效排行成功。
- 管理员校验日志哈希链成功。

## 6. 快递员业务

- 快递员只能查看本人待揽收任务。
- 快递员单个揽收成功。
- 快递员批量揽收成功。
- 快递员揽收他人任务失败。
- 重复揽收失败。
- 揽收后状态变为待签收。
- 揽收后管理员余额减少 50%，快递员收入增加 50%。
- 快递员查询本人任务时可按状态筛选。
- 快递员查看个人绩效成功。

## 7. 并发与一致性

- 两个 client 同时签收同一快递，只成功一次。
- 两个 client 同时揽收同一快递，只成功一次。可运行默认监听服务端后执行 `bin\logistics_v3_client.exe --selftest-concurrency` 自动复现。
- `--selftest-concurrency` 预期结果：一个 `OK|SUCCESS`，另一个 `ERR|STATE_CONFLICT`。
- 并发揽收后管理员余额只扣一次 50% 提成，快递员收入只增加一次 50% 提成。
- 并发冲突请求写入 `COURIER_PICKUP_STATE_CONFLICT` 审计日志。
- 管理员自动分配时，快递员同时揽收不会造成数据损坏。
- 多个用户同时寄件后快递单号不重复。
- 多个请求同时写日志时日志序号连续或至少不破坏哈希链。
- 多客户端同时执行查询类请求时，server 不阻塞 accept 新连接。
- 一个连接异常断开时，不影响其他连接正在进行的 `PICKUP_EXPRESS` 或 `SIGN_EXPRESS`。

## 8. 持久化与日志

- server 重启后用户、快递员、快递、余额、状态仍保留。
- 注册、登录失败、寄件、分配、揽收、签收写业务日志。
- 协议错误、权限不足、token 无效写网络日志或业务日志。
- 手动篡改日志后哈希链校验失败。
- 保存失败时业务操作回滚，不提示假成功。

## 9. 即时查重与格式校验

- `CHECK_USERNAME_AVAILABLE` 检测用户名已存在返回 `DUPLICATE`。
- `CHECK_USERNAME_AVAILABLE` 跨角色查重（用户、快递员、管理员不可重名）。
- `CHECK_PHONE_AVAILABLE` 校验 11 位手机号格式。
- 客户端注册流程中实时调用查重/校验，不通过则拒绝继续。

## 10. 密码修改

- 用户修改密码：旧密码正确且新密码强度合法则成功。
- 快递员修改密码：旧密码正确且新密码强度合法则成功。
- 管理员修改密码：旧密码正确且新密码强度合法则成功。
- 旧密码错误返回 `AUTH_FAILED`。
- 新旧密码相同返回 `PASSWORD_UNCHANGED`。
- 新密码纯数字或纯字母被拒绝。

## 11. 重复登录拦截

- 同一用户名+角色已持有活跃 session 时，新的登录请求返回 `ALREADY_LOGGED_IN`。
- 客户端正常登出后，同一账号可再次登录。
- 客户端断线后，服务端自动清理该连接的 session，账号可重新登录。

## 12. 余额不足即时充值

- 寄件时余额不足返回 `BALANCE_NOT_ENOUGH`，并附带当前余额、费用、缺口金额。
- 客户端提示用户选择是否立即充值。
- 充值成功后询问是否用原寄件信息重新提交。
- 重新提交成功后正确显示单号、费用、剩余余额。

## 13. 个人信息修改

- 用户自助修改 name/phone/address（`UPDATE_MY_PROFILE`）。
- 快递员自助修改 name/phone（`UPDATE_COURIER_PROFILE`）。
- 管理员修改任意用户信息含冻结状态（`ADMIN_UPDATE_USER`）。
- 管理员修改任意快递员信息含冻结状态（`ADMIN_UPDATE_COURIER`）。
- 修改后数据持久化，重启 server 后仍保留。

## 14. 快递改派

- 改派仅允许 WaitingPickup 状态的快递（未分配或已分配均可）。
- WaitingSign 状态拒绝改派（需回滚提成）。
- Signed 状态拒绝改派。
- 改派后旧快递员收到 `TASK_REASSIGNED_AWAY` 通知。
- 改派后新快递员收到 `TASK_REASSIGNED` 通知。

## 15. 快递备注修改

- 发件人可修改自己快递的备注（仅 WaitingPickup）。
- 管理员可修改任意快递的备注（仅 WaitingPickup）。
- 非 WaitingPickup 状态或非本人/管理员返回 `PERMISSION_DENIED`。

## 16. 评分

- 收件人可对已签收快递评分（1-5 分）。
- 非收件人评分返回 `PERMISSION_DENIED`。
- 非已签收状态评分返回 `STATE_CONFLICT`。
- 重复评分返回 `DUPLICATE_RATING`。
- 评分后快递员收到 `RATING_RECEIVED` 通知。

## 17. 通知中心

- 业务事件（冻结、分配、揽收、签收、改派、评分）自动生成通知。
- `QUERY_MY_NOTIFICATIONS` 查询全部通知。
- `QUERY_MY_NOTIFICATIONS 1` 仅查未读通知。
- `MARK_NOTIFICATION_READ <id>` 标记单条已读，越权或不存在返回 `NOT_FOUND`。
- `QUERY_UNREAD_NOTIFICATION_COUNT` 查询未读数量。
- 登录成功后自动显示未读通知数量。
- `notifications.txt` 文件不存在时 server 正常启动，不报错。
- 通知写入失败不回滚核心业务（best-effort）。

## 18. 资金流水一致性

- 寄件：用户扣款 100%，平台收款 100%。
- 揽收：平台付款 50%，快递员收款 50%。
- 并发揽收时只执行一次资金流转（Mutex 保护）。
- 管理后台余额显示与 `admin.txt` 一致。
- 快递员收入与揽收记录可交叉验证。

## 19. q/Q 取消交互

- 所有交互式输入流程支持输入 `q` 或 `Q` 取消当前操作。
- 取消后不发送任何业务请求到服务端。
- 取消返回上级菜单或主流程，不残留状态。

## 20. 通知中心专项验证

- 通知 ID 格式 `NT000001` 递增。
- 通知文件 8 字段 pipe 分隔，`readFlag` 为 0 或 1。
- 跨角色通知隔离：用户只能看到自己角色的通知。
- 通知持久化：重启 server 后历史通知仍可查询。
- `admin.txt` 中不包含密码明文。
- 日志中不出现 token 完整值和密码明文。

## 21. 最终回归指令

提交前建议按以下顺序执行：

```bat
build_all.bat

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-admin

bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-courier

bin\logistics_v3_server.exe
bin\logistics_v3_client.exe --selftest-concurrency
```

最后一组命令需要手动保留 server 终端运行，并在并发测试结束后关闭 server。
