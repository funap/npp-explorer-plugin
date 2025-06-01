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

#include <memory>
#include <set>
#include <stdexcept>

#include "Graphics.h"

struct ThemeColors
{
    COLORREF body           = ::GetSysColor(COLOR_WINDOWTEXT);
    COLORREF body_bg        = ::GetSysColor(COLOR_WINDOW);
    COLORREF secondary      = ::GetSysColor(COLOR_GRAYTEXT);
    COLORREF secondary_bg   = ::GetSysColor(COLOR_BTNFACE);
    COLORREF border         = ::GetSysColor(COLOR_3DSHADOW);
    COLORREF primary        = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    COLORREF primary_bg     = ::GetSysColor(COLOR_HIGHLIGHT);
    COLORREF primary_border = ::GetSysColor(COLOR_ACTIVEBORDER);
};


struct Brushes
{
    Brush body;           // for body text
    Brush body_bg;        // for main background
    Brush secondary;      // for secondary text
    Brush secondary_bg;   // for secondary background
    Brush border;         // for borders
    Brush primary;        // for primary/highlighted text
    Brush primary_bg;     // for primary/highlighted background
    Brush primary_border; // for primary element borders
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

    void SetTheme(BOOL isDarkMode, const ThemeColors& colors);

    void Register(HWND hwnd);
    void ApplyTheme(HWND hwnd);

    // ブラシ取得用関数
    enum class BrushType {
        Body,
        BodyBg,
        Secondary,
        SecondaryBg,
        Border,
        Primary,
        PrimaryBg,
        PrimaryBorder,
    };

    HBRUSH GetBrush(BrushType type) const;
    const ThemeColors& GetColors() const { return m_colors; }

private:
    static LRESULT CALLBACK DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT RebarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL    m_isDarkMode;
    ThemeColors  m_colors;
    Brushes m_brushes;
    std::set<HWND>  m_windows;
};

