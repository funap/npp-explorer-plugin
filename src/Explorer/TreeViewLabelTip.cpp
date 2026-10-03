#include "pch.h"
#include "TreeViewLabelTip.h"
#include <algorithm>

TreeViewLabelTip::TreeViewLabelTip(HWND hTreeView)
    : InPlaceLabelTip(hTreeView)
{
}

void TreeViewLabelTip::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if (!_hTargetWnd) {
        return;
    }

    TVHITTESTINFO hitInfo = {};
    ::GetCursorPos(&hitInfo.pt);
    ::ScreenToClient(_hTargetWnd, &hitInfo.pt);
    HTREEITEM hitItem = TreeView_HitTest(_hTargetWnd, &hitInfo);

    if (!(hitInfo.flags & (TVHT_ONITEMLABEL | TVHT_ONITEM))) {
        hitItem = nullptr;
    }

    if (hitItem != _hoverItem) {
        _hoverItem = hitItem;
        if (_hoverItem) {
            ShowTip(_hoverItem);
        } else {
            Hide();
        }
    }

    EnsureMouseLeaveTracking();
}

void TreeViewLabelTip::OnMouseHover()
{
    if (_hoverItem && !_isTipVisible) {
        ShowTip(_hoverItem);
    }
}

void TreeViewLabelTip::OnMouseLeave()
{
    _hoverItem = nullptr;
    _displayedItem = nullptr;
    InPlaceLabelTip::OnMouseLeave();
}

void TreeViewLabelTip::OnScroll()
{
    _hoverItem = nullptr;
    InPlaceLabelTip::OnScroll();
}

void TreeViewLabelTip::Hide()
{
    _displayedItem = nullptr;
    InPlaceLabelTip::Hide();
}

void TreeViewLabelTip::Reset()
{
    _hoverItem = nullptr;
    _displayedItem = nullptr;
    InPlaceLabelTip::Reset();
}

void TreeViewLabelTip::ShowTip(HTREEITEM hItem)
{
    std::wstring text;
    RECT rcLabel = {};
    if (!IsTextTruncated(hItem, text, rcLabel)) {
        Hide();
        return;
    }

    _displayedItem = hItem;
    HFONT hFont = GetItemFont(hItem);
    ShowTipWindow(text, rcLabel, hFont);
}

bool TreeViewLabelTip::IsTextTruncated(HTREEITEM hItem, std::wstring& outText, RECT& outLabelRect) const
{
    if (!hItem || !_hTargetWnd) {
        return false;
    }

    // Retrieve item text
    WCHAR szBuffer[MAX_PATH] = {};
    TVITEM tvi = {};
    tvi.mask = TVIF_TEXT;
    tvi.hItem = hItem;
    tvi.pszText = szBuffer;
    tvi.cchTextMax = static_cast<int>(std::size(szBuffer));
    if (!TreeView_GetItem(_hTargetWnd, &tvi)) {
        return false;
    }
    outText = szBuffer;
    if (outText.empty()) {
        return false;
    }

    RECT rcClient = {};
    ::GetClientRect(_hTargetWnd, &rcClient);

    // Get item text rect (TRUE = label rect only)
    RECT textRect = {};
    if (!TreeView_GetItemRect(_hTargetWnd, hItem, &textRect, TRUE)) {
        return false;
    }

    // Check if vertically visible
    if (textRect.bottom <= rcClient.top || textRect.top >= rcClient.bottom) {
        return false;
    }

    int availableWidth = rcClient.right - textRect.left;
    if (availableWidth <= 0) {
        return true;
    }

    // Measure rendered text width using the item font
    HDC hdc = ::GetDC(_hTargetWnd);
    if (!hdc) {
        return false;
    }

    HFONT hFont = GetItemFont(hItem);
    HGDIOBJ hOldFont = nullptr;
    if (hFont) {
        hOldFont = ::SelectObject(hdc, hFont);
    }

    SIZE textSize = {};
    ::GetTextExtentPoint32W(hdc, outText.c_str(), static_cast<int>(outText.length()), &textSize);

    if (hOldFont) {
        ::SelectObject(hdc, hOldFont);
    }
    ::ReleaseDC(_hTargetWnd, hdc);

    outLabelRect = textRect;

    // Truncated if rendered text width exceeds available client width
    return textSize.cx > availableWidth;
}

HFONT TreeViewLabelTip::GetItemFont(HTREEITEM hItem) const
{
    if (_fontCallback) {
        HFONT hCustomFont = _fontCallback(hItem);
        if (hCustomFont) {
            return hCustomFont;
        }
    }
    return reinterpret_cast<HFONT>(::SendMessage(_hTargetWnd, WM_GETFONT, 0, 0));
}
