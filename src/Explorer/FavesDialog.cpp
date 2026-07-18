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
#include "IPluginContext.h"
#include "PropDlg.h"
#include "resource.h"
#include "FileSystemService.h"
#include "StringUtil.h"
#include "ThemeRenderer.h"
#include "../NppPlugin/menuCmdID.h"

namespace {
ToolBarButtonUnit toolBarIcons[] = {
    {IDM_EX_EXPLORER,           IDI_FL_EXPLORER, IDI_FL_EXPLORER, IDI_FL_EXPLORER_GRAY, IDB_TB_EXPLORER,        0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_NEW_FILE,      IDI_FL_LINKNEWFILE, IDI_FL_LINKNEWFILE, IDI_FL_LINKNEWFILE_GRAY, IDB_EX_LINKNEWFILE,     0},
    {IDM_EX_LINK_NEW_FOLDER,    IDI_FL_LINKNEWFOLDER, IDI_FL_LINKNEWFOLDER, IDI_FL_LINKNEWFOLDER_GRAY, IDB_EX_LINKNEWFOLDER,   0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_NEW,           IDI_FL_LINKNEW, IDI_FL_LINKNEW, IDI_FL_LINKNEW_GRAY, IDB_EX_LINKNEW,         0},
    {IDM_EX_LINK_DELETE,        IDI_FL_LINKDELETE, IDI_FL_LINKDELETE, IDI_FL_LINKDELETE_GRAY, IDB_EX_LINKDELETE,      0},
    {0,                         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON,     0},
    {IDM_EX_LINK_EDIT,          IDI_FL_LINKEDIT, IDI_FL_LINKEDIT, IDI_FL_LINKEDIT_GRAY, IDB_EX_LINKEDIT,        0}
};

constexpr wchar_t FAVES_DATA[] = L"Favorites.dat";

LinkDlg MapPropDlg(FavesType root) {
    switch (root) {
    case FavesType::Folder:  return LinkDlg::FOLDER;
    case FavesType::File:    return LinkDlg::FILE;
    case FavesType::Session: return LinkDlg::FILE;
    default:                 return LinkDlg::NONE;
    }
}

LPCWSTR GetNameStrFromCmd(UINT resourceId)
{
    switch (resourceId) {
    case IDM_EX_EXPLORER:           return L"Explorer";
    case IDM_EX_LINK_NEW_FILE:      return L"Link Current File...";
    case IDM_EX_LINK_NEW_FOLDER:    return L"Link Current Folder...";
    case IDM_EX_LINK_NEW:           return L"New Link...";
    case IDM_EX_LINK_DELETE:        return L"Delete Link";
    case IDM_EX_LINK_EDIT:          return L"Edit Link...";
    default:                        return nullptr;
    }
}

} // namespace

FavesDialog::FavesDialog()
    : DockingDlgInterface(IDD_EXPLORER_DLG)
{
}

void FavesDialog::init(HINSTANCE hInst, HWND hParent, Settings* prop, IPluginContext* pluginContext)
{
    _pSettings = prop;
    _pluginContext = pluginContext;
    DockingDlgInterface::init(hInst, hParent);

    ReadSettings();
}
 
void FavesDialog::UpdateTheme(bool isDarkMode)
{
    toolBarStatusType toolbarType = _pSettings->IsUseFluentIcons() ? TB_SMALL : TB_STANDARD;
    _ToolBar.updateIcons(toolbarType, isDarkMode);
    ::SendMessage(_hSelf, WM_SIZE, 0, 0);

    updateDockingDlg();
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
        LPCWSTR iconResourceName = _pSettings->IsUseFluentIcons()
                                        ? _pluginContext->IsDarkMode()  ? MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES_DARKMODE)
                                                                        : MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES)
                                        : MAKEINTRESOURCE(IDI_HEART);
        data.hIconTab       = (HICON)::LoadImage(_hInst, iconResourceName, IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
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
    AddSaveSession(nullptr, true);
}

