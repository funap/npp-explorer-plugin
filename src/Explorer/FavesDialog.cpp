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
#include <sstream>
#include <functional>

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "resource.h"
#include "NppInterface.h"
#include "StringUtil.h"
#include "ThemeRenderer.h"

static ToolBarButtonUnit toolBarIcons[] = {
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

static TCHAR FAVES_DATA[] = _T("\\Favorites.dat");

static LPCTSTR szToolTip[] = {
    _T("Explorer"),
    _T("Link Current File..."),
    _T("Link Current Folder..."),
    _T("New Link..."),
    _T("Delete Link"),
    _T("Edit Link...")
};

LPCTSTR FavesDialog::GetNameStrFromCmd(UINT resID)
{
    if ((IDM_EX_EXPLORER <= resID) && (resID <= IDM_EX_LINK_EDIT)) {
        return szToolTip[resID - IDM_EX_EXPLORER];
    }
    return nullptr;
}

FavesDialog::FavesDialog(void)
    : DockingDlgInterface(IDD_EXPLORER_DLG)
    , _hDefaultTreeProc(nullptr)
    , _hImageList(nullptr)
    , _hImageListSys(nullptr)
    , _isCut(FALSE)
    , _hTreeCutCopy(nullptr)
    , _ToolBar()
    , _Rebar()
    , _addToSession(FALSE)
    , _peOpenLink(nullptr)
    , _pExProp(nullptr)
    , _model()
{
}

FavesDialog::~FavesDialog(void)
{
    ImageList_Destroy(_hImageList);
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
    if (!isCreated())
    {
        tTbData data{};
        create(&data);

        // define the default docking behaviour
        data.pszName        = _T("Favorites");
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


void FavesDialog::SaveSession(void)
{
    AddSaveSession(nullptr, TRUE);
}


void FavesDialog::NotifyNewFile(void)
{
    if (isCreated() && isVisible())
    {
        TCHAR TEMP[MAX_PATH] = {};

        /* update "new file link" icon */
        ::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
        _ToolBar.enable(IDM_EX_LINK_NEW_FILE, PathFileExists(TEMP));

        /* update "new folder link" icon */
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
        _ToolBar.enable(IDM_EX_LINK_NEW_FOLDER, (_tcslen(TEMP) != 0));
    }
}


INT_PTR CALLBACK FavesDialog::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_INITDIALOG:
        /* get handle of dialogs */
        _hTreeCtrl    = ::GetDlgItem(_hSelf, IDC_TREE_FOLDER);
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
                    TCHAR textBuffer[MAX_PATH]{};
                    TVITEM tvi = {
                        .mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
                        .hItem = hItem,
                        .pszText = textBuffer,
                        .cchTextMax = MAX_PATH,
                    };
                    if (TRUE == TreeView_GetItem(nmhdr->hwndFrom, &tvi)) {
                        const auto elem = reinterpret_cast<FavesItemPtr>(GetParam(hItem));
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
                HTREEITEM hItem = TreeView_HitTest(_hTreeCtrl, &ht);
                if (hItem != nullptr) {
                    OpenContext(hItem, pt);
                }
                break;
            }
            case TVN_ITEMEXPANDING: {
                LPNMTREEVIEW pnmtv = reinterpret_cast<LPNMTREEVIEW>(lParam);
                HTREEITEM hItem = pnmtv->itemNew.hItem;

                if (hItem != nullptr) {
                    /* get element information */
                    FavesItemPtr pElem = reinterpret_cast<FavesItemPtr>(pnmtv->itemNew.lParam);

                    /* update expand state */
                    pElem->IsExpanded(!pElem->IsExpanded());

                    // reload session's children
                    if ((pElem->Type() == FAVES_SESSION) && pElem->IsLink()) {
                        DeleteChildren(hItem);
                        DrawSessionChildren(hItem);
                    }

                    if (!TreeView_GetChild(_hTreeCtrl, hItem)) {
                        if (pElem == nullptr) {
                            /* nothing to do */
                        }
                        else if ((pElem->Type() == FAVES_SESSION) && pElem->IsLink()) {
                            DeleteChildren(hItem);
                            DrawSessionChildren(hItem);
                        }
                        else {
                            UpdateLink(hItem);
                        }
                    }
                }
                break;
            }
            case TVN_SELCHANGED: {
                HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);

                if (hItem != nullptr) {
                    FavesItemPtr pElem = (FavesItemPtr)GetParam(hItem);

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

                FavesItemPtr pElem = reinterpret_cast<FavesItemPtr>(GetParam(item));
                if (pElem) {
                    // show full file path
                    std::wstring tipText;
                    tipText += pElem->Link();
                    if ((pElem->Type() == FAVES_SESSION) && pElem->IsLink()) {
                        INT count = GetChildrenCount(item);
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
                            tipText += StringUtil::format(L"\nThis session has %d files", count);
                            if (nonExistentFileCount > 0) {
                                tipText += StringUtil::format(L" (%d are non-existent)", nonExistentFileCount);
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
            int idButton = int(lpttt->hdr.idFrom);
            lpttt->lpszText = const_cast<LPTSTR>(GetNameStrFromCmd(idButton));
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
            HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
            if (OpenTreeViewItem(hItem)) {
                return TRUE;
            }
        }
        if (wParam == VK_DELETE) {
            HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
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

        HTREEITEM hItem = TreeView_HitTest(_hTreeCtrl, &hti);
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

BOOL FavesDialog::OpenTreeViewItem(const HTREEITEM hItem)
{
    if (hItem) {
        FavesItemPtr    pElem = reinterpret_cast<FavesItemPtr>(GetParam(hItem));
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
        TCHAR TEMP[MAX_PATH] = {};
        ::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)TEMP);
        if (PathFileExists(TEMP)) {
            AddToFavorties(FALSE, TEMP);
        }
        break;
    }
    case IDM_EX_LINK_NEW_FOLDER: {
        TCHAR TEMP[MAX_PATH] = {};
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
        if (_tcslen(TEMP) != 0) {
            AddToFavorties(TRUE, TEMP);
        }
        break;
    }
    case IDM_EX_LINK_NEW: {
        HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
        FavesType type  = ((FavesItemPtr)GetParam(hItem))->Type();
        if (type == FAVES_SESSION) {
            AddSaveSession(hItem, FALSE);
        }
        else {
            NewItem(hItem);
        }
        break;
    }
    case IDM_EX_LINK_EDIT:
        EditItem(TreeView_GetSelection(_hTreeCtrl));
        break;
    case IDM_EX_LINK_DELETE:
        DeleteItem(TreeView_GetSelection(_hTreeCtrl));
        break;
    default:
        break;
    }
}

void FavesDialog::InitialDialog(void)
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
    UpdateLink(InsertItem(_model.FolderRoot()->Name(),  ICON_FOLDER,  ICON_FOLDER,  0, 0, TVI_ROOT, TVI_LAST, _model.FolderRoot()->HasChildren(),   _model.FolderRoot()));
    UpdateLink(InsertItem(_model.FileRoot()->Name(),    ICON_FILE,    ICON_FILE,    0, 0, TVI_ROOT, TVI_LAST, _model.FileRoot()->HasChildren(),     _model.FileRoot()));
    UpdateLink(InsertItem(_model.WebRoot()->Name(),     ICON_WEB,     ICON_WEB,     0, 0, TVI_ROOT, TVI_LAST, _model.WebRoot()->HasChildren(),      _model.WebRoot()));
    UpdateLink(InsertItem(_model.SessionRoot()->Name(), ICON_SESSION, ICON_SESSION, 0, 0, TVI_ROOT, TVI_LAST, _model.SessionRoot()->HasChildren(),  _model.SessionRoot()));
    SendMessage(_hTreeCtrl, WM_SETREDRAW, TRUE, 0);
}

void FavesDialog::SetFont(const HFONT font)
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
    FavesItemPtr destination = (FavesItemPtr)GetParam(hItem);
    FavesItemPtr source      = (FavesItemPtr)GetParam(_hTreeCutCopy);

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
            auto parent = source->m_parent;
            source->Remove();

            auto parentTreeItem    = TreeView_GetParent(_hTreeCtrl, _hTreeCutCopy);
            UpdateLink(parentTreeItem);
            UpdateNode(parentTreeItem, parent->HasChildren());
            ExpandElementsRecursive(parentTreeItem);
        }

        /* update information */
        UpdateLink(hItem);
        UpdateNode(hItem, TRUE);
        ExpandElementsRecursive(hItem);
        if (destination->IsExpanded()) {
            TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }

        _hTreeCutCopy = nullptr;
    }
    else {
        TCHAR    msgBoxTxt[128];
        _stprintf(msgBoxTxt, _T("Could only be paste into %s"), source->Root()->Name().c_str());
        ::MessageBox(_hParent, msgBoxTxt, _T("Error"), MB_OK);
    }
}

void FavesDialog::RefreshTree(HTREEITEM item)
{
    if (item) {
        /* update information */
        HTREEITEM    hParentItem = TreeView_GetParent(_hTreeCtrl, item);
        if (hParentItem != nullptr) {
            UpdateLink(hParentItem);
        }
        UpdateLink(item);
        // expand item
        TreeView_Expand(_hTreeCtrl, item, TVM_EXPAND | TVE_COLLAPSERESET);
    }
}

void FavesDialog::AddToFavorties(BOOL isFolder, LPTSTR szLink)
{
    PropDlg     dlgProp;
    FavesType   type    = (isFolder ? FAVES_FOLDER : FAVES_FILE);
    LPTSTR      pszName = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR      pszLink = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR      pszDesc = (LPTSTR)new TCHAR[MAX_PATH];

    /* fill out params */
    pszName[0] = '\0';
    _tcscpy(pszLink, szLink);

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
        auto group = dlgProp.getSelectedGroup();
        auto newItem = std::make_unique<FavesItem>(group, type, pszName, pszLink);
        group->AddChild(std::move(newItem));

        auto item = FindTreeItemByParam(group);
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
        auto group = dlgProp.getSelectedGroup();

        if (group != nullptr) {
            for (auto&& path : paths) {
                auto newItem = std::make_unique<FavesItem>(group, type, PathFindFileName(path.c_str()), path);
                group->AddChild(std::move(newItem));
            }

            auto item = FindTreeItemByParam(group);
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
    LPTSTR          pszName     = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR          pszLink     = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR          pszDesc     = (LPTSTR)new TCHAR[MAX_PATH];

    /* fill out params */
    pszName[0] = '\0';
    pszLink[0] = '\0';

    if (bSave == TRUE) {
        _tcscpy(pszDesc, _T("Save current Session"));
    } else {
        _tcscpy(pszDesc, _T("Add existing Session"));
    }

    /* if hItem is empty, extended dialog is necessary */
    if (hItem == nullptr) {
        /* this is called when notepad menu triggers this function */
        dlgProp.setRoot(_model.SessionRoot(), ICON_SESSION, TRUE);
    }
    else {
        /* get group or session information */
        pElem = (FavesItemPtr)GetParam(hItem);
    }

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);

    /* open dialog */
    if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type), bSave) == TRUE)
    {
        /* this is called when notepad menu triggers this function */
        if (hItem == nullptr) {
            /* get group name */
            pElem = dlgProp.getSelectedGroup();
            hParentItem = FindTreeItemByParam(pElem);

            if (pElem->IsLink()) {
                hItem = FindTreeItemByParam(pElem);
                hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
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
            TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }

        if ((hParentItem != nullptr) && (hItem == nullptr)) {
            /* update the session items */
            UpdateLink(hParentItem);
            TreeView_Expand(_hTreeCtrl, hParentItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }
    }

    delete [] pszName;
    delete [] pszLink;
    delete [] pszDesc;
}

void FavesDialog::NewItem(HTREEITEM hItem)
{
    PropDlg         dlgProp;
    FavesItemPtr    pElem   = (FavesItemPtr)GetParam(hItem);
    FavesType       type    = pElem->Type();
    BOOL            isOk    = FALSE;
    LPTSTR          pszName = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR          pszLink = (LPTSTR)new TCHAR[MAX_PATH];
    LPTSTR          pszDesc = (LPTSTR)new TCHAR[MAX_PATH];

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
            UpdateLink(TreeView_GetParent(_hTreeCtrl, hItem));
        }
        UpdateLink(hItem);

        TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
    }

    delete [] pszName;
    delete [] pszLink;
    delete [] pszDesc;
}

void FavesDialog::EditItem(HTREEITEM hItem)
{
    HTREEITEM       hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);
    FavesItemPtr    pElem       = (FavesItemPtr)GetParam(hItem);

    if (!pElem->IsRoot()) {
        FavesType   type        = pElem->Type();
        BOOL        needsUpdate = FALSE;
        LPTSTR      pszName     = (LPTSTR)new TCHAR[MAX_PATH];
        LPTSTR      pszLink     = (LPTSTR)new TCHAR[MAX_PATH];
        LPTSTR      pszDesc     = (LPTSTR)new TCHAR[MAX_PATH];
        LPTSTR      pszComm     = (LPTSTR)new TCHAR[MAX_PATH];

        if (pElem->IsGroup()) {
            /* get data of current selected element */
            _tcscpy(pszName, pElem->m_name.c_str());
            /* rename comment */
            _tcscpy(pszDesc, _T("Properties"));
            _tcscpy(pszComm, _T("Favorites"));

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
            _tcscpy(pszName, pElem->m_name.c_str());
            _tcscpy(pszLink, pElem->m_link.c_str());
            _tcscpy(pszDesc, _T("Properties"));

            PropDlg dlgProp;
            dlgProp.init(_hInst, _hParent);
            dlgProp.setRoot(pElem->Root(), ICON_FILE);
            dlgProp.setSelectedGroup(pElem->m_parent);
            if (dlgProp.doDialog(pszName, pszLink, pszDesc, MapPropDlg(type)) == TRUE) {
                auto group = dlgProp.getSelectedGroup();
                auto selectedGroup = FindTreeItemByParam(group);
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
    HTREEITEM       hItemParent = TreeView_GetParent(_hTreeCtrl, hItem);
    FavesItemPtr    pElem       = (FavesItemPtr)GetParam(hItem);

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
    TreeView_DeleteItem(_hTreeCtrl, hItem);

    /* update only parent of parent when current item is a group folder */
    if (((FavesItemPtr)GetParam(hItemParent))->IsGroup()) {
        UpdateLink(TreeView_GetParent(_hTreeCtrl, hItemParent));
    }
    UpdateLink(hItemParent);
}

void FavesDialog::OpenContext(HTREEITEM hItem, POINT pt)
{
    FavesItemPtr pElem = (FavesItemPtr)GetParam(hItem);

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
                ::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, _T("Properties..."));
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
                LPTSTR pszName = (LPTSTR)new TCHAR[MAX_PATH];
                LPTSTR pszDesc = (LPTSTR)new TCHAR[MAX_PATH];
                LPTSTR pszComm = (LPTSTR)new TCHAR[MAX_PATH];

                pszName[0] = '\0';

                _tcscpy(pszComm, _T("New group in %s"));
                _stprintf(pszDesc, pszComm, pElem->Root()->Name().c_str());

                /* init new dialog */
                NewDlg dlgNew;
                dlgNew.init(_hInst, _hParent, _T("Favorites"));

                /* open dialog */
                if (dlgNew.doDialog(pszName, pszDesc) == TRUE) {
                    auto newItem = std::make_unique<FavesItem>(pElem, type, pszName);
                    pElem->AddChild(std::move(newItem));

                    /* update information */
                    if (pElem->IsGroup()) {
                        UpdateLink(TreeView_GetParent(_hTreeCtrl, hItem));
                    }
                    UpdateLink(hItem);
                    TreeView_Expand(_hTreeCtrl, hItem, TVM_EXPAND | TVE_COLLAPSERESET);
                }

                delete [] pszName;
                delete [] pszDesc;
                delete [] pszComm;
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

            ::AppendMenu(hMenu, MF_STRING, FM_OPEN, _T("Open"));

            if (type == FAVES_FILE) {
                ::AppendMenu(hMenu, MF_STRING, FM_OPENOTHERVIEW, _T("Open in Other View"));
                ::AppendMenu(hMenu, MF_STRING, FM_OPENNEWINSTANCE, _T("Open in New Instance"));
                ::AppendMenu(hMenu, MF_STRING, FM_GOTO_FILE_LOCATION, _T("Go to File Location"));
            }
            else if (type == FAVES_SESSION) {
                ::AppendMenu(hMenu, MF_STRING, FM_ADDTOSESSION, _T("Add to Current Session"));
                ::AppendMenu(hMenu, MF_STRING, FM_SAVESESSION, _T("Save Current Session"));
            }

            if ((type != FAVES_FILE) || (pElem->m_parent->Type() != FAVES_SESSION)) {
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_COPY, _T("Copy"));
                ::AppendMenu(hMenu, MF_STRING, FM_CUT, _T("Cut"));

                ::AppendMenu(hMenu, MF_STRING, FM_DELETE, _T("Delete"));
                ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
                ::AppendMenu(hMenu, MF_STRING, FM_PROPERTIES, _T("Properties..."));
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
                LPTSTR pszNpp = (LPTSTR)new TCHAR[MAX_PATH];
                // get notepad++.exe path
                ::GetModuleFileName(nullptr, pszNpp, MAX_PATH);

                std::wstring params = L"-multiInst " + pElem->Link();
                ::ShellExecute(_hParent, _T("open"), pszNpp, params.c_str(), _T("."), SW_SHOW);

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
                DeleteChildren(hItem);
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
            ::MessageBox(_hParent, _T("Element not found in List!"), _T("Error"), MB_OK);
        }
    }
}


void FavesDialog::UpdateLink(HTREEITEM hParentItem)
{
    HTREEITEM       hCurrentItem    = TreeView_GetNextItem(_hTreeCtrl, hParentItem, TVGN_CHILD);
    FavesItemPtr    parentElement   = (FavesItemPtr)GetParam(hParentItem);

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
                UpdateItem(hCurrentItem, child->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren, child.get());
            }
            else {
                hCurrentItem = InsertItem(child->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, hParentItem, TVI_LAST, haveChildren, child.get());
            }

            /* control item expand state and correct if necessary */
            BOOL isTreeExp = (TreeView_GetItemState(_hTreeCtrl, hCurrentItem, TVIS_EXPANDED) & TVIS_EXPANDED ? TRUE : FALSE);

            /* toggle if state is not equal */
            if (isTreeExp != child->IsExpanded()) {
                child->IsExpanded(isTreeExp);
                TreeView_Expand(_hTreeCtrl, hCurrentItem, TVE_TOGGLE);
            }

            /* in any case redraw the session children items */
            if (child->Type() == FAVES_SESSION) {
                DeleteChildren(hCurrentItem);
                DrawSessionChildren(hCurrentItem);
            }

            hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
        }

        // Update current node
        UpdateNode(hParentItem, parentElement->HasChildren());

        /* delete possible not existed items */
        while (hCurrentItem != nullptr) {
            HTREEITEM   pPrevItem   = hCurrentItem;
            hCurrentItem            = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
            TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
        }
    }
}

void FavesDialog::UpdateNode(HTREEITEM hItem, BOOL haveChildren)
{
    if (hItem != nullptr) {
        TCHAR TEMP[MAX_PATH] = {};
        TVITEM tvi = {
            .mask       = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM,
            .hItem      = hItem,
            .pszText    = TEMP,
            .cchTextMax = MAX_PATH,
        };

        if (TreeView_GetItem(_hTreeCtrl, &tvi) == TRUE) {
            UpdateItem(hItem, TEMP, tvi.iImage, tvi.iSelectedImage, 0, 0, haveChildren, (void*)tvi.lParam);
        }
    }
}

void FavesDialog::DrawSessionChildren(HTREEITEM hItem)
{
    FavesItemPtr session = (FavesItemPtr)GetParam(hItem);
    if (session->IsGroup()) {
        return;
    }
    session->m_children.clear();

    BOOL hasMissingFile = FALSE;
    auto sessionFiles = NppInterface::getSessionFiles(session->Link());
    for (const auto &path : sessionFiles) {
        auto newItem = std::make_unique<FavesItem>(session, FAVES_FILE, path.substr(path.find_last_of(L"\\") + 1), path);
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
        InsertItem(newItem->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, hItem, TVI_LAST, FALSE, newItem.get());
        session->AddChild(std::move(newItem));
    }

    if (hasMissingFile) {
        session->uParam |= FAVES_PARAM_USERIMAGE;
        SetItemIcons(hItem, ICON_WARN_SESSION, ICON_WARN_SESSION, 0);
    }
    else {
        session->uParam |= FAVES_PARAM_USERIMAGE;
        SetItemIcons(hItem, ICON_SESSION, ICON_SESSION, 0);
    }
}

