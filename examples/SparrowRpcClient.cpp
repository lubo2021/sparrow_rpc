#include "SparrowRpcClient.h"
#include "SparrowTask.h"
#include "nlohmann/json.hpp"
#include "FileUtils.h"
#include "StringUtils.h"

// 简单的 JSON 解析辅助函数
std::string extractJsonField(const std::string& json, const std::string& field) {
	size_t pos = json.find("\"" + field + "\":");
	if (pos == std::string::npos) return "";

	pos = json.find_first_of(":", pos) + 1;
	// 跳过空白字符
	while (pos < json.size() && std::isspace(json[pos])) pos++;

	// 检查是否为字符串
	if (json[pos] == '"') {
		pos++;
		size_t endPos = json.find_first_of("\"", pos);
		return json.substr(pos, endPos - pos);
	}

	// 数字或其他类型
	size_t endPos = json.find_first_of(",}", pos);
	return json.substr(pos, endPos - pos);
}

SparrowRpcClient::SparrowRpcClient() : hStdInWrite(NULL), hStdOutRead(NULL),
hStdErrRead(NULL), hProcess(NULL),
running(false), workingDir(nullptr) {
	std::wstring configPath = FileUtils::GetPluginDir() + L"\\stop_signal.txt";
	stopSignalFile = ToString(configPath);
	workingDir = FileUtils::GetPluginDir();
}

bool SparrowRpcClient::start(std::shared_ptr<SparrowTask> task) {
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	// 创建管道
	HANDLE hStdInRead, hStdOutWrite, hStdErrWrite;

	if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
		std::cerr << "Failed to create stdin pipe\n";
		return false;
	}
	if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
		std::cerr << "Failed to create stdout pipe\n";
		return false;
	}
	// 确保写端不被继承（用于子进程的 stdout/stderr）
	SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);
	// 启动进程
	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdInput = hStdInRead;
	si.hStdOutput = hStdOutWrite;
	si.hStdError = NULL;
	si.dwFlags = STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	// 设置 Rust 工作目录为插件目录，确保控制文件路径一致
	std::wstring pluginDir = workingDir;
	
	if (!CreateProcessA(
		NULL,                    // 不指定模块名
		(LPSTR)task->command.c_str(),  // 命令行
		NULL,                    // 进程安全属性
		NULL,                    // 线程安全属性
		TRUE,                    // 继承句柄
		CREATE_NO_WINDOW,        // 无窗口
		NULL,                    // 环境变量
		NULL,                    // 当前目录
		&si,
		&pi)) {
		std::cerr << "Failed to start process: " << GetLastError() << "\n";
		task->status = TaskStatus::Failed;
		return false;
	}

	hProcess = pi.hProcess;
	pid = pi.dwProcessId;

	// 关闭不需要的句柄
	CloseHandle(pi.hThread);
	CloseHandle(hStdInRead);
	CloseHandle(hStdOutWrite);
	// 保存进程信息以便外部终止
	task->processHandle = pi.hProcess;
	task->processId = pi.dwProcessId;  // 保存PID用于发送Ctrl+C
	task->status = TaskStatus::Running;
	// 发送输入数据
	DWORD written;
	WriteFile(hStdInWrite, task->inputPath.c_str(), (DWORD)task->inputPath.size(), &written, NULL);
	CloseHandle(hStdInWrite);
	hStdInWrite = NULL;
	// 启动读取线程
	running = true;
	//readerThread = std::thread(&SparrowRpcClient::readOutput, this, task->id);
	//readerThread.join();
	readOutput(task->id);
	std::cout << "[C++] Rust process started (PID: " << pid << ")\n";
	return true;
}

void SparrowRpcClient::readOutput(int taskId) {
	char buffer[4096];
	std::string lineBuffer;
	DWORD bytesRead;

	while (running) {
		if (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)&& bytesRead > 0) {
			lineBuffer.append(buffer, bytesRead);
			// 处理行
			size_t pos;
			while ((pos = lineBuffer.find('\n')) != std::string::npos) {
				std::string line = lineBuffer.substr(0, pos);
				lineBuffer.erase(0, pos + 1);
				if (line.empty()) continue;

				try {
					/*auto j = nlohmann::json::parse(line, nullptr, false);
					if (j.contains("GracefulShutdown"))
					{
						mQueue.push({ taskId, line });
					}*/
					if (line.back() == '\r') {
						line.pop_back();
					}
					mQueue.push({ taskId, line }); // 推JSON对象
					//processEvent(line);
				}
				catch (...) {
					// ❗如果解析失败，说明还没接完整
					// 不要丢！应该拼回去
				}



			}

		}
		
	}
	printf(".......");
}

