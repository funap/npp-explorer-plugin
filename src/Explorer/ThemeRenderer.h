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

struct Colors
{
    COLORREF face = ::GetSysColor(COLOR_3DFACE);
    COLORREF text = ::GetSysColor(COLOR_WINDOWTEXT);
    COLORREF bg = 0;
    COLORREF fg = 0;
    COLORREF selected_bg = 0;
    COLORREF selected_fg = 0;
};

struct Brushs
{
    Brush face;
    Brush text;
    Brush bg;
    Brush fg;
    Brush hot;
    Brush selected;
    Brush hotSelected;
    Brush selectedNotFocus;
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
    static void Destory();

    static ThemeRenderer& Instance();

    void SetTheme(BOOL isDarkMode, Colors colors);

    void Register(HWND hwnd);
    void ApplyTheme(HWND hwnd);

private:
    static LRESULT CALLBACK DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT RebarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ButtonProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT ListViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL    m_isDarkMode;
    Colors  m_colors;
    Brushs  m_brushs;
    std::set<HWND>  m_windows;
};

