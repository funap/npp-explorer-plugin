#include "pch.h"
#include "ListViewLabelTip.h"
#include <algorithm>

ListViewLabelTip::ListViewLabelTip(HWND hListView)
    : InPlaceLabelTip(hListView)
{
}

void ListViewLabelTip::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    if (!_hTargetWnd) {
        return;
    }

    // Determine current mouse position using physical cursor coordinates
    LVHITTESTINFO hitInfo = {};
    ::GetCursorPos(&hitInfo.pt);
    ::ScreenToClient(_hTargetWnd, &hitInfo.pt);
    ::SendMessage(_hTargetWnd, LVM_SUBITEMHITTEST, 0, reinterpret_cast<LPARAM>(&hitInfo));

    HWND hHeader = ListView_GetHeader(_hTargetWnd);
    int headerHeight = 0;
    if (hHeader) {
        RECT rcH = {};
        if (::GetWindowRect(hHeader, &rcH)) {
            headerHeight = rcH.bottom - rcH.top;
        }
    }

    int hitItem = -1;
    int hitSubItem = -1;
    if (hitInfo.pt.y >= headerHeight && hitInfo.iItem >= 0 && hitInfo.iItem < ListView_GetItemCount(_hTargetWnd)) {
        if (!(hitInfo.flags & (LVHT_NOWHERE | LVHT_ABOVE | LVHT_BELOW | LVHT_TOLEFT | LVHT_TORIGHT))) {
            hitItem = hitInfo.iItem;
            hitSubItem = (hitInfo.iSubItem >= 0) ? hitInfo.iSubItem : 0;
        }
    }

    if (hitItem != _hoverItem || hitSubItem != _hoverSubItem) {
        _hoverItem = hitItem;
        _hoverSubItem = hitSubItem;

        // Immediate display: show tip instantly when hovering over a truncated item
        if (_hoverItem >= 0) {
            ShowTip(_hoverItem, _hoverSubItem);
        } else {
            Hide();
        }
    }

    EnsureMouseLeaveTracking();
}

void ListViewLabelTip::OnMouseHover()
{
    if (_hoverItem >= 0 && !_isTipVisible) {
        ShowTip(_hoverItem, _hoverSubItem);
    }
}

void ListViewLabelTip::OnMouseLeave()
{
    _hoverItem = -1;
    _hoverSubItem = -1;
    _displayedItem = -1;
    _displayedSubItem = -1;
    InPlaceLabelTip::OnMouseLeave();
}

void ListViewLabelTip::OnScroll()
{
    _hoverItem = -1;
    _hoverSubItem = -1;
    InPlaceLabelTip::OnScroll();
}

void ListViewLabelTip::Hide()
{
    _displayedItem = -1;
    _displayedSubItem = -1;
    InPlaceLabelTip::Hide();
}

void ListViewLabelTip::Reset()
{
    _hoverItem = -1;
    _hoverSubItem = -1;
    _displayedItem = -1;
    _displayedSubItem = -1;
    InPlaceLabelTip::Reset();
}

void ListViewLabelTip::ShowTip(int itemIndex, int subItemIndex)
{
    std::wstring text;
    RECT rcLabel = {};
    if (!IsTextTruncated(itemIndex, subItemIndex, text, rcLabel)) {
        Hide();
        return;
    }

    _displayedItem = itemIndex;
    _displayedSubItem = subItemIndex;
    HFONT hFont = GetItemFont(itemIndex, subItemIndex);
    ShowTipWindow(text, rcLabel, hFont);
}

