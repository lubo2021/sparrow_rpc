#include "SparrowDlg.h"
#include "DocumentContext.h"
#include "StringUtils.h"
#include "FileUtils.h"

namespace ap {


	SparrowDlg::SparrowDlg()
	{
		setup.dialogId = DIALOG_SPARROW;
		mConfig.Load(ToString(FileUtils::GetPluginDir().append(L"\\config\\sparrow_config.json")));
		if(!mSparrowOperate)
		{
			mSparrowOperate = std::make_shared<SparrowOperate>(mConfig);
		}
	}

	BOOL SparrowDlg::OnInitDialog()
	{
		ASErr error = kNoErr;
		char notifierName[kMaxStringLength];
		error = RegisterNotifier(kStartSparrowNotifier, fStartSparrowNotifier);
		CHECK_ERROR(error);
		error = RegisterNotifier(kDisposalImportNotifier, fDisposalImportNotifier);
		CHECK_ERROR(error);
	
		
		void InitConfig();
		
		return TRUE;
	}

	void SparrowDlg::StartSvgPreview()
	{

		if (!mUISvgPreview)
		{
			mUISvgPreview = std::make_unique<UISvgPreview>();
			mUISvgPreview->Attach(hwnd(), GetDlgItem(hwnd(), stSvgPreview));
			GenerateOriginalArtSvg();
			ShowOriginalArt();
		}
		if (!hwnd()) return;
		SetTimer(hwnd(), 1, 2000, nullptr);
	}
	
	void SparrowDlg::InitConfig()
	{
		UIConfigManager& manager = UIConfigManager::GetIns();

		std::wstring configPath = FileUtils::GetPluginDir().append(L"\\config\\sparrow_config.json");
		manager.Initialize(ToString(configPath));

		
	}

	void SparrowDlg::OnStart()
	{
		ResetSparrowOperate();
		if (mIsRunning) return;
		// 清空历史，开始新排料
		mHistory.clear();
		mCurrentHistoryIndex = -1;
		mNextResultId = 1;
		//排料
		sAINotifier->Notify(
			kStartSparrowNotifier,
			(void*)this
		);
		mIsRunning = true;
	}
	void SparrowDlg::StartSparrow()
	{
		// 构建命令行		
		std::string cmd = "\\sparrow.exe  -i - ";
		for (const auto& arg : mConfig.buildArgs()) cmd += arg + " ";
		//ApplyConfigToCommand(cmd);
		//cmd += " --output-mode stdout";
		std::wstring command = FileUtils::GetPluginDir().append(ToWString(cmd));
		std::string inputStr = mSparrowOperate->mInputJson.dump();
		mCurrentTaskId = mTaskManager.AddTask(ToString(command), inputStr);
	}

	void SparrowDlg::ResetSparrowOperate(std::shared_ptr<SparrowOperate> operate)
	{
		if (operate)
		{
			mSparrowOperate = std::move(operate);
		}else 
		{
			mSparrowOperate.reset(new SparrowOperate(mConfig));
		}
	}

	void SparrowDlg::OnPause(int taskId)
	{
		auto& task = mTaskManager.GetTask(taskId);
		if (task) task->pause = true;
	}
	
	void SparrowDlg::OnResume(int taskId)
	{
		auto& task = mTaskManager.GetTask(taskId);
		if (task) task->pause = false;
	}

	void SparrowDlg::OnCancel(int taskId)
	{
		auto& task = mTaskManager.GetTask(taskId);
		if (task && task->processHandle) {
			TerminateProcess(task->processHandle, 0);
			task->status = TaskStatus::Failed;
		}
	}
	
	void SparrowDlg::OnOk()
	{
		sAINotifier->Notify(
			kDisposalImportNotifier,
			(void*)mSparrowOperate.get()
		);

		std::wstring configPath = FileUtils::GetPluginDir().append(L"\\config\\sparrow_config.json");
		mConfig.Save(ToString(configPath));
    }

