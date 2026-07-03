# 题目三：物流管理系统网络版

## 程序说明

本程序对应综合实验题目三，是独立的传统 C/S 网络程序。服务器和客户端分别编译为两个进程，通过 Winsock TCP socket 和自定义 `REQ/RES` 文本协议通信，不使用 RPC 框架。

客户端负责菜单、输入、密码掩码、请求构造和结果展示；服务器负责身份认证、token 会话、权限判断、业务处理、持久化、日志与并发冲突控制。客户端不直接访问 `data/server/`。

## 目录结构

```text
题目三-物流管理系统网络版/
├─ server/              服务端入口、监听、会话路由
├─ client/              客户端入口、菜单和网络请求
├─ common/
│  ├─ models/           实体和多态物品
│  ├─ protocol/         REQ/RES 协议
│  ├─ security/         哈希、密码和输入校验
│  ├─ service/          核心业务服务
│  └─ storage/          仓储、原子保存和日志
├─ data/server/         仅由服务器访问的数据
├─ docs/                架构、协议、验收和注释说明
├─ tests/               网络版测试清单
├─ build_all.bat
├─ build_server.bat
├─ build_client.bat
├─ run_server.bat
└─ run_client.bat
```

## 编译

环境：Windows、MinGW-w64 g++ 8.1.0 或兼容编译器、C++17、Winsock2。

```bat
build_all.bat
```

也可分别执行：

```bat
build_server.bat
build_client.bat
```

## 启动顺序

必须先启动服务器，再启动客户端。

终端一：

```bat
run_server.bat
```

终端二：

```bat
run_client.bat
```

默认监听地址为 `127.0.0.1`，端口为 `9000`。如果端口被占用，请先关闭占用 9000 端口的进程；若必须改端口，需要同步修改 `server/main.cpp` 与 `client/main.cpp` 中创建 Socket 对象时使用的端口后重新编译。

## 默认演示账号

- 管理员：`admin / Admin0219`
- 普通用户：`login_user / User1234`
- 快递员：`demo_courier / Courier1234`

服务器启动时会确保演示账号存在。测试数据会写入 `data/server/`。

## 一次典型网络请求

1. 客户端连接 `127.0.0.1:9000`。
2. 用户登录，客户端发送 `LOGIN_USER` 请求。
3. 服务器校验密码并返回 token。
4. 客户端寄件时发送带 token 的 `SEND_EXPRESS`。
5. 服务器从 token 解析真实身份，校验参数并多态计价。
6. 服务器完成扣款、订单创建、持久化和日志记录。
7. 服务器编码响应，客户端接收完整换行帧并显示结果。

## 自动验证

服务器 `--once` 模式适合单客户端自测：

```bat
bin\logistics_v3_server.exe --once
bin\logistics_v3_client.exe --selftest-user
```

管理员和快递员自测参数分别为 `--selftest-admin`、`--selftest-courier`。并发测试 `--selftest-concurrency` 需要服务器以默认持续监听模式运行。

## 注意事项

- TCP 为字节流，程序通过换行分帧和接收缓冲区处理半包、粘包。
- 服务器不信任客户端提交的用户名，后续身份均从 token 会话解析。
- 两个客户端同时揽收同一订单时，仅允许一个成功，另一个返回 `STATE_CONFLICT`。
- `bin/` 和 `.exe` 未随提交包附带，请先执行构建脚本。

