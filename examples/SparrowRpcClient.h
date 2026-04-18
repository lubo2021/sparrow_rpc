#pragma once


/**
 * C++ 与 Rust Sparrow RPC 集成示例
 *
 * 展示了如何使用 IPC 协议优雅地停止 Rust 程序并接收结果
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include "EventQueue.h"


class SparrowTask;
class SparrowRpcClient {
private:
    HANDLE hStdInWrite;
    HANDLE hStdOutRead;
    HANDLE hStdErrRead;
    HANDLE hProcess;
    DWORD pid;
    std::thread readerThread;
    std::atomic<bool> running;
    std::vector<std::string> receivedEvents;
    EventQueue mQueue;
    std::string stopSignalFile;
public:
    SparrowRpcClient();

    ~SparrowRpcClient() {
        // 清理控制文件
        cleanupControlFile();

        stop();
    }

    // 启动 Rust 程序
    bool start(std::shared_ptr<SparrowTask> task);

    // 读取输出线程
    void readOutput(int taskId);
    // 处理接收到的事件
    void processEvent(const std::string& json);

    // 请求优雅停止
    void requestStop();

    // 清理控制文件（如果需要）
    void cleanupControlFile();
    // 强制停止
    void forceStop();

    // 等待进程结束
    bool wait(DWORD timeoutMs = INFINITE);

    // 停止读取和清理
    void stop();
    
    bool PopEventQueue(TaskEvent& e)
    {
        return mQueue.pop(e);
    }
    // 获取所有接收到的原始事件
    std::vector<std::string> getAllEvents();

private:
    std::mutex eventMutex;
   
};

#if 0
int main1() {
    SparrowRpcClient client;

    // 1. 启动 Rust 程序并发送输入
    std::string input = createSampleInput();
    if (!client.start("sparrow_rpc.exe", input)) {
        std::cerr << "Failed to start Rust process\n";
        return 1;
    }

    // 2. 让程序运行一段时间
    std::cout << "[C++] Letting Rust run for 5 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 3. 发送优雅停止命令
    std::cout << "[C++] Requesting graceful shutdown...\n";
    client.requestStop();

    // 4. 等待 Rust 进程结束（最多等待30秒）
    std::cout << "[C++] Waiting for Rust to finish...\n";
    if (client.wait(30000)) {
        std::cout << "[C++] Rust process ended gracefully\n";
    }
    else {
        std::cout << "[C++] Timeout, force stopping...\n";
        client.forceStop();
    }

    // 5. 获取所有事件
    auto events = client.getAllEvents();
    std::cout << "[C++] Total events received: " << events.size() << "\n";

    // 查找最终结果
    bool foundFinalResult = false;
    for (const auto& event : events) {
        if (event.find("FinalResult") != std::string::npos) {
            foundFinalResult = true;
            std::cout << "[C++] Found FinalResult event - data successfully received!\n";
            break;
        }
    }

    if (!foundFinalResult) {
        std::cerr << "[C++] Warning: FinalResult not received - data may have been lost!\n";
    }

    client.stop();
    return 0;
}
#endif