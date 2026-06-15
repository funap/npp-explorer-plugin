// The MIT License (MIT)
//
// Copyright (c) 2026 funap
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "AddressBar.h"

#include "ExplorerDialog.h"
#include "ThemeRenderer.h"
#include "Explorer.h"
#include <shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

AddressBar::~AddressBar()
{
    if (_hEdit) {
        ::RemoveWindowSubclass(_hEdit, EditSubclassProc, 101);
        ::DestroyWindow(_hEdit);
    }
    if (_hBreadcrumbs) {
        ::DestroyWindow(_hBreadcrumbs);
    }
}

void AddressBar::Init(HINSTANCE hInst, HWND parent, ExplorerDialog* dialog)
{
    _hParent = parent;
    _dialog = dialog;

    // Register Breadcrumbs class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc   = BreadcrumbsWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = ::LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NppExplorerBreadcrumbs";
    wc.style         = CS_DBLCLKS; // Handle double-clicks
    ::RegisterClass(&wc);

    // Create Breadcrumbs control
    _hBreadcrumbs = ::CreateWindowEx(
        0, L"NppExplorerBreadcrumbs", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 0, 0, parent, nullptr, hInst, nullptr
    );
    ::SetWindowLongPtr(_hBreadcrumbs, GWLP_USERDATA, (LONG_PTR)this);

    // Create Edit control
    _hEdit = ::CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | ES_AUTOHSCROLL | ES_LEFT | WS_CLIPSIBLINGS,
        0, 0, 0, 0, parent, nullptr, hInst, nullptr
    );
    ::SetWindowSubclass(_hEdit, EditSubclassProc, 101, reinterpret_cast<DWORD_PTR>(this));

    // Register both controls with ThemeRenderer
    ThemeRenderer::Instance().Register(_hEdit);
}

void AddressBar::SetPath(const std::wstring& path)
{
    _currentPath = path;
    if (_hEdit && !::IsWindowVisible(_hEdit)) {
        ::SetWindowText(_hEdit, _currentPath.c_str());
    }
    if (_hBreadcrumbs) {
        ::InvalidateRect(_hBreadcrumbs, nullptr, TRUE);
    }
}

void AddressBar::UpdateTheme(bool isDarkMode)
{
    _isDarkMode = isDarkMode;
    if (_hBreadcrumbs) {
        ::InvalidateRect(_hBreadcrumbs, nullptr, TRUE);
    }
}

void AddressBar::SetFont(HFONT font)
{
    _hFont = font;
    if (_hEdit) {
        ::SendMessage(_hEdit, WM_SETFONT, (WPARAM)font, TRUE);
    }
    if (_hBreadcrumbs) {
        ::SendMessage(_hBreadcrumbs, WM_SETFONT, (WPARAM)font, TRUE);
    }
}

void AddressBar::ShowEdit(bool show)
{
    if (show) {
        ::ShowWindow(_hBreadcrumbs, SW_HIDE);
        ::SetWindowText(_hEdit, _currentPath.c_str());
        ::ShowWindow(_hEdit, SW_SHOW);
        ::SetFocus(_hEdit);
        ::SendMessage(_hEdit, EM_SETSEL, 0, -1);
    }
    else {
        ::ShowWindow(_hEdit, SW_HIDE);
        ::ShowWindow(_hBreadcrumbs, SW_SHOW);
        ::InvalidateRect(_hBreadcrumbs, nullptr, TRUE);
    }
}

void AddressBar::Resize(int x, int y, int width, int height)
{
    if (_hEdit) {
        ::MoveWindow(_hEdit, x, y, width, height, TRUE);
    }
    if (_hBreadcrumbs) {
        ::MoveWindow(_hBreadcrumbs, x, y, width, height, TRUE);
    }
}

