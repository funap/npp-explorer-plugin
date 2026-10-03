#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>

class InPlaceLabelTip {
public:
    explicit InPlaceLabelTip(HWND hTargetWnd);
    virtual ~InPlaceLabelTip();

    InPlaceLabelTip(const InPlaceLabelTip&) = delete;
    InPlaceLabelTip& operator=(const InPlaceLabelTip&) = delete;

    HWND GetTargetWnd() const { return _hTargetWnd; }

    virtual void OnMouseMove(WPARAM wParam, LPARAM lParam) = 0;
    virtual void OnMouseHover();
    virtual void OnMouseLeave();
    virtual void OnScroll();
    virtual void Hide();
    virtual void Reset();

    bool IsTipVisible() const { return _isTipVisible; }

protected:
    void EnsureWindowCreated();
    void DestroyTipWindow();
    void ShowTipWindow(const std::wstring& text, const RECT& textRect, HFONT hFont);
    void EnsureMouseLeaveTracking();

    virtual void OnPaint(HDC hdc);

    HWND _hTargetWnd = nullptr;
    HWND _hWnd = nullptr;

    bool _isTipVisible = false;
    bool _trackingLeave = false;

    std::wstring _currentText;
    HFONT _currentFont = nullptr;

private:
    static LRESULT CALLBACK LabelTipWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