void FavesDialog::NotifyNewFile()
{
    if (isCreated() && isVisible()) {
        /* update "new file link" icon */
        std::filesystem::path currentPath = _pluginContext->GetFullCurrentPath();
        _ToolBar.enable(IDM_EX_LINK_NEW_FILE, PathFileExists(currentPath.c_str()));

        /* update "new folder link" icon */
        std::filesystem::path currentDir = _pluginContext->GetCurrentDirectory();
        _ToolBar.enable(IDM_EX_LINK_NEW_FOLDER, (!currentDir.empty()));
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
            case NM_CUSTOMDRAW:
                return HandleCustomDraw(reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam), nmhdr);
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
                    FavesItem* pElem = reinterpret_cast<FavesItem*>(pnmtv->itemNew.lParam);
                    if (pElem == nullptr) {
                        break;
                    }
                    // update expand state
                    pElem->IsExpanded(!pElem->IsExpanded());

                    // reload session's children
                    if ((pElem->Type() == FavesType::Session) && pElem->IsLink()) {
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
                    FavesItem* pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));

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

                FavesItem* pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(item));
                if (pElem) {
                    // show full file path
                    std::wstring tipText;
                    tipText += pElem->Link();
                    if ((pElem->Type() == FavesType::Session) && pElem->IsLink()) {
                        INT count = _hTreeCtrl.GetChildrenCount(item);
                        if (count > 0) {
                            // Check non-existent files
                            auto sessionFiles = _pluginContext->GetSessionFiles(pElem->Link());
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
                HandleToolBarCommand(_ToolBar.doPopop(pt));
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
            _pluginContext->SetFocusToCurrentEdit();
            return TRUE;
        }

        if ((HWND)lParam == _ToolBar.getHSelf()) {
            HandleToolBarCommand(LOWORD(wParam));
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

INT_PTR FavesDialog::HandleCustomDraw(LPNMTVCUSTOMDRAW cd, LPNMHDR nmhdr)
{
    static HTHEME s_theme = nullptr;
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
            const auto *elem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
            if (elem && (elem->Type() == FavesType::File) && elem->IsLink()) {
                if (IsFileOpen(elem->Link()) == TRUE) {
                    ::SelectObject(cd->nmcd.hdc, _pSettings->GetUnderlineFont());
                }
            }
            SetBkMode(cd->nmcd.hdc, TRANSPARENT);

            COLORREF textColor = TreeView_GetTextColor(nmhdr->hwndFrom);
            SetTextColor(cd->nmcd.hdc, textColor);
            ::DrawText(cd->nmcd.hdc, tvi.pszText, -1, &textRect, DT_SINGLELINE | DT_VCENTER);
            ::SelectObject(cd->nmcd.hdc, _pSettings->GetDefaultFont());

            const SIZE iconSize = {
                .cx = GetSystemMetrics(SM_CXSMICON),
                .cy = GetSystemMetrics(SM_CYSMICON),
            };
            const INT top = (textRect.top + textRect.bottom - iconSize.cy) / 2;
            const INT left = textRect.left - iconSize.cx - GetSystemMetrics(SM_CXEDGE);
            if ((_pSettings->IsUseSystemIcons() == FALSE) || (elem && (elem->IsGroup() || (elem->Type() == FavesType::Web) || (elem->Data() & FAVES_PARAM_USERIMAGE)))) {
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
    return FALSE;
}

LRESULT FavesDialog::RunTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
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
            _pluginContext->SetFocusToCurrentEdit();
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

bool FavesDialog::OpenTreeViewItem(HTREEITEM hItem)
{
    if (hItem) {
        FavesItem* pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
        if (pElem) {
            if (pElem->IsLink()) {
                _peOpenLink = pElem;
                ::PostMessage(_hSelf, EXM_OPENLINK, 0, 0);
                return TRUE;
            }
            return false;
        }
    }
    return false;
}

void FavesDialog::HandleToolBarCommand(UINT message)
{
    switch (message) {
    case IDM_EX_EXPLORER:
        ToggleExplorerDialog();
        break;
    case IDM_EX_LINK_NEW_FILE: {
        std::filesystem::path currentPath = _pluginContext->GetFullCurrentPath();
        if (PathFileExists(currentPath.c_str())) {
            AddToFavorites(false, currentPath.wstring());
        }
        break;
    }
    case IDM_EX_LINK_NEW_FOLDER: {
        std::filesystem::path currentDir = _pluginContext->GetCurrentDirectory();
        if (!currentDir.empty()) {
            AddToFavorites(true, currentDir.wstring());
        }
        break;
    }
    case IDM_EX_LINK_NEW: {
        HTREEITEM hItem = _hTreeCtrl.GetSelection();
        FavesType type  = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem))->Type();
        if (type == FavesType::Session) {
            AddSaveSession(hItem, false);
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
    ::SetWindowSubclass(_hTreeCtrl, WndDefaultTreeProc, 'tree', reinterpret_cast<DWORD_PTR>(this));

    /* Load Image List */
    _hImageListSys = GetSmallImageList(TRUE);
    _hImageList    = GetSmallImageList(FALSE);

    /* set image list */
    ::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)_hImageListSys);

    // set font
    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)_pSettings->GetDefaultFont(), TRUE);

    /* create toolbar */
    bool isDarkMode = _pluginContext->IsDarkMode();
    toolBarStatusType toolbarType = _pSettings->IsUseFluentIcons() ? TB_SMALL : TB_STANDARD;
    _ToolBar.init(_hInst, _hSelf, toolbarType, toolBarIcons, sizeof(toolBarIcons)/sizeof(ToolBarButtonUnit), isDarkMode);
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
    _isCut          = false;
    _hTreeCutCopy   = hItem;
}

void FavesDialog::CutItem(HTREEITEM hItem)
{
    _isCut          = true;
    _hTreeCutCopy   = hItem;
}

void FavesDialog::PasteItem(HTREEITEM hItem)
{
    FavesItem* destination = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
    FavesItem* source      = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(_hTreeCutCopy));

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
        auto newItem = std::make_unique<FavesItem>(destination, source);
        destination->AddChild(std::move(newItem));

        if (_isCut) {
            auto *parent = source->Parent();
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
        std::wstring msgBoxTxt = std::format(L"Could only be paste into {}", source->Root()->Name());
        ::MessageBox(_hParent, msgBoxTxt.c_str(), L"Error", MB_OK);
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

void FavesDialog::AddToFavorites(bool isFolder, const std::wstring& link)
{
    PropDlg     dlgProp;
    FavesType   type    = (isFolder ? FavesType::Folder : FavesType::File);
    std::wstring name;
    std::wstring linkBuf = link;
    std::wstring desc = std::format(L"New element in {}", isFolder ? _model.FolderRoot()->Name()
                                                                   : _model.FileRoot()->Name());

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);

    /* select root element */
    dlgProp.setRoot((isFolder ? _model.FolderRoot() : _model.FileRoot()),
                    (isFolder ? ICON_FOLDER         : ICON_FILE));

    /* open dialog */
    if (dlgProp.doDialog(&name, &linkBuf, desc, MapPropDlg(type)) == TRUE) {
        auto *group = dlgProp.getSelectedGroup();
        auto newItem = std::make_unique<FavesItem>(group, type, name, linkBuf);
        group->AddChild(std::move(newItem));

        auto *item = _hTreeCtrl.FindTreeItemByParam(group);
        RefreshTree(item);
    }
}

void FavesDialog::AddToFavorites(bool isFolder, std::vector<std::wstring>&& paths)
{
    PropDlg     dlgProp;
    FavesType   type = (isFolder ? FavesType::Folder : FavesType::File);

    std::wstring name;
    for (auto&& path : paths) {
        if (path.back() == L'\\') {
            path.pop_back();
        }
        name += PathFindFileName(path.c_str());
        name += L", ";
    }
    if (name.length() >= 2) {
        name.pop_back();
        name.pop_back();
    }
    std::wstring description = std::format(L"New element in {}", isFolder ? _model.FolderRoot()->Name() : _model.FileRoot()->Name());

    dlgProp.init(_hInst, _hParent);
    dlgProp.setRoot((isFolder ? _model.FolderRoot() : _model.FileRoot()),
                    (isFolder ? ICON_FOLDER         : ICON_FILE));
    if (dlgProp.doDialog(&name, nullptr, description, MapPropDlg(type)) == TRUE) {
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

void FavesDialog::AddSaveSession(HTREEITEM hItem, bool bSave)
{
    PropDlg         dlgProp;
    HTREEITEM       hParentItem = nullptr;
    FavesItem*      pElem       = nullptr;
    FavesType       type        = FavesType::Session;
    std::wstring    name;
    std::wstring    link;
    std::wstring    desc;

    if (bSave) {
        desc = L"Save current Session";
    } else {
        desc = L"Add existing Session";
    }

    /* if hItem is empty, extended dialog is necessary */
    if (hItem == nullptr) {
        /* this is called when notepad menu triggers this function */
        dlgProp.setRoot(_model.SessionRoot(), ICON_SESSION, TRUE);
    }
    else {
        /* get group or session information */
        pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
    }

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);

    /* open dialog */
    if (dlgProp.doDialog(&name, &link, desc, MapPropDlg(type), bSave) == TRUE) {
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
            pElem->Name(name);
            pElem->Link(link);
        }
        else {
            /* push information back */
            auto newItem = std::make_unique<FavesItem>(pElem, FavesType::Session, name, link);
            pElem->AddChild(std::move(newItem));
        }

        /* save current session when expected */
        if (bSave) {
            ::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)link.c_str());
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
}

void FavesDialog::NewItem(HTREEITEM hItem)
{
    PropDlg         dlgProp;
    FavesItem*      pElem   = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
    FavesType       type    = pElem->Type();
    bool            isOk    = false;
    std::wstring    name;
    std::wstring    link;
    std::wstring    desc    = std::format(L"New element in {}", pElem->Root()->Name());

    /* init properties dialog */
    dlgProp.init(_hInst, _hParent);
    while (!isOk) {
        /* open dialog */
        if (dlgProp.doDialog(&name, &link, desc, MapPropDlg(type)) == TRUE) {
            isOk = DoesLinkExist(link, type);
            if (isOk) {
                auto newItem = std::make_unique<FavesItem>(pElem, type, name, link);
                pElem->AddChild(std::move(newItem));
            }
        }
        else {
            break;
        }
    }

    if (isOk) {
        /* update information */
        if (pElem->IsGroup()) {
            UpdateLink(_hTreeCtrl.GetParent(hItem));
        }
        UpdateLink(hItem);

        _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
    }
}

void FavesDialog::EditItem(HTREEITEM hItem)
{
    HTREEITEM       hParentItem = _hTreeCtrl.GetParent(hItem);
    FavesItem*      pElem       = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));

    if (!pElem->IsRoot()) {
        FavesType   type        = pElem->Type();
        bool        needsUpdate = false;
        std::wstring name;
        std::wstring link;
        std::wstring desc = L"Properties";
        std::wstring comm = L"Favorites";

        if (pElem->IsGroup()) {
            /* get data of current selected element */
            name = pElem->Name();

            /* init new dialog */
            NewDlg dlgNew;
            dlgNew.init(_hInst, _hParent, comm);

            /* open dialog */
            if (dlgNew.doDialog(&name, desc) == TRUE) {
                pElem->Name(name);
                needsUpdate = true;
            }
        }
        else if (pElem->IsLink()) {
            /* get data of current selected element */
            name = pElem->Name();
            link = pElem->Link();

            PropDlg dlgProp;
            dlgProp.init(_hInst, _hParent);
            dlgProp.setRoot(pElem->Root(), ICON_FILE);
            dlgProp.setSelectedGroup(pElem->Parent());
            if (dlgProp.doDialog(&name, &link, desc, MapPropDlg(type)) == TRUE) {
                auto *group = dlgProp.getSelectedGroup();
                auto *selectedGroup = _hTreeCtrl.FindTreeItemByParam(group);
                if (hParentItem != selectedGroup) {
                    pElem->Remove();
                    auto newItem = std::make_unique<FavesItem>(group, type, name, link);
                    group->AddChild(std::move(newItem));
                    RefreshTree(selectedGroup);
                }
                else {
                    pElem->Name(name);
                    pElem->Link(link);
                }
                needsUpdate = true;
            }
        }

        /* update text of item */
        if (needsUpdate) {
            UpdateLink(hParentItem);
        }
    }
}

