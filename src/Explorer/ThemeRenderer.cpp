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

namespace {
constexpr UINT_PTR REBAR_SUBCLASS_ID = 1;
constexpr UINT_PTR BUTTON_SUBCLASS_ID = 2;

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
    , m_baseBrush()
    , m_windows()
{
}

ThemeRenderer::~ThemeRenderer()
{
}

void ThemeRenderer::Create(BOOL isDarkMode, Colors colors)
{
    if (!s_instance) {
        s_instance = new ThemeRenderer();
        s_instance->SetTheme(isDarkMode, colors);
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
    m_baseBrush.CreateSolidBrush(colors.base);

    for (auto hwnd : m_windows) {
        ApplyTheme(hwnd);
    }
}

void ThemeRenderer::Register(HWND hwnd)
{
    m_windows.insert(hwnd);

    EnumChildWindows(hwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
        ThemeRenderer* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(hwnd);
        if (className == REBARCLASSNAME) {
            ::SetWindowSubclass(hwnd, DefaultSubclassProc, REBAR_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_BUTTON) {
            ::SetWindowSubclass(hwnd, DefaultSubclassProc, BUTTON_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    ApplyTheme(hwnd);
}

void ThemeRenderer::ApplyTheme(HWND hwnd)
{
    EnumChildWindows(hwnd, [](HWND hwnd, LPARAM lParam) -> BOOL {
        ThemeRenderer* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(hwnd);
        if (className == TOOLBARCLASSNAME) {
            COLORSCHEME scheme{
                .dwSize          = sizeof(COLORSCHEME),
                .clrBtnHighlight = self->m_colors.selected_bg,
                .clrBtnShadow    = self->m_colors.base,
            };
            ::SendMessage(hwnd, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
        }
        else if (className == WC_TREEVIEW) {
            TreeView_SetBkColor(hwnd, self->m_colors.bg);
            TreeView_SetTextColor(hwnd, self->m_colors.text);
            ::SetWindowTheme(hwnd, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
        else if (className == WC_LISTVIEW) {
            ListView_SetBkColor(hwnd, self->m_colors.bg);
            ListView_SetTextColor(hwnd, self->m_colors.text);
            ListView_SetTextBkColor(hwnd, CLR_NONE);
            ::SetWindowTheme(hwnd, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

LRESULT CALLBACK ThemeRenderer::DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uIdSubclass) {
    case REBAR_SUBCLASS_ID:
        return reinterpret_cast<ThemeRenderer*>(dwRefData)->RebarProc(hWnd, uMsg, wParam, lParam);
    case BUTTON_SUBCLASS_ID:
        return reinterpret_cast<ThemeRenderer*>(dwRefData)->ButtonProc(hWnd, uMsg, wParam, lParam);
    default:
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
        ::FillRect((HDC)wParam, &rc, m_baseBrush);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
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
        ::FillRect((HDC)wParam, &rc, m_baseBrush);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
