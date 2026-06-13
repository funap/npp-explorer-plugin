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

#include "ComboBox.h"

#include <utility>
#include <algorithm>
#include <commctrl.h>

ComboBox::ComboBox()
{
}

ComboBox::~ComboBox()
{
    if (_editWindow && ::IsWindow(_editWindow)) {
        ::RemoveWindowSubclass(_editWindow, SubclassProc, SUBCLASS_ID);
    }
}

void ComboBox::Init(HWND hCombo, HWND hParent)
{
    if (_editWindow && ::IsWindow(_editWindow)) {
        ::RemoveWindowSubclass(_editWindow, SubclassProc, SUBCLASS_ID);
    }

    _comboWindow = hCombo;
    _parentWindow = hParent;
    _editWindow = nullptr;

    COMBOBOXINFO comboBoxInfo {
        .cbSize = sizeof(COMBOBOXINFO)
    };

    if (::SendMessage(_comboWindow, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&comboBoxInfo))) {
        _editWindow = comboBoxInfo.hwndItem;
        if (_editWindow) {
            ::SetWindowSubclass(_editWindow, SubclassProc, SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        }
    }
}

void ComboBox::AddText(const std::wstring& text)
{
    if (text.empty()) {
        return;
    }

    // Find if it already exists
    LRESULT index = ::SendMessage(_comboWindow, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str()));
    if (index != CB_ERR) {
        ::SendMessage(_comboWindow, CB_DELETESTRING, index, 0);
    }

    // Insert at the top
    ::SendMessage(_comboWindow, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));

    // Limit history items to MAX_HISTORY_ITEMS
    LRESULT count = ::SendMessage(_comboWindow, CB_GETCOUNT, 0, 0);
    while (count > static_cast<LRESULT>(MAX_HISTORY_ITEMS)) {
        ::SendMessage(_comboWindow, CB_DELETESTRING, count - 1, 0);
        count--;
    }

    SelectComboText(text);
}

void ComboBox::SetText(const std::wstring& text)
{
    ::SendMessage(_comboWindow, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

std::wstring ComboBox::GetText() const
{
    LRESULT len = ::SendMessage(_comboWindow, WM_GETTEXTLENGTH, 0, 0);
    if (len > 0) {
        std::wstring result(len, L'\0');
        ::SendMessage(_comboWindow, WM_GETTEXT, len + 1, reinterpret_cast<LPARAM>(result.data()));
        return result;
    }
    return {};
}

std::wstring ComboBox::GetSelectedText() const
{
    INT curSel = static_cast<INT>(::SendMessage(_comboWindow, CB_GETCURSEL, 0, 0));
    if (curSel != CB_ERR) {
        LRESULT len = ::SendMessage(_comboWindow, CB_GETLBTEXTLEN, curSel, 0);
        if (len != CB_ERR && len > 0) {
            std::wstring result(len, L'\0');
            ::SendMessage(_comboWindow, CB_GETLBTEXT, curSel, reinterpret_cast<LPARAM>(result.data()));
            return result;
        }
    }
    return {};
}

void ComboBox::SelectComboText(const std::wstring& text)
{
    LRESULT lResult = ::SendMessage(_comboWindow, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str()));
    ::SendMessage(_comboWindow, CB_SETCURSEL, lResult, 0);
}

void ComboBox::SetComboList(const std::vector<std::wstring>& items)
{
    ::SendMessage(_comboWindow, CB_RESETCONTENT, 0, 0);
    
    size_t count = 0;
    for (const auto& item : items) {
        if (count >= MAX_HISTORY_ITEMS) {
            break;
        }
        ::SendMessage(_comboWindow, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        count++;
    }

    if (!items.empty()) {
        SelectComboText(items.front());
    }
}

std::vector<std::wstring> ComboBox::GetComboList() const
{
    std::vector<std::wstring> result;
    LRESULT count = ::SendMessage(_comboWindow, CB_GETCOUNT, 0, 0);
    if (count != CB_ERR) {
        for (LRESULT i = 0; i < count; ++i) {
            LRESULT len = ::SendMessage(_comboWindow, CB_GETLBTEXTLEN, i, 0);
            if (len != CB_ERR && len > 0) {
                std::wstring text(len, L'\0');
                ::SendMessage(_comboWindow, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(text.data()));
                result.push_back(text);
            }
            else {
                result.push_back(L"");
            }
        }
    }
    return result;
}

void ComboBox::ClearComboList()
{
    ::SendMessage(_comboWindow, CB_RESETCONTENT, 0, 0);
}

void ComboBox::SetDefaultOnCharHandler(std::function<BOOL(UINT, UINT, UINT)> onCharHandler)
{
    _onCharHandler = std::move(onCharHandler);
}

LRESULT CALLBACK ComboBox::SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* pThis = reinterpret_cast<ComboBox*>(dwRefData);
    if (pThis && uIdSubclass == SUBCLASS_ID) {
        return pThis->RunProc(hwnd, uMsg, wParam, lParam);
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ComboBox::RunProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CHAR:
        if (_onCharHandler) {
            BOOL handled = _onCharHandler(static_cast<UINT>(wParam), LOWORD(lParam), HIWORD(lParam));
            if (handled) {
                return TRUE;
            }
        }
        break;
    case WM_KEYUP:
        switch (wParam) {
        case VK_RETURN: {
            std::wstring text = GetText();
            AddText(text);
            ::PostMessage(_parentWindow, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(_comboWindow), CBN_SELCHANGE), reinterpret_cast<LPARAM>(_comboWindow));
            return TRUE;
        }
        default:
            break;
        }
        break;
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hwnd, SubclassProc, SUBCLASS_ID);
        _editWindow = nullptr;
        break;
    default:
        break;
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
