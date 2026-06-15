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

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>
#include <commctrl.h>

class ExplorerDialog;

class AddressBar
{
public:
    struct Slice {
        std::wstring name;
        std::wstring path;
        RECT rect;
    };

    AddressBar() = default;
    ~AddressBar();

    void Init(HINSTANCE hInst, HWND parent, ExplorerDialog* dialog);
    void SetPath(const std::wstring& path);
    void UpdateTheme(bool isDarkMode);
    void SetFont(HFONT font);
    void ShowEdit(bool show);
    void Resize(int x, int y, int width, int height);
    int GetHeight() const;
    void HandleNavigateOrExecute(const std::wstring& input);

    HWND GetEditHandle() const { return _hEdit; }
    HWND GetBreadcrumbsHandle() const { return _hBreadcrumbs; }

private:
    HWND _hParent = nullptr;
    HWND _hEdit = nullptr;
    HWND _hBreadcrumbs = nullptr;
    ExplorerDialog* _dialog = nullptr;
    std::wstring _currentPath;
    HFONT _hFont = nullptr;
    bool _isDarkMode = false;

    // Hover tracking
    int _hoverSliceIndex = -1;
    bool _hoverIsEllipsis = false;
    std::vector<Slice> _renderedSlices;

    static LRESULT CALLBACK BreadcrumbsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void RenderBreadcrumbs(HDC hdc, const RECT& clientRect);
    void TrackMouse(HWND hwnd);
    std::vector<Slice> ParsePath(const std::wstring& path) const;
};
