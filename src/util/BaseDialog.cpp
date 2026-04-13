#include "BaseDialog.h"
#include "Suites.hpp"

namespace ap {

    INT_PTR CALLBACK BaseDialog::StaticDlgProc(
        HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        BaseDialog* self = nullptr;

        if (msg == WM_INITDIALOG)
        {
            self = reinterpret_cast<BaseDialog*>(lParam);
            SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)self);

            self->_hWnd = hwnd;

        }
        else
        {
            self = reinterpret_cast<BaseDialog*>(
                GetWindowLongPtrW(hwnd, DWLP_USER));
        }

        if (self)
        {
            INT_PTR ret = self->DlgProc(msg, wParam, lParam);
            return ret;
        }

        return FALSE;
    }

    BaseDialog::BaseDialog(const char* pluginName) :Wnd(_hWnd, _hParent, _hInstance), setup(pluginName)
    {

        if (!setup.pluginName.empty()) {
            sSPPlugins->GetNamedPlugin(setup.pluginName.c_str(), &_pluginRef);
        }
    }


    const SPPluginRef  BaseDialog::GetSPPluginRef() const
    {
        return _pluginRef;
    }

    INT_PTR BaseDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_INITDIALOG:
            hBrush = CreateSolidBrush(RGB(85, 85, 85));
            return OnInitDialog();
        case WM_CTLCOLORDLG:
            return (INT_PTR)hBrush;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetBkColor(hdc, bgColor);
            SetTextColor(hdc, RGB(255, 255, 255)); // 根据需要修改文字颜色
            return (INT_PTR)hBrush;
        }
        case WM_COMMAND:
            OnCommand(
                LOWORD(wParam),
                HIWORD(wParam),
                (HWND)lParam
            );
            return TRUE;
        case WM_NOTIFY:
            OnNotify(lParam);
            return TRUE;
        case WM_DRAWITEM:
            return OnDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
        case WM_SIZE:
            OnSize(
                (UINT)wParam,
                LOWORD(lParam),
                HIWORD(lParam)
            );
            return TRUE;
        case WM_TIMER:
            OnTimer(msg, wParam, lParam);
            break;
        case WM_CLOSE:
            if (hBrush)
            {
                DeleteObject(hBrush);
                hBrush = NULL;
            }
            return OnClose(LOWORD(wParam));
        case WM_SHOWWINDOW:
            OnShowWindow(wParam);
            break;
        case WM_DESTROY:
            OnDestroy();
            return TRUE;
        }

        return FALSE;
    }

    BaseDialog::~BaseDialog() {
        if (this->_hWnd) {
            SetWindowLongPtrW(this->_hWnd, GWLP_USERDATA, 0);
        }

    }

    void BaseDialog::Create(HWND hParent, HINSTANCE hInstance)
    {
        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icc);
        _hInstance = hInstance;
        _hParent = hParent;
        if (setup.bModal)
        {
            INT_PTR result = DialogBoxParam(
                hInstance,
                MAKEINTRESOURCE(setup.dialogId),
                hParent,
                BaseDialog::StaticDlgProc,
                reinterpret_cast<LPARAM>(this)
            );
        }
        else {
            _hWnd = CreateDialogParamW(
                hInstance,
                MAKEINTRESOURCEW(setup.dialogId),
                hParent,
                BaseDialog::StaticDlgProc,
                reinterpret_cast<LPARAM>(this)
            );
        }
    }

    AIErr BaseDialog::RegisterNotifier(const char* type,AINotifierHandle& notifier)
    {
        ASErr error = kNoErr;
        
        if(!notifier)
        {
            sprintf_s(notifierName, type, "Plugin");
            error = sAINotifier->AddNotifier(_pluginRef, notifierName, type, &notifier);
        }
        return error;
    }


    void BaseDialog::Toggle()
    {
        if (_hWnd)
        {
            BOOL shown = false;
            shown = IsWindowVisible(_hWnd);
            Show(!shown);
        }

    }

    void BaseDialog::Show(bool show)
    {
        if (_hWnd)
        {
            ShowWindow(_hWnd, show ? SW_SHOW : SW_HIDE);
        }
    }

    void BaseDialog::OnDestroy()
    {
        if (_hWnd)
        {
            _hWnd = nullptr;
        }
    }

    BOOL BaseDialog::OnClose(int nResult)
    {
        // 清理画刷
        if (hBrush)
        {
            DeleteObject(hBrush);
            hBrush = NULL;
        }

        // 根据模式选择关闭方式
        if (setup.bModal)
        {
            EndDialog(_hWnd, nResult);  // 模态对话框
        }
        else
        {
            DestroyWindow(_hWnd);       // 非模态对话框
        }

        // 重要：立即将句柄置空，防止后续访问
        _hWnd = nullptr;
        return TRUE;
    }

    LRESULT BaseDialog::OnDrawItem(LPDRAWITEMSTRUCT lpDIS)
    {
        if (!lpDIS) return FALSE;

        switch (lpDIS->CtlType)
        {
        case ODT_BUTTON:
            DrawOwnerButton(lpDIS);
            return TRUE;
        case ODT_STATIC:
        {
            LONG style = GetWindowLong(lpDIS->hwndItem, GWL_STYLE);
            if (style & BS_GROUPBOX)
                DrawOwnerGroupBox(lpDIS);
            else
                DrawOwnerStatic(lpDIS);
            return TRUE;
        }
        }
        return FALSE;
    }

    void BaseDialog::DrawOwnerButton(LPDRAWITEMSTRUCT dis)
    {
        HDC hdc = dis->hDC;
        HWND hwnd = dis->hwndItem;
        RECT rc = dis->rcItem;
        // 创建内存DC用于双缓冲
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        // 检查按钮状态
        bool isHot = (dis->itemState & ODS_HOTLIGHT) != 0;
        bool isPressed = (dis->itemState & ODS_SELECTED) != 0;
        bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
        // 根据按钮状态选择颜色
        COLORREF bgColor, borderColor, textColor;

        if (isDisabled) // 禁用状态
        {
            bgColor = RGB(120, 120, 120);    // 更浅的灰色
            borderColor = RGB(80, 80, 80);   // 深灰色边框
            textColor = RGB(200, 200, 200);  // 浅灰色文本
        }
        else if (isPressed) // 按下状态
        {
            bgColor = RGB(65, 65, 65);       // 更深的灰色
            borderColor = RGB(0, 120, 215);  // 蓝色边框
            textColor = RGB(255, 255, 255);  // 白色文本

            // 为按下状态添加偏移效果
            OffsetRect(&rc, 1, 1);
        }
        else if (isHot)
        {
            bgColor = RGB(100, 100, 100);
            borderColor = RGB(0, 120, 215);
            textColor = RGB(255, 255, 255);
        }
        else // 正常状态
        {
            bgColor = RGB(85, 85, 85);       // 灰色背景
            borderColor = RGB(0, 0, 0);      // 黑色边框
            textColor = RGB(255, 255, 255);  // 白色文本
        }
        // 1. 创建渐变背景
        TRIVERTEX vertex[2];
        GRADIENT_RECT gRect;

        vertex[0].x = rc.left;
        vertex[0].y = rc.top;
        vertex[0].Red = (COLOR16)(GetRValue(bgColor) * 256);
        vertex[0].Green = (COLOR16)(GetGValue(bgColor) * 256);
        vertex[0].Blue = (COLOR16)(GetBValue(bgColor) * 256);
        vertex[0].Alpha = 0x0000;

        // 创建轻微渐变
        COLORREF bgColor2 = isHot ? RGB(110, 110, 110) : RGB(75, 75, 75);

        vertex[1].x = rc.right;
        vertex[1].y = rc.bottom;
        vertex[1].Red = (COLOR16)(GetRValue(bgColor2) * 256);
        vertex[1].Green = (COLOR16)(GetGValue(bgColor2) * 256);
        vertex[1].Blue = (COLOR16)(GetBValue(bgColor2) * 256);
        vertex[1].Alpha = 0x0000;

        gRect.UpperLeft = 0;
        gRect.LowerRight = 1;

        GradientFill(hdcMem, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

        // 2. 绘制圆角边框
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, borderColor);
        HPEN hOldPen = (HPEN)SelectObject(hdcMem, hBorderPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcMem, GetStockObject(NULL_BRUSH));

        RECT drawRect = { 0, 0, rc.right - rc.left, rc.bottom - rc.top };
        RoundRect(hdcMem, drawRect.left, drawRect.top, drawRect.right, drawRect.bottom, 8, 8);

        // 3. 绘制内阴影（悬停效果）
        if (isHot && !isPressed)
        {
            HPEN hInnerPen = CreatePen(PS_SOLID, 1, RGB(0, 120, 215, 100));
            SelectObject(hdcMem, hInnerPen);
            SelectObject(hdcMem, GetStockObject(NULL_BRUSH));

            RECT innerRect = drawRect;
            InflateRect(&innerRect, -1, -1);
            RoundRect(hdcMem, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom, 7, 7);

            DeleteObject(hInnerPen);
        }

        // 4. 绘制文本
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, textColor);

        wchar_t szText[100];
        GetWindowTextW(hwnd, szText, sizeof(szText) / sizeof(wchar_t));

        HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont ? hFont : GetStockObject(DEFAULT_GUI_FONT));

        DrawTextW(hdcMem, szText, -1, &drawRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 5. 绘制按下效果
        if (isPressed)
        {
            // 添加内阴影
            HBRUSH hShadowBrush = CreateSolidBrush(RGB(0, 0, 0, 30));
            FrameRect(hdcMem, &drawRect, hShadowBrush);
            DeleteObject(hShadowBrush);
        }

        // 6. 将内存DC内容复制到屏幕DC
        BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            hdcMem, 0, 0, SRCCOPY);

        // 7. 清理资源
        SelectObject(hdcMem, hOldFont);
        SelectObject(hdcMem, hOldPen);
        SelectObject(hdcMem, hOldBrush);
        SelectObject(hdcMem, hOldBitmap);

        DeleteObject(hBorderPen);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
    }

