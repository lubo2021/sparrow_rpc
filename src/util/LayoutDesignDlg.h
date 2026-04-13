#pragma once

#include "BaseDialog.h"
#include "PluginContext.h"
#include "SimpleListView.h"
#include "PieceListView.h"
#include "SimplePresenter.h"
#include "PiecePresenter.h"
#include "DialogParam.h"
#include "UIControlManager.h"
#include "UIUtils.h"
#include "UIGroupBox.h"
#include "UIButton.h"
#include "UIEdit.h"
#include "UIStatic.h"
#include "AIDxfRenderer.h"
#include "SettingsContext.h"

namespace ap {

    class LayoutDesignDlg : public BaseDialog
    {
        bool initialized = false;
        bool m_sortAsc = false;
        std::unique_ptr<SimpleListView<PatternPieceUnit>> simpleLV;
        std::unique_ptr<PieceListView<PatternPieceUnit>> pieceLV;
        std::unique_ptr<PiecePresenter> piecePresenter;
        std::unique_ptr<SimplePresenter<PatternPieceUnit>> simplePresenter;
        std::vector<ListViewColumn<PatternPieceUnit>> colums;
        //ui
        std::unique_ptr<UIGroupBox> m_gbShape;
        std::unique_ptr<UIGroupBox> m_gbPieces;
        std::unique_ptr<UIGroupBox> m_gbJiankou;
        std::unique_ptr<UIGroupBox> m_gbObject;
        std::unique_ptr<UIGroupBox> m_gbLayout;
        LayoutDesignDlgParam &simpleParam;
        LayoutDesignDlgParam &pieceParam;
        bool listViewColumnInitialized = false;
        bool pieceLvColumnInitialized = false;
        AINotifierHandle fFileSelectedNotifier = nullptr;
        AINotifierHandle fOpenFileSelectorNotifier = nullptr;
        AINotifierHandle fCreateDocmentNotifier = nullptr;
        AINotifierHandle fDocumentViewZoomNotifier = nullptr;
        AINotifierHandle fRenderSettingsNotifier = nullptr;
        AINotifierHandle fRenderToAIDocmentNotifier = nullptr;
        
        LayoutDesignSettings &m_settings;
        //HWND
        HWND comboxContent = nullptr;
        HWND comboxFont = nullptr;
        HWND comboxDistance = nullptr;
        HWND comboxReferenceSize = nullptr;

        UIControlManager m_ui;
    public:
        LayoutDesignDlg();
        std::vector<ListViewItem<PatternPieceUnit>> m_patternPieceUnits;

        bool GetCheckboxStateFromData(int row);
        // 辅助方法：设置数据中的复选框状态
        void SetCheckboxStateToData(int row, bool checked);
        void OnImportData();
    protected:
        virtual BOOL OnInitDialog() override;
        // 获取 / 更新 UI 数据
        void ReadSettingsFromUI();
        void ApplySettingsToUI();
        void SetControlColor(int id, AIColor& color);
        // 消息分发
        virtual void OnCommand(WORD id, WORD code, HWND hCtrl) override;
        virtual LRESULT OnNotify(LPARAM lParam) override;
        virtual void OnSize(UINT state, int cx, int cy) override;
        virtual LRESULT  OnDrawItem(LPDRAWITEMSTRUCT dis) override;
        virtual void OnShowWindow(WPARAM wParam) override;
        virtual BOOL OnClose(int nResult = IDOK) override;
    private:
        void InitControl();
        void InitListView();
        void InitListViewConfig(LayoutDesignDlgParam& param,const std::string& context, const std::string& sectionName);
        void InitSimpleColumn();
        //void InitPieceColumn();
        void UpdateComboBoxOptions();
        //void UpdateEditDlgComboBoxOptions();
        JKMode GetJiankouMode();
        //void InitListViewEvents();
        //
        const std::unordered_map<std::string, std::vector<PatternPiece>> LoadBlocks(const std::vector<DxfBlock>& blocks);

        const std::unordered_map<std::string, std::vector<PatternPiece>> LoadBlocks(const DxfDocumentContext& ctx);
        void AddSimpleItem(const std::unordered_map<std::string, std::vector<PatternPiece>>& shapeMap);
        void AddPieceItem();
        //编辑裁片对话框函数
        void EditPieceName(int row, int pieceIndex);
        void EditPieceMaterial(int row, int pieceIndex);
        void EditPieceCategory(int row, int pieceIndex);
#if 0
        //自绘
        LRESULT HandleHeaderCustomDraw(LPNMCUSTOMDRAW cd);
        LRESULT HandleListViewCustomDraw(LPNMLVCUSTOMDRAW cd);
#endif
        inline void ParseName(const std::string& s, std::string& category, std::string& size);
    };
}