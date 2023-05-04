/*
  The MIT License (MIT)

  Copyright (c) 2023 funap

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#include "ThemeRenderer.h"

#include <uxtheme.h>
#include <Vsstyle.h>
#include <Vssym32.h>

namespace {
constexpr UINT_PTR WINDOW_SUBCLASS_ID   = 0;
constexpr UINT_PTR REBAR_SUBCLASS_ID    = 1;
constexpr UINT_PTR BUTTON_SUBCLASS_ID   = 2;

auto GetClassName(HWND hwnd) -> std::wstring
{
    std::wstring className(MAX_PATH, L'\0');
    auto length = ::GetClassName(hwnd, className.data(), static_cast<INT>(className.size()));
    className.resize(length);
    return className;
}

} // namespace

ThemeRenderer* ThemeRenderer::s_instance = nullptr;

ThemeRenderer::ThemeRenderer()
    : m_isDarkMode(false)
    , m_colors()
    , m_brushs()
    , m_windows()
{
}

ThemeRenderer::~ThemeRenderer()
{
}

void ThemeRenderer::Create()
{
    if (!s_instance) {
        s_instance = new ThemeRenderer();
    }
}

void ThemeRenderer::Destory()
{
    delete s_instance;
    s_instance = nullptr;
}

ThemeRenderer& ThemeRenderer::Instance()
{
    return *s_instance;
}


void ThemeRenderer::SetTheme(BOOL isDarkMode, Colors colors)
{
    m_isDarkMode = isDarkMode;
    m_colors     = colors;
    m_brushs.face.CreateSolidBrush(colors.face);
    m_brushs.bg.CreateSolidBrush(colors.bg);

    if (isDarkMode) {
        m_brushs.hot.CreateSolidBrush(RGB(0x4d, 0x4d, 0x4d));
        m_brushs.hotSelected.CreateSolidBrush(RGB(0x77, 0x77, 0x77));
        m_brushs.selected.CreateSolidBrush(RGB(0x33, 0x33, 0x33));
        m_brushs.selectedNotFocus.CreateSolidBrush(RGB(0x33, 0x33, 0x33));
    }
    else {
        m_brushs.hot.CreateSolidBrush(RGB(0xd8, 0xe6, 0xf2));
        m_brushs.hotSelected.CreateSolidBrush(RGB(0xc0, 0xdc, 0xf3));
        m_brushs.selected.CreateSolidBrush(RGB(0xc0, 0xdc, 0xf3));
        m_brushs.selectedNotFocus.CreateSolidBrush(RGB(0xcc, 0xcc, 0xcc));
    }

    for (auto hwnd : m_windows) {
        ApplyTheme(hwnd);
    }
}

void ThemeRenderer::Register(HWND hwnd)
{
    m_windows.insert(hwnd);

    ::SetWindowSubclass(hwnd, DefaultSubclassProc, WINDOW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));

    EnumChildWindows(hwnd, [](HWND childWindow, LPARAM lParam) -> BOOL {
        ThemeRenderer* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(childWindow);
        if (className == REBARCLASSNAME) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, REBAR_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_BUTTON) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, BUTTON_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    ApplyTheme(hwnd);
}

void ThemeRenderer::ApplyTheme(HWND hwnd)
{
    EnumChildWindows(hwnd, [](HWND childWindow, LPARAM lParam) -> BOOL {
        ThemeRenderer* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(childWindow);
        if (className == TOOLBARCLASSNAME) {
            COLORSCHEME scheme{
                .dwSize          = sizeof(COLORSCHEME),
                .clrBtnHighlight = self->m_colors.selected_bg,
                .clrBtnShadow    = self->m_colors.face,
            };
            ::SendMessage(childWindow, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
        }
        else if (className == WC_TREEVIEW) {
            TreeView_SetBkColor(childWindow, self->m_colors.bg);
            TreeView_SetTextColor(childWindow, self->m_colors.fg);
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
        else if (className == WC_LISTVIEW) {
            ListView_SetBkColor(childWindow, self->m_colors.bg);
            ListView_SetTextColor(childWindow, self->m_colors.fg);
            ListView_SetTextBkColor(childWindow, CLR_NONE);
            ::InvalidateRect(childWindow, NULL, TRUE);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

LRESULT CALLBACK ThemeRenderer::DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uIdSubclass) {
    case WINDOW_SUBCLASS_ID:
        return reinterpret_cast<ThemeRenderer*>(dwRefData)->WindowProc(hWnd, uMsg, wParam, lParam);
    case REBAR_SUBCLASS_ID:
        return reinterpret_cast<ThemeRenderer*>(dwRefData)->RebarProc(hWnd, uMsg, wParam, lParam);
    case BUTTON_SUBCLASS_ID:
        return reinterpret_cast<ThemeRenderer*>(dwRefData)->ButtonProc(hWnd, uMsg, wParam, lParam);
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushs.face);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
        m_windows.erase(hWnd);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::RebarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushs.face);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
        m_windows.erase(hWnd);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushs.face);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, BUTTON_SUBCLASS_ID);
        m_windows.erase(hWnd);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