BOOL FavesDialog::DoesLinkExist(LPTSTR pszLink, FavesType type)
{
    BOOL bRet = FALSE;

    switch (type) {
    case FAVES_FOLDER:
        /* test if path exists */
        bRet = ::PathFileExists(pszLink);
        if (bRet == FALSE) {
            ::MessageBox(_hParent, _T("Folder doesn't exist!"), _T("Error"), MB_OK);
        }
        break;
    case FAVES_FILE:
    case FAVES_SESSION:
        /* test if path exists */
        bRet = ::PathFileExists(pszLink);
        if (bRet == FALSE) {
            ::MessageBox(_hParent, _T("File doesn't exist!"), _T("Error"), MB_OK);
        }
        break;
    case FAVES_WEB:
        bRet = TRUE;
        break;
    default:
        ::MessageBox(_hParent, _T("Faves element doesn't exist!"), _T("Error"), MB_OK);
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
            if (explorerDlg.isCreated() == false) {
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
            TCHAR pszFilePath[MAX_PATH];
            if (ResolveShortCut(pElem->Link(), pszFilePath, MAX_PATH) == S_OK) {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pszFilePath);
            } else {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->Link().c_str());
            }
            break;
        }
        case FAVES_WEB:
            ::ShellExecute(_hParent, _T("open"), pElem->Link().c_str(), nullptr, nullptr, SW_SHOW);
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
                if (IDCANCEL == ::MessageBox(_hSelf, StringUtil::format(L"This session has %d non-existent files. Processing will delete all non-existent files in the session. Are you sure you want to continue?", nonExistentFileCount).c_str(), L"Open Session", MB_OKCANCEL | MB_ICONWARNING)) {
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
    HTREEITEM hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_CHILD);
    while (hCurrentItem) {
        FavesItemPtr pElem = (FavesItemPtr)GetParam(hCurrentItem);
        if (pElem->IsExpanded()) {
            UpdateLink(hCurrentItem);

            /* toggle only the main items, because groups were updated automatically in UpdateLink() */
            if (pElem->IsRoot()) {
                /* if node needs to be expand, delete the indicator first,
                   because TreeView_Expand() function toggles the flag     */
                pElem->IsExpanded(!pElem->IsExpanded());
                TreeView_Expand(_hTreeCtrl, hCurrentItem, TVE_TOGGLE);
            }

            /* traverse into the tree */
            ExpandElementsRecursive(hCurrentItem);
        }

        hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
    }
}