void FavesDialog::DeleteItem(HTREEITEM hItem)
{
    HTREEITEM       hItemParent = _hTreeCtrl.GetParent(hItem);
    FavesItem*      pElem       = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));

    if (!pElem) {
        return;
    }

    if (pElem->IsRoot()) {
        return;
    }

    if ((pElem->Root()->Type() == FavesType::Session) && (pElem->Type() == FavesType::File)) {
        return;
    }

    pElem->Remove();
    _hTreeCtrl.DeleteItem(hItem);

    /* update only parent of parent when current item is a group folder */
    if (reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItemParent))->IsGroup()) {
        UpdateLink(_hTreeCtrl.GetParent(hItemParent));
    }
    UpdateLink(hItemParent);
}

void FavesDialog::OpenContext(HTREEITEM hItem, POINT pt)
{
    FavesItem* pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));

    /* get element and level depth */
    if (pElem != nullptr) {
        if (pElem->IsGroup()) {
            OpenGroupContext(hItem, pt, pElem);
        }
        else if (pElem->IsLink()) {
            OpenLinkContext(hItem, pt, pElem);
        }
        else {
            ::MessageBox(_hParent, L"Element not found in List!", L"Error", MB_OK);
        }
    }
}

void FavesDialog::OpenGroupContext(HTREEITEM hItem, POINT pt, FavesItem* pElem)
{
    FavesType type = pElem->Type();
    HMENU hMenu = ::CreatePopupMenu();

    if (type != FavesType::Session) {
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::NewLink), L"New Link...");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::NewGroup), L"New Group...");
    }
    else {
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::AddSession), L"Add existing Session...");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::SaveSession), L"Save Current Session...");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::NewGroup), L"New Group...");
    }

    if (!pElem->IsRoot() && pElem->IsGroup()) {
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Copy), L"Copy");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Cut), L"Cut");
        if (_hTreeCutCopy != nullptr) {
            ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Paste), L"Paste");
        }
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Delete), L"Delete");
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Properties), L"Properties...");
    }
    else if (pElem->IsRoot() && (_hTreeCutCopy != nullptr)) {
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Paste), L"Paste");
    }

    /* track menu */
    auto command = static_cast<MenuID>(::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr));
    switch (command) {
    case MenuID::NewLink:
        NewItem(hItem);
        break;
    case MenuID::AddSession:
        AddSaveSession(hItem, false);
        break;
    case MenuID::SaveSession:
        AddSaveSession(hItem, true);
        break;
    case MenuID::NewGroup: {
        std::wstring name;
        std::wstring desc = std::format(L"New group in {}", pElem->Root()->Name());

        /* init new dialog */
        NewDlg dlgNew;
        dlgNew.init(_hInst, _hParent, L"Favorites");

        /* open dialog */
        if (dlgNew.doDialog(&name, desc) == TRUE) {
            auto newItem = std::make_unique<FavesItem>(pElem, type, name);
            pElem->AddChild(std::move(newItem));

            /* update information */
            if (pElem->IsGroup()) {
                UpdateLink(_hTreeCtrl.GetParent(hItem));
            }
            UpdateLink(hItem);
            _hTreeCtrl.Expand(hItem, TVM_EXPAND | TVE_COLLAPSERESET);
        }
        break;
    }
    case MenuID::Copy:
        CopyItem(hItem);
        break;
    case MenuID::Cut:
        CutItem(hItem);
        break;
    case MenuID::Paste:
        PasteItem(hItem);
        break;
    case MenuID::Delete:
        DeleteItem(hItem);
        break;
    case MenuID::Properties:
        EditItem(hItem);
        break;
    default:
        break;
    }

    ::DestroyMenu(hMenu);
}

