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
#pragma once

#include <windows.h>
#include <commctrl.h>

#include <functional>
#include <memory>
#include <set>
#include <stdexcept>

#include "Graphics.h"

struct ThemeColors
{
    COLORREF foreground             = ::GetSysColor(COLOR_WINDOWTEXT);
    COLORREF disabled_text          = ::GetSysColor(COLOR_GRAYTEXT);
    COLORREF content_background     = ::GetSysColor(COLOR_WINDOW);

    COLORREF control_foreground     = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    COLORREF control_background     = ::GetSysColor(COLOR_3DFACE);

    COLORREF border                 = ::GetSysColor(COLOR_3DSHADOW);
    COLORREF primary_border         = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    COLORREF secondary_border       = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    COLORREF disabled_border        = ::GetSysColor(COLOR_3DLIGHT);

    COLORREF hot_background         = ::GetSysColor(COLOR_HIGHLIGHT);
    COLORREF primary_background     = ::GetSysColor(COLOR_HIGHLIGHT);
    COLORREF secondary_background   = ::GetSysColor(COLOR_3DFACE);
};

struct Brushes
{
    Brush foreground;
    Brush control_foreground;
    Brush disabled_text;
    Brush content_background;
    Brush control_background;
    Brush border;
    Brush primary_border;
    Brush secondary_border;
    Brush disabled_border;
    Brush hot_background;
    Brush primary_background;
    Brush secondary_background;
};

class ThemeRenderer
{
private:
    ThemeRenderer();
    ~ThemeRenderer();
    static ThemeRenderer* s_instance;
public:
    ThemeRenderer(const ThemeRenderer&)             = delete;
    ThemeRenderer& operator=(const ThemeRenderer&)  = delete;
    ThemeRenderer(ThemeRenderer&&)                  = delete;
    ThemeRenderer& operator=(ThemeRenderer&&)       = delete;


    static void Create();
    static void Destroy();

    static ThemeRenderer& Instance();

    static bool IsDarkColor(COLORREF rgb)
    {
        uint8_t r = GetRValue(rgb);
        uint8_t g = GetGValue(rgb);
        uint8_t b = GetBValue(rgb);
        float brightness = (0.2126F * r + 0.7152F * g + 0.0722F * b) / 255.0F;
        return brightness < 0.5F;
    }

    static bool IsDarkControlBackground()
    {
        return IsDarkColor(Instance().GetColors().control_background);
    }

    void SetTheme(const ThemeColors& colors);

    void Register(HWND hwnd);
    void ApplyTheme(HWND hwnd);

    // ブラシ取得用関数
    enum class BrushType {
        Foreground,
        HighlightText,
        DisabledText,
        ContentBackground,
        ControlBackground,
        Border,
        PrimaryBorder,
        SecondaryBorder,
        DisabledBorder,
        HotBackground,
        PrimaryBackground,
        SecondaryBackground,
    };

    HBRUSH GetBrush(BrushType type) const;
    const ThemeColors& GetColors() const { return m_colors; }

private:
    static LRESULT CALLBACK DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT TreeViewCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HeaderCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HeaderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ListViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT RebarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ToolBarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ToolBarCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ComboBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PaintListView(HWND hWnd, HDC hdc);
    void DrawChevron(HDC hdc, const RECT& rect, bool isExpanded, COLORREF color);

    ThemeColors m_colors;
    Brushes m_brushes;
    std::set<HWND>  m_windows;
};