	void SparrowDlg::OnTimer(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (wParam != 1) return;

		TaskEvent e;

		while (mTaskManager.PopEventQueue(e))
		{
			// 解析 JSON（你已有 nlohmann）
			auto j = nlohmann::json::parse(e.msg, nullptr, false);

			if (j.is_discarded()) continue;

			if (j.contains("Progress")) {
				//{"Progress":{"step":0,"strip_width":3019.178955078125,"kind":"ExplImproving"}}
				auto& p = j["Progress"];
				UpdateTaskProgress(
					p.value("step", 0),
					p.value("strip_width", 0.0),
					p.value("kind", "")
				);
			}
			else if (j.contains("Svg"))
			{
				//{"Svg":{"name":"0_3019.179_expl_i","content":"P
				auto& s = j["Svg"];
				std::string name = s.value("name", "");
				std::string content = s.value("content", "");
				auto data = Base64Decode(content);
				std::string svg(data.begin(), data.end());
				// 更新SvgPreview
				mUISvgPreview->SetSvg(svg);
				// 保存到历史
				NestingResult result;
				result.id = mNextResultId++;
				result.timestamp = GetCurrentTimestamp();
				result.svgContent = svg;

				// 尝试解析当前宽度
				//result.stripWidth = mCurrentStripWidth;  // 从 Progress 记录

				AddToHistory(result);
			}
			else if (j.contains("FinalResult"))
			{
				//{"FinalResult":{"data":{"name":"ai_import"
				auto& s = j["FinalResult"];
				std::string content = s.value("data", "");
				auto data = Base64Decode(content);
				std::string finalResult(data.begin(), data.end());

				// 保存最终结果
				NestingResult finalRes;
				finalRes.id = mNextResultId++;
				finalRes.timestamp = GetCurrentTimestamp();
				finalRes.finalResultJson = finalResult;
				finalRes.isFinal = true;

				// 解析利用率等信息
				auto fj = nlohmann::json::parse(finalResult, nullptr, false);
				if (!fj.is_discarded()) {
					if (fj.contains("solution")) {
						finalRes.utilization = fj["solution"].value("utilization", 0.0);
						finalRes.placedCount = fj["solution"].value("placed_count", 0);
						finalRes.totalCount = fj["solution"].value("total_count", 0);
					}
				}

				AddToHistory(finalRes);

				// 应用最终结果
				if (mSparrowOperate->SetDisposalResult(finalResult)) {
					// 成功
					mIsRunning = false;
				}
			}
			else if (j.contains("log"))
			{
				std::string msg = j["msg"];
			}

			//AppendLog(e.msg);
		}

	}
	
	void SparrowDlg::AddToHistory(const NestingResult& result)
	{
		mHistory.push_back(result);
		mCurrentHistoryIndex = mHistory.size() - 1;

		// 更新 UI 显示可以浏览历史
		// EnableWindow(GetDlgItem(hwnd(), btPrevious), mHistory.size() > 1);
	}

	void SparrowDlg::OnPreviousResult()
	{
		if (mCurrentHistoryIndex > 0) {
			mCurrentHistoryIndex--;
			ShowResult(mCurrentHistoryIndex);
		}
	}

	void SparrowDlg::OnNextResult()
	{
		if (mCurrentHistoryIndex >= 0 && mCurrentHistoryIndex < (int)mHistory.size() - 1) {
			mCurrentHistoryIndex++;
			ShowResult(mCurrentHistoryIndex);
		}
	}

	void SparrowDlg::ShowResult(int index)
	{
		if (index < 0 || index >= (int)mHistory.size()) return;

		const auto& result = mHistory[index];

		// 如果有 SVG 内容，显示 SVG
		if (!result.svgContent.empty()) {
			mUISvgPreview->SetSvg(result.svgContent);
		}

		// 更新状态栏显示信息
		std::ostringstream status;
		status << "结果 " << (index + 1) << "/" << mHistory.size();
		if (result.isFinal) {
			status << " [最终] 利用率: " << std::fixed << std::setprecision(2)
				<< (result.utilization * 100) << "%";
		}
		else {
			status << " [中间] 宽度: " << std::fixed << std::setprecision(2)
				<< result.stripWidth;
		}
		// SetWindowText(GetDlgItem(hwnd(), stStatus), status.str().c_str());
	}

	std::string SparrowDlg::GetCurrentTimestamp()
	{
		auto now = std::time(nullptr);
		auto tm = *std::localtime(&now);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		return oss.str();
	}

	void SparrowDlg::UpdateTaskProgress(int step, double width, const std::string& kind)
	{
		/* int row = FindRowByTaskId(id);
		 if (row >= 0) {
			 std::string text = std::to_string(int(progress * 100)) + "%";
			 ListView_SetItemText(hList, row, 2, (LPSTR)text.c_str());
		 }*/
	}

	void SparrowDlg::OnCommand(WORD id, WORD code, HWND hCtrl)
	{
		switch (id) {
		case btStart:
			OnStart();
			break;
		case btCancel:
			OnCancel(0);
			break;
		case btPause:
			OnPause(0);
			break;

		case IDOK:
			OnOk();
			OnClose(IDOK);
			break;
		case IDCANCEL:
			OnClose(IDCANCEL);
			break;
		define:
			break;
		}
	}


