# IPC 优雅停止协议

## 协议说明

Rust Sparrow RPC 支持通过**控制文件**接收 C++ 发送的停止命令，实现优雅关机。

### 为什么使用控制文件而不是 stdin？

- **stdin 冲突**：Rust 主程序需要从 stdin 读取输入数据，读取时使用 `read_to_string()` 会阻塞直到 EOF
- **如果 C++ 不关闭 stdin**：Rust 永远无法完成输入读取，无法开始优化
- **如果 C++ 关闭 stdin**：无法再通过 stdin 发送 STOP 命令

**解决方案**：使用控制文件作为信号机制，避免与 stdin 冲突。

### 通信流程

1. **C++ 启动 Rust 程序**，通过 stdin 发送输入数据，**然后关闭 stdin 写端**
2. **Rust 程序开始运行**，通过 stdout 发送 `SolverEvent` JSON 事件
3. **C++ 创建控制文件**：在 Rust 工作目录创建 `stop_signal.txt` 文件
4. **Rust 检测到控制文件**，完成当前优化迭代
5. **Rust 发送 GracefulShutdown 事件**，通知 C++ 正在停止
6. **Rust 继续输出最终结果** (`FinalResult`)
7. **Rust 程序正常退出**，自动删除控制文件
8. **C++ 清理控制文件**（可选）

### 协议详情

#### 1. C++ → Rust：停止命令

**创建控制文件**：
```cpp
// 创建 stop_signal.txt 文件通知 Rust 停止
HANDLE hFile = CreateFileA(
    "stop_signal.txt",
    GENERIC_WRITE,
    0,
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL
);
WriteFile(hFile, "STOP", 4, &written, NULL);
CloseHandle(hFile);
```

**默认控制文件路径**：`stop_signal.txt`（在 Rust 程序的工作目录）

#### 2. Rust → C++：事件消息

所有消息都是单行 JSON，以换行符结束：

**GracefulShutdown 事件**
```json
{"GracefulShutdown":{"reason":"Stop command received from C++"}}
```

**FinalResult 事件**
```json
{"FinalResult":{"data":"base64_encoded_json_data..."}}
```

**Progress 事件**
```json
{"Progress":{"step":1,"strip_width":85.5,"kind":"ExplFeas"}}
```

**Svg 事件**
```json
{"Svg":{"name":"0_100.000_expl_f","content":"base64_encoded_svg..."}}
```

### 关键特性

1. **不丢失数据**：收到停止命令后，Rust 会完成当前优化迭代并发送最终结果
2. **Ctrl+C 支持**：也支持 Ctrl+C 优雅停止
3. **即时响应**：停止命令立即生效，不会等待超时
4. **超时保护**：如果设置了超时，超时也会触发停止

## C++ 集成指南

### 基本用法

```cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>

// 1. 创建管道和进程
HANDLE hStdInWrite, hStdOutRead;
// ... 创建管道代码 ...

// 2. 启动 Rust 程序
CreateProcessA(NULL, "sparrow_rpc.exe --output-mode stdout -", 
               ..., &hStdInWrite, &hStdOutRead, ...);

// 3. 发送输入数据并关闭 stdin
std::string inputJson = "...";  // 你的 SPP 实例数据
WriteFile(hStdInWrite, inputJson.c_str(), inputJson.size(), &written, NULL);
CloseHandle(hStdInWrite);  // 关闭 stdin，Rust 才能开始运行

// 4. 在需要时创建控制文件发送停止命令
HANDLE hFile = CreateFileA(
    "stop_signal.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL, NULL);
WriteFile(hFile, "STOP", 4, &written, NULL);
CloseHandle(hFile);

// 5. 读取输出
char buffer[4096];
ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL);
// 解析 JSON 事件...

// 6. 等待进程结束（给足够时间发送结果）
WaitForSingleObject(hProcess, 30000);  // 等待最多30秒
```

### 重要注意事项

1. **使用 `--output-mode stdout`**：确保结果输出到 stdout 而不是文件
2. **使用 `-` 作为输入**：告诉 Rust 从 stdin 读取输入
3. **及时关闭 stdin**：发送输入数据后必须关闭 `hStdInWrite`，否则 Rust 无法开始优化
4. **等待进程结束**：发送 STOP 后要给 Rust 足够时间完成当前迭代
5. **处理所有事件**：读取 stdout 直到进程结束，不要错过 `FinalResult`

### 完整示例

参见 `cpp_integration_example.cpp`

## 与旧版对比

| 特性 | 旧版 (Ctrl+C) | 新版 (IPC 协议) |
|------|---------------|-----------------|
| 触发方式 | 两次 Ctrl+C | C++ 创建控制文件 |
| 数据丢失 | 强制停止后丢失 | 优雅停止，完整接收 |
| 程序退出 | 可能非正常退出 | 正常退出，返回0 |
| 结果通知 | 无 | 有 GracefulShutdown 事件 |

## 命令行参数

```bash
# 仅输出到 stdout（用于 C++ 集成）
sparrow_rpc.exe --output-mode stdout -

# 文件 + stdout 输出
sparrow_rpc.exe --output-mode both input.json

# 仅文件输出
sparrow_rpc.exe --output-mode file input.json
```