bool ListViewLabelTip::IsTextTruncated(int itemIndex, int subItemIndex, std::wstring& outText, RECT& outLabelRect) const
{
    if (itemIndex < 0 || itemIndex >= ListView_GetItemCount(_hTargetWnd)) {
        return false;
    }

    // Retrieve item text
    WCHAR szBuffer[1024] = {};
    LVITEM lvi = {};
    lvi.iSubItem = subItemIndex;
    lvi.cchTextMax = static_cast<int>(std::size(szBuffer));
    lvi.pszText = szBuffer;
    ::SendMessage(_hTargetWnd, LVM_GETITEMTEXT, itemIndex, reinterpret_cast<LPARAM>(&lvi));
    outText = szBuffer;
    if (outText.empty()) {
        return false;
    }

    RECT rcListClient = {};
    ::GetClientRect(_hTargetWnd, &rcListClient);

    int availableWidth = 0;

    if (subItemIndex == 0) {
        // Main item (Column 0):
        RECT col0Bounds = {};
        if (!ListView_GetSubItemRect(_hTargetWnd, itemIndex, 0, LVIR_BOUNDS, &col0Bounds)) {
            return false;
        }

        // Check if item is vertically within visible area
        if (col0Bounds.bottom <= rcListClient.top || col0Bounds.top >= rcListClient.bottom) {
            return false;
        }

        RECT labelRect = {};
        if (!ListView_GetSubItemRect(_hTargetWnd, itemIndex, 0, LVIR_LABEL, &labelRect) || labelRect.bottom <= labelRect.top) {
            labelRect = col0Bounds;
            RECT rcIcon = {};
            if (ListView_GetItemRect(_hTargetWnd, itemIndex, &rcIcon, LVIR_ICON)) {
                labelRect.left = rcIcon.right;
            }
        }
        // Match ThemeRenderer PaintListView: col0TextRect = labelRect; col0TextRect.left += 2;
        labelRect.left += 2;

        int col0Width = ListView_GetColumnWidth(_hTargetWnd, 0);
        int col0RightLimit = col0Bounds.left + col0Width - 4;

        LONG visibleLeft = (std::max)(labelRect.left, static_cast<LONG>(rcListClient.left));
        LONG visibleRight = (std::min)(static_cast<LONG>(col0RightLimit), static_cast<LONG>(rcListClient.right));
        availableWidth = static_cast<int>(visibleRight - visibleLeft);

        outLabelRect.left = labelRect.left;
        outLabelRect.top = labelRect.top;
        outLabelRect.right = col0RightLimit;
        outLabelRect.bottom = labelRect.bottom;
    } else {
        // Subitem (Columns > 0):
        RECT rcSub = {};
        rcSub.top = subItemIndex;
        rcSub.left = LVIR_BOUNDS;
        if (!::SendMessage(_hTargetWnd, LVM_GETSUBITEMRECT, static_cast<WPARAM>(itemIndex), reinterpret_cast<LPARAM>(&rcSub))) {
            return false;
        }

        // Check if item is vertically within visible area
        if (rcSub.bottom <= rcListClient.top || rcSub.top >= rcListClient.bottom) {
            return false;
        }

        LONG visibleLeft = (std::max)(rcSub.left, rcListClient.left);
        LONG visibleRight = (std::min)(rcSub.right, rcListClient.right);
        availableWidth = static_cast<int>((visibleRight - visibleLeft) - 12);

        outLabelRect.left = rcSub.left + 6;
        outLabelRect.top = rcSub.top;
        outLabelRect.right = rcSub.right;
        outLabelRect.bottom = rcSub.bottom;
    }

    if (availableWidth <= 0) {
        return true;
    }

    // Measure rendered text width using the item font
    HDC hdc = ::GetDC(_hTargetWnd);
    if (!hdc) {
        return false;
    }

    HFONT hFont = GetItemFont(itemIndex, subItemIndex);
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

    // Truncated if rendered text width exceeds available cell width
    return textSize.cx > availableWidth;
}

HFONT ListViewLabelTip::GetItemFont(int itemIndex, int subItemIndex) const
{
    if (_fontCallback) {
        HFONT hCustomFont = _fontCallback(itemIndex, subItemIndex);
        if (hCustomFont) {
            return hCustomFont;
        }
    }
    return reinterpret_cast<HFONT>(::SendMessage(_hTargetWnd, WM_GETFONT, 0, 0));
}
