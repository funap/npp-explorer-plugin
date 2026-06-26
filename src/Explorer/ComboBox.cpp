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
#include "ThemeRenderer.h"

#include <algorithm>
#include <commctrl.h>
#include <utility>

ComboBox::ComboBox()
{
}

ComboBox::~ComboBox()
{
    if (_editWindow && ::IsWindow(_editWindow)) {
        ::RemoveWindowSubclass(_editWindow, EditSubclassProc, EDIT_SUBCLASS_ID);
    }
    if (_listWindow && ::IsWindow(_listWindow)) {
        ::RemoveWindowSubclass(_listWindow, ListSubclassProc, LIST_SUBCLASS_ID);
    }
}

void ComboBox::Init(HWND hCombo, HWND hParent)
{
    if (_editWindow && ::IsWindow(_editWindow)) {
        ::RemoveWindowSubclass(_editWindow, EditSubclassProc, EDIT_SUBCLASS_ID);
    }
    if (_listWindow && ::IsWindow(_listWindow)) {
        ::RemoveWindowSubclass(_listWindow, ListSubclassProc, LIST_SUBCLASS_ID);
    }

    _comboWindow = hCombo;
    _parentWindow = hParent;
    _editWindow = nullptr;
    _listWindow = nullptr;

    COMBOBOXINFO comboBoxInfo {
        .cbSize = sizeof(COMBOBOXINFO)
    };

    if (::SendMessage(_comboWindow, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(&comboBoxInfo))) {
        _editWindow = comboBoxInfo.hwndItem;
        if (_editWindow) {
            ::SetWindowSubclass(_editWindow, EditSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        }
        _listWindow = comboBoxInfo.hwndList;
        if (_listWindow) {
            ::SetWindowSubclass(_listWindow, ListSubclassProc, LIST_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
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
    LRESULT insertedIndex = ::SendMessage(_comboWindow, CB_INSERTSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    if (insertedIndex != CB_ERR && insertedIndex != CB_ERRSPACE) {
        ::SendMessage(_comboWindow, CB_SETCURSEL, insertedIndex, 0);
    } else {
        SelectComboText(text);
    }

    // Limit history items to MAX_HISTORY_ITEMS
    LRESULT count = ::SendMessage(_comboWindow, CB_GETCOUNT, 0, 0);
    while (count > static_cast<LRESULT>(MAX_HISTORY_ITEMS)) {
        ::SendMessage(_comboWindow, CB_DELETESTRING, count - 1, 0);
        count--;
    }
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
    return { };
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
    if (lResult != CB_ERR) {
        ::SendMessage(_comboWindow, CB_SETCURSEL, lResult, 0);
    }
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
        ::SendMessage(_comboWindow, CB_SETCURSEL, 0, 0);
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


LRESULT CALLBACK ComboBox::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* pThis = reinterpret_cast<ComboBox*>(dwRefData);
    if (pThis && uIdSubclass == EDIT_SUBCLASS_ID) {
        return pThis->RunEditProc(hwnd, uMsg, wParam, lParam);
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ComboBox::RunEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (_keyPreviewCallback && _keyPreviewCallback(hwnd, uMsg, wParam, lParam)) {
        return TRUE;
    }

    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam == VK_DELETE) {
            BOOL isDropDownOpen = (BOOL)::SendMessage(_comboWindow, CB_GETDROPPEDSTATE, 0, 0);
            if (isDropDownOpen) {
                INT curSel = (INT)::SendMessage(_comboWindow, CB_GETCURSEL, 0, 0);
                if (curSel != CB_ERR) {
                    ::SendMessage(_comboWindow, CB_DELETESTRING, curSel, 0);

                    INT count = (INT)::SendMessage(_comboWindow, CB_GETCOUNT, 0, 0);
                    if (count > 0) {
                        if (curSel >= count) curSel = count - 1;
                        ::SendMessage(_comboWindow, CB_SETCURSEL, curSel, 0);
                    }
                }
                return TRUE;
            }
        }
        break;
    case WM_CHAR:
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
        ::RemoveWindowSubclass(hwnd, EditSubclassProc, EDIT_SUBCLASS_ID);
        _editWindow = nullptr;
        break;
    default:
        break;
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void ComboBox::DrawItem(DRAWITEMSTRUCT* lpDrawItemStruct)
{
    if (!lpDrawItemStruct || lpDrawItemStruct->itemID == -1) {
        return;
    }

    HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(_comboWindow, WM_GETFONT, 0, 0));
    HFONT hOldFont = nullptr;
    if (hFont) {
        hOldFont = reinterpret_cast<HFONT>(::SelectObject(lpDrawItemStruct->hDC, hFont));
    }

    const ThemeColors& colors = ThemeRenderer::Instance().GetColors();
    COLORREF bgCol, fgCol;
    HBRUSH hBgBrush;

    if (lpDrawItemStruct->itemState & ODS_SELECTED) {
        bgCol = colors.primary_bg;
        fgCol = colors.primary;
        hBgBrush = ThemeRenderer::Instance().GetBrush(ThemeRenderer::BrushType::PrimaryBg);
    } else {
        bgCol = colors.body_bg;
        fgCol = colors.body;
        hBgBrush = ThemeRenderer::Instance().GetBrush(ThemeRenderer::BrushType::BodyBg);
    }

    ::FillRect(lpDrawItemStruct->hDC, &lpDrawItemStruct->rcItem, hBgBrush);

    LRESULT len = ::SendMessage(_comboWindow, CB_GETLBTEXTLEN, lpDrawItemStruct->itemID, 0);
    std::wstring text;
    if (len != CB_ERR && len > 0) {
        text.resize(len);
        ::SendMessage(_comboWindow, CB_GETLBTEXT, lpDrawItemStruct->itemID, reinterpret_cast<LPARAM>(text.data()));
    }

    RECT textRect = lpDrawItemStruct->rcItem;
    int height = textRect.bottom - textRect.top;
    textRect.right -= height;      // Dynamically allocate space for the delete button
    textRect.left += height / 5;   // Scale the left margin dynamically

    ::SetBkMode(lpDrawItemStruct->hDC, TRANSPARENT);
    ::SetTextColor(lpDrawItemStruct->hDC, fgCol);
    ::DrawText(lpDrawItemStruct->hDC, text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Draw the delete button
    RECT btnRect = GetDeleteButtonRect(lpDrawItemStruct->rcItem);
    BOOL isHovered = (_hotDeleteIndex == static_cast<int>(lpDrawItemStruct->itemID));

    if (isHovered) {
        HBRUSH hHoverBrush = ThemeRenderer::Instance().GetBrush(ThemeRenderer::BrushType::SecondaryBg);
        ::FillRect(lpDrawItemStruct->hDC, &btnRect, hHoverBrush);
    }

    COLORREF btnFgCol;
    if (lpDrawItemStruct->itemState & ODS_SELECTED) {
        btnFgCol = isHovered ? colors.primary : colors.secondary;
    } else {
        btnFgCol = isHovered ? colors.body : colors.secondary;
    }

    ::SetTextColor(lpDrawItemStruct->hDC, btnFgCol);
    ::DrawText(lpDrawItemStruct->hDC, L"\u00d7", -1, &btnRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    if (hOldFont) {
        ::SelectObject(lpDrawItemStruct->hDC, hOldFont);
    }
}

void ComboBox::MeasureItem(MEASUREITEMSTRUCT* lpMeasureItemStruct)
{
    if (!lpMeasureItemStruct) {
        return;
    }

    HDC hdc = ::GetDC(_comboWindow);
    HFONT hFont = reinterpret_cast<HFONT>(::SendMessage(_comboWindow, WM_GETFONT, 0, 0));
    HFONT hOldFont = nullptr;
    if (hFont) {
        hOldFont = reinterpret_cast<HFONT>(::SelectObject(hdc, hFont));
    }
    TEXTMETRIC tm;
    ::GetTextMetrics(hdc, &tm);
    if (hOldFont) {
        ::SelectObject(hdc, hOldFont);
    }
    ::ReleaseDC(_comboWindow, hdc);

    if (lpMeasureItemStruct->itemID == static_cast<UINT>(-1)) {
        // Edit control height. Minimize margin to fit within parent window resize limits.
        lpMeasureItemStruct->itemHeight = tm.tmHeight + 2;
    } else {
        // Dropdown list item height. Allocate slightly larger margin to display the delete button.
        lpMeasureItemStruct->itemHeight = tm.tmHeight + 6;
    }
}

RECT ComboBox::GetDeleteButtonRect(const RECT& itemRect) const
{
    RECT rc = itemRect;
    int height = rc.bottom - rc.top;

    // Scale button size and margins dynamically based on item height for HiDPI support.
    int margin = height / 8;
    if (margin < 2) {
        margin = 2;
    }
    int btnSize = height - (margin * 2);

    rc.right = itemRect.right - margin;
    rc.left = rc.right - btnSize;
    rc.top = itemRect.top + margin;
    rc.bottom = rc.top + btnSize;
    return rc;
}

BOOL ComboBox::IsPointInDeleteButton(const POINT& pt, const RECT& itemRect) const
{
    RECT btnRect = GetDeleteButtonRect(itemRect);
    return ::PtInRect(&btnRect, pt);
}

LRESULT CALLBACK ComboBox::ListSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* pThis = reinterpret_cast<ComboBox*>(dwRefData);
    if (pThis && uIdSubclass == LIST_SUBCLASS_ID) {
        return pThis->RunListProc(hwnd, uMsg, wParam, lParam);
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT ComboBox::RunListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        LRESULT res = ::SendMessage(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
        int index = LOWORD(res);
        BOOL outside = HIWORD(res);

        int newHotIndex = -1;
        if (!outside && index != -1) {
            RECT itemRect;
            if (::SendMessage(hwnd, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&itemRect)) != LB_ERR) {
                if (IsPointInDeleteButton(pt, itemRect)) {
                    newHotIndex = index;
                }
            }
        }

        if (newHotIndex != _hotDeleteIndex) {
            int oldHotIndex = _hotDeleteIndex;
            _hotDeleteIndex = newHotIndex;

            if (oldHotIndex != -1) {
                RECT rect;
                if (::SendMessage(hwnd, LB_GETITEMRECT, oldHotIndex, reinterpret_cast<LPARAM>(&rect)) != LB_ERR) {
                    ::InvalidateRect(hwnd, &rect, FALSE);
                }
            }
            if (_hotDeleteIndex != -1) {
                RECT rect;
                if (::SendMessage(hwnd, LB_GETITEMRECT, _hotDeleteIndex, reinterpret_cast<LPARAM>(&rect)) != LB_ERR) {
                    ::InvalidateRect(hwnd, &rect, FALSE);
                }
            }
        }

        TRACKMOUSEEVENT tme {
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_LEAVE,
            .hwndTrack = hwnd,
            .dwHoverTime = 0
        };
        ::TrackMouseEvent(&tme);
        break;
    }
    case WM_MOUSELEAVE: {
        if (_hotDeleteIndex != -1) {
            int oldHotIndex = _hotDeleteIndex;
            _hotDeleteIndex = -1;
            RECT rect;
            if (::SendMessage(hwnd, LB_GETITEMRECT, oldHotIndex, reinterpret_cast<LPARAM>(&rect)) != LB_ERR) {
                ::InvalidateRect(hwnd, &rect, FALSE);
            }
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        LRESULT res = ::SendMessage(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
        int index = LOWORD(res);
        BOOL outside = HIWORD(res);

        if (!outside && index != -1) {
            RECT itemRect;
            if (::SendMessage(hwnd, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&itemRect)) != LB_ERR) {
                if (IsPointInDeleteButton(pt, itemRect)) {
                    LRESULT len = ::SendMessage(_comboWindow, CB_GETLBTEXTLEN, index, 0);
                    std::wstring deletedText;
                    if (len != CB_ERR && len > 0) {
                        deletedText.resize(len);
                        ::SendMessage(_comboWindow, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(deletedText.data()));
                    }

                    ::SendMessage(_comboWindow, CB_DELETESTRING, index, 0);
                    _hotDeleteIndex = -1;

                    LRESULT curSel = ::SendMessage(_comboWindow, CB_GETCURSEL, 0, 0);
                    LRESULT count = ::SendMessage(_comboWindow, CB_GETCOUNT, 0, 0);

                    if (count == 0) {
                        ::SendMessage(_comboWindow, CB_SHOWDROPDOWN, FALSE, 0);
                        ::SendMessage(_comboWindow, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                        ::SendMessage(_comboWindow, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
                    } else {
                        std::wstring currentText = GetText();
                        if (currentText == deletedText) {
                            LRESULT newSel = index;
                            if (newSel >= count) {
                                newSel = count - 1;
                            }
                            ::SendMessage(_comboWindow, CB_SETCURSEL, newSel, 0);
                            
                            LRESULT newLen = ::SendMessage(_comboWindow, CB_GETLBTEXTLEN, newSel, 0);
                            if (newLen != CB_ERR && newLen > 0) {
                                std::wstring newText(newLen, L'\0');
                                ::SendMessage(_comboWindow, CB_GETLBTEXT, newSel, reinterpret_cast<LPARAM>(newText.data()));
                                SetText(newText);
                            } else {
                                SetText(L"");
                            }
                        } else {
                            if (curSel != CB_ERR && curSel > index) {
                                ::SendMessage(_comboWindow, CB_SETCURSEL, curSel - 1, 0);
                            }
                        }
                        ::InvalidateRect(hwnd, nullptr, TRUE);
                    }

                    ::PostMessage(_parentWindow, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(_comboWindow), CBN_SELCHANGE), reinterpret_cast<LPARAM>(_comboWindow));
                    return TRUE;
                }
            }
        }
        break;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hwnd, ListSubclassProc, LIST_SUBCLASS_ID);
        _listWindow = nullptr;
        break;
    }
    return ::DefSubclassProc(hwnd, uMsg, wParam, lParam);
}
