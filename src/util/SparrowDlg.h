#pragma once

#include "BaseDialog.h"
#include "TaskManager.h"
#include "UISvgPreview.h"
#include "SparrowOperate.h"
#include "UIConfigManager.h"

namespace ap {
    // 历史结果项
    struct NestingResult {
        int id = 0;
        std::string timestamp;
        std::string svgContent;           // 排料 SVG
        std::string finalResultJson;      // FinalResult JSON
        double stripWidth = 0;            // 板材宽度
        double utilization = 0;           // 利用率
        int placedCount = 0;              // 成功放置数量
        int totalCount = 0;               // 总数量
        bool isFinal = false;             // 是否最终结果
    };
    class SparrowDlg : public BaseDialog
    {
        //排料
        AINotifierHandle fStartSparrowNotifier = nullptr;
        AINotifierHandle fDisposalImportNotifier = nullptr;
        TaskManager mTaskManager;
        std::unique_ptr<UISvgPreview> mUISvgPreview;
        std::shared_ptr<SparrowOperate> mSparrowOperate;
        // 配置
        SparrowConfig mConfig;

        // 历史结果
        std::vector<NestingResult> mHistory;
        int mCurrentHistoryIndex = -1;
        int mNextResultId = 1;

        // 当前状态
        bool mIsRunning = false;
        int mCurrentTaskId = -1;
        std::string mOriginalArtSvg;      // 原始 art SVG
    public:
        using BaseDialog::BaseDialog;
        SparrowDlg();
        void StartSvgPreview();
        void StartSparrow();
        void ResetSparrowOperate(std::shared_ptr<SparrowOperate> operate=nullptr);
    private:
        void OnStart();
        void OnPause(int taskId);
        void OnResume(int taskId);
        void OnCancel(int taskId);
        
        // 配置
        void InitConfig();
        void OnOk();                    
        // 历史浏览
        void OnPreviousResult();
        void OnNextResult();
        void OnSaveHistory();
        void OnLoadHistory();
        void ShowResult(int index);
        void AddToHistory(const NestingResult& result);

        // 预览
        void ShowOriginalArt();             // 显示原始 art
        void UpdateTaskProgress(int step, double width, const std::string& kind);
        void GenerateOriginalArtSvg();      // 从 mInputJson 生成
        std::string GetCurrentTimestamp();
    protected:
        virtual BOOL OnInitDialog() override;
        virtual void OnTimer(UINT msg, WPARAM wParam, LPARAM lParam) override;
        // 消息分发
        virtual void OnCommand(WORD id, WORD code, HWND hCtrl) override;
        virtual LRESULT OnNotify(LPARAM lParam) override;
        virtual void OnSize(UINT state, int cx, int cy) override;
        virtual LRESULT  OnDrawItem(LPDRAWITEMSTRUCT dis) override;
        virtual BOOL OnClose(int nResult = IDOK) override;
   
    };
}