void FavesDialog::ReadSettings(void)
{
    extern TCHAR configPath[MAX_PATH];
    LPTSTR       readFilePath = (LPTSTR)new TCHAR[MAX_PATH];
    DWORD        hasRead      = 0;
    HANDLE       hFile        = nullptr;

    /* fill out tree and vDB */
    _tcscpy(readFilePath, configPath);
    _tcscat(readFilePath, FAVES_DATA);

    hFile = ::CreateFile(readFilePath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD    size = ::GetFileSize(hFile, nullptr);

        if (size != -1) {
            LPTSTR ptr  = nullptr;
            LPTSTR data = (LPTSTR)new TCHAR[size / sizeof(TCHAR)];

            if (data != nullptr) {
                /* read data from file */
                ::ReadFile(hFile, data, size, &hasRead, nullptr);

                TCHAR    szBOM = 0xFEFF;
                if (data[0] != szBOM) {
                    ::MessageBox(_hParent, _T("Error in file 'Favorites.dat'"), _T("Error"), MB_OK | MB_ICONERROR);
                }
                else {
                    ptr = data + 1;
                    ptr = _tcstok(ptr, _T("\n"));

                    /* finaly, fill out the tree and the vDB */
                    for (auto root : {_model.FolderRoot(), _model.FileRoot(), _model.WebRoot(), _model.SessionRoot()}) {
                        /* error */
                        if (ptr == nullptr) {
                            break;
                        }

                        /* step over name tag */
                        if (_tcscmp(root->m_name.c_str(), ptr) == 0) {
                            ptr = _tcstok(nullptr, _T("\n"));
                            if (ptr == nullptr) {
                                break;
                            } else if (_tcsstr(ptr, _T("Expand=")) == ptr) {
                                root->IsExpanded(ptr[7] == '1');
                                ptr = _tcstok(nullptr, _T("\n"));
                            }
                        }
                        else {
                            ::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'"), _T("Error"), MB_OK);
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
        if (_tcscmp(*ptr, _T("#LINK")) == 0) {
            std::wstring name;
            std::wstring link;

            // get element name
            *ptr = _tcstok(nullptr, _T("\n"));
            if (_tcsstr(*ptr, _T("\tName=")) == *ptr) {
                name = &(*ptr)[6];
                *ptr = _tcstok(nullptr, _T("\n"));
            }
            else {
                ::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in LINK not correct!"), _T("Error"), MB_OK);
            }

            // get next element link
            if (_tcsstr(*ptr, _T("\tLink=")) == *ptr) {
                link = &(*ptr)[6];
                *ptr = _tcstok(nullptr, _T("\n"));
            }
            else {
                ::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nLink in LINK not correct!"), _T("Error"), MB_OK);
            }

            auto newItem = std::make_unique<FavesItem>(elem, type, name, link);
            elem->AddChild(std::move(newItem));
        }
        else if ((_tcscmp(*ptr, _T("#GROUP")) == 0) || (_tcscmp(*ptr, _T("#GROUP")) == 0)) {
            // group is found, get information and fill out the struct

            /* get element name */
            std::wstring name;
            *ptr = _tcstok(nullptr, _T("\n"));
            if (_tcsstr(*ptr, _T("\tName=")) == *ptr) {
                name = &(*ptr)[6];
                *ptr = _tcstok(nullptr, _T("\n"));
            }
            else {
                ::MessageBox(_hSelf, _T("Error in file 'Favorites.dat'\nName in GROUP not correct!"), _T("Error"), MB_OK);
            }

            BOOL isExpanded = false;
            if (_tcsstr(*ptr, _T("\tExpand=")) == *ptr) {
                if ((*ptr)[8] == '1') {
                    isExpanded = true;
                }
                *ptr = _tcstok(nullptr, _T("\n"));
            }

            auto newItem = std::make_unique<FavesItem>(elem, type, name);
            newItem->IsExpanded(isExpanded);
            ReadElementTreeRecursive(type, newItem.get(), ptr);
            elem->m_children.push_back(std::move(newItem));
        }
        else if (_tcscmp(*ptr, _T("")) == 0) {
            /* step over empty lines */
            *ptr = _tcstok(nullptr, _T("\n"));
        }
        else if (_tcscmp(*ptr, _T("#END")) == 0) {
            /* on group end leave the recursion */
            *ptr = _tcstok(nullptr, _T("\n"));
            break;
        }
        else {
            /* there is garbage information/tag */
            break;
        }
    }
}


void FavesDialog::SaveSettings(void)
{
    extern TCHAR configPath[MAX_PATH];
    LPTSTR       saveFilePath = (LPTSTR)new TCHAR[MAX_PATH];
    DWORD        hasWritten   = 0;
    HANDLE       hFile        = nullptr;
    BYTE         szBOM[]      = {0xFF, 0xFE};

    _tcscpy(saveFilePath, configPath);
    _tcscat(saveFilePath, FAVES_DATA);

    hFile = ::CreateFile(saveFilePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        ::WriteFile(hFile, szBOM, sizeof(szBOM), &hasWritten, nullptr);

        for (auto root : { _model.FolderRoot(), _model.FileRoot(), _model.WebRoot(), _model.SessionRoot() }) {
            std::wstring temp = StringUtil::format(L"%s\nExpand=%i\n\n", root->Name().c_str(), root->IsExpanded());
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
            ::WriteFile(hFile, _T("#GROUP\n"), (DWORD)_tcslen(_T("#GROUP\n")) * sizeof(TCHAR), &hasWritten, nullptr);

            std::wstring temp = StringUtil::format(L"\tName=%s\n", child->Name().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            temp = StringUtil::format(L"\tExpand=%i\n\n", child->IsExpanded());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            SaveElementTreeRecursive(child.get(), hFile);

            ::WriteFile(hFile, _T("#END\n\n"), (DWORD)_tcslen(_T("#END\n\n")) * sizeof(TCHAR), &hasWritten, nullptr);
        }
        else if (child->IsLink()) {
            ::WriteFile(hFile, _T("#LINK\n"), (DWORD)_tcslen(_T("#LINK\n")) * sizeof(TCHAR), &hasWritten, nullptr);

            std::wstring temp = StringUtil::format(L"\tName=%s\n", child->Name().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);

            temp = StringUtil::format(L"\tLink=%s\n\n", child->Link().c_str());
            ::WriteFile(hFile, temp.c_str(), (DWORD)temp.length() * sizeof(WCHAR), &hasWritten, nullptr);
        }
    }
}