	LRESULT SparrowDlg::OnNotify(LPARAM lParam)
	{
		return TRUE;
	}

	void SparrowDlg::OnSize(UINT state, int cx, int cy)
	{

	}

	LRESULT  SparrowDlg::OnDrawItem(LPDRAWITEMSTRUCT dis)
	{

		return BaseDialog::OnDrawItem(dis);
	}

	BOOL SparrowDlg::OnClose(int nResult)
	{
		if (hwnd())
		{
			KillTimer(hwnd(), 1);
			// 取消正在进行的任务
			if (mIsRunning && mCurrentTaskId >= 0)
			{
				OnCancel(mCurrentTaskId);
			}
		}
		// 清理预览控件（防止 D2D 资源泄漏）
		if (mUISvgPreview)
		{
			mUISvgPreview.reset();  // 先销毁 UI 控件
		}
		// 重置运行状态
		mIsRunning = false;
		mCurrentTaskId = -1;
		return BaseDialog::OnClose(nResult);
	}

	void SparrowDlg::GenerateOriginalArtSvg()
	{
		const auto& inputJson = mSparrowOperate->mInputJson;
		std::ostringstream svg;

		double stripHeight = 2000;// inputJson.value("strip_height", mConfig.stripHeight);
		double padding = 50.0;
		double currentX = padding;
		double maxY = 0;

		// 计算总宽度
		for (auto& item : inputJson["items"]) {
			auto& shape = item["shape"]["data"];
			double itemMaxX = 0, itemMaxY = 0;
			for (auto& pt : shape) {
				itemMaxX = std::max(itemMaxX, pt[0].get<double>());
				itemMaxY = std::max(itemMaxY, pt[1].get<double>());
			}
			maxY = std::max(maxY, itemMaxY);
		}

		double totalWidth = currentX;
		int colorIndex = 0;
		const char* colors[] = { "#7A7A7A", "#5B9BD5", "#ED7D31", "#A5A5A5", "#FFC000" };

		for (auto& item : inputJson["items"]) {
			auto& shape = item["shape"]["data"];
			if (shape.empty()) continue;

			// 计算物品尺寸
			double minX = 1e10, minY = 1e10, maxX = -1e10, maxY = -1e10;
			for (auto& pt : shape) {
				minX = std::min(minX, pt[0].get<double>());
				minY = std::min(minY, pt[1].get<double>());
				maxX = std::max(maxX, pt[0].get<double>());
				maxY = std::max(maxY, pt[1].get<double>());
			}

			double itemWidth = maxX - minX;
			double itemHeight = maxY - minY;

			// 简单水平排列，换行
			if (currentX + itemWidth + padding > 3000) {  // 超过 3000 换行
				currentX = padding;
				maxY += itemHeight + padding;
			}

			// 生成 path
			svg << "<g transform=\"translate(" << (currentX - minX) << "," << (padding - minY) << ")\">";
			svg << "<path d=\"M" << shape[0][0].get<double>() << "," << shape[0][1].get<double>();
			for (size_t i = 1; i < shape.size(); ++i) {
				svg << " L" << shape[i][0].get<double>() << "," << shape[i][1].get<double>();
			}
			if (shape.size() > 2) {
				svg << " Z";  // 闭合
			}
			svg << "\" fill=\"" << colors[colorIndex % 5] << "\" fill-opacity=\"0.5\" ";
			svg << "stroke=\"black\" stroke-width=\"2\"/>";
			svg << "</g>";

			currentX += itemWidth + padding;
			totalWidth = std::max(totalWidth, currentX);
			colorIndex++;
		}

		double viewHeight = std::max(stripHeight, maxY + padding * 2);

		// 包装完整 SVG
		std::ostringstream fullSvg;
		fullSvg << "<?xml version=\"1.0\"?>";
		fullSvg << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
		fullSvg << "viewBox=\"0 0 " << totalWidth << " " << viewHeight << "\">";
		fullSvg << "<rect x=\"0\" y=\"0\" width=\"" << totalWidth << "\" height=\"" << viewHeight << "\" ";
		fullSvg << "fill=\"#f5f5f5\" stroke=\"none\"/>";
		fullSvg << svg.str();
		fullSvg << "<text x=\"10\" y=\"30\" font-size=\"24\" fill=\"black\">原始图形 (共"
			<< inputJson["items"].size() << "个)</text>";
		fullSvg << "</svg>";

		mOriginalArtSvg = fullSvg.str();
	}

	void SparrowDlg::ShowOriginalArt()
	{
		if (!mOriginalArtSvg.empty() && mUISvgPreview) {
			mUISvgPreview->SetSvg(mOriginalArtSvg);
		}
	}

}