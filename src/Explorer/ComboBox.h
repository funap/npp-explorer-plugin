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

#include "Explorer.h"

#include <string>
#include <vector>
#include <functional>

class ComboBox
{
public:
    ComboBox();
    ~ComboBox();

    // Disable copy/move to prevent double-subclass cleanup
    ComboBox(const ComboBox&)             = delete;
    ComboBox& operator=(const ComboBox&)  = delete;
    ComboBox(ComboBox&&)                  = delete;
    ComboBox& operator=(ComboBox&&)       = delete;

    void Init(HWND hCombo, HWND hParent);
    void DrawItem(DRAWITEMSTRUCT* lpDrawItemStruct);
    void MeasureItem(MEASUREITEMSTRUCT* lpMeasureItemStruct);

    void AddText(const std::wstring& text);
    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    std::wstring GetSelectedText() const;

    void SetComboList(const std::vector<std::wstring>& items);
    std::vector<std::wstring> GetComboList() const;

    void ClearComboList();

    using KeyPreviewCallback = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
    void SetKeyPreviewCallback(KeyPreviewCallback callback) { _keyPreviewCallback = callback; }

private:
    void SelectComboText(const std::wstring& text);
    LRESULT RunEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT RunListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    RECT GetDeleteButtonRect(const RECT& itemRect) const;
    BOOL IsPointInDeleteButton(const POINT& pt, const RECT& itemRect) const;

    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    static LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    HWND _comboWindow{nullptr};
    HWND _parentWindow{nullptr};
    HWND _editWindow{nullptr};
    HWND _listWindow{nullptr};
    KeyPreviewCallback              _keyPreviewCallback;
    int _hotDeleteIndex{-1};

    static constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;
    static constexpr UINT_PTR LIST_SUBCLASS_ID = 2;
    static constexpr size_t MAX_HISTORY_ITEMS = 20;
};