int AddressBar::GetHeight() const
{
    if (_hFont) {
        HDC hdc = ::GetDC(_hParent);
        HGDIOBJ old = ::SelectObject(hdc, _hFont);
        TEXTMETRIC tm;
        ::GetTextMetrics(hdc, &tm);
        ::SelectObject(hdc, old);
        ::ReleaseDC(_hParent, hdc);
        return tm.tmHeight + 10; // Font height + padding
    }
    return 24;
}

void AddressBar::HandleNavigateOrExecute(const std::wstring& input)
{
    std::wstring trimmed = input;
    size_t first = trimmed.find_first_not_of(L" \t\r\n\"'");
    if (first != std::wstring::npos) {
        size_t last = trimmed.find_last_not_of(L" \t\r\n\"'");
        trimmed = trimmed.substr(first, (last - first + 1));
    }
    else {
        trimmed.clear();
    }

    if (trimmed.empty()) {
        return;
    }

    bool isPathValid = false;
    try {
        std::filesystem::path inputPath(trimmed);
        std::filesystem::path resolvedPath = inputPath;
        if (!inputPath.is_absolute()) {
            resolvedPath = std::filesystem::path(_currentPath) / inputPath;
        }

        std::error_code ec;
        std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(resolvedPath, ec);
        if (!ec) {
            resolvedPath = canonicalPath;
        }

        if (std::filesystem::exists(resolvedPath)) {
            isPathValid = true;
            std::wstring checkPath = resolvedPath.wstring();
            if (!std::filesystem::is_directory(resolvedPath)) {
                checkPath = resolvedPath.parent_path().wstring();
            }

            if (_dialog->GetSettings()->IsShowWorkspaceMode() && !_dialog->GetSettings()->IsPathInWorkspace(checkPath)) {
                ToggleWorkspaceMode();
            }

            if (std::filesystem::is_directory(resolvedPath)) {
                _dialog->NavigateTo(resolvedPath.wstring());
            }
            else {
                ::SendMessage(_dialog->getHParent(), NPPM_DOOPEN, 0, (LPARAM)resolvedPath.c_str());
                _dialog->GotoFileLocation(resolvedPath.wstring());
            }
        }
    }
    catch (const std::exception&) {
        isPathValid = false;
    }

    if (!isPathValid) {
        std::wstring exe;
        std::wstring args;
        size_t space = trimmed.find(L' ');
        if (space != std::wstring::npos) {
            exe = trimmed.substr(0, space);
            args = trimmed.substr(space + 1);
        }
        else {
            exe = trimmed;
        }

        HINSTANCE hInst = ::ShellExecute(nullptr, L"open", exe.c_str(), args.empty() ? nullptr : args.c_str(), _currentPath.c_str(), SW_SHOWNORMAL);
        if ((INT_PTR)hInst <= 32) {
            ::MessageBox(_hParent, (L"The path or command '" + trimmed + L"' is not valid.").c_str(), L"Explorer", MB_OK | MB_ICONWARNING);
        }
    }
}

void AddressBar::TrackMouse(HWND hwnd)
{
    TRACKMOUSEEVENT tme = { 0 };
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    ::TrackMouseEvent(&tme);
}

