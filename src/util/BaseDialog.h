#pragma once

#include "ControlBase.h"
#include "UIControlManager.h"
#include "CustomNotifier.h"
#include "Wnd.h"
#include "PanleId.h"
#include "ErrorCheck.h"
#include "Plugin.hpp"

namespace ap {

    enum class ProTagPage
    {
        Design,
        ReplaceObj,
        Upload,
        Disposal,

    };

    // Specific control type
    class BaseDialog:public Wnd
    {
    private:
        HWND _hWnd = nullptr;
        HWND _hParent = nullptr;
        HINSTANCE _hInstance = nullptr;
        SPPluginRef _pluginRef = nullptr;
        char notifierName[kMaxStringLength];
    public:
        struct SetupVars {
            int dialogId = 0;
            std::string pluginName="";
            bool bModal = false;  
            ProTagPage tagPage = ProTagPage::Design;
            SetupVars() = default;
            explicit SetupVars(const char* name = kPanelPluginName)
                : pluginName(name ? name : "") {
            }
        };
        SetupVars setup;
    public:
        void Show(bool show = true);
        virtual void Create(HWND hParent, HINSTANCE hInstance);
        virtual ~BaseDialog();
        void Toggle();
        AIErr RegisterNotifier(const char* type, AINotifierHandle& notifier);
        const SPPluginRef  GetSPPluginRef() const;
        BaseDialog(const char* pluginName = kPanelPluginName);
    protected:
        HBRUSH hBrush = nullptr;
        COLORREF bgColor = RGB(85, 85, 85);
        UIControlManager m_ui;
    protected:
        // 生命周期
        virtual void OnDestroy();
        virtual BOOL OnInitDialog() = 0;
        virtual void  OnTimer(UINT msg, WPARAM wParam, LPARAM lParam) {};

        // 消息分发
        virtual void OnCommand(WORD id, WORD code, HWND hCtrl) = 0;
        virtual LRESULT OnNotify(LPARAM lParam) = 0;
        virtual void OnSize(UINT state, int cx, int cy) = 0;
        virtual LRESULT  OnDrawItem(LPDRAWITEMSTRUCT dis);
        virtual BOOL OnClose(int nResult = IDOK) = 0;
        virtual void OnShowWindow(WPARAM wParam) {};
        // DialogProc
        static INT_PTR CALLBACK StaticDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
        //tools class 
        void DrawOwnerButton(LPDRAWITEMSTRUCT lpDIS);
        void DrawOwnerGroupBox(LPDRAWITEMSTRUCT lpDIS);
        void DrawOwnerStatic(LPDRAWITEMSTRUCT lpDIS);
        void AppendTitle(HWND hDlg, const std::wstring& extra);
        void CenterToAI(HWND hparent, HWND hwnd)
        {
            HWND hAI = GetAncestor(hparent, GA_ROOTOWNER);
            if (!hAI || !hwnd) return;

            RECT rcAI{}, rcDlg{};
            GetWindowRect(hAI, &rcAI);
            GetWindowRect(hwnd, &rcDlg);

            int dlgW = rcDlg.right - rcDlg.left;
            int dlgH = rcDlg.bottom - rcDlg.top;

            int x = rcAI.left + ((rcAI.right - rcAI.left) - dlgW) / 2;
            int y = rcAI.top + ((rcAI.bottom - rcAI.top) - dlgH) / 2;

            SetWindowPos(
                hwnd,
                HWND_TOP,
                x, y,
                0, 0,
                SWP_NOSIZE | SWP_SHOWWINDOW
            );
        }
        // 工具函数实现
        void AddComboBoxString(HWND hCombo, const std::wstring& text);
        void SetComboBoxSelection(HWND hCombo, const std::wstring& text);
        std::wstring GetComboBoxText(HWND hCombo);
    private:
        bool IsOwnerDrawButton(HWND hwnd)
        {
            if (!hwnd) return false;

            WCHAR cls[32]{};
            GetClassNameW(hwnd, cls, 32);
            if (wcscmp(cls, L"Button") != 0)
                return false;

            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            return (style & BS_OWNERDRAW) != 0;
        }

    };

}