/*
This file is part of Explorer Plugin for Notepad++
Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "FavesDialog.h"

#include <dbt.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <Vsstyle.h>
#include <Vssym32.h>

#include <format>

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "NppInterface.h"
#include "PropDlg.h"
#include "resource.h"
#include "StringUtil.h"
#include "ThemeRenderer.h"
#include "../NppPlugin/menuCmdID.h"

namespace {
ToolBarButtonUnit toolBarIcons[] = {
    {IDM_EX_EXPLORER,           IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_TB_EXPLORER,        0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_NEW_FILE,      IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_LINKNEWFILE,     0},
    {IDM_EX_LINK_NEW_FOLDER,    IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_LINKNEWFOLDER,   0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_NEW,           IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_LINKNEW,         0},
    {IDM_EX_LINK_DELETE,        IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_LINKDELETE,      0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_EDIT,          IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_LINKEDIT,        0}
};

WCHAR FAVES_DATA[] = L"\\Favorites.dat";

LinkDlg MapPropDlg(int root) {
    switch (root) {
    case FAVES_FOLDER:  return LinkDlg::FOLDER;
    case FAVES_FILE:    return LinkDlg::FILE;
    case FAVES_SESSION: return LinkDlg::FILE;
    default:            return LinkDlg::NONE;
    }
};

LPCWSTR GetNameStrFromCmd(UINT resourceId)
{
    LPCWSTR szToolTip[] = {
        L"Explorer",
        L"Link Current File...",
        L"Link Current Folder...",
        L"New Link...",
        L"Delete Link",
        L"Edit Link...",
    };

    if ((IDM_EX_EXPLORER <= resourceId) && (resourceId <= IDM_EX_LINK_EDIT)) {
        return szToolTip[resourceId - IDM_EX_EXPLORER];
    }
    return nullptr;
}

} // namespace



FavesDialog::FavesDialog()
    : DockingDlgInterface(IDD_EXPLORER_DLG)
    , _hDefaultTreeProc(nullptr)
    , _hImageList(nullptr)
    , _hImageListSys(nullptr)
    , _isCut(FALSE)
    , _hTreeCutCopy(nullptr)
    , _addToSession(FALSE)
    , _peOpenLink(nullptr)
    , _pExProp(nullptr)
{
}

FavesDialog::~FavesDialog()
{
}


void FavesDialog::init(HINSTANCE hInst, HWND hParent, ExProp *prop)
{
    _pExProp = prop;
    DockingDlgInterface::init(hInst, hParent);

    /* init database */
    ReadSettings();
}


void FavesDialog::doDialog(bool willBeShown)
{
    if (!isCreated()) {
        tTbData data{};
        create(&data);

        // define the default docking behaviour
        data.pszName        = L"Favorites";
        data.dlgID          = DOCKABLE_FAVORTIES_INDEX;
        data.uMask          = DWS_DF_CONT_LEFT | DWS_ICONTAB | DWS_USEOWNDARKMODE;
        data.hIconTab       = (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_HEART), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
        data.pszModuleName  = getPluginFileName();

        ::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);

        ThemeRenderer::Instance().Register(_hSelf);

        /* Update "Add current..." icons */
        NotifyNewFile();
        ExpandElementsRecursive(TVI_ROOT);
    }
    display(willBeShown);
}


void FavesDialog::SaveSession()
{
    AddSaveSession(nullptr, TRUE);
}


void FavesDialog::NotifyNewFile()
{
    if (isCreated() && isVisible()) {
        WCHAR TEMP[MAX_PATH] = {};

        /* update "new file link" icon */
        ::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
        _ToolBar.enable(IDM_EX_LINK_NEW_FILE, PathFileExists(TEMP));

        /* update "new folder link" icon */
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
        _ToolBar.enable(IDM_EX_LINK_NEW_FOLDER, (wcslen(TEMP) != 0));
    }
}


INT_PTR CALLBACK FavesDialog::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_INITDIALOG:
        /* get handle of dialogs */
        _hTreeCtrl.Attach(:: GetDlgItem(_hSelf, IDC_TREE_FOLDER));
        ::DestroyWindow(::GetDlgItem(_hSelf, IDC_LIST_FILE));
        ::DestroyWindow(::GetDlgItem(_hSelf, IDC_BUTTON_SPLITTER));
        ::DestroyWindow(::GetDlgItem(_hSelf, IDC_COMBO_FILTER));
        InitialDialog();
        break;
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;

        if (nmhdr->hwndFrom == _hTreeCtrl) {
            switch (nmhdr->code) {
            case NM_CUSTOMDRAW: {
                static HTHEME s_theme = nullptr;
                LPNMTVCUSTOMDRAW cd = (LPNMTVCUSTOMDRAW)lParam;
                switch (cd->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    s_theme = OpenThemeData(nmhdr->hwndFrom, L"TreeView");
                    SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, (LONG)CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT);
                    return TRUE;
                case CDDS_ITEMPREPAINT: {
                    HTREEITEM   hItem = reinterpret_cast<HTREEITEM>(cd->nmcd.dwItemSpec);

                    // background
                    auto maskedItemState = cd->nmcd.uItemState & (CDIS_SELECTED | CDIS_HOT);
                    int itemState = maskedItemState == (CDIS_SELECTED | CDIS_HOT) ? TREIS_HOTSELECTED
                        : maskedItemState == CDIS_SELECTED ? TREIS_SELECTED
                        : maskedItemState == CDIS_HOT ? TREIS_HOT
                        : TREIS_NORMAL;
                    if ((itemState == TREIS_SELECTED) && (nmhdr->hwndFrom != GetFocus())) {
                        itemState = TREIS_SELECTEDNOTFOCUS;
                    }
                    if (itemState != TREIS_NORMAL) {
                        DrawThemeBackground(s_theme, cd->nmcd.hdc, TVP_TREEITEM, itemState, &cd->nmcd.rc, &cd->nmcd.rc);
                    }

                    // [+]/[-] signs
                    RECT glyphRect{};
                    TVGETITEMPARTRECTINFO info{
                        .hti = hItem,
                        .prc = &glyphRect,
                        .partID = TVGIPR_BUTTON
                    };
                    if (TRUE == SendMessage(nmhdr->hwndFrom, TVM_GETITEMPARTRECT, 0, (LPARAM)&info)) {
                        BOOL isExpanded = (TreeView_GetItemState(nmhdr->hwndFrom, hItem, TVIS_EXPANDED) & TVIS_EXPANDED) ? TRUE : FALSE;
                        const int glyphStates = isExpanded ? GLPS_OPENED : GLPS_CLOSED;

                        SIZE glythSize;
                        GetThemePartSize(s_theme, cd->nmcd.hdc, TVP_GLYPH, glyphStates, nullptr, THEMESIZE::TS_DRAW, &glythSize);

                        glyphRect.top += ((glyphRect.bottom - glyphRect.top) - glythSize.cy) / 2;
                        glyphRect.bottom = glyphRect.top + glythSize.cy;
                        glyphRect.right = glyphRect.left + glythSize.cx;
                        DrawThemeBackground(s_theme, cd->nmcd.hdc, TVP_GLYPH, glyphStates, &glyphRect, nullptr);
                    }

                    // Text & Icon
                    RECT textRect{};
                    TreeView_GetItemRect(nmhdr->hwndFrom, hItem, &textRect, TRUE);
                    WCHAR textBuffer[MAX_PATH]{};
                    TVITEM tvi = {
                        .mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
                        .hItem = hItem,
                        .pszText = textBuffer,
                        .cchTextMax = MAX_PATH,
                    };
                    if (TRUE == TreeView_GetItem(nmhdr->hwndFrom, &tvi)) {
                        const auto *elem = reinterpret_cast<FavesItemPtr>(_hTreeCtrl.GetParam(hItem));
                        if (elem && (elem->Type() == FAVES_FILE) && elem->IsLink()) {
                            if (IsFileOpen(elem->Link()) == TRUE) {
                                ::SelectObject(cd->nmcd.hdc, _pExProp->underlineFont);
                            }
                        }
                        SetBkMode(cd->nmcd.hdc, TRANSPARENT);

                        COLORREF textColor = TreeView_GetTextColor(nmhdr->hwndFrom);
                        SetTextColor(cd->nmcd.hdc, textColor);
                        ::DrawText(cd->nmcd.hdc, tvi.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
                        ::SelectObject(cd->nmcd.hdc, _pExProp->defaultFont);

                        const SIZE iconSize = {
                            .cx = GetSystemMetrics(SM_CXSMICON),
                            .cy = GetSystemMetrics(SM_CYSMICON),
                        };
                        const INT top = (textRect.top + textRect.bottom - iconSize.cy) / 2;
                        const INT left = textRect.left - iconSize.cx - GetSystemMetrics(SM_CXEDGE);
                        if ((_pExProp->bUseSystemIcons == FALSE) || (elem && (elem->IsGroup() || (elem->Type() == FAVES_WEB) || (elem->uParam & FAVES_PARAM_USERIMAGE)))) {
                            ImageList_DrawEx(_hImageList, tvi.iImage, cd->nmcd.hdc, left, top, iconSize.cx, iconSize.cy, CLR_NONE, CLR_NONE, ILD_TRANSPARENT | ILD_SCALE);
                        }
                        else {
                            ImageList_Draw(_hImageListSys, tvi.iImage, cd->nmcd.hdc, left, top, ILD_TRANSPARENT);
                        }
                    }
                    SetWindowLongPtr(_hSelf, DWLP_MSGRESULT, (LONG)CDRF_SKIPDEFAULT);
                    return TRUE;
                }
                case CDDS_POSTPAINT:
                    CloseThemeData(s_theme);
                    s_theme = nullptr;
                    break;
                default:
                    break;
                }
                break;
            }
            case NM_RCLICK: {
                DWORD dwpos = ::GetMessagePos();
                POINT pt = {
                    .x = GET_X_LPARAM(dwpos),
                    .y = GET_Y_LPARAM(dwpos),
                };
                TVHITTESTINFO ht = {
                    .pt = pt
                };
                ::ScreenToClient(_hTreeCtrl, &ht.pt);
                HTREEITEM hItem = _hTreeCtrl.HitTest(&ht);
                if (hItem != nullptr) {
                    OpenContext(hItem, pt);
                }
                break;
            }
            case TVN_ITEMEXPANDING: {
                LPNMTREEVIEW pnmtv = reinterpret_cast<LPNMTREEVIEW>(lParam);
                HTREEITEM hItem = pnmtv->itemNew.hItem;

                if (hItem != nullptr) {
                    // get element information
                    FavesItemPtr pElem = reinterpret_cast<FavesItemPtr>(pnmtv->itemNew.lParam);
                    if (pElem == nullptr) {
                        break;
                    }
                    // update expand state
                    pElem->IsExpanded(!pElem->IsExpanded());

                    // reload session's children
                    if ((pElem->Type() == FAVES_SESSION) && pElem->IsLink()) {
                        _hTreeCtrl.DeleteChildren(hItem);
                        DrawSessionChildren(hItem);
                    }

                    if (!_hTreeCtrl.ItemHasChildren(hItem) && pElem->IsGroup()) {
                        UpdateLink(hItem);
                    }
                }
                break;
            }
            case TVN_SELCHANGED: {
                HTREEITEM hItem = _hTreeCtrl.GetSelection();

                if (hItem != nullptr) {
                    FavesItemPtr pElem = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);

                    if (pElem != nullptr) {
                        _ToolBar.enable(IDM_EX_LINK_NEW,    pElem->IsGroup());
                        _ToolBar.enable(IDM_EX_LINK_EDIT,   !pElem->IsRoot());
                        _ToolBar.enable(IDM_EX_LINK_DELETE, !pElem->IsRoot());
                        NotifyNewFile();
                    }
                    else {
                        _ToolBar.enable(IDM_EX_LINK_NEW, false);
                        _ToolBar.enable(IDM_EX_LINK_EDIT, false);
                        _ToolBar.enable(IDM_EX_LINK_DELETE, false);
                        NotifyNewFile();
                    }
                }
                break;
            }
            case TVN_GETINFOTIP: {
                LPNMTVGETINFOTIP pTip = reinterpret_cast<LPNMTVGETINFOTIP>(lParam);
                HTREEITEM item = pTip->hItem;

                FavesItemPtr pElem = reinterpret_cast<FavesItemPtr>(_hTreeCtrl.GetParam(item));
                if (pElem) {
                    // show full file path
                    std::wstring tipText;
                    tipText += pElem->Link();
                    if ((pElem->Type() == FAVES_SESSION) && pElem->IsLink()) {
                        INT count = _hTreeCtrl.GetChildrenCount(item);
                        if (count > 0) {
                            // Check non-existent files
                            auto sessionFiles = NppInterface::getSessionFiles(pElem->Link());
                            int nonExistentFileCount = 0;
                            for (auto &&file : sessionFiles) {
                                if (!::PathFileExists(file.c_str())) {
                                    ++nonExistentFileCount;
                                }
                            }

                            // make tooltip text.
                            tipText += std::format(L"\nThis session has {} files", count);
                            if (nonExistentFileCount > 0) {
                                tipText += std::format(L" ({} are non-existent)", nonExistentFileCount);
                            }
                            tipText += L".";
                        }
                    }
                    if (!tipText.empty()) {
                        wcscpy_s(pTip->pszText, pTip->cchTextMax, tipText.c_str());
                        return TRUE;
                    }
                    return FALSE; // show default tooltip text
                }
                break;
            }
            default:
                break;
            }
        }
        else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED)) {
            NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
            if (lpnm->wID == REBAR_BAR_TOOLBAR) {
                POINT pt = {
                    .x = lpnm->rc.left,
                    .y = lpnm->rc.bottom,
                };
                ClientToScreen(nmhdr->hwndFrom, &pt);
                tb_cmd(_ToolBar.doPopop(pt));
                return TRUE;
            }
            break;
        }
        else if (nmhdr->code == TTN_GETDISPINFO) {
            LPTOOLTIPTEXT lpttt = reinterpret_cast<LPTOOLTIPTEXT>(nmhdr);
            lpttt->hinst = _hInst;

            // Specify the resource identifier of the descriptive
            // text for the given button.
            int resourceId = int(lpttt->hdr.idFrom);
            lpttt->lpszText = const_cast<LPWSTR>(GetNameStrFromCmd(resourceId));
            return TRUE;
        }

        DockingDlgInterface::run_dlgProc(Message, wParam, lParam);

        return FALSE;
    }
    case WM_SIZE:
    case WM_MOVE: {
        RECT rc = {};
        /* set position of toolbar */
        getClientRect(rc);
        _ToolBar.reSizeTo(rc);
        _Rebar.reSizeTo(rc);

        auto toolBarHeight = _ToolBar.getHeight();

        /* set position of tree control */
        rc.top    += toolBarHeight;
        rc.bottom -= toolBarHeight;
        ::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

        break;
    }
    case WM_COMMAND:
        // ESC key has been pressed
        if (LOWORD(wParam) == IDCANCEL) {
            NppInterface::setFocusToCurrentEdit();
            return TRUE;
        }

        if ((HWND)lParam == _ToolBar.getHSelf()) {
            tb_cmd(LOWORD(wParam));
        }
        break;
    case WM_PAINT:
        ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
        break;
    case WM_DESTROY:
        SaveSettings();
        _model.Clear();
        _ToolBar.destroy();
        break;
    case EXM_OPENLINK:
        OpenLink(_peOpenLink);
        break;
    default:
        DockingDlgInterface::run_dlgProc(Message, wParam, lParam);
        break;
    }

    return FALSE;
}

LRESULT FavesDialog::runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_GETDLGCODE:
        switch (wParam) {
        case VK_RETURN:
            return DLGC_WANTALLKEYS;
        default:
            break;
        }
        break;
    case WM_KEYDOWN:
        if (VK_ESCAPE == wParam) {
            NppInterface::setFocusToCurrentEdit();
            return TRUE;
        }
        if (wParam == VK_RETURN) {
            HTREEITEM hItem = _hTreeCtrl.GetSelection();
            if (OpenTreeViewItem(hItem)) {
                return TRUE;
            }
        }
        if (wParam == VK_DELETE) {
            HTREEITEM hItem = _hTreeCtrl.GetSelection();
            DeleteItem(hItem);
        }
        break;
    case WM_LBUTTONDBLCLK: {
        TVHITTESTINFO hti = {
            .pt = {
                .x = GET_X_LPARAM(lParam),
                .y = GET_Y_LPARAM(lParam),
            }
        };

        HTREEITEM hItem = _hTreeCtrl.HitTest(&hti);
        if ((hti.flags & TVHT_ONITEM) && OpenTreeViewItem(hItem)) {
            return TRUE;
        }
        break;
    }
    default:
        break;
    }

    return ::DefSubclassProc(hwnd, Message, wParam, lParam);
}

BOOL FavesDialog::OpenTreeViewItem(HTREEITEM hItem)
{
    if (hItem) {
        FavesItemPtr pElem = reinterpret_cast<FavesItemPtr>(_hTreeCtrl.GetParam(hItem));
        if (pElem) {
            if (pElem->IsLink()) {
                _peOpenLink = pElem;
                ::PostMessage(_hSelf, EXM_OPENLINK, 0, 0);
                return TRUE;
            }
            return FALSE;
        }
    }
    return FALSE;
}

void FavesDialog::tb_cmd(UINT message)
{
    switch (message) {
    case IDM_EX_EXPLORER:
        toggleExplorerDialog();
        break;
    case IDM_EX_LINK_NEW_FILE: {
        WCHAR TEMP[MAX_PATH] = {};
        ::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
        if (PathFileExists(TEMP)) {
            AddToFavorties(FALSE, TEMP);
        }
        break;
    }
    case IDM_EX_LINK_NEW_FOLDER: {
        WCHAR TEMP[MAX_PATH] = {};
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
        if (wcslen(TEMP) != 0) {
            AddToFavorties(TRUE, TEMP);
        }
        break;
    }
    case IDM_EX_LINK_NEW: {
        HTREEITEM hItem = _hTreeCtrl.GetSelection();
        FavesType type  = ((FavesItemPtr)_hTreeCtrl.GetParam(hItem))->Type();
        if (type == FAVES_SESSION) {
            AddSaveSession(hItem, FALSE);
        }
        else {
            NewItem(hItem);
        }
        break;
    }
    case IDM_EX_LINK_EDIT:
        EditItem(_hTreeCtrl.GetSelection());
        break;
    case IDM_EX_LINK_DELETE:
        DeleteItem(_hTreeCtrl.GetSelection());
        break;
    default:
        break;
    }
}

void FavesDialog::InitialDialog()
{
    /* subclass tree */
    ::SetWindowSubclass(_hTreeCtrl, wndDefaultTreeProc, 'tree', reinterpret_cast<DWORD_PTR>(this));

    /* Load Image List */
    _hImageListSys = GetSmallImageList(TRUE);
    _hImageList    = GetSmallImageList(FALSE);

    /* set image list */
    ::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)_hImageListSys);

    // set font
    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)_pExProp->defaultFont, TRUE);

    /* create toolbar */
    _ToolBar.init(_hInst, _hSelf, TB_STANDARD, toolBarIcons, sizeof(toolBarIcons)/sizeof(ToolBarButtonUnit));
    _Rebar.init(_hInst, _hSelf);
    _ToolBar.addToRebar(&_Rebar);
    _Rebar.setIDVisible(REBAR_BAR_TOOLBAR, true);

    /* add new items in list and make reference to items */
    SendMessage(_hTreeCtrl, WM_SETREDRAW, FALSE, 0);
    UpdateLink(_hTreeCtrl.InsertItem(_model.FolderRoot()->Name(),  ICON_FOLDER,  ICON_FOLDER,  0, 0, TVI_ROOT, TVI_LAST, _model.FolderRoot()->HasChildren(),   _model.FolderRoot()));
    UpdateLink(_hTreeCtrl.InsertItem(_model.FileRoot()->Name(),    ICON_FILE,    ICON_FILE,    0, 0, TVI_ROOT, TVI_LAST, _model.FileRoot()->HasChildren(),     _model.FileRoot()));
    UpdateLink(_hTreeCtrl.InsertItem(_model.WebRoot()->Name(),     ICON_WEB,     ICON_WEB,     0, 0, TVI_ROOT, TVI_LAST, _model.WebRoot()->HasChildren(),      _model.WebRoot()));
    UpdateLink(_hTreeCtrl.InsertItem(_model.SessionRoot()->Name(), ICON_SESSION, ICON_SESSION, 0, 0, TVI_ROOT, TVI_LAST, _model.SessionRoot()->HasChildren(),  _model.SessionRoot()));
    SendMessage(_hTreeCtrl, WM_SETREDRAW, TRUE, 0);
}

void FavesDialog::SetFont(HFONT font)
{
    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

void FavesDialog::CopyItem(HTREEITEM hItem)
{
    _isCut          = FALSE;
    _hTreeCutCopy   = hItem;
}

void FavesDialog::CutItem(HTREEITEM hItem)
{
    _isCut          = TRUE;
    _hTreeCutCopy   = hItem;
}

void FavesDialog::PasteItem(HTREEITEM hItem)
{
    FavesItemPtr destination = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);
    FavesItemPtr source      = (FavesItemPtr)_hTreeCtrl.GetParam(_hTreeCutCopy);

    if (!source) {
        return;
    }

    if (destination == source) {
        _hTreeCutCopy = nullptr;
        return;
    }

    if (source->IsNodeDescendant(destination)) {
        _hTreeCutCopy = nullptr;
        return;
    }

    if (destination->Type() == source->Type()) {
        auto newItem = std::make_unique<FavesItem>(destination, source->Type(), source->Name(), source->Link());
        newItem->uParam = source->uParam;
        newItem->CopyChildren(source);
        destination->AddChild(std::move(newItem));

        if (_isCut == TRUE) {
            auto *parent = source->m_parent;
            source->Remove();

            auto *parentTreeItem = _hTreeCtrl.GetParent(_hTreeCutCopy);
            UpdateLink(parentTreeItem);
            _hTreeCtrl.SetItemHasChildren(parentTreeItem, parent->HasChildren());
            ExpandElementsRecursive(parentTreeItem);
        }

        /* update information */
        UpdateLink(hItem);
        _hTreeCtrl.SetItemHasChildren(hItem, TRUE);
        ExpandElementsRecursive(hItem);
        if (destination->IsExpanded()) {
            _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }

        _hTreeCutCopy = nullptr;
    }
    else {
        WCHAR msgBoxTxt[128];
        _stprintf(msgBoxTxt, L"Could only be paste into %ls", source->Root()->Name().c_str());
        ::MessageBox(_hParent, msgBoxTxt, L"Error", MB_OK);
    }
}

void FavesDialog::RefreshTree(HTREEITEM item)
{
    if (item) {
        /* update information */
        HTREEITEM hParentItem = _hTreeCtrl.GetParent(item);
        if (hParentItem != nullptr) {
            UpdateLink(hParentItem);
        }
        UpdateLink(item);
        // expand item
        _hTreeCtrl.Expand(item, TVM_EXPAND | TVE_COLLAPSERESET);
    }
}

void FavesDialog::AddToFavorties(BOOL isFolder, LPTSTR szLink)
{
    PropDlg     dlgProp;
    FavesType   type    = (isFolder ? FAVES_FOLDER : FAVES_FILE);
    LPTSTR      pszName = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR      pszLink = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR      pszDesc = (LPTSTR)new WCHAR[MAX_PATH];

    /* fill out params */
    pszName[0] = '\0';
    wcscpy(pszLink, szLink);

    /* create description */
    _stprintf(pszDesc, L"New element in % s", isFolder ? _model.FolderRoot()->Name().c_str()
                                                       : _model.FileRoot()->Name().c_str());

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);

    /* select root element */
    dlgProp.setRoot((isFolder ? _model.FolderRoot() : _model.FileRoot()),
                    (isFolder ? ICON_FOLDER         : ICON_FILE));

    /* open dialog */
    if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type)) == TRUE) {
        auto *group = dlgProp.getSelectedGroup();
        auto newItem = std::make_unique<FavesItem>(group, type, pszName, pszLink);
        group->AddChild(std::move(newItem));

        auto *item = _hTreeCtrl.FindTreeItemByParam(group);
        RefreshTree(item);
    }

    delete [] pszName;
    delete [] pszLink;
    delete [] pszDesc;
}

void FavesDialog::AddToFavorties(BOOL isFolder, std::vector<std::wstring>&& paths)
{
    PropDlg     dlgProp;
    FavesType   type = (isFolder ? FAVES_FOLDER : FAVES_FILE);

    std::wstring name;
    for (auto&& path : paths) {
        if (path.back() == '\\') {
            path.pop_back();
        }
        name += PathFindFileName(path.c_str());
        name += L", ";
    }
    std::wstring desctiption = std::wstring(L"New element in ") + (isFolder ? _model.FolderRoot()->Name() : _model.FileRoot()->Name());

    dlgProp.init(_hInst, _hParent);
    dlgProp.setRoot((isFolder ? _model.FolderRoot() : _model.FileRoot()),
                    (isFolder ? ICON_FOLDER         : ICON_FILE));
    if (dlgProp.doDialog(name.data(), nullptr, desctiption.data(), MapPropDlg(type)) == TRUE) {
        /* get selected item */
        auto *group = dlgProp.getSelectedGroup();

        if (group != nullptr) {
            for (auto&& path : paths) {
                auto newItem = std::make_unique<FavesItem>(group, type, PathFindFileName(path.c_str()), path);
                group->AddChild(std::move(newItem));
            }

            auto *item = _hTreeCtrl.FindTreeItemByParam(group);
            RefreshTree(item);
        }
    }
}


void FavesDialog::AddSaveSession(HTREEITEM hItem, BOOL bSave)
{
    PropDlg         dlgProp;
    HTREEITEM       hParentItem = nullptr;
    FavesItemPtr    pElem       = nullptr;
    FavesType       type        = FAVES_SESSION;
    LPTSTR          pszName     = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR          pszLink     = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR          pszDesc     = (LPTSTR)new WCHAR[MAX_PATH];

    /* fill out params */
    pszName[0] = '\0';
    pszLink[0] = '\0';

    if (bSave == TRUE) {
        wcscpy(pszDesc, L"Save current Session");
    } else {
        wcscpy(pszDesc, L"Add existing Session");
    }

    /* if hItem is empty, extended dialog is necessary */
    if (hItem == nullptr) {
        /* this is called when notepad menu triggers this function */
        dlgProp.setRoot(_model.SessionRoot(), ICON_SESSION, TRUE);
    }
    else {
        /* get group or session information */
        pElem = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);
    }

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);

    /* open dialog */
    if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type), bSave) == TRUE) {
        /* this is called when notepad menu triggers this function */
        if (hItem == nullptr) {
            /* get group name */
            pElem = dlgProp.getSelectedGroup();
            hParentItem = _hTreeCtrl.FindTreeItemByParam(pElem);

            if (pElem->IsLink()) {
                hItem = _hTreeCtrl.FindTreeItemByParam(pElem);
                hParentItem = _hTreeCtrl.GetParent(hItem);
            }
        }

        /* if the parent element is LINK element -> replace informations */
        if (pElem->IsLink()) {
            pElem->m_name = pszName;
            pElem->m_link = pszLink;
        }
        else {
            /* push information back */
            auto newItem = std::make_unique<FavesItem>(pElem, FAVES_SESSION, pszName, pszLink);
            pElem->AddChild(std::move(newItem));
        }

        /* save current session when expected */
        if (bSave == TRUE) {
            ::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pszLink);
        }

        /* special case for notepad menu trigger */
        if ((hParentItem == nullptr) && (hItem != nullptr)) {
            /* update the session items */
            UpdateLink(hItem);
            _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }

        if ((hParentItem != nullptr) && (hItem == nullptr)) {
            /* update the session items */
            UpdateLink(hParentItem);
            _hTreeCtrl.Expand(hParentItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }
    }

    delete [] pszName;
    delete [] pszLink;
    delete [] pszDesc;
}

void FavesDialog::NewItem(HTREEITEM hItem)
{
    PropDlg         dlgProp;
    FavesItemPtr    pElem   = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);
    FavesType       type    = pElem->Type();
    BOOL            isOk    = FALSE;
    LPTSTR          pszName = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR          pszLink = (LPTSTR)new WCHAR[MAX_PATH];
    LPTSTR          pszDesc = (LPTSTR)new WCHAR[MAX_PATH];

    /* init link and name */
    pszName[0] = '\0';
    pszLink[0] = '\0';

    /* set description text */
    _stprintf(pszDesc, L"New element in % s", pElem->Root()->Name().c_str());

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);
    while (isOk == FALSE) {
        /* open dialog */
        if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type)) == TRUE) {
            isOk = DoesLinkExist(pszLink, type);
            if (isOk == TRUE) {
                auto newItem = std::make_unique<FavesItem>(pElem, type, pszName, pszLink);
                pElem->AddChild(std::move(newItem));
            }
        }
        else {
            break;
        }
    }

    if (isOk == TRUE) {
        /* update information */
        if (pElem->IsGroup()) {
            UpdateLink(_hTreeCtrl.GetParent(hItem));
        }
        UpdateLink(hItem);

        _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
    }

    delete [] pszName;
    delete [] pszLink;
    delete [] pszDesc;
}

void FavesDialog::EditItem(HTREEITEM hItem)
{
    HTREEITEM       hParentItem = _hTreeCtrl.GetParent(hItem);
    FavesItemPtr    pElem       = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);

    if (!pElem->IsRoot()) {
        FavesType   type        = pElem->Type();
        BOOL        needsUpdate = FALSE;
        LPTSTR      pszName     = (LPTSTR)new WCHAR[MAX_PATH];
        LPTSTR      pszLink     = (LPTSTR)new WCHAR[MAX_PATH];
        LPTSTR      pszDesc     = (LPTSTR)new WCHAR[MAX_PATH];
        LPTSTR      pszComm     = (LPTSTR)new WCHAR[MAX_PATH];

        if (pElem->IsGroup()) {
            /* get data of current selected element */
            wcscpy(pszName, pElem->m_name.c_str());
            /* rename comment */
            wcscpy(pszDesc, L"Properties");
            wcscpy(pszComm, L"Favorites");

            /* init new dialog */
            NewDlg dlgNew;
            dlgNew.init(_hInst, _hParent, pszComm);

            /* open dialog */
            if (dlgNew.doDialog(pszName, pszDesc) == TRUE) {
                pElem->m_name = pszName;
                needsUpdate = TRUE;
            }
        }
        else if (pElem->IsLink()) {
            /* get data of current selected element */
            wcscpy(pszName, pElem->m_name.c_str());
            wcscpy(pszLink, pElem->m_link.c_str());
            wcscpy(pszDesc, L"Properties");

            PropDlg dlgProp;
            dlgProp.init(_hInst, _hParent);
            dlgProp.setRoot(pElem->Root(), ICON_FILE);
            dlgProp.setSelectedGroup(pElem->m_parent);
            if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type)) == TRUE) {
                auto *group = dlgProp.getSelectedGroup();
                auto *selectedGroup = _hTreeCtrl.FindTreeItemByParam(group);
                if (hParentItem != selectedGroup) {
                    pElem->Remove();
                    auto newItem = std::make_unique<FavesItem>(group, type, pszName, pszLink);
                    group->AddChild(std::move(newItem));
                    RefreshTree(selectedGroup);
                }
                else {
                    pElem->m_name = pszName;
                    pElem->m_link = pszLink;
                }
                needsUpdate = TRUE;
            }
        }

        /* update text of item */
        if (needsUpdate == TRUE) {
            UpdateLink(hParentItem);
        }

        delete [] pszName;
        delete [] pszLink;
        delete [] pszDesc;
        delete [] pszComm;
    }
}

void FavesDialog::DeleteItem(HTREEITEM hItem)
{
    HTREEITEM       hItemParent = _hTreeCtrl.GetParent(hItem);
    FavesItemPtr    pElem       = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);

    if (!pElem) {
        return;
    }

    if (pElem->IsRoot()) {
        return;
    }

    if ((pElem->Root()->Type() == FAVES_SESSION) && (pElem->Type() == FAVES_FILE)) {
        return;
    }

    pElem->Remove();
    _hTreeCtrl.DeleteItem(hItem);

    /* update only parent of parent when current item is a group folder */
    if (((FavesItemPtr)_hTreeCtrl.GetParam(hItemParent))->IsGroup()) {
        UpdateLink(_hTreeCtrl.GetParent(hItemParent));
    }
    UpdateLink(hItemParent);
}

void FavesDialog::OpenContext(HTREEITEM hItem, POINT pt)
{
    FavesItemPtr pElem = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);

    /* get element and level depth */
    if (pElem != nullptr) {
        FavesType type = pElem->Type();

        if (pElem->IsGroup()) {
            /* create menu and attach one element */
            HMENU hMenu = ::CreatePopupMenu();

            if (type != FAVES_SESSION) {
                ::AppendMenu(hMenu, MF_STRING, FM_NEWLINK, L"New Link...");
                ::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, L"New Group...");
            }
            else {
                ::AppendMenu(hMenu, MF_STRING, FM_ADDSESSION, L"Add existing Session...");
                ::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, L"Save Current Session...");
                ::AppendMenu(hMenu, MF_STRING, FM_NEWGROUP, L"New Group...");
            }

            if (!pElem->IsRoot() && pElem->IsGroup()) {
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_COPY, L"Copy");
                ::AppendMenu(hMenu, MF_STRING, FM_CUT, L"Cut");
                if (_hTreeCutCopy != nullptr) {
                    ::AppendMenu(hMenu, MF_STRING, FM_PASTE, L"Paste");
                }
                ::AppendMenu(hMenu, MF_STRING, FM_DELETE, L"Delete");
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, L"Properties...");
            }
            else if (pElem->IsRoot() && (_hTreeCutCopy != nullptr)) {
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_PASTE, L"Paste");
            }

            /* track menu */
            switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr)) {
            case FM_NEWLINK:
                NewItem(hItem);
                break;
            case FM_ADDSESSION:
                AddSaveSession(hItem, FALSE);
                break;
            case FM_SAVESESSION:
                AddSaveSession(hItem, TRUE);
                break;
            case FM_NEWGROUP: {
                LPTSTR pszName = (LPTSTR)new WCHAR[MAX_PATH];
                LPTSTR pszDesc = (LPTSTR)new WCHAR[MAX_PATH];

                pszName[0] = '\0';

                _stprintf(pszDesc, L"New group in %ls", pElem->Root()->Name().c_str());

                /* init new dialog */
                NewDlg dlgNew;
                dlgNew.init(_hInst, _hParent, L"Favorites");

                /* open dialog */
                if (dlgNew.doDialog(pszName, pszDesc) == TRUE) {
                    auto newItem = std::make_unique<FavesItem>(pElem, type, pszName);
                    pElem->AddChild(std::move(newItem));

                    /* update information */
                    if (pElem->IsGroup()) {
                        UpdateLink(_hTreeCtrl.GetParent(hItem));
                    }
                    UpdateLink(hItem);
                    _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
                }

                delete [] pszName;
                delete [] pszDesc;
                break;
            }
            case FM_COPY:
                CopyItem(hItem);
                break;
            case FM_CUT:
                CutItem(hItem);
                break;
            case FM_PASTE:
                PasteItem(hItem);
                break;
            case FM_DELETE:
                DeleteItem(hItem);
                break;
            case FM_PROPERTIES:
                EditItem(hItem);
                break;
            }

            /* free resources */
            ::DestroyMenu(hMenu);
        }
        else if (pElem->IsLink()) {
            /* create menu and attach one element */
            HMENU hMenu = ::CreatePopupMenu();

            ::AppendMenu(hMenu, MF_STRING, FM_OPEN, L"Open");

            if (type == FAVES_FILE) {
                ::AppendMenu(hMenu, MF_STRING, FM_OPENOTHERVIEW, L"Open in Other View");
                ::AppendMenu(hMenu, MF_STRING, FM_OPENNEWINSTANCE, L"Open in New Instance");
                ::AppendMenu(hMenu, MF_STRING, FM_GOTO_FILE_LOCATION, L"Go to File Location");
            }
            else if (type == FAVES_SESSION) {
                ::AppendMenu(hMenu, MF_STRING, FM_ADDTOSESSION, L"Add to Current Session");
                ::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, L"Save Current Session");
            }

            if ((type != FAVES_FILE) || (pElem->m_parent->Type() != FAVES_SESSION)) {
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_COPY, L"Copy");
                ::AppendMenu(hMenu, MF_STRING, FM_CUT, L"Cut");

                ::AppendMenu(hMenu, MF_STRING, FM_DELETE, L"Delete");
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, L"Properties...");
            }

            /* track menu */
            switch (::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr)) {
            case FM_OPEN:
                OpenLink(pElem);
                break;
            case FM_OPENOTHERVIEW:
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->Link().c_str());
                ::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_GOTO_ANOTHER_VIEW, 0);
                break;
            case FM_OPENNEWINSTANCE:
            {
                LPTSTR pszNpp = (LPTSTR)new WCHAR[MAX_PATH];
                // get notepad++.exe path
                ::GetModuleFileName(nullptr, pszNpp, MAX_PATH);

                std::wstring params = L"-multiInst " + pElem->Link();
                ::ShellExecute(_hParent, L"open", pszNpp, params.c_str(), L".", SW_SHOW);

                delete [] pszNpp;
                break;
            }
            case FM_GOTO_FILE_LOCATION: {
                extern ExplorerDialog explorerDlg;

                explorerDlg.gotoFileLocation(pElem->Link());
                explorerDlg.doDialog();
                break;
            }
            case FM_ADDTOSESSION:
                _addToSession = TRUE;
                OpenLink(pElem);
                _addToSession = FALSE;
                break;
            case FM_SAVESESSION:
                ::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pElem->Link().c_str());
                _hTreeCtrl.DeleteChildren(hItem);
                DrawSessionChildren(hItem);
                break;
            case FM_COPY:
                CopyItem(hItem);
                break;
            case FM_CUT:
                CutItem(hItem);
                break;
            case FM_PASTE:
                PasteItem(hItem);
                break;
            case FM_DELETE:
                DeleteItem(hItem);
                break;
            case FM_PROPERTIES:
                EditItem(hItem);
                break;
            default:
                break;
            }
            /* free resources */
            ::DestroyMenu(hMenu);
        }
        else
        {
            ::MessageBox(_hParent, L"Element not found in List!", L"Error", MB_OK);
        }
    }
}


