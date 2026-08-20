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

#include <windowsx.h>
#include <uxtheme.h>
#include <Vsstyle.h>
#include <Vssym32.h>

namespace {
constexpr UINT_PTR WINDOW_SUBCLASS_ID   = 0;
constexpr UINT_PTR REBAR_SUBCLASS_ID    = 1;
constexpr UINT_PTR BUTTON_SUBCLASS_ID   = 2;
constexpr UINT_PTR EDIT_SUBCLASS_ID     = 3;
constexpr UINT_PTR COMBOBOX_SUBCLASS_ID = 4;
constexpr UINT_PTR LISTVIEW_SUBCLASS_ID = 5;

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
    , m_brushes()
{
}

ThemeRenderer::~ThemeRenderer()
{
}

void ThemeRenderer::Create()
{
    if (!s_instance) {
        s_instance = new ThemeRenderer();
    }
}

void ThemeRenderer::Destroy()
{
    delete s_instance;
    s_instance = nullptr;
}

ThemeRenderer& ThemeRenderer::Instance()
{
    return *s_instance;
}

HBRUSH ThemeRenderer::GetBrush(BrushType type) const
{
    switch (type) {
    case BrushType::Body:
        return m_brushes.body;
    case BrushType::BodyBg:
        return m_brushes.body_bg;
    case BrushType::Secondary:
        return m_brushes.secondary;
    case BrushType::SecondaryBg:
        return m_brushes.secondary_bg;
    case BrushType::Border:
        return m_brushes.border;
    case BrushType::Primary:
        return m_brushes.primary;
    case BrushType::PrimaryBg:
        return m_brushes.primary_bg;
    case BrushType::PrimaryBorder:
        return m_brushes.primary_border;
    default:
        return m_brushes.body_bg;
    }
}

void ThemeRenderer::SetTheme(BOOL isDarkMode, const ThemeColors& colors)
{
    m_isDarkMode = isDarkMode;
    m_colors     = colors;

    m_brushes.body.CreateSolidBrush(colors.body);
    m_brushes.body_bg.CreateSolidBrush(colors.body_bg);
    m_brushes.secondary.CreateSolidBrush(colors.secondary);
    m_brushes.secondary_bg.CreateSolidBrush(colors.secondary_bg);
    m_brushes.border.CreateSolidBrush(colors.border);
    m_brushes.primary.CreateSolidBrush(colors.primary);
    m_brushes.primary_bg.CreateSolidBrush(colors.primary_bg);
    m_brushes.primary_border.CreateSolidBrush(colors.primary_border);

    for (const auto& hwnd : m_windows) {
        ApplyTheme(hwnd);
    }
}

void ThemeRenderer::Register(HWND hwnd)
{
    m_windows.insert(hwnd);

    ::SetWindowSubclass(hwnd, DefaultSubclassProc, WINDOW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));

    EnumChildWindows(hwnd, [](HWND childWindow, LPARAM lParam) -> BOOL {
        ThemeRenderer* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(childWindow);
        if (className == REBARCLASSNAME) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, REBAR_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_BUTTON) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, BUTTON_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_EDIT) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_COMBOBOX) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, COMBOBOX_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        else if (className == WC_LISTVIEW) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));

    ApplyTheme(hwnd);
}

