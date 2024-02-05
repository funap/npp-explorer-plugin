/*
  The MIT License (MIT)

  Copyright (c) 2019 funap

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

#include <string>
#include <vector>

#include "../NppPlugin/PluginInterface.h"


struct NppColors
{
    COLORREF background         = 0;
    COLORREF softerBackground   = 0;
    COLORREF hotBackground      = 0;
    COLORREF pureBackground     = 0;
    COLORREF errorBackground    = 0;
    COLORREF text               = 0;
    COLORREF darkerText         = 0;
    COLORREF disabledText       = 0;
    COLORREF linkText           = 0;
    COLORREF edge               = 0;
    COLORREF hotEdge            = 0;
    COLORREF disabledEdge       = 0;
};


class NppInterface
{
private:
    NppInterface() = default;
    ~NppInterface() = default;
public:
    NppInterface(const NppInterface&)               = delete;
    NppInterface& operator=(const NppInterface&)    = delete;
    NppInterface(NppInterface&&)                    = delete;
    NppInterface& operator=(NppInterface&&)         = delete;

    static void         setNppData(NppData nppData);
    static HWND         getWindow();
    static BOOL         doOpen(std::wstring_view);
    static std::wstring getSelectedText();
    static COLORREF     getEditorDefaultForegroundColor();
    static COLORREF     getEditorDefaultBackgroundColor();
    static COLORREF     getEditorCurrentLineBackgroundColor();
    static BOOL         IsDarkMode();
    static NppColors    GetColors();
    static void         setFocusToCurrentEdit();
    static std::vector<std::wstring> getSessionFiles(const std::wstring& sessionFilePath);
    static std::wstring getCurrentDirectory();
    static INT      getNppVersion();
    static BOOL     isSupportFluentUI();
private:
    static NppData _nppData;
};