#if 0
    void BaseDialog::DrawOwnerButton(LPDRAWITEMSTRUCT lpDIS)
    {
        HDC hdc = lpDIS->hDC;
        RECT rc = lpDIS->rcItem;
        bool pressed = lpDIS->itemState & ODS_SELECTED;

        HBRUSH hBrush = CreateSolidBrush(pressed ? RGB(100, 100, 100) : bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        DrawEdge(hdc, &rc, EDGE_RAISED, BF_RECT);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        WCHAR text[128] = { 0 };
        GetWindowTextW(lpDIS->hwndItem, text, _countof(text));
        DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
#endif
    void BaseDialog::DrawOwnerGroupBox(LPDRAWITEMSTRUCT lpDIS)
    {
        HDC hdc = lpDIS->hDC;
        RECT rc = lpDIS->rcItem;

        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        DrawEdge(hdc, &rc, EDGE_ETCHED, BF_RECT);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        WCHAR text[128] = { 0 };
        GetWindowTextW(lpDIS->hwndItem, text, _countof(text));
        RECT textRc = rc;
        textRc.bottom = textRc.top + 20; // 标题文字区域
        DrawTextW(hdc, text, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void BaseDialog::DrawOwnerStatic(LPDRAWITEMSTRUCT lpDIS)
    {
        HDC hdc = lpDIS->hDC;
        RECT rc = lpDIS->rcItem;

        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        WCHAR text[128] = { 0 };
        GetWindowTextW(lpDIS->hwndItem, text, _countof(text));
        DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    void BaseDialog::AppendTitle(HWND hDlg, const std::wstring& extra)
    {
        wchar_t buffer[256] = { 0 };

        // 获取当前标题
        GetWindowTextW(hDlg, buffer, 256);

        std::wstring newTitle = buffer;
        newTitle += extra;

        // 设置新标题
        SetWindowTextW(hDlg, newTitle.c_str());
    }
    void BaseDialog::AddComboBoxString(HWND hCombo, const std::wstring& text) {
        if (!hCombo)
            return;

        int index = ComboBox_FindStringExact(hCombo, -1, text.c_str());
        if (index == CB_ERR)   // 没找到
        {
            ComboBox_AddString(hCombo, text.c_str());
        }
    }
    void BaseDialog::SetComboBoxSelection(HWND hCombo, const std::wstring& text) {
        if (hCombo && !text.empty()) {
            // 尝试找到匹配项
            int count = ComboBox_GetCount(hCombo);
            for (int i = 0; i < count; i++) {
                wchar_t itemText[256];
                ComboBox_GetLBText(hCombo, i, itemText);
                if (text == itemText) {
                    ComboBox_SetCurSel(hCombo, i);
                    return;
                }
            }

            // 没找到，设置文本
            ComboBox_SetText(hCombo, text.c_str());
        }
    }
    std::wstring BaseDialog::GetComboBoxText(HWND hCombo) {
        if (!hCombo) return L"";

        wchar_t buffer[256];
        if (ComboBox_GetText(hCombo, buffer, 256) > 0) {
            return buffer;
        }
        return L"";
    }
}