void ThemeRenderer::ApplyTheme(HWND hwnd)
{
    EnumChildWindows(hwnd, [](HWND childWindow, LPARAM lParam) -> BOOL {
        auto* self = reinterpret_cast<ThemeRenderer*>(lParam);
        std::wstring className = GetClassName(childWindow);
        if (className == TOOLBARCLASSNAME) {
            COLORSCHEME scheme{
                .dwSize          = sizeof(COLORSCHEME),
                .clrBtnHighlight = self->m_colors.primary_bg,
                .clrBtnShadow    = self->m_colors.secondary_bg,
            };
            ::SendMessage(childWindow, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
        }
        else if (className == WC_TREEVIEW) {
            TreeView_SetBkColor(childWindow, self->m_colors.secondary_bg);
            TreeView_SetTextColor(childWindow, self->m_colors.secondary);
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
        else if (className == WC_LISTVIEW) {
            ListView_SetBkColor(childWindow, self->m_colors.secondary_bg);
            ListView_SetTextColor(childWindow, self->m_colors.secondary);
            ListView_SetTextBkColor(childWindow, self->m_colors.secondary_bg);
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
            HWND hHeader = ListView_GetHeader(childWindow);
            if (hHeader) {
                ::SetWindowTheme(hHeader, self->m_isDarkMode ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
            }
            HWND hToolTips = ListView_GetToolTips(childWindow);
            if (hToolTips) {
                ::SetWindowTheme(hToolTips, self->m_isDarkMode ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
            }
        }
        else if (className == WC_COMBOBOX) {
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_CFD" : L"Explorer", nullptr);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
}

LRESULT ThemeRenderer::TreeViewCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPNMTVCUSTOMDRAW customDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
    static LRESULT s_prePaintResult = 0;
    static HTHEME s_theme = nullptr;
    static HIMAGELIST s_imageList = nullptr;

    switch (customDraw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
        s_prePaintResult = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (s_prePaintResult == CDRF_DODEFAULT) {
            s_prePaintResult = ::GetWindowLongPtr(hWnd, DWLP_MSGRESULT);
        }
        s_theme = ::OpenThemeData(customDraw->nmcd.hdr.hwndFrom, L"TreeView");
        s_imageList = TreeView_GetImageList(customDraw->nmcd.hdr.hwndFrom, TVSIL_NORMAL);
        LRESULT result = s_prePaintResult | CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, result);
        return result;
    }
    case CDDS_ITEMPREPAINT: {
        HTREEITEM hItem = reinterpret_cast<HTREEITEM>(customDraw->nmcd.dwItemSpec);

        // background
        auto maskedItemState = customDraw->nmcd.uItemState & (CDIS_SELECTED | CDIS_HOT);
        int itemState = maskedItemState == (CDIS_SELECTED | CDIS_HOT) ? TREIS_HOTSELECTED
            : maskedItemState == CDIS_SELECTED ? TREIS_SELECTED
            : maskedItemState == CDIS_HOT ? TREIS_HOT
            : TREIS_NORMAL;
        if ((itemState == TREIS_SELECTED) && (customDraw->nmcd.hdr.hwndFrom != ::GetFocus())) {
            itemState = TREIS_SELECTEDNOTFOCUS;
        }
        if (itemState != TREIS_NORMAL && s_theme) {
            ::DrawThemeBackground(s_theme, customDraw->nmcd.hdc, TVP_TREEITEM, itemState, &customDraw->nmcd.rc, &customDraw->nmcd.rc);
        }

        // [+]/[-] signs
        RECT glyphRect{};
        TVGETITEMPARTRECTINFO info{
            .hti = hItem,
            .prc = &glyphRect,
            .partID = TVGIPR_BUTTON
        };
        if (TRUE == ::SendMessage(customDraw->nmcd.hdr.hwndFrom, TVM_GETITEMPARTRECT, 0, (LPARAM)&info)) {
            BOOL isExpanded = (TreeView_GetItemState(customDraw->nmcd.hdr.hwndFrom, hItem, TVIS_EXPANDED) & TVIS_EXPANDED) ? TRUE : FALSE;
            const int glyphStates = isExpanded ? GLPS_OPENED : GLPS_CLOSED;

            SIZE glythSize;
            if (s_theme) {
                ::GetThemePartSize(s_theme, customDraw->nmcd.hdc, TVP_GLYPH, glyphStates, nullptr, THEMESIZE::TS_DRAW, &glythSize);
                glyphRect.top += ((glyphRect.bottom - glyphRect.top) - glythSize.cy) / 2;
                glyphRect.bottom = glyphRect.top + glythSize.cy;
                glyphRect.right = glyphRect.left + glythSize.cx;
                ::DrawThemeBackground(s_theme, customDraw->nmcd.hdc, TVP_GLYPH, glyphStates, &glyphRect, nullptr);
            }
        }

        LRESULT itemPrePaintResult = CDRF_DODEFAULT;
        if (s_prePaintResult & CDRF_NOTIFYITEMDRAW) {
            ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
            itemPrePaintResult = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
            if (itemPrePaintResult == CDRF_DODEFAULT) {
                LRESULT dlgResult = ::GetWindowLongPtr(hWnd, DWLP_MSGRESULT);
                if (dlgResult != CDRF_DODEFAULT) {
                    itemPrePaintResult = dlgResult;
                }
            }
        }

        // Text & Icon
        RECT textRect{};
        TreeView_GetItemRect(customDraw->nmcd.hdr.hwndFrom, hItem, &textRect, TRUE);
        WCHAR textBuffer[MAX_PATH]{};
        TVITEM tvi = {
            .mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
            .hItem = hItem,
            .pszText = textBuffer,
            .cchTextMax = MAX_PATH,
        };
        if (TRUE == TreeView_GetItem(customDraw->nmcd.hdr.hwndFrom, &tvi)) {
            HFONT hOrigFont = (HFONT)::GetCurrentObject(customDraw->nmcd.hdc, OBJ_FONT);

            ::SetBkMode(customDraw->nmcd.hdc, TRANSPARENT);
            COLORREF textColor = TreeView_GetTextColor(customDraw->nmcd.hdr.hwndFrom);
            ::SetTextColor(customDraw->nmcd.hdc, textColor);
            ::DrawText(customDraw->nmcd.hdc, tvi.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER);

            if (hOrigFont) {
                ::SelectObject(customDraw->nmcd.hdc, hOrigFont);
            }

            if ((itemPrePaintResult & CDRF_SKIPDEFAULT) == 0) {
                const SIZE iconSize = {
                    .cx = ::GetSystemMetrics(SM_CXSMICON),
                    .cy = ::GetSystemMetrics(SM_CYSMICON),
                };
                const INT top = (textRect.top + textRect.bottom - iconSize.cy) / 2;
                const INT left = textRect.left - iconSize.cx - ::GetSystemMetrics(SM_CXEDGE);
                if (s_imageList && tvi.iImage >= 0) {
                    ::ImageList_Draw(s_imageList, tvi.iImage, customDraw->nmcd.hdc, left, top, ILD_TRANSPARENT);
                }
            }
        }

        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, (LONG)CDRF_SKIPDEFAULT);
        return CDRF_SKIPDEFAULT;
    }
    case CDDS_POSTPAINT: {
        if (s_theme) {
            ::CloseThemeData(s_theme);
            s_theme = nullptr;
        }
        if (s_prePaintResult & CDRF_NOTIFYPOSTPAINT) {
            ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
        return CDRF_DODEFAULT;
    }
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ThemeRenderer::DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto *self = reinterpret_cast<ThemeRenderer*>(dwRefData);
    switch (uIdSubclass) {
    case WINDOW_SUBCLASS_ID:
        return self->WindowProc(hWnd, uMsg, wParam, lParam);
    case REBAR_SUBCLASS_ID:
        return self->RebarProc(hWnd, uMsg, wParam, lParam);
    case BUTTON_SUBCLASS_ID:
        return self->ButtonProc(hWnd, uMsg, wParam, lParam);
    case EDIT_SUBCLASS_ID:
        return self->EditProc(hWnd, uMsg, wParam, lParam);
    case COMBOBOX_SUBCLASS_ID:
        return self->ComboBoxProc(hWnd, uMsg, wParam, lParam);
    case LISTVIEW_SUBCLASS_ID:
        return self->ListViewProc(hWnd, uMsg, wParam, lParam);
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr && nmhdr->code == NM_CUSTOMDRAW) {
            std::wstring className = GetClassName(nmhdr->hwndFrom);
            if (className == WC_TREEVIEW) {
                return TreeViewCustomDrawProc(hWnd, uMsg, wParam, lParam);
            }
            else if (className == WC_HEADER) {
                return HeaderCustomDrawProc(hWnd, uMsg, wParam, lParam);
            }
        }
        break;
    }
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushes.body_bg);
        return TRUE;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        ::SetTextColor(hdc, m_colors.secondary);
        ::SetBkColor(hdc, m_colors.secondary_bg);
        return (LRESULT)(HBRUSH)m_brushes.secondary_bg;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, WINDOW_SUBCLASS_ID);
        m_windows.erase(hWnd);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ListViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr && nmhdr->code == NM_CUSTOMDRAW) {
            std::wstring className = GetClassName(nmhdr->hwndFrom);
            if (className == WC_HEADER) {
                return HeaderCustomDrawProc(hWnd, uMsg, wParam, lParam);
            }
        }
        break;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::HeaderCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPNMCUSTOMDRAW customDraw = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
    switch (customDraw->dwDrawStage) {
    case CDDS_PREPAINT: {
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPREPAINT: {
        if (m_isDarkMode) {
            HDC hdc = customDraw->hdc;
            RECT rc = customDraw->rc;

            // Fill background
            bool isSelected = (customDraw->uItemState & CDIS_SELECTED) != 0;
            HBRUSH bgBrush = isSelected ? m_brushes.primary_bg : m_brushes.secondary_bg;
            ::FillRect(hdc, &rc, bgBrush);

            // Draw right separator
            RECT sepRect = rc;
            sepRect.left = sepRect.right - 1;
            ::FillRect(hdc, &sepRect, m_brushes.border);

            // Get header text
            WCHAR text[MAX_PATH] = {};
            HDITEM hdi{};
            hdi.mask = HDI_TEXT | HDI_FORMAT;
            hdi.pszText = text;
            hdi.cchTextMax = MAX_PATH;
            ::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEM, customDraw->dwItemSpec, reinterpret_cast<LPARAM>(&hdi));

            // Draw text
            ::SetBkMode(hdc, TRANSPARENT);
            ::SetTextColor(hdc, isSelected ? m_colors.primary : m_colors.secondary);

            RECT textRect = rc;
            textRect.left += 6;
            textRect.right -= 6;
            UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
            if (hdi.fmt & HDF_RIGHT) {
                format |= DT_RIGHT;
            } else if (hdi.fmt & HDF_CENTER) {
                format |= DT_CENTER;
            } else {
                format |= DT_LEFT;
            }
            ::DrawText(hdc, text, -1, &textRect, format);

            return CDRF_SKIPDEFAULT;
        }
        break;
    }
    case CDDS_POSTPAINT: {
        if (m_isDarkMode) {
            RECT headerRect{};
            ::GetClientRect(customDraw->hdr.hwndFrom, &headerRect);
            int count = static_cast<int>(::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEMCOUNT, 0, 0));
            if (count > 0) {
                RECT lastItemRect{};
                if (::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEMRECT, count - 1, reinterpret_cast<LPARAM>(&lastItemRect))) {
                    if (lastItemRect.right < headerRect.right) {
                        RECT fillRect = headerRect;
                        fillRect.left = lastItemRect.right;
                        ::FillRect(customDraw->hdc, &fillRect, m_brushes.secondary_bg);
                    }
                }
            } else {
                ::FillRect(customDraw->hdc, &headerRect, m_brushes.secondary_bg);
            }
        }
        return CDRF_DODEFAULT;
    }
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
        ::FillRect((HDC)wParam, &rc, m_brushes.secondary_bg);
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
        ::FillRect((HDC)wParam, &rc, m_brushes.body_bg);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, BUTTON_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        break;
    case WM_NCPAINT: {
        ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

        HDC hdc = ::GetWindowDC(hWnd);
        RECT rect;
        ::GetWindowRect(hWnd, &rect);
        ::OffsetRect(&rect, -rect.left, -rect.top);

        HWND hFocusWnd = ::GetFocus();
        if (hFocusWnd == hWnd) {
            ::FrameRect(hdc, &rect, m_brushes.primary_border);
        } else {
            ::FrameRect(hdc, &rect, m_brushes.border);
        }

        ::ReleaseDC(hWnd, hdc);
        return 0;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, EDIT_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ComboBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        break;
    case WM_PAINT: {
        LRESULT res = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (m_isDarkMode) {
            HDC hdc = ::GetWindowDC(hWnd);
            RECT rect;
            ::GetWindowRect(hWnd, &rect);
            ::OffsetRect(&rect, -rect.left, -rect.top);

            HWND hFocusWnd = ::GetFocus();
            bool isFocused = (hFocusWnd == hWnd || ::IsChild(hWnd, hFocusWnd));
            ::FrameRect(hdc, &rect, isFocused ? m_brushes.primary_border : m_brushes.border);

            ::ReleaseDC(hWnd, hdc);
        }
        return res;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, COMBOBOX_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