void AddressBar::RenderBreadcrumbs(HDC hdc, const RECT& clientRect)
{
    _renderedSlices.clear();

    auto& theme = ThemeRenderer::Instance();
    ThemeColors colors = theme.GetColors();

    COLORREF bgColor = colors.secondary_bg; 
    COLORREF textColor = colors.secondary;
    COLORREF hoverBgColor = colors.primary_bg;
    COLORREF hoverTextColor = colors.primary;
    COLORREF borderColor = _isDarkMode ? colors.border : ::GetSysColor(COLOR_3DSHADOW);

    bool isWorkspaceMode = _dialog->GetSettings()->IsShowWorkspaceMode();
    COLORREF disabledTextColor = RGB((GetRValue(textColor) + GetRValue(bgColor)) / 2,
                                     (GetGValue(textColor) + GetGValue(bgColor)) / 2,
                                     (GetBValue(textColor) + GetBValue(bgColor)) / 2);

    // Background
    HBRUSH bgBrush = ::CreateSolidBrush(bgColor);
    ::FillRect(hdc, &clientRect, bgBrush);
    ::DeleteObject(bgBrush);

    // Border (1px)
    HPEN borderPen = ::CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldPen = ::SelectObject(hdc, borderPen);
    ::MoveToEx(hdc, clientRect.left, clientRect.top, nullptr);
    ::LineTo(hdc, clientRect.right - 1, clientRect.top);
    ::LineTo(hdc, clientRect.right - 1, clientRect.bottom - 1);
    ::LineTo(hdc, clientRect.left, clientRect.bottom - 1);
    ::LineTo(hdc, clientRect.left, clientRect.top);
    ::SelectObject(hdc, oldPen);
    ::DeleteObject(borderPen);

    if (_currentPath.empty()) {
        return;
    }

    // Parse path slices
    std::vector<Slice> slices = ParsePath(_currentPath);

    HGDIOBJ oldFont = nullptr;
    if (_hFont) {
        oldFont = ::SelectObject(hdc, _hFont);
    }

    ::SetBkMode(hdc, TRANSPARENT);

    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    int availableWidth = clientWidth - 14; 

    SIZE ellipsisSize = { 0 };
    ::GetTextExtentPoint32(hdc, L"... > ", 6, &ellipsisSize);

    int totalNeeded = 0;
    int fitCount = 0;
    std::vector<SIZE> sliceSizes(slices.size());
    std::vector<SIZE> sepSizes(slices.size());

    for (int i = (int)slices.size() - 1; i >= 0; --i) {
        ::GetTextExtentPoint32(hdc, slices[i].name.c_str(), (int)slices[i].name.size(), &sliceSizes[i]);
        ::GetTextExtentPoint32(hdc, L" > ", 3, &sepSizes[i]);

        int neededForThis = sliceSizes[i].cx;
        if (i < (int)slices.size() - 1) {
            neededForThis += sepSizes[i].cx;
        }

        if (totalNeeded + neededForThis <= availableWidth - (i > 0 ? ellipsisSize.cx : 0)) {
            totalNeeded += neededForThis;
            fitCount++;
        }
        else {
            break;
        }
    }

    int startIndex = (fitCount < (int)slices.size()) ? ((int)slices.size() - fitCount) : 0;
    int x = 7;

    // Ellipsis
    if (startIndex > 0) {
        std::wstring ellipsisText = L"...";
        SIZE sz;
        ::GetTextExtentPoint32(hdc, ellipsisText.c_str(), (int)ellipsisText.size(), &sz);

        RECT r = { x, 1, x + sz.cx, clientHeight - 1 };

        bool isEllipsisAllowed = !isWorkspaceMode || _dialog->GetSettings()->IsPathInWorkspace(slices[startIndex - 1].path);

        if (_hoverSliceIndex == 0 && _hoverIsEllipsis && isEllipsisAllowed) {
            HBRUSH hoverBrush = ::CreateSolidBrush(hoverBgColor);
            RECT hoverRect = { r.left - 2, 2, r.right + 2, clientHeight - 2 };
            ::FillRect(hdc, &hoverRect, hoverBrush);
            ::DeleteObject(hoverBrush);
            ::SetTextColor(hdc, hoverTextColor);
        }
        else {
            ::SetTextColor(hdc, isEllipsisAllowed ? textColor : disabledTextColor);
        }

        ::DrawText(hdc, ellipsisText.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER);

        Slice ellipsisSlice = { L"...", slices[startIndex - 1].path, r };
        _renderedSlices.push_back(ellipsisSlice);

        x += sz.cx;

        ::SetTextColor(hdc, textColor);
        std::wstring sep = L" > ";
        SIZE sepSz;
        ::GetTextExtentPoint32(hdc, sep.c_str(), (int)sep.size(), &sepSz);
        RECT rSep = { x, 1, x + sepSz.cx, clientHeight - 1 };
        ::DrawText(hdc, sep.c_str(), -1, &rSep, DT_SINGLELINE | DT_VCENTER);
        x += sepSz.cx;
    }

    for (int i = startIndex; i < (int)slices.size(); ++i) {
        RECT r = { x, 1, x + sliceSizes[i].cx, clientHeight - 1 };

        bool isAllowed = !isWorkspaceMode || _dialog->GetSettings()->IsPathInWorkspace(slices[i].path);
        bool isHovered = false;
        int sliceMapIndex = (int)_renderedSlices.size();
        if (_hoverSliceIndex == sliceMapIndex && !_hoverIsEllipsis && isAllowed) {
            isHovered = true;
        }

        if (isHovered) {
            HBRUSH hoverBrush = ::CreateSolidBrush(hoverBgColor);
            RECT hoverRect = { r.left - 2, 2, r.right + 2, clientHeight - 2 };
            ::FillRect(hdc, &hoverRect, hoverBrush);
            ::DeleteObject(hoverBrush);
            ::SetTextColor(hdc, hoverTextColor);
        }
        else {
            ::SetTextColor(hdc, isAllowed ? textColor : disabledTextColor);
        }

        ::DrawText(hdc, slices[i].name.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER);

        slices[i].rect = r;
        _renderedSlices.push_back(slices[i]);

        x += sliceSizes[i].cx;

        if (i < (int)slices.size() - 1) {
            ::SetTextColor(hdc, textColor);
            std::wstring sep = L" > ";
            RECT rSep = { x, 1, x + sepSizes[i].cx, clientHeight - 1 };
            ::DrawText(hdc, sep.c_str(), -1, &rSep, DT_SINGLELINE | DT_VCENTER);
            x += sepSizes[i].cx;
        }
    }

    if (oldFont) {
        ::SelectObject(hdc, oldFont);
    }
}