void FavesDialog::UpdateLink(HTREEITEM hParentItem)
{
    HTREEITEM       hCurrentItem    = _hTreeCtrl.GetNextItem(hParentItem, TVGN_CHILD);
    FavesItemPtr    parentElement   = (FavesItemPtr)_hTreeCtrl.GetParam(hParentItem);

    if (parentElement != nullptr) {
        parentElement->SortChildren();

        /* update elements in parent tree */
        for (auto&& child : parentElement->m_children) {
            BOOL haveChildren   = FALSE;
            INT iIconNormal     = 0;
            INT iIconSelected   = 0;
            INT iIconOverlayed  = 0;

            if (child->IsGroup()) {
                iIconNormal     = ICON_GROUP;
                iIconOverlayed  = 0;
                haveChildren    = child->HasChildren();
            }
            else {
                /* get icons */
                switch (child->Type()) {
                case FAVES_FOLDER:
                    /* get icons and update item */
                    ExtractIcons(child->Link().c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected, &iIconOverlayed);
                    break;
                case FAVES_FILE:
                    /* get icons and update item */
                    ExtractIcons(child->Link().c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
                    break;
                case FAVES_SESSION:
                    haveChildren    = (0 != ::SendMessage(_hParent, NPPM_GETNBSESSIONFILES, 0, (LPARAM)child->Link().c_str()));
                    iIconNormal     = ICON_SESSION;
                    break;
                case FAVES_WEB:
                    iIconNormal     = ICON_WEB;
                    break;
                default:
                    break;
                }
            }
            iIconSelected = iIconNormal;

            /* update or add new item */
            if (hCurrentItem != nullptr) {
                _hTreeCtrl.UpdateItem(hCurrentItem, child->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren, child.get());
            }
            else {
                hCurrentItem = _hTreeCtrl.InsertItem(child->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, hParentItem, TVI_LAST, haveChildren, child.get());
            }

            /* control item expand state and correct if necessary */
            BOOL isTreeExp = _hTreeCtrl.IsItemExpanded(hCurrentItem);

            /* toggle if state is not equal */
            if (isTreeExp != child->IsExpanded()) {
                child->IsExpanded(isTreeExp);
                _hTreeCtrl.Expand(hCurrentItem, TVE_TOGGLE);
            }

            /* in any case redraw the session children items */
            if (child->Type() == FAVES_SESSION) {
                _hTreeCtrl.DeleteChildren(hCurrentItem);
                DrawSessionChildren(hCurrentItem);
            }

            hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
        }

        // Update current node
        _hTreeCtrl.SetItemHasChildren(hParentItem, parentElement->HasChildren());

        /* delete possible not existed items */
        while (hCurrentItem != nullptr) {
            HTREEITEM   pPrevItem   = hCurrentItem;
            hCurrentItem            = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
            _hTreeCtrl.DeleteItem(pPrevItem);
        }
    }
}

void FavesDialog::DrawSessionChildren(HTREEITEM hItem)
{
    FavesItemPtr session = (FavesItemPtr)_hTreeCtrl.GetParam(hItem);
    if (session->IsGroup()) {
        return;
    }
    session->m_children.clear();

    BOOL hasMissingFile = FALSE;
    auto sessionFiles = NppInterface::getSessionFiles(session->Link());
    for (const auto &path : sessionFiles) {
        auto newItem = std::make_unique<FavesItem>(session, FAVES_FILE, path.substr(path.find_last_of(L'\\') + 1), path);
        INT iIconNormal = 0;
        INT iIconSelected = 0;
        INT iIconOverlayed = 0;
        if (::PathFileExists(newItem->Link().c_str())) {
            ExtractIcons(newItem->Link().c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
        }
        else {
            newItem->uParam |= FAVES_PARAM_USERIMAGE;
            iIconNormal = ICON_MISSING_FILE;
            iIconSelected = iIconNormal;
            hasMissingFile = TRUE;
        }
        _hTreeCtrl.InsertItem(newItem->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, hItem, TVI_LAST, FALSE, newItem.get());
        session->AddChild(std::move(newItem));
    }

    if (hasMissingFile) {
        session->uParam |= FAVES_PARAM_USERIMAGE;
        _hTreeCtrl.SetItemIcons(hItem, ICON_WARN_SESSION, ICON_WARN_SESSION, 0);
    }
    else {
        session->uParam |= FAVES_PARAM_USERIMAGE;
        _hTreeCtrl.SetItemIcons(hItem, ICON_SESSION, ICON_SESSION, 0);
    }
}

BOOL FavesDialog::DoesLinkExist(LPTSTR link, FavesType type)
{
    BOOL bRet = FALSE;

    switch (type) {
    case FAVES_FOLDER:
        /* test if path exists */
        bRet = ::PathFileExists(link);
        if (bRet == FALSE) {
            ::MessageBox(_hParent, L"Folder doesn't exist!", L"Error", MB_OK);
        }
        break;
    case FAVES_FILE:
    case FAVES_SESSION:
        /* test if path exists */
        bRet = ::PathFileExists(link);
        if (bRet == FALSE) {
            ::MessageBox(_hParent, L"File doesn't exist!", L"Error", MB_OK);
        }
        break;
    case FAVES_WEB:
        bRet = TRUE;
        break;
    default:
        ::MessageBox(_hParent, L"Faves element doesn't exist!", L"Error", MB_OK);
        break;
    }

    return bRet;
}


void FavesDialog::OpenLink(FavesItemPtr pElem)
{
    if (pElem->IsLink()) {
        switch (pElem->Type()) {
        case FAVES_FOLDER: {
            extern ExplorerDialog explorerDlg;

            /* two-step to avoid flickering */
            if (!explorerDlg.isCreated()) {
                explorerDlg.doDialog();
            }

            explorerDlg.NavigateTo(pElem->Link());

            /* two-step to avoid flickering */
            if (explorerDlg.isVisible() == FALSE) {
                explorerDlg.doDialog();
            }

            ::SendMessage(_hParent, NPPM_DMMVIEWOTHERTAB, 0, (LPARAM)"Explorer");
            ::SetFocus(explorerDlg.getHSelf());
            break;
        }
        case FAVES_FILE: {
            /* open possible link */
            WCHAR pszFilePath[MAX_PATH];
            if (ResolveShortCut(pElem->Link(), pszFilePath, MAX_PATH) == S_OK) {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pszFilePath);
            } else {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->Link().c_str());
            }
            break;
        }
        case FAVES_WEB:
            ::ShellExecute(_hParent, L"open", pElem->Link().c_str(), nullptr, nullptr, SW_SHOW);
            break;
        case FAVES_SESSION: {
            // Check non-existent files
            auto sessionFiles = NppInterface::getSessionFiles(pElem->Link());
            int nonExistentFileCount = 0;
            for (auto&& file : sessionFiles) {
                if (!::PathFileExists(file.c_str())) {
                    ++nonExistentFileCount;
                }
            }
            if (0 < nonExistentFileCount) {
                const std::wstring msg = std::format(L"This session has {} non-existent files. "
                                                     L"Processing will delete all non-existent files in the session. Are you sure you want to continue?",
                                                     nonExistentFileCount);
                if (IDCANCEL == ::MessageBox(_hSelf, msg.c_str(), L"Open Session", MB_OKCANCEL | MB_ICONWARNING)) {
                    return;
                }
            }

            /* in normal case close files previously */
            if (_addToSession == FALSE) {
                ::SendMessage(_hParent, WM_COMMAND, IDM_FILE_CLOSEALL, 0);
                _addToSession = FALSE;
            }
            ::SendMessage(_hParent, NPPM_LOADSESSION, 0, (LPARAM)pElem->Link().c_str());
            break;
        }
        default:
            break;
        }
    }
}

void FavesDialog::ExpandElementsRecursive(HTREEITEM hItem)
{
    HTREEITEM hCurrentItem = _hTreeCtrl.GetNextItem(hItem, TVGN_CHILD);
    while (hCurrentItem) {
        FavesItemPtr pElem = (FavesItemPtr)_hTreeCtrl.GetParam(hCurrentItem);
        if (pElem->IsExpanded()) {
            UpdateLink(hCurrentItem);

            /* toggle only the main items, because groups were updated automatically in UpdateLink() */
            if (pElem->IsRoot()) {
                /* if node needs to be expand, delete the indicator first,
                   because TreeView Expand() function toggles the flag     */
                pElem->IsExpanded(!pElem->IsExpanded());
                _hTreeCtrl.Expand(hCurrentItem, TVE_TOGGLE);
            }

            /* traverse into the tree */
            ExpandElementsRecursive(hCurrentItem);
        }

        hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
    }
}


