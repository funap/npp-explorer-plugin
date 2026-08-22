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
constexpr UINT_PTR TOOLBAR_SUBCLASS_ID  = 6;
constexpr UINT_PTR HEADER_SUBCLASS_ID   = 7;

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
    : m_colors()
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
    case BrushType::Foreground:
        return m_brushes.foreground;
    case BrushType::HighlightText:
        return m_brushes.control_foreground;
    case BrushType::DisabledText:
        return m_brushes.disabled_text;
    case BrushType::ContentBackground:
        return m_brushes.content_background;
    case BrushType::ControlBackground:
        return m_brushes.control_background;
    case BrushType::Border:
        return m_brushes.border;
    case BrushType::PrimaryBorder:
        return m_brushes.primary_border;
    case BrushType::SecondaryBorder:
        return m_brushes.secondary_border;
    case BrushType::DisabledBorder:
        return m_brushes.disabled_border;
    case BrushType::HotBackground:
        return m_brushes.hot_background;
    case BrushType::PrimaryBackground:
        return m_brushes.primary_background;
    case BrushType::SecondaryBackground:
        return m_brushes.secondary_background;
    default:
        return m_brushes.content_background;
    }
}

void ThemeRenderer::SetTheme(const ThemeColors& colors)
{
    m_colors     = colors;

    m_brushes.foreground.CreateSolidBrush(colors.foreground);
    m_brushes.control_foreground.CreateSolidBrush(colors.control_foreground);
    m_brushes.disabled_text.CreateSolidBrush(colors.disabled_text);
    m_brushes.content_background.CreateSolidBrush(colors.content_background);
    m_brushes.control_background.CreateSolidBrush(colors.control_background);
    m_brushes.border.CreateSolidBrush(colors.border);
    m_brushes.primary_border.CreateSolidBrush(colors.primary_border);
    m_brushes.secondary_border.CreateSolidBrush(colors.secondary_border);
    m_brushes.disabled_border.CreateSolidBrush(colors.disabled_border);
    m_brushes.hot_background.CreateSolidBrush(colors.hot_background);
    m_brushes.primary_background.CreateSolidBrush(colors.primary_background);
    m_brushes.secondary_background.CreateSolidBrush(colors.secondary_background);

    for (const auto& hwnd : m_windows) {
        ApplyTheme(hwnd);
        ::RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
}

void ThemeRenderer::Register(HWND hwnd)
{
    m_windows.insert(hwnd);
    ::SetWindowSubclass(hwnd, DefaultSubclassProc, WINDOW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
    ApplyTheme(hwnd);
}

void ThemeRenderer::ApplyTheme(HWND hwnd)
{
    auto applySingle = [this](HWND target) {
        std::wstring className = GetClassName(target);
        if (className == TOOLBARCLASSNAME) {
            ::SetWindowSubclass(target, DefaultSubclassProc, TOOLBAR_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            ::SetWindowTheme(target, L"", L"");
            COLORSCHEME scheme{
                .dwSize          = sizeof(COLORSCHEME),
                .clrBtnHighlight = m_colors.primary_border,
                .clrBtnShadow    = m_colors.control_background,
            };
            ::SendMessage(target, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
        }
        else if (className == REBARCLASSNAME) {
            ::SetWindowSubclass(target, DefaultSubclassProc, REBAR_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            ::SetWindowTheme(target, L"", L"");
            ::SendMessage(target, RB_SETBKCOLOR, 0, (LPARAM)m_colors.control_background);
            ::SendMessage(target, RB_SETTEXTCOLOR, 0, (LPARAM)m_colors.foreground);
        }
        else if (className == WC_BUTTON) {
            ::SetWindowSubclass(target, DefaultSubclassProc, BUTTON_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        }
        else if (className == WC_EDIT) {
            ::SetWindowSubclass(target, DefaultSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        }
        else if (className == WC_COMBOBOX) {
            ::SetWindowSubclass(target, DefaultSubclassProc, COMBOBOX_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            ::SetWindowTheme(target, L"Explorer", nullptr);
        }
        else if (className == WC_TREEVIEW) {
            ::SendMessage(target, TVM_SETEXTENDEDSTYLE, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
            TreeView_SetBkColor(target, m_colors.content_background);
            TreeView_SetTextColor(target, m_colors.foreground);
            ::SetWindowTheme(target, L"Explorer", nullptr);
        }
        else if (className == WC_LISTVIEW) {
            LONG_PTR style = ::GetWindowLongPtr(target, GWL_STYLE);
            if (style & LVS_OWNERDRAWFIXED) {
                ListView_SetBkColor(target, m_colors.content_background);
                ListView_SetTextColor(target, m_colors.foreground);
                ListView_SetTextBkColor(target, m_colors.content_background);
                return;
            }
            if ((style & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) != (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) {
                ::SetWindowLongPtr(target, GWL_STYLE, style | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
            }
            ::SetWindowSubclass(target, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
            ListView_SetExtendedListViewStyleEx(target, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(target, m_colors.content_background);
            ListView_SetTextColor(target, m_colors.foreground);
            ListView_SetTextBkColor(target, m_colors.content_background);
            ::SetWindowTheme(target, L"Explorer", nullptr);
            HWND hHeader = ListView_GetHeader(target);
            if (hHeader) {
                ::SetWindowSubclass(hHeader, DefaultSubclassProc, HEADER_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
                ::SetWindowTheme(hHeader, L"", L"");
            }
            HWND hToolTips = ListView_GetToolTips(target);
            if (hToolTips) {
                ::SetWindowTheme(hToolTips, L"Explorer", nullptr);
            }
        }
    };

    applySingle(hwnd);
    EnumChildWindows(hwnd, [](HWND childWindow, LPARAM lParam) -> BOOL {
        auto* fn = reinterpret_cast<decltype(applySingle)*>(lParam);
        (*fn)(childWindow);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&applySingle));
}

void ThemeRenderer::DrawChevron(HDC hdc, const RECT& rect, bool isExpanded, COLORREF color)
{
    int cx = (rect.left + rect.right) / 2;
    int cy = (rect.top + rect.bottom) / 2;

    int h = (rect.bottom - rect.top);
    int s = (h >= 24) ? 5 : 4;

    HPEN hPen = ::CreatePen(PS_SOLID, 2, color);
    HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);

    if (isExpanded) {
        // Downward chevron: ∨
        POINT pts[3] = {
            { cx - s, cy - s / 2 },
            { cx,     cy + s / 2 },
            { cx + s, cy - s / 2 }
        };
        ::Polyline(hdc, pts, 3);
    } else {
        // Rightward chevron: >
        POINT pts[3] = {
            { cx - s / 2, cy - s },
            { cx + s / 2, cy },
            { cx - s / 2, cy + s }
        };
        ::Polyline(hdc, pts, 3);
    }

    ::SelectObject(hdc, hOldPen);
    ::DeleteObject(hPen);
}

LRESULT ThemeRenderer::TreeViewCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPNMTVCUSTOMDRAW customDraw = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
    static LRESULT s_prePaintResult = 0;
    static HIMAGELIST s_imageList = nullptr;

    switch (customDraw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
        s_prePaintResult = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
        if (s_prePaintResult == CDRF_DODEFAULT) {
            s_prePaintResult = ::GetWindowLongPtr(hWnd, DWLP_MSGRESULT);
        }
        s_imageList = TreeView_GetImageList(customDraw->nmcd.hdr.hwndFrom, TVSIL_NORMAL);
        LRESULT result = s_prePaintResult | CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, result);
        return result;
    }
    case CDDS_ITEMPREPAINT: {
        HWND hTree = customDraw->nmcd.hdr.hwndFrom;
        HDC hdc = customDraw->nmcd.hdc;
        HTREEITEM hItem = reinterpret_cast<HTREEITEM>(customDraw->nmcd.dwItemSpec);

        UINT itemState = TreeView_GetItemState(hTree, hItem, TVIS_DROPHILITED | TVIS_SELECTED);
        bool isDropHilited = ((customDraw->nmcd.uItemState & CDIS_DROPHILITED) != 0) ||
                             ((itemState & TVIS_DROPHILITED) != 0) ||
                             (TreeView_GetDropHilight(hTree) == hItem);
        bool isSelected = ((customDraw->nmcd.uItemState & CDIS_SELECTED) != 0) ||
                          ((itemState & TVIS_SELECTED) != 0);
        bool isHot = (customDraw->nmcd.uItemState & CDIS_HOT) != 0;
        bool isFocused = (hTree == ::GetFocus());

        // 1. Draw Item Background & Border purely with GDI
        RECT itemRect = customDraw->nmcd.rc;
        if (isDropHilited || isSelected) {
            if (isFocused || isHot) {
                ::FillRect(hdc, &itemRect, m_brushes.primary_background);
            } else {
                ::FillRect(hdc, &itemRect, m_brushes.secondary_background);
            }
        } else if (isHot) {
            ::FillRect(hdc, &itemRect, m_brushes.hot_background);
        } else {
            ::FillRect(hdc, &itemRect, m_brushes.content_background);
        }

        if (isSelected) {
            if (isFocused || isHot) {
                ::FrameRect(hdc, &itemRect, m_brushes.primary_border);
            }
            else {
                ::FrameRect(hdc, &itemRect, m_brushes.secondary_border);
            }
        }

        // 2. Text Rect (Used for alignment of Icon and Chevron)
        RECT textRect{};
        TreeView_GetItemRect(hTree, hItem, &textRect, TRUE);

        // 3. Draw [+]/[-] (Chevron > / ∨) Glyph
        RECT glyphRect{};
        TVGETITEMPARTRECTINFO info{
            .hti = hItem,
            .prc = &glyphRect,
            .partID = TVGIPR_BUTTON
        };
        if (TRUE == ::SendMessage(hTree, TVM_GETITEMPARTRECT, 0, reinterpret_cast<LPARAM>(&info))) {
            glyphRect.top = textRect.top;
            glyphRect.bottom = textRect.bottom;

            BOOL isExpanded = (TreeView_GetItemState(hTree, hItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
            COLORREF glyphColor = m_colors.foreground;
            DrawChevron(hdc, glyphRect, isExpanded, glyphColor);
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

        // 4. Text & Icon
        WCHAR textBuffer[MAX_PATH]{};
        TVITEM tvi = {
            .mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE,
            .hItem = hItem,
            .stateMask = TVIS_CUT,
            .pszText = textBuffer,
            .cchTextMax = MAX_PATH,
        };
        if (TRUE == TreeView_GetItem(hTree, &tvi)) {
            HFONT hOrigFont = (HFONT)::GetCurrentObject(hdc, OBJ_FONT);

            ::SetBkMode(hdc, TRANSPARENT);
            COLORREF textColor = (tvi.state & TVIS_CUT) ? m_colors.disabled_text
                                                        : m_colors.foreground;
            ::SetTextColor(hdc, textColor);
            ::DrawText(hdc, tvi.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

            if (hOrigFont) {
                ::SelectObject(hdc, hOrigFont);
            }

            if ((itemPrePaintResult & CDRF_SKIPDEFAULT) == 0) {
                const SIZE iconSize = {
                    .cx = ::GetSystemMetrics(SM_CXSMICON),
                    .cy = ::GetSystemMetrics(SM_CYSMICON),
                };
                const INT top = (textRect.top + textRect.bottom - iconSize.cy) / 2;
                const INT left = textRect.left - iconSize.cx - ::GetSystemMetrics(SM_CXEDGE);
                if (s_imageList && tvi.iImage >= 0) {
                    ::ImageList_Draw(s_imageList, tvi.iImage, hdc, left, top, ILD_TRANSPARENT);
                }
            }
        }

        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, (LONG)CDRF_SKIPDEFAULT);
        return CDRF_SKIPDEFAULT;
    }
    case CDDS_POSTPAINT: {
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
    case TOOLBAR_SUBCLASS_ID:
        return self->ToolBarProc(hWnd, uMsg, wParam, lParam);
    case HEADER_SUBCLASS_ID:
        return self->HeaderProc(hWnd, uMsg, wParam, lParam);
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
            else if (className == TOOLBARCLASSNAME) {
                return ToolBarCustomDrawProc(hWnd, uMsg, wParam, lParam);
            }
        }
        break;
    }
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushes.control_background);
        return TRUE;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        ::SetTextColor(hdc, m_colors.foreground);
        ::SetBkColor(hdc, m_colors.content_background);
        return (LRESULT)(HBRUSH)m_brushes.content_background;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN:
    {
        HDC hdc = (HDC)wParam;
        ::SetTextColor(hdc, m_colors.foreground);
        ::SetBkColor(hdc, m_colors.control_background);
        return (LRESULT)(HBRUSH)m_brushes.control_background;
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
    LONG_PTR style = ::GetWindowLongPtr(hWnd, GWL_STYLE);
    if (style & LVS_OWNERDRAWFIXED) {
        return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_PRINTCLIENT:
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = reinterpret_cast<HDC>(wParam);
        if (hdc == nullptr) {
            hdc = ::BeginPaint(hWnd, &ps);
        }

        PaintListView(hWnd, hdc);

        if (ps.hdc != nullptr) {
            ::EndPaint(hWnd, &ps);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        LVHITTESTINFO ht{};
        ht.pt = pt;
        int currentHot = ListView_HitTest(hWnd, &ht);
        int lastHot = static_cast<int>(reinterpret_cast<INT_PTR>(::GetPropW(hWnd, L"Theme_LastHotItem")));
        if (currentHot != lastHot) {
            ::SetPropW(hWnd, L"Theme_LastHotItem", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(currentHot)));
            ::InvalidateRect(hWnd, nullptr, FALSE);

            TRACKMOUSEEVENT tme{
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE,
                .hwndTrack = hWnd,
                .dwHoverTime = 0
            };
            ::TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        ::SetPropW(hWnd, L"Theme_LastHotItem", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-1)));
        ::InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
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
        ::RemovePropW(hWnd, L"Theme_LastHotItem");
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void ThemeRenderer::PaintListView(HWND hWnd, HDC hdc)
{
    RECT clientRect{};
    ::GetClientRect(hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;
    if (width <= 0 || height <= 0) {
        return;
    }

    HDC memDC = ::CreateCompatibleDC(hdc);
    HBITMAP memBitmap = ::CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = (HBITMAP)::SelectObject(memDC, memBitmap);

    HWND hHeader = ListView_GetHeader(hWnd);
    int headerBottom = 0;
    if (hHeader && ::IsWindowVisible(hHeader)) {
        RECT headerRect{};
        if (::GetWindowRect(hHeader, &headerRect)) {
            POINT pt = { 0, headerRect.bottom };
            ::ScreenToClient(hWnd, &pt);
            headerBottom = pt.y;
        }
    }

    // 1. Fill background (below header only)
    RECT itemAreaRect = clientRect;
    itemAreaRect.top = headerBottom;
    ::FillRect(memDC, &itemAreaRect, m_brushes.content_background);

    // 2. Setup Font & Drawing settings
    HFONT hFont = (HFONT)::SendMessage(hWnd, WM_GETFONT, 0, 0);
    HFONT oldFont = hFont ? (HFONT)::SelectObject(memDC, hFont) : nullptr;
    ::SetBkMode(memDC, TRANSPARENT);

    int colCount = hHeader ? static_cast<int>(::SendMessage(hHeader, HDM_GETITEMCOUNT, 0, 0)) : 1;
    if (colCount <= 0) colCount = 1;

    HIMAGELIST s_imageList = ListView_GetImageList(hWnd, LVSIL_SMALL);
    int totalItems = ListView_GetItemCount(hWnd);
    int topIndex = ListView_GetTopIndex(hWnd);
    int countPerPage = ListView_GetCountPerPage(hWnd);
    int endIndex = std::min(totalItems - 1, topIndex + countPerPage + 1);

    HWND hParent = ::GetParent(hWnd);
    UINT_PTR ctrlId = ::GetDlgCtrlID(hWnd);
    NMLVCUSTOMDRAW lvcd{};
    lvcd.nmcd.hdr.hwndFrom = hWnd;
    lvcd.nmcd.hdr.idFrom = ctrlId;
    lvcd.nmcd.hdr.code = NM_CUSTOMDRAW;
    lvcd.nmcd.hdc = memDC;

    // Notify CDDS_PREPAINT
    lvcd.nmcd.dwDrawStage = CDDS_PREPAINT;
    lvcd.nmcd.rc = clientRect;
    lvcd.nmcd.dwItemSpec = 0;
    lvcd.nmcd.uItemState = 0;
    lvcd.iSubItem = 0;
    ::SetWindowLongPtr(hParent, DWLP_MSGRESULT, CDRF_DODEFAULT);
    LRESULT prePaintResult = ::SendMessage(hParent, WM_NOTIFY, ctrlId, reinterpret_cast<LPARAM>(&lvcd));
    if (prePaintResult == 0 || prePaintResult == CDRF_DODEFAULT) {
        LRESULT dlgResult = ::GetWindowLongPtr(hParent, DWLP_MSGRESULT);
        if (dlgResult != CDRF_DODEFAULT) {
            prePaintResult = dlgResult;
        }
    }

    POINT cursorPos{};
    int hotItem = -1;
    if (::GetCursorPos(&cursorPos)) {
        HWND hWndAtPt = ::WindowFromPoint(cursorPos);
        if (hWndAtPt == hWnd && ::ScreenToClient(hWnd, &cursorPos)) {
            LVHITTESTINFO ht{};
            ht.pt = cursorPos;
            hotItem = ListView_HitTest(hWnd, &ht);
        }
    }

    int lastVisibleBottom = 0;
    bool hasVisibleItems = false;
    RECT lastItemCol0Bounds{};

    // 3. Draw Visible Items
    for (int itemIndex = topIndex; itemIndex <= endIndex; ++itemIndex) {
        if (itemIndex < 0 || itemIndex >= totalItems) continue;

        RECT rowRect{};
        if (!ListView_GetItemRect(hWnd, itemIndex, &rowRect, LVIR_BOUNDS)) {
            continue;
        }

        if (rowRect.bottom < clientRect.top || rowRect.top > clientRect.bottom) {
            continue;
        }

        hasVisibleItems = true;
        if (rowRect.bottom > lastVisibleBottom) {
            lastVisibleBottom = rowRect.bottom;
        }

        // Item state
        UINT itemStateMask = ListView_GetItemState(hWnd, itemIndex, LVIS_SELECTED | LVIS_DROPHILITED | LVIS_FOCUSED);
        bool isDropHilited = (itemStateMask & LVIS_DROPHILITED) != 0;
        bool isSelected = (itemStateMask & LVIS_SELECTED) != 0;
        bool isHot = (itemIndex == hotItem);
        bool isFocused = (hWnd == ::GetFocus());

        // Draw row background & selection
        if (isDropHilited || isSelected) {
            if (isFocused || isHot) {
                ::FillRect(memDC, &rowRect, m_brushes.primary_background);
            } else {
                ::FillRect(memDC, &rowRect, m_brushes.secondary_background);
            }
        } else if (isHot) {
            ::FillRect(memDC, &rowRect, m_brushes.hot_background);
        } else {
            ::FillRect(memDC, &rowRect, m_brushes.content_background);
        }

        if (isSelected) {
            if (isFocused || isHot) {
                ::FrameRect(memDC, &rowRect, m_brushes.primary_border);
            }
            else {
                ::FrameRect(memDC, &rowRect, m_brushes.secondary_border);
            }
        }

        // Notify CDDS_ITEMPREPAINT
        LRESULT itemPrePaintResult = CDRF_DODEFAULT;
        if (prePaintResult & CDRF_NOTIFYITEMDRAW) {
            lvcd.nmcd.dwDrawStage = CDDS_ITEMPREPAINT;
            lvcd.nmcd.rc = rowRect;
            lvcd.nmcd.dwItemSpec = itemIndex;
            lvcd.nmcd.uItemState = (isSelected ? CDIS_SELECTED : 0) | (isFocused ? CDIS_FOCUS : 0) | (isHot ? CDIS_HOT : 0) | (isDropHilited ? CDIS_DROPHILITED : 0);
            lvcd.iSubItem = 0;

            ::SetWindowLongPtr(hParent, DWLP_MSGRESULT, CDRF_DODEFAULT);
            itemPrePaintResult = ::SendMessage(hParent, WM_NOTIFY, ctrlId, reinterpret_cast<LPARAM>(&lvcd));
            if (itemPrePaintResult == 0 || itemPrePaintResult == CDRF_DODEFAULT) {
                LRESULT dlgResult = ::GetWindowLongPtr(hParent, DWLP_MSGRESULT);
                if (dlgResult != CDRF_DODEFAULT) {
                    itemPrePaintResult = dlgResult;
                }
            }
        }

        ::SetTextColor(memDC, m_colors.foreground);

        // Column 0: Icon + Text
        RECT labelRect{};
        ListView_GetSubItemRect(hWnd, itemIndex, 0, LVIR_LABEL, &labelRect);
        RECT iconRect{};
        ListView_GetSubItemRect(hWnd, itemIndex, 0, LVIR_ICON, &iconRect);

        // Draw Icon if CustomDraw didn't skip it
        LVITEM lvi{};
        lvi.mask = LVIF_IMAGE | LVIF_STATE;
        lvi.iItem = itemIndex;
        lvi.iSubItem = 0;
        lvi.stateMask = LVIS_OVERLAYMASK | LVIS_CUT;
        if (ListView_GetItem(hWnd, &lvi)) {
            if (lvi.state & LVIS_CUT) {
                ::SetTextColor(memDC, m_colors.disabled_text);
            }
            if ((itemPrePaintResult & CDRF_SKIPDEFAULT) == 0 && lvi.iImage >= 0 && s_imageList) {
                int iconHeight = ::GetSystemMetrics(SM_CYSMICON);
                int cx = 0, cy = 0;
                if (ImageList_GetIconSize(s_imageList, &cx, &cy) && cy > 0) {
                    iconHeight = cy;
                }
                int iconTop = rowRect.top + ((rowRect.bottom - rowRect.top) - iconHeight) / 2;

                UINT fStyle = ILD_TRANSPARENT;
                if (lvi.state & LVIS_CUT) fStyle |= ILD_BLEND50;
                ImageList_Draw(s_imageList, lvi.iImage, memDC, iconRect.left, iconTop, fStyle);
            }
        }

        // Notify CDDS_ITEMPOSTPAINT if requested
        if (itemPrePaintResult & CDRF_NOTIFYPOSTPAINT) {
            lvcd.nmcd.dwDrawStage = CDDS_ITEMPOSTPAINT;
            ::SendMessage(hParent, WM_NOTIFY, ctrlId, reinterpret_cast<LPARAM>(&lvcd));
        }

        // Column 0 Text (respect column width and scroll position)
        int col0Width = ListView_GetColumnWidth(hWnd, 0);
        RECT col0Bounds{};
        ListView_GetSubItemRect(hWnd, itemIndex, 0, LVIR_BOUNDS, &col0Bounds);
        lastItemCol0Bounds = col0Bounds;

        WCHAR textBuffer[MAX_PATH]{};
        ListView_GetItemText(hWnd, itemIndex, 0, textBuffer, MAX_PATH);
        RECT col0TextRect = labelRect;
        col0TextRect.left += 2;
        int col0RightLimit = col0Bounds.left + col0Width - 4;
        if (col0RightLimit > col0TextRect.left) {
            col0TextRect.right = col0RightLimit;
        }
        ::DrawText(memDC, textBuffer, -1, &col0TextRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

        // Column 0 right divider line
        if (colCount > 1) {
            RECT divRect{
                .left   = col0Bounds.left + col0Width - 1,
                .top    = rowRect.top,
                .right  = col0Bounds.left + col0Width,
                .bottom = rowRect.bottom
            };
            ::FillRect(memDC, &divRect, m_brushes.disabled_border);
        }

        // Columns 1..N-1
        for (int col = 1; col < colCount; ++col) {
            RECT subItemRect{};
            ListView_GetSubItemRect(hWnd, itemIndex, col, LVIR_BOUNDS, &subItemRect);
            if (subItemRect.right <= subItemRect.left) {
                continue;
            }

            HDITEM hdi{};
            hdi.mask = HDI_FORMAT;
            ::SendMessage(hHeader, HDM_GETITEM, col, reinterpret_cast<LPARAM>(&hdi));

            UINT format = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
            if (hdi.fmt & HDF_RIGHT) {
                format |= DT_RIGHT;
            } else if (hdi.fmt & HDF_CENTER) {
                format |= DT_CENTER;
            } else {
                format |= DT_LEFT;
            }

            textBuffer[0] = L'\0';
            ListView_GetItemText(hWnd, itemIndex, col, textBuffer, MAX_PATH);
            RECT textRect = subItemRect;
            textRect.left += 6;
            textRect.right -= 6;
            ::DrawText(memDC, textBuffer, -1, &textRect, format);

            // Right divider line (except for the last column)
            if (col < colCount - 1) {
                RECT divRect{
                    .left   = subItemRect.right - 1,
                    .top    = rowRect.top,
                    .right  = subItemRect.right,
                    .bottom = rowRect.bottom
                };
                ::FillRect(memDC, &divRect, m_brushes.disabled_border);
            }
        }
    }

    // 4. Draw Column Dividers in Empty Area below items
    int topY = hasVisibleItems ? lastVisibleBottom : headerBottom;
    if (topY < clientRect.bottom && topY >= 0) {
        int col0Width = ListView_GetColumnWidth(hWnd, 0);
        int col0Left = hasVisibleItems ? lastItemCol0Bounds.left : 0;

        if (colCount > 1) {
            RECT div0Rect{
                .left   = col0Left + col0Width - 1,
                .top    = topY,
                .right  = col0Left + col0Width,
                .bottom = clientRect.bottom
            };
            ::FillRect(memDC, &div0Rect, m_brushes.disabled_border);
        }

        int runningX = col0Left + col0Width;
        for (int col = 1; col < colCount - 1; ++col) {
            int colWidth = ListView_GetColumnWidth(hWnd, col);
            runningX += colWidth;
            RECT divRect{
                .left   = runningX - 1,
                .top    = topY,
                .right  = runningX,
                .bottom = clientRect.bottom
            };
            ::FillRect(memDC, &divRect, m_brushes.disabled_border);
        }
    }

    // Notify CDDS_POSTPAINT
    if (prePaintResult & CDRF_NOTIFYPOSTPAINT) {
        lvcd.nmcd.dwDrawStage = CDDS_POSTPAINT;
        lvcd.nmcd.rc = clientRect;
        lvcd.nmcd.dwItemSpec = 0;
        lvcd.nmcd.uItemState = 0;
        lvcd.iSubItem = 0;
        ::SendMessage(hParent, WM_NOTIFY, ctrlId, reinterpret_cast<LPARAM>(&lvcd));
    }

    // 5. Transfer to screen (only item area, never overwrite header!)
    if (height > headerBottom) {
        ::BitBlt(hdc, 0, headerBottom, width, height - headerBottom, memDC, 0, headerBottom, SRCCOPY);
    }

    // Cleanup
    if (oldFont) {
        ::SelectObject(memDC, oldFont);
    }
    ::SelectObject(memDC, oldBitmap);
    ::DeleteObject(memBitmap);
    ::DeleteDC(memDC);
}

LRESULT ThemeRenderer::HeaderCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LPNMCUSTOMDRAW customDraw = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
    switch (customDraw->dwDrawStage) {
    case CDDS_PREPAINT: {
        return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPREPAINT: {
        HDC hdc = customDraw->hdc;
        RECT rc = customDraw->rc;

        // Fill background
        bool isHot = (customDraw->uItemState & CDIS_HOT) != 0;
        bool isSelected = (customDraw->uItemState & CDIS_SELECTED) != 0;

        HBRUSH bgBrush = isHot ? m_brushes.hot_background : m_brushes.control_background;
        ::FillRect(hdc, &rc, bgBrush);

        // Draw right separator
        RECT sepRect = rc;
        sepRect.left = sepRect.right - 1;
        ::FillRect(hdc, &sepRect, m_brushes.disabled_border);

        // Get header text
        WCHAR text[MAX_PATH] = {};
        HDITEM hdi{};
        hdi.mask = HDI_TEXT | HDI_FORMAT;
        hdi.pszText = text;
        hdi.cchTextMax = MAX_PATH;
        ::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEM, customDraw->dwItemSpec, reinterpret_cast<LPARAM>(&hdi));

        // Draw text
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, m_colors.control_foreground);

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

        // Draw Sort Indicator (chevron ^ or v) at top center
        if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) {
            int cx = (rc.left + rc.right) / 2;
            int topY = rc.top;
            POINT pts[3]{};
            if (hdi.fmt & HDF_SORTUP) {
                // Ascending: pointing up (^)
                pts[0] = { cx - 3, topY + 3 };
                pts[1] = { cx,     topY     };
                pts[2] = { cx + 3, topY + 3 };
            } else {
                // Descending: pointing down (v)
                pts[0] = { cx - 3, topY     };
                pts[1] = { cx,     topY + 3 };
                pts[2] = { cx + 3, topY     };
            }
            COLORREF sortArrowColor = m_colors.border;
            HPEN hPen = ::CreatePen(PS_SOLID, 1, sortArrowColor);
            HPEN oldPen = (HPEN)::SelectObject(hdc, hPen);
            ::Polyline(hdc, pts, 3);
            ::SetPixel(hdc, pts[2].x, pts[2].y, sortArrowColor);
            ::SelectObject(hdc, oldPen);
            ::DeleteObject(hPen);
        }

        return CDRF_SKIPDEFAULT;
    }
    case CDDS_POSTPAINT: {
        RECT headerRect{};
        ::GetClientRect(customDraw->hdr.hwndFrom, &headerRect);
        int count = static_cast<int>(::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEMCOUNT, 0, 0));
        if (count > 0) {
            RECT lastItemRect{};
            if (::SendMessage(customDraw->hdr.hwndFrom, HDM_GETITEMRECT, count - 1, reinterpret_cast<LPARAM>(&lastItemRect))) {
                if (lastItemRect.right < headerRect.right) {
                    RECT fillRect = headerRect;
                    fillRect.left = lastItemRect.right;
                    ::FillRect(customDraw->hdc, &fillRect, m_brushes.control_background);
                }
            }
        } else {
            ::FillRect(customDraw->hdc, &headerRect, m_brushes.control_background);
        }
        return CDRF_DODEFAULT;
    }
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::HeaderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, HEADER_SUBCLASS_ID);
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
        ::FillRect((HDC)wParam, &rc, m_brushes.control_background);
        return TRUE;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr && nmhdr->code == NM_CUSTOMDRAW) {
            std::wstring className = GetClassName(nmhdr->hwndFrom);
            if (className == TOOLBARCLASSNAME) {
                return ToolBarCustomDrawProc(hWnd, uMsg, wParam, lParam);
            }
        }
        break;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ToolBarProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushes.control_background);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, TOOLBAR_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ToolBarCustomDrawProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto lpNMTBCD = reinterpret_cast<LPNMTBCUSTOMDRAW>(lParam);
    switch (lpNMTBCD->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: {
        return CDRF_NOTIFYITEMDRAW;
    }
    case CDDS_ITEMPREPAINT: {
        lpNMTBCD->hbrMonoDither = m_brushes.content_background;
        lpNMTBCD->hbrLines = m_brushes.secondary_border;
        lpNMTBCD->clrText = (lpNMTBCD->nmcd.uItemState & CDIS_DISABLED) ? m_colors.disabled_text : m_colors.foreground;
        lpNMTBCD->clrTextHighlight = m_colors.foreground;
        lpNMTBCD->clrBtnFace = m_colors.control_background;
        lpNMTBCD->clrBtnHighlight = m_colors.secondary_background;
        lpNMTBCD->clrHighlightHotTrack = m_colors.hot_background;
        lpNMTBCD->nStringBkMode = TRANSPARENT;
        lpNMTBCD->nHLStringBkMode = TRANSPARENT;

        constexpr int roundCornerValue = 5;
        HDC hdc = lpNMTBCD->nmcd.hdc;
        const RECT& rc = lpNMTBCD->nmcd.rc;

        if ((lpNMTBCD->nmcd.uItemState & CDIS_HOT) == CDIS_HOT) {
            HPEN hHotEdgePen = ::CreatePen(PS_SOLID, 1, m_colors.primary_border);
            HGDIOBJ holdBrush = ::SelectObject(hdc, m_brushes.hot_background);
            HGDIOBJ holdPen = ::SelectObject(hdc, hHotEdgePen);
            ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, roundCornerValue, roundCornerValue);
            ::SelectObject(hdc, holdBrush);
            ::SelectObject(hdc, holdPen);
            ::DeleteObject(hHotEdgePen);

            lpNMTBCD->nmcd.uItemState &= ~(CDIS_CHECKED | CDIS_HOT);
        }
        else if ((lpNMTBCD->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED) {
            HPEN hEdgePen = ::CreatePen(PS_SOLID, 1, m_colors.secondary_border);
            HGDIOBJ holdBrush = ::SelectObject(hdc, m_brushes.secondary_background);
            HGDIOBJ holdPen = ::SelectObject(hdc, hEdgePen);
            ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, roundCornerValue, roundCornerValue);
            ::SelectObject(hdc, holdBrush);
            ::SelectObject(hdc, holdPen);
            ::DeleteObject(hEdgePen);

            lpNMTBCD->nmcd.uItemState &= ~CDIS_CHECKED;
        }

        LRESULT lr = TBCDRF_USECDCOLORS;
        if ((lpNMTBCD->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED) {
            lr |= TBCDRF_NOBACKGROUND;
        }

        return lr;
    }
    default:
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
        ::FillRect((HDC)wParam, &rc, m_brushes.control_background);
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
    HWND hParent = ::GetParent(hWnd);
    WCHAR parentClass[32] = {};
    if (hParent && ::GetClassName(hParent, parentClass, 32) && _wcsicmp(parentClass, WC_COMBOBOX) == 0) {
        // Child edit of ComboBox: suppress Windows 3D client edge painting and coordinate focus
        switch (uMsg) {
        case WM_NCPAINT:
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            ::RedrawWindow(hParent, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
            break;
        }
        return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        break;
    case WM_NCPAINT: {
        ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

        HDC hdc = ::GetWindowDC(hWnd);
        if (hdc) {
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
        }
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
    case WM_ERASEBKGND:
        return TRUE;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        ::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        break;
    case WM_PAINT: {
        LRESULT res = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

        HDC hdc = ::GetWindowDC(hWnd);
        if (hdc) {
            RECT rcWindow{};
            ::GetWindowRect(hWnd, &rcWindow);
            ::OffsetRect(&rcWindow, -rcWindow.left, -rcWindow.top);

            HWND hFocusWnd = ::GetFocus();
            bool isFocused = (hFocusWnd == hWnd || ::IsChild(hWnd, hFocusWnd));

            COMBOBOXINFO cbi{ .cbSize = sizeof(COMBOBOXINFO) };
            if (::GetComboBoxInfo(hWnd, &cbi)) {
                RECT rcClient{};
                ::GetClientRect(hWnd, &rcClient);
                POINT ptClient{ 0, 0 };
                ::ClientToScreen(hWnd, &ptClient);
                RECT rcScreenWnd{};
                ::GetWindowRect(hWnd, &rcScreenWnd);
                int clientOffsetX = ptClient.x - rcScreenWnd.left;
                int clientOffsetY = ptClient.y - rcScreenWnd.top;

                RECT rcButton = cbi.rcButton;
                ::OffsetRect(&rcButton, clientOffsetX, clientOffsetY);

                RECT rcItem = cbi.rcItem;
                ::OffsetRect(&rcItem, clientOffsetX, clientOffsetY);

                // 1. Fill the entire dropdown button and gap area from rcItem.right to the outer border
                RECT rcRightArea = { rcItem.right, rcWindow.top + 1, rcWindow.right - 1, rcWindow.bottom - 1 };
                ::FillRect(hdc, &rcRightArea, m_brushes.control_background);

                // 2. Erase any 3D white bevel borders around top/bottom/left
                RECT rcTopBorder = { rcWindow.left, rcWindow.top, rcWindow.right, rcItem.top };
                ::FillRect(hdc, &rcTopBorder, m_brushes.control_background);

                RECT rcBottomBorder = { rcWindow.left, rcItem.bottom, rcWindow.right, rcWindow.bottom };
                ::FillRect(hdc, &rcBottomBorder, m_brushes.control_background);

                RECT rcLeftBorder = { rcWindow.left, rcWindow.top, rcItem.left, rcWindow.bottom };
                ::FillRect(hdc, &rcLeftBorder, m_brushes.control_background);

                // 3. Draw down chevron in the dropdown button
                int cx = (rcButton.left + rcButton.right) / 2;
                int cy = (rcButton.top + rcButton.bottom) / 2;
                POINT pts[3] = {
                    { cx - 4, cy - 2 },
                    { cx,     cy + 2 },
                    { cx + 4, cy - 2 }
                };
                COLORREF arrowColor = m_colors.foreground;
                HPEN hPen = ::CreatePen(PS_SOLID, 2, arrowColor);
                HPEN oldPen = (HPEN)::SelectObject(hdc, hPen);
                ::Polyline(hdc, pts, 3);
                ::SelectObject(hdc, oldPen);
                ::DeleteObject(hPen);
            }

            // 4. Draw clean 1px outer border of the ComboBox
            ::FrameRect(hdc, &rcWindow, m_brushes.border);

            ::ReleaseDC(hWnd, hdc);
        }
        return res;
    }
    case WM_NCPAINT: {
        HDC hdc = ::GetWindowDC(hWnd);
        if (hdc) {
            RECT rcWindow{};
            ::GetWindowRect(hWnd, &rcWindow);
            ::OffsetRect(&rcWindow, -rcWindow.left, -rcWindow.top);

            HWND hFocusWnd = ::GetFocus();
            bool isFocused = (hFocusWnd == hWnd || ::IsChild(hWnd, hFocusWnd));
            ::FrameRect(hdc, &rcWindow, m_brushes.border);

            ::ReleaseDC(hWnd, hdc);
        }
        return 0;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, COMBOBOX_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