LRESULT CALLBACK AddressBar::BreadcrumbsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AddressBar* self = reinterpret_cast<AddressBar*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self) {
        return ::DefWindowProc(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hwnd, &ps);
        RECT rect;
        ::GetClientRect(hwnd, &rect);

        HDC memDC = ::CreateCompatibleDC(hdc);
        HBITMAP memBmp = ::CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
        HGDIOBJ oldBmp = ::SelectObject(memDC, memBmp);

        self->RenderBreadcrumbs(memDC, rect);

        ::BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);

        ::SelectObject(memDC, oldBmp);
        ::DeleteObject(memBmp);
        ::DeleteDC(memDC);

        ::EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int oldHover = self->_hoverSliceIndex;
        bool oldHoverIsEllipsis = self->_hoverIsEllipsis;

        self->_hoverSliceIndex = -1;
        self->_hoverIsEllipsis = false;

        bool isWorkspaceMode = self->_dialog->GetSettings()->IsShowWorkspaceMode();

        for (size_t i = 0; i < self->_renderedSlices.size(); ++i) {
            RECT r = self->_renderedSlices[i].rect;
            RECT padded = { r.left - 2, r.top, r.right + 2, r.bottom };
            POINT pt = { x, y };
            if (::PtInRect(&padded, pt)) {
                bool isAllowed = !isWorkspaceMode || self->_dialog->GetSettings()->IsPathInWorkspace(self->_renderedSlices[i].path);
                if (isAllowed) {
                    self->_hoverSliceIndex = (int)i;
                    if (i == 0 && self->_renderedSlices[i].name == L"...") {
                        self->_hoverIsEllipsis = true;
                    }
                }
                break;
            }
        }

        if (self->_hoverSliceIndex != oldHover || self->_hoverIsEllipsis != oldHoverIsEllipsis) {
            ::InvalidateRect(hwnd, nullptr, TRUE);
        }

        self->TrackMouse(hwnd);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (self->_hoverSliceIndex != -1) {
            self->_hoverSliceIndex = -1;
            self->_hoverIsEllipsis = false;
            ::InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        POINT pt = { x, y };

        bool sliceClicked = false;
        bool isWorkspaceMode = self->_dialog->GetSettings()->IsShowWorkspaceMode();

        for (size_t i = 0; i < self->_renderedSlices.size(); ++i) {
            RECT r = self->_renderedSlices[i].rect;
            RECT padded = { r.left - 2, r.top, r.right + 2, r.bottom };
            if (::PtInRect(&padded, pt)) {
                bool isAllowed = !isWorkspaceMode || self->_dialog->GetSettings()->IsPathInWorkspace(self->_renderedSlices[i].path);
                if (isAllowed) {
                    self->_dialog->NavigateTo(self->_renderedSlices[i].path);
                    sliceClicked = true;
                } else {
                    // Block navigation, but count as sliceClicked so we don't open the edit control
                    sliceClicked = true;
                }
                break;
            }
        }

        if (!sliceClicked) {
            self->ShowEdit(true);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        self->ShowEdit(true);
        return 0;
    }
    case WM_USER + 1: { // Asynchronously switch to breadcrumbs
        // Make sure focus hasn't returned to edit control
        if (::GetFocus() != self->_hEdit) {
            self->ShowEdit(false);
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK AddressBar::EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    AddressBar* self = reinterpret_cast<AddressBar*>(dwRefData);
    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            WCHAR path[MAX_PATH];
            ::GetWindowText(hWnd, path, MAX_PATH);
            self->HandleNavigateOrExecute(path);
            self->ShowEdit(false);
            return 0;
        }
        else if (wParam == VK_ESCAPE) {
            self->ShowEdit(false);
            return 0;
        }
        break;
    case WM_GETDLGCODE:
        if (lParam) {
            MSG* pMsg = reinterpret_cast<MSG*>(lParam);
            if (pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)) {
                return DLGC_WANTALLKEYS;
            }
        }
        break;
    case WM_KILLFOCUS:
        ::PostMessage(self->_hBreadcrumbs, WM_USER + 1, 0, 0);
        break;
    case WM_NCPAINT:
        if (!self->_isDarkMode) {
            HDC hdc = ::GetWindowDC(hWnd);
            RECT rect;
            ::GetWindowRect(hWnd, &rect);
            ::OffsetRect(&rect, -rect.left, -rect.top);

            HBRUSH borderBrush = ::CreateSolidBrush(::GetSysColor(COLOR_3DSHADOW));
            ::FrameRect(hdc, &rect, borderBrush);
            ::DeleteObject(borderBrush);

            ::ReleaseDC(hWnd, hdc);
            return 0;
        }
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

std::vector<AddressBar::Slice> AddressBar::ParsePath(const std::wstring& path) const
{
    std::vector<Slice> slices;
    if (path.empty()) {
        return slices;
    }

    std::filesystem::path p(path);
    std::filesystem::path current_accumulated;
    bool hasRootName = p.has_root_name();

    for (auto& part : p) {
        if (part.empty()) continue;
        std::wstring name = part.wstring();

        if (hasRootName && (name == L"\\" || name == L"/")) {
            if (!current_accumulated.empty() && 
                current_accumulated.wstring().back() != L'\\' && 
                current_accumulated.wstring().back() != L'/') {
                current_accumulated = current_accumulated.wstring() + L"\\";
            }
            continue;
        }

        if (current_accumulated.empty()) {
            current_accumulated = part;
            if (name.back() == L':') {
                current_accumulated = part / L"\\";
            }
        }
        else {
            current_accumulated /= part;
        }

        if (name.back() == L'\\' && name.size() > 1 && name[name.size() - 2] != L':') {
            name.pop_back();
        }
        if (name.back() == L'/') {
            name.pop_back();
        }

        slices.push_back({ name, current_accumulated.wstring(), {0} });
    }

    return slices;
}