void FavesDialog::ReadSettings()
{
    extern WCHAR configPath[MAX_PATH];
    LPTSTR       readFilePath = (LPTSTR)new WCHAR[MAX_PATH];
    DWORD        hasRead      = 0;
    HANDLE       hFile        = nullptr;

    /* fill out tree and vDB */
    wcscpy(readFilePath, configPath);
    wcscat(readFilePath, FAVES_DATA);

    hFile = ::CreateFile(readFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD    size = ::GetFileSize(hFile, nullptr);

        if (size != -1) {
            LPTSTR ptr  = nullptr;
            LPTSTR data = (LPTSTR)new WCHAR[size / sizeof(WCHAR)];

            if (data != nullptr) {
                /* read data from file */
                ::ReadFile(hFile, data, size, &hasRead, nullptr);

                WCHAR    szBOM = 0xFEFF;
                if (data[0] != szBOM) {
                    ::MessageBox(_hParent, L"Error in file 'Favorites.dat'", L"Error", MB_OK | MB_ICONERROR);
                }
                else {
                    ptr = data + 1;
                    ptr = _tcstok(ptr, L"\n");

                    /* finally, fill out the tree and the vDB */
                    for (auto *root : {_model.FolderRoot(), _model.FileRoot(), _model.WebRoot(), _model.SessionRoot()}) {
                        /* error */
                        if (ptr == nullptr) {
                            break;
                        }

                        /* step over name tag */
                        if (_tcscmp(root->m_name.c_str(), ptr) == 0) {
                            ptr = _tcstok(nullptr, L"\n");
                            if (ptr == nullptr) {
                                break;
                            }
                            if (_tcsstr(ptr, L"Expand=") == ptr) {
                                root->IsExpanded(ptr[7] == '1');
                                ptr = _tcstok(nullptr, L"\n");
                            }
                        }
                        else {
                            ::MessageBox(_hSelf, L"Error in file 'Favorites.dat'", L"Error", MB_OK);
                            break;
                        }

                        /* now read the information */
                        ReadElementTreeRecursive(root->Type(), root, &ptr);
                    }
                }
                delete [] data;
            }
        }

        ::CloseHandle(hFile);
    }

    delete [] readFilePath;
}


void FavesDialog::ReadElementTreeRecursive(FavesType type, FavesItemPtr elem, LPTSTR* ptr)
{
    while (1) {
        if (*ptr == nullptr) {
            /* reached end of file -> leave */
            break;
        }
        if (_tcscmp(*ptr, L"#LINK") == 0) {
            std::wstring name;
            std::wstring link;

            // get element name
            *ptr = _tcstok(nullptr, L"\n");
            if (_tcsstr(*ptr, L"\tName=") == *ptr) {
                name = &(*ptr)[6];
                *ptr = _tcstok(nullptr, L"\n");
            }
            else {
                ::MessageBox(_hSelf, L"Error in file 'Favorites.dat'\nName in LINK not correct!", L"Error", MB_OK);
            }

            // get next element link
            if (_tcsstr(*ptr, L"\tLink=") == *ptr) {
                link = &(*ptr)[6];
                *ptr = _tcstok(nullptr, L"\n");
            }
            else {
                ::MessageBox(_hSelf, L"Error in file 'Favorites.dat'\nLink in LINK not correct!", L"Error", MB_OK);
            }

            auto newItem = std::make_unique<FavesItem>(elem, type, name, link);
            elem->AddChild(std::move(newItem));
        }
        else if ((_tcscmp(*ptr, L"#GROUP") == 0) || (_tcscmp(*ptr, L"#GROUP") == 0)) {
            // group is found, get information and fill out the struct

            /* get element name */
            std::wstring name;
            *ptr = _tcstok(nullptr, L"\n");
            if (_tcsstr(*ptr, L"\tName=") == *ptr) {
                name = &(*ptr)[6];
                *ptr = _tcstok(nullptr, L"\n");
            }
            else {
                ::MessageBox(_hSelf, L"Error in file 'Favorites.dat'\nName in GROUP not correct!", L"Error", MB_OK);
            }

            BOOL isExpanded = false;
            if (_tcsstr(*ptr, L"\tExpand=") == *ptr) {
                if ((*ptr)[8] == '1') {
                    isExpanded = true;
                }
                *ptr = _tcstok(nullptr, L"\n");
            }

            auto newItem = std::make_unique<FavesItem>(elem, type, name);
            newItem->IsExpanded(isExpanded);
            ReadElementTreeRecursive(type, newItem.get(), ptr);
            elem->m_children.push_back(std::move(newItem));
        }
        else if (_tcscmp(*ptr, L"") == 0) {
            /* step over empty lines */
            *ptr = _tcstok(nullptr, L"\n");
        }
        else if (_tcscmp(*ptr, L"#END") == 0) {
            /* on group end leave the recursion */
            *ptr = _tcstok(nullptr, L"\n");
            break;
        }
        else {
            /* there is garbage information/tag */
            break;
        }
    }
}


void FavesDialog::SaveSettings()
{
    extern WCHAR configPath[MAX_PATH];
    LPTSTR       saveFilePath = (LPTSTR)new WCHAR[MAX_PATH];
    DWORD        hasWritten   = 0;
    HANDLE       hFile        = nullptr;
    BYTE         szBOM[]      = {0xFF, 0xFE};

    wcscpy(saveFilePath, configPath);
    wcscat(saveFilePath, FAVES_DATA);

    hFile = ::CreateFile(saveFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        ::WriteFile(hFile, szBOM, sizeof(szBOM), &hasWritten, nullptr);

        for (auto *root : { _model.FolderRoot(), _model.FileRoot(), _model.WebRoot(), _model.SessionRoot() }) {
            std::wstring temp = std::format(L"{}\nExpand={}\n\n", root->Name().c_str(), root->IsExpanded());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);
            SaveElementTreeRecursive(root, hFile);
        }

        ::CloseHandle(hFile);
    }
    else {
        ErrorMessage(GetLastError());
    }

    delete [] saveFilePath;
}


void FavesDialog::SaveElementTreeRecursive(FavesItemPtr pElem, HANDLE hFile)
{
    DWORD        hasWritten = 0;

    /* delete elements of child items */
    for (auto&& child : pElem->m_children) {
        if (child->IsGroup()) {
            ::WriteFile(hFile, L"#GROUP\n", (DWORD)wcslen(L"#GROUP\n") * sizeof(WCHAR), &hasWritten, nullptr);

            std::wstring temp = std::format(L"\tName={}\n", child->Name().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            temp = std::format(L"\tExpand={}\n\n", child->IsExpanded());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            SaveElementTreeRecursive(child.get(), hFile);

            ::WriteFile(hFile, L"#END\n\n", (DWORD)wcslen(L"#END\n\n") * sizeof(WCHAR), &hasWritten, nullptr);
        }
        else if (child->IsLink()) {
            ::WriteFile(hFile, L"#LINK\n", (DWORD)wcslen(L"#LINK\n") * sizeof(WCHAR), &hasWritten, nullptr);

            std::wstring temp = std::format(L"\tName={}\n", child->Name().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            temp = std::format(L"\tLink={}\n\n", child->Link().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);
        }
    }
}