void SparrowRpcClient::processEvent(const std::string& json) {
	std::lock_guard<std::mutex> lock(eventMutex);
	receivedEvents.push_back(json);

	// 解析事件类型
	std::string eventType = extractJsonField(json, "type");
	if (eventType.empty()) {
		// 尝试其他方式识别
		if (json.find("\"Svg\"")) eventType = "Svg";
		else if (json.find("\"Progress\"")) eventType = "Progress";
		else if (json.find("\"GracefulShutdown\"")) eventType = "GracefulShutdown";
		else if (json.find("\"FinalResult\"")) eventType = "FinalResult";
	}

	std::cout << "[C++] Received event: " << eventType << "\n";

	if (json.find("GracefulShutdown") != std::string::npos) {
		std::string reason = extractJsonField(json, "reason");
		std::cout << "[C++] Graceful shutdown notification: " << reason << "\n";
	}
	else if (json.find("FinalResult") != std::string::npos) {
		std::cout << "[C++] Final result received (base64 encoded)\n";
	}
	else if (json.find("Progress") != std::string::npos) {
		std::string step = extractJsonField(json, "step");
		std::string width = extractJsonField(json, "strip_width");
		std::cout << "[C++] Progress - Step " << step << ", Width: " << width << "\n";
	}
}

void SparrowRpcClient::requestStop() {
	// 创建控制文件通知 Rust 停止
	std::string controlFile = stopSignalFile;
	HANDLE hFile = CreateFileA(
		controlFile.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (hFile != INVALID_HANDLE_VALUE) {
		std::string content = "STOP";
		DWORD written;
		WriteFile(hFile, content.c_str(), content.size(), &written, NULL);
		CloseHandle(hFile);
		std::cout << "[C++] Stop control file created: " << controlFile << "\n";
	}
	else {
		std::cerr << "[C++] Failed to create stop control file: " << GetLastError() << "\n";
	}
}

void SparrowRpcClient::cleanupControlFile() {
	DeleteFileA(stopSignalFile.c_str());
}

void SparrowRpcClient::forceStop() {
	if (hProcess != NULL) {
		TerminateProcess(hProcess, 1);
		std::cout << "[C++] Process forcefully terminated\n";
	}
}

bool SparrowRpcClient::wait(DWORD timeoutMs) {
	if (hProcess == NULL) return false;

	DWORD result = WaitForSingleObject(hProcess, timeoutMs);
	return result == WAIT_OBJECT_0;
}

void SparrowRpcClient::stop() {
	running = false;

	if (readerThread.joinable()) {
		readerThread.join();
	}

	if (hStdInWrite != NULL) {
		CloseHandle(hStdInWrite);
		hStdInWrite = NULL;
	}

	if (hStdOutRead != NULL) {
		CloseHandle(hStdOutRead);
		hStdOutRead = NULL;
	}

	if (hStdErrRead != NULL) {
		CloseHandle(hStdErrRead);
		hStdErrRead = NULL;
	}

	if (hProcess != NULL) {
		CloseHandle(hProcess);
		hProcess = NULL;
	}
}

std::vector<std::string> SparrowRpcClient::getAllEvents() {
	std::lock_guard<std::mutex> lock(eventMutex);
	return receivedEvents;
}

// 创建示例输入数据
std::string createSampleInput() {
	// 这是一个简化的 SPP 实例 JSON
	return R"({
        "name": "test_instance",
        "bin": {"width": 100.0, "height": 100.0},
        "items": [
            {"id": 1, "width": 20.0, "height": 20.0, "demand": 5},
            {"id": 2, "width": 30.0, "height": 15.0, "demand": 3}
        ]
    })";
}
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