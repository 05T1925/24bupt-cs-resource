# v3_LOG01：Phase 0 独立工程初始化完成记录

## 1. 本次目标

正式启动 V3 网络版 Phase 0，建立一个与 V1/V2 物理隔离的 C/S 工程骨架，并验证 server/client 可以分别编译为独立进程。

## 2. 已生成工程文件

根目录脚本：

```text
init_phase0.bat
build_server.bat
build_client.bat
build_all.bat
run_server.bat
run_client.bat
```

入口源码：

```text
server/main.cpp
client/main.cpp
```

占位目录：

```text
common/models
common/protocol
common/security
common/service
common/storage
data
docs
tests
LOG
bin
```

## 3. 构建策略

两个构建脚本分别生成：

```text
bin/logistics_v3_server.exe
bin/logistics_v3_client.exe
```

编译参数：

```text
g++ -std=c++17 -Wall -Wextra -pedantic -O2 -finput-charset=UTF-8 -fexec-charset=GBK
```

链接参数：

```text
-lws2_32
```

提前链接 Winsock 的原因：Phase 0 虽然还不实现 socket 收发，但先验证 Windows 网络库编译环境，避免 Phase 3 才暴露工具链问题。

## 4. 占位程序设计

`server/main.cpp`：

- 使用 `ServerBootstrap` 类封装初始化和退出。
- 调用 `WSAStartup()`。
- 打印服务端启动信息。
- 调用 `WSACleanup()`。

`client/main.cpp`：

- 使用 `ClientBootstrap` 类封装初始化和退出。
- 调用 `WSAStartup()`。
- 打印客户端启动信息。
- 调用 `WSACleanup()`。

这样做保持课程规范：主函数尽量简洁，除 `main()` 外逻辑放入类成员函数。

## 5. 验证结果

已运行：

```text
init_phase0.bat
build_all.bat
bin/logistics_v3_server.exe
bin/logistics_v3_client.exe
```

结果：

```text
server build success
client build success
server placeholder starts successfully
client placeholder starts successfully
```

说明当前 Phase 0 的物理隔离、双进程编译和基线运行测试已通过。

## 6. 下一阶段入口

Phase 1 建议开始迁移 V2 业务核心到 `common/`：

```text
common/models      实体模型
common/storage     仓储和日志
common/security    密码哈希和输入校验
common/service     LogisticsSystem
```

迁移时继续遵守红线：

```text
common 不放菜单
common 不使用 std::cin
client 不直接改数据文件
server 不信任客户端用户名
```

