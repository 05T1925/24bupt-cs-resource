# v3_LOG03：Phase 2 协议编解码完成记录

## 1. 本次目标

实现 V3 自定义纯文本 Socket 协议的本地编解码层，暂不接入真实 socket。协议层位于：

```text
common/protocol/ProtocolCodec.h
common/protocol/ProtocolCodec.cpp
```

## 2. 核心结构

请求结构：

```text
Request
  command
  token
  args
```

响应结构：

```text
Response
  ok
  code
  message
  records
```

协议异常：

```text
ProtocolError
  code()
  what()
```

## 3. 实际帧格式

请求：

```text
REQ|command|token|argCount|arg1|arg2|...\n
```

响应：

```text
RES|1/0|code|message|recordCount|record1|record2|...\n
```

加入 `argCount` 和 `recordCount` 的原因：

- 明确识别参数缺失。
- 明确识别多余字段。
- 便于 Phase 3 在 socket 流中检测畸形帧。

## 4. 转义规则

编码时：

```text
%  -> %25
|  -> %7C
\n -> %0A
\r -> %0D
```

解码时反向还原。上层业务只看到原始字符串，不需要手动处理协议控制字符。

## 5. 安全限制

当前协议层限制：

```text
MaxMessageLength = 8192
MaxFieldLength   = 1024
MaxVectorItems   = 256
```

异常码：

```text
PROTOCOL_ERROR
UNKNOWN_COMMAND
INVALID_ARGUMENT
```

## 6. 自测结果

`server/main.cpp` 新增 Phase 2 round-trip 自测：

- Request 中包含 `|`、`\n`、`\r`、`%`。
- Response 中包含 `|`、`\n`、`\r`、`%`。
- 执行 Encode -> Decode 后逐字段比对。
- 构造畸形请求，确认抛出 `PROTOCOL_ERROR`。

已执行：

```text
build_server.bat
run_server.bat
build_all.bat
```

结果：

```text
server build success
client build success
ProtocolTest 编解码无损还原成功
Phase 1 business self-test still passes
```

## 7. 解耦检查

已检查 `common/**/*.h` 和 `common/**/*.cpp`：

```text
无 #include <iostream>
无 std::cin
无 std::cout
无 ConsoleUI
```

协议模块保持纯函数式编解码，不依赖 UI、不依赖 socket、不依赖业务服务。

## 8. 后续 Phase 3 接入建议

Phase 3 的 `SocketServer` 每个连接应维护接收缓冲区：

```text
recv bytes
append buffer
while buffer contains '\n':
  extract frame
  ProtocolCodec::decodeRequest(frame)
  ServerController dispatch
  ProtocolCodec::encodeResponse(response)
  send bytes
```

