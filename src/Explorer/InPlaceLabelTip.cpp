#include "pch.h"
#include "InPlaceLabelTip.h"
#include "ThemeRenderer.h"
#include <algorithm>

InPlaceLabelTip::InPlaceLabelTip(HWND hTargetWnd)
    : _hTargetWnd(hTargetWnd)
{
}

InPlaceLabelTip::~InPlaceLabelTip()
{
    DestroyTipWindow();
}

void InPlaceLabelTip::EnsureWindowCreated()
{
    if (_hWnd != nullptr) {
        return;
    }

    static const WCHAR* const CLASS_NAME = L"NppExplorerInPlaceTip";
    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = LabelTipWndProc;
        wc.hInstance = ::GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
        if (::RegisterClassEx(&wc) || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
            s_registered = true;
        }
    }

    _hWnd = ::CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLASS_NAME,
        nullptr,
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        _hTargetWnd,
        nullptr,
        ::GetModuleHandle(nullptr),
        this
    );
}

void InPlaceLabelTip::DestroyTipWindow()
{
    if (_hWnd) {
        ::SetWindowLongPtr(_hWnd, GWLP_USERDATA, 0);
        ::DestroyWindow(_hWnd);
        _hWnd = nullptr;
    }
    _isTipVisible = false;
    _trackingLeave = false;
    _currentText.clear();
    _currentFont = nullptr;
}

LRESULT CALLBACK InPlaceLabelTip::LabelTipWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* pThis = reinterpret_cast<InPlaceLabelTip*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_CREATE: {
        auto* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
        ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pcs->lpCreateParams));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hWnd, &ps);
        if (pThis) {
            pThis->OnPaint(hdc);
        }
        ::EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_NCHITTEST:
        // Always transparent to mouse hit test: all clicks, double clicks,
        // right clicks, and mouse movements pass directly through to the target control
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        // Never activate the popup window
        return MA_NOACTIVATE;
    default:
        return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

void InPlaceLabelTip::OnPaint(HDC hdc)
{
    if (!_hWnd) {
        return;
    }

    RECT rcClient = {};
    ::GetClientRect(_hWnd, &rcClient);

    // Modern light tooltip colors matching standard Windows tooltips (applied in both Light and Dark modes)
    COLORREF bg = RGB(249, 249, 249);
    COLORREF fg = RGB(0, 0, 0);
    COLORREF borderColor = RGB(204, 204, 204);

    // Double buffering using memory DC
    HDC memDC = ::CreateCompatibleDC(hdc);
    HBITMAP memBitmap = ::CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
    HGDIOBJ oldBitmap = ::SelectObject(memDC, memBitmap);

    // Fill background
    HBRUSH bgBrush = ::CreateSolidBrush(bg);
    ::FillRect(memDC, &rcClient, bgBrush);
    ::DeleteObject(bgBrush);

    // Draw 1px border
    HBRUSH borderBrush = ::CreateSolidBrush(borderColor);
    ::FrameRect(memDC, &rcClient, borderBrush);
    ::DeleteObject(borderBrush);

    // Set font to match specified font or target control font
    HFONT hFont = _currentFont;
    if (!hFont) {
        hFont = reinterpret_cast<HFONT>(::SendMessage(_hTargetWnd, WM_GETFONT, 0, 0));
    }
    HGDIOBJ oldFont = nullptr;
    if (hFont) {
        oldFont = ::SelectObject(memDC, hFont);
    }

    // Draw text with 3px horizontal padding
    ::SetBkMode(memDC, TRANSPARENT);
    ::SetTextColor(memDC, fg);

    RECT rcText = rcClient;
    rcText.left += 3;
    rcText.right -= 3;
    ::DrawTextW(memDC, _currentText.c_str(), static_cast<int>(_currentText.length()), &rcText,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (oldFont) {
        ::SelectObject(memDC, oldFont);
    }

    // Blit to target DC
    ::BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);

    ::SelectObject(memDC, oldBitmap);
    ::DeleteObject(memBitmap);
    ::DeleteDC(memDC);
}

void InPlaceLabelTip::ShowTipWindow(const std::wstring& text, const RECT& textRect, HFONT hFont)
{
    EnsureWindowCreated();
    if (!_hWnd) {
        return;
    }

    _currentText = text;
    _currentFont = hFont;

    // Measure rendered text width
    HDC hdc = ::GetDC(_hTargetWnd);
    if (!hdc) {
        return;
    }

    HFONT fontToUse = _currentFont;
    if (!fontToUse) {
        fontToUse = reinterpret_cast<HFONT>(::SendMessage(_hTargetWnd, WM_GETFONT, 0, 0));
    }
    HGDIOBJ hOldFont = nullptr;
    if (fontToUse) {
        hOldFont = ::SelectObject(hdc, fontToUse);
    }

    SIZE textSize = {};
    ::GetTextExtentPoint32W(hdc, _currentText.c_str(), static_cast<int>(_currentText.length()), &textSize);

    if (hOldFont) {
        ::SelectObject(hdc, hOldFont);
    }
    ::ReleaseDC(_hTargetWnd, hdc);

    // Convert client coordinates to screen coordinates
    POINT pt = { textRect.left, textRect.top };
    ::ClientToScreen(_hTargetWnd, &pt);

    // Tip dimensions: 3px padding on left and right + 1px border on each side = +8px
    int tipWidth = textSize.cx + 8;
    int tipHeight = textRect.bottom - textRect.top;
    if (tipHeight <= 0) {
        tipHeight = textSize.cy + 4;
    }

    // Keep tip within monitor work area
    HMONITOR hMon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (::GetMonitorInfo(hMon, &mi)) {
        if (pt.x + tipWidth > mi.rcWork.right) {
            pt.x = mi.rcWork.right - tipWidth;
        }
        if (pt.x < mi.rcWork.left) {
            pt.x = mi.rcWork.left;
        }
    }

    // Position tip window slightly offset by -3px so text perfectly aligns over the item
    ::SetWindowPos(_hWnd, HWND_TOPMOST,
        pt.x - 3, pt.y,
        tipWidth, tipHeight,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    ::InvalidateRect(_hWnd, nullptr, TRUE);
    ::UpdateWindow(_hWnd);

    _isTipVisible = true;
}

void InPlaceLabelTip::EnsureMouseLeaveTracking()
{
    if (!_trackingLeave && _hTargetWnd) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = _hTargetWnd;
        if (::TrackMouseEvent(&tme)) {
            _trackingLeave = true;
        }
    }
}

void InPlaceLabelTip::OnMouseHover()
{
}

void InPlaceLabelTip::OnMouseLeave()
{
    _trackingLeave = false;
    Hide();
}

void InPlaceLabelTip::OnScroll()
{
    Hide();
}

void InPlaceLabelTip::Hide()
{
    if (_hWnd && _isTipVisible) {
        ::ShowWindow(_hWnd, SW_HIDE);
    }
    _isTipVisible = false;
}

void InPlaceLabelTip::Reset()
{
    Hide();
    _trackingLeave = false;
}
