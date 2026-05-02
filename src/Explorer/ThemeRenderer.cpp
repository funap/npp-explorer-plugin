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

#include <uxtheme.h>
#include <Vsstyle.h>
#include <Vssym32.h>

namespace {
constexpr UINT_PTR WINDOW_SUBCLASS_ID   = 0;
constexpr UINT_PTR REBAR_SUBCLASS_ID    = 1;
constexpr UINT_PTR BUTTON_SUBCLASS_ID   = 2;
constexpr UINT_PTR EDIT_SUBCLASS_ID     = 3;
constexpr UINT_PTR LISTVIEW_SUBCLASS_ID = 4;
constexpr UINT_PTR HEADER_SUBCLASS_ID   = 5;

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
    , m_hotHeaderItem(-1)
    , m_hHeaderTracked(nullptr)
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
        else if (className == WC_LISTVIEW) {
            ::SetWindowSubclass(childWindow, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
            HWND hHeader = ListView_GetHeader(childWindow);
            if (hHeader) {
                ::SetWindowSubclass(hHeader, DefaultSubclassProc, HEADER_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
            }
        }
        else if (className == WC_TREEVIEW) {
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);
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
            ListView_SetTextBkColor(childWindow, CLR_NONE);
            ::SetWindowTheme(childWindow, self->m_isDarkMode ? L"DarkMode_Explorer" : L"Explorer", nullptr);

            HWND hHeader = ListView_GetHeader(childWindow);
            if (hHeader) {
                ::SetWindowSubclass(hHeader, DefaultSubclassProc, HEADER_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(self));
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(this));
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
    case LISTVIEW_SUBCLASS_ID:
        return self->ListViewProc(hWnd, uMsg, wParam, lParam);
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
    case WM_ERASEBKGND: {
        RECT rc{};
        ::GetClientRect(hWnd, &rc);
        ::FillRect((HDC)wParam, &rc, m_brushes.body_bg);
        return TRUE;
    }
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        ::SetTextColor(hdc, m_colors.secondary);
        ::SetBkColor(hdc, m_colors.secondary_bg);
        return (LRESULT)(HBRUSH)m_brushes.secondary_bg;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;
        if (nmhdr->code == NM_CUSTOMDRAW) {
            std::wstring className = GetClassName(nmhdr->hwndFrom);
            if (className == WC_LISTVIEW) {
                LPNMLVCUSTOMDRAW lpCD = (LPNMLVCUSTOMDRAW)lParam;
                if (lpCD->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    if (m_isDarkMode) {
                        lpCD->clrText = m_colors.secondary;
                        lpCD->clrTextBk = m_colors.secondary_bg;

                        if (lpCD->nmcd.uItemState & CDIS_SELECTED) {
                            lpCD->clrText = m_colors.primary;
                            lpCD->clrTextBk = m_colors.primary_bg;
                        }
                        else if (lpCD->nmcd.uItemState & CDIS_HOT) {
                            lpCD->clrText = m_colors.primary;
                            lpCD->clrTextBk = m_colors.primary_bg;
                        }

                        LRESULT res = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
                        LRESULT msgRes = ::GetWindowLongPtr(hWnd, DWLP_MSGRESULT);
                        ::SetWindowLongPtr(hWnd, DWLP_MSGRESULT, msgRes | CDRF_NEWFONT);
                        return res;
                    }
                }
            }
        }
        break;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, REBAR_SUBCLASS_ID);
        m_windows.erase(hWnd);
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
        m_windows.erase(hWnd);
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
        m_windows.erase(hWnd);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCPAINT: {
        HDC hdc = GetWindowDC(hWnd);
        RECT rect;
        GetWindowRect(hWnd, &rect);
        OffsetRect(&rect, -rect.left, -rect.top);

        HWND hFocusWnd = GetFocus();
        if (hFocusWnd == hWnd) {
            FrameRect(hdc, &rect, m_brushes.primary_border);
        } else {
            FrameRect(hdc, &rect, m_brushes.border);
        }

        ReleaseDC(hWnd, hdc);
        return TRUE;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, EDIT_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::ListViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ThemeRenderer::HeaderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_PAINT: {
        if (!m_isDarkMode) break;
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rcHeader;
        GetClientRect(hWnd, &rcHeader);

        // Fill background
        FillRect(hdc, &rcHeader, m_brushes.secondary_bg);

        int count = Header_GetItemCount(hWnd);
        for (int i = 0; i < count; i++) {
            RECT rcItem;
            Header_GetItemRect(hWnd, i, &rcItem);

            if (i == m_hotHeaderItem && m_hHeaderTracked == hWnd) {
                FillRect(hdc, &rcItem, m_brushes.primary_bg);
            }

            // Draw text
            WCHAR text[MAX_PATH];
            HDITEM hdi = { .mask = HDI_TEXT | HDI_FORMAT, .pszText = text, .cchTextMax = MAX_PATH };
            Header_GetItem(hWnd, i, &hdi);

            SetTextColor(hdc, (i == m_hotHeaderItem && m_hHeaderTracked == hWnd) ? m_colors.primary : m_colors.secondary);
            SetBkMode(hdc, TRANSPARENT);

            RECT rcText = rcItem;
            rcText.left += 6;
            DrawText(hdc, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Draw sort arrow
            if (hdi.fmt & (HDF_SORTUP | HDF_SORTDOWN)) {
                HPEN hPen = CreatePen(PS_SOLID, 1, (i == m_hotHeaderItem && m_hHeaderTracked == hWnd) ? m_colors.primary : m_colors.secondary);
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

                int x = rcItem.right - 15;
                int y = rcItem.top + (rcItem.bottom - rcItem.top) / 2;

                if (hdi.fmt & HDF_SORTUP) {
                    MoveToEx(hdc, x, y + 2, nullptr);
                    LineTo(hdc, x + 4, y - 2);
                    LineTo(hdc, x + 8, y + 2);
                    LineTo(hdc, x, y + 2);
                }
                else {
                    MoveToEx(hdc, x, y - 2, nullptr);
                    LineTo(hdc, x + 4, y + 2);
                    LineTo(hdc, x + 8, y - 2);
                    LineTo(hdc, x, y - 2);
                }

                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            }

            // Draw separator
            if (i < count - 1) {
                RECT rcSep = { rcItem.right - 1, rcItem.top + 4, rcItem.right, rcItem.bottom - 4 };
                FillRect(hdc, &rcSep, m_brushes.border);
            }
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!m_isDarkMode) break;
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        HDHITTESTINFO hdht = { .pt = pt };
        int item = (int)::SendMessage(hWnd, HDM_HITTEST, 0, (LPARAM)&hdht);

        if (item != m_hotHeaderItem) {
            m_hotHeaderItem = item;
            m_hHeaderTracked = hWnd;
            InvalidateRect(hWnd, nullptr, FALSE);

            TRACKMOUSEEVENT tme = {
                .cbSize = sizeof(TRACKMOUSEEVENT),
                .dwFlags = TME_LEAVE,
                .hwndTrack = hWnd,
                .dwHoverTime = HOVER_DEFAULT
            };
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (!m_isDarkMode) break;
        m_hotHeaderItem = -1;
        m_hHeaderTracked = nullptr;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, HEADER_SUBCLASS_ID);
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