void FavesDialog::OpenLinkContext(HTREEITEM hItem, POINT pt, FavesItem* pElem)
{
    FavesType type = pElem->Type();
    HMENU hMenu = ::CreatePopupMenu();

    ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Open), L"Open");

    if (type == FavesType::File) {
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::OpenOtherView), L"Open in Other View");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::OpenNewInstance), L"Open in New Instance");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::GotoFileLocation), L"Go to File Location");
    }
    else if (type == FavesType::Session) {
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::AddToSession), L"Add to Current Session");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::SaveSession), L"Save Current Session");
    }

    if ((type != FavesType::File) || (pElem->Parent()->Type() != FavesType::Session)) {
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Copy), L"Copy");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Cut), L"Cut");
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Delete), L"Delete");
        ::AppendMenu(hMenu, MF_SEPARATOR, 0, 0);
        ::AppendMenu(hMenu, MF_STRING, static_cast<UINT_PTR>(MenuID::Properties), L"Properties...");
    }

    /* track menu */
    auto command = static_cast<MenuID>(::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hParent, nullptr));
    switch (command) {
    case MenuID::Open:
        OpenLink(pElem);
        break;
    case MenuID::OpenOtherView:
        ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pElem->Link().c_str());
        ::SendMessage(_hParent, WM_COMMAND, IDM_VIEW_GOTO_ANOTHER_VIEW, 0);
        break;
    case MenuID::OpenNewInstance: {
        std::wstring nppPath(MAX_PATH, L'\0');
        ::GetModuleFileName(nullptr, nppPath.data(), MAX_PATH);
        nppPath.resize(std::wcslen(nppPath.c_str()));

        std::wstring params = L"-multiInst " + pElem->Link();
        ::ShellExecute(_hParent, L"open", nppPath.c_str(), params.c_str(), L".", SW_SHOW);
        break;
    }
    case MenuID::GotoFileLocation: {
        extern ExplorerDialog explorerDlg;
        explorerDlg.GotoFileLocation(pElem->Link());
        explorerDlg.doDialog();
        break;
    }
    case MenuID::AddToSession:
        _addToSession = true;
        OpenLink(pElem);
        _addToSession = false;
        break;
    case MenuID::SaveSession:
        ::SendMessage(_hParent, NPPM_SAVECURRENTSESSION, 0, (LPARAM)pElem->Link().c_str());
        _hTreeCtrl.DeleteChildren(hItem);
        DrawSessionChildren(hItem);
        break;
    case MenuID::Copy:
        CopyItem(hItem);
        break;
    case MenuID::Cut:
        CutItem(hItem);
        break;
    case MenuID::Paste:
        PasteItem(hItem);
        break;
    case MenuID::Delete:
        DeleteItem(hItem);
        break;
    case MenuID::Properties:
        EditItem(hItem);
        break;
    default:
        break;
    }

    ::DestroyMenu(hMenu);
}

void FavesDialog::UpdateLink(HTREEITEM hParentItem)
{
    HTREEITEM       hCurrentItem    = _hTreeCtrl.GetNextItem(hParentItem, TVGN_CHILD);
    FavesItem*      parentElement   = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hParentItem));

    if (parentElement != nullptr) {
        parentElement->SortChildren();

        /* update elements in parent tree */
        for (auto&& child : parentElement->Children()) {
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
                case FavesType::Folder:
                    FetchIcons(child->Link().c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected, &iIconOverlayed);
                    break;
                case FavesType::File:
                    FetchIcons(child->Link().c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
                    break;
                case FavesType::Session:
                    haveChildren    = (0 != ::SendMessage(_hParent, NPPM_GETNBSESSIONFILES, 0, (LPARAM)child->Link().c_str()));
                    iIconNormal     = ICON_SESSION;
                    break;
                case FavesType::Web:
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
            bool isTreeExp = _hTreeCtrl.IsItemExpanded(hCurrentItem);

            /* toggle if state is not equal */
            if (isTreeExp != child->IsExpanded()) {
                child->IsExpanded(isTreeExp);
                _hTreeCtrl.Expand(hCurrentItem, TVE_TOGGLE);
            }

            /* in any case redraw the session children items */
            if (child->Type() == FavesType::Session) {
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
    FavesItem* session = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hItem));
    if (session->IsGroup()) {
        return;
    }
    session->ClearChildren();

    BOOL hasMissingFile = FALSE;
    auto sessionFiles = _pluginContext->GetSessionFiles(session->Link());
    for (const auto &path : sessionFiles) {
        auto newItem = std::make_unique<FavesItem>(session, FavesType::File, path.substr(path.find_last_of(L'\\') + 1), path);
        INT iIconNormal = 0;
        INT iIconSelected = 0;
        INT iIconOverlayed = 0;
        if (::PathFileExists(newItem->Link().c_str())) {
            FetchIcons(newItem->Link().c_str(), nullptr, DEVT_FILE, &iIconNormal, &iIconSelected, &iIconOverlayed);
        }
        else {
            newItem->Data(FAVES_PARAM_USERIMAGE);
            iIconNormal = ICON_MISSING_FILE;
            iIconSelected = iIconNormal;
            hasMissingFile = TRUE;
        }
        _hTreeCtrl.InsertItem(newItem->Name(), iIconNormal, iIconSelected, iIconOverlayed, 0, hItem, TVI_LAST, FALSE, newItem.get());
        session->AddChild(std::move(newItem));
    }

    session->Data(FAVES_PARAM_USERIMAGE);
    if (hasMissingFile) {
        _hTreeCtrl.SetItemIcons(hItem, ICON_WARN_SESSION, ICON_WARN_SESSION, 0);
    }
    else {
        _hTreeCtrl.SetItemIcons(hItem, ICON_SESSION, ICON_SESSION, 0);
    }
}

bool FavesDialog::DoesLinkExist(const std::wstring& link, FavesType type)
{
    bool bRet = false;

    switch (type) {
    case FavesType::Folder:
    case FavesType::File:
    case FavesType::Session:
        /* test if path exists */
        bRet = ::PathFileExists(link.c_str());
        if (!bRet) {
            if (type == FavesType::Folder) {
                ::MessageBox(_hParent, L"Folder doesn't exist!", L"Error", MB_OK);
            } else {
                ::MessageBox(_hParent, L"File doesn't exist!", L"Error", MB_OK);
            }
        }
        break;
    case FavesType::Web:
        bRet = true;
        break;
    default:
        ::MessageBox(_hParent, L"Faves element doesn't exist!", L"Error", MB_OK);
        break;
    }

    return bRet;
}

void FavesDialog::OpenLink(FavesItem* pElem)
{
    if (pElem->IsLink()) {
        switch (pElem->Type()) {
        case FavesType::Folder: {
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
        case FavesType::File: {
            /* open possible link */
            std::wstring resolvedPath;
            if (FileSystemService::ResolveShortCut(pElem->Link(), resolvedPath)) {
                _pluginContext->DoOpen(resolvedPath);
            } else {
                _pluginContext->DoOpen(pElem->Link());
            }
            break;
        }
        case FavesType::Web:
            ::ShellExecute(_hParent, L"open", pElem->Link().c_str(), nullptr, nullptr, SW_SHOW);
            break;
        case FavesType::Session: {
            // Check non-existent files
            auto sessionFiles = _pluginContext->GetSessionFiles(pElem->Link());
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
            if (!_addToSession) {
                ::SendMessage(_hParent, WM_COMMAND, IDM_FILE_CLOSEALL, 0);
                _addToSession = false;
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
        FavesItem* pElem = reinterpret_cast<FavesItem*>(_hTreeCtrl.GetParam(hCurrentItem));
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
    std::filesystem::path favorites_dat(_pSettings->GetConfigDir());
    favorites_dat /= FAVES_DATA;

    if (!std::filesystem::exists(favorites_dat)) {
        _model.Clear();
        return;
    }

    try {
        _model.Load(favorites_dat);
    }
    catch (const std::exception& e) {
        ::MessageBoxA(_hParent, e.what(), "Error", MB_OK | MB_ICONERROR);
    }
}

void FavesDialog::SaveSettings()
{
    std::filesystem::path favorites_dat(_pSettings->GetConfigDir());
    favorites_dat /= FAVES_DATA;
    try {
        _model.Save(favorites_dat);
    }
    catch (const std::exception& e) {
        ::MessageBoxA(_hParent, e.what(), "Error", MB_OK | MB_ICONERROR);
    }
}
