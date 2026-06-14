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


#include "ExplorerDialog.h"

#include <shellapi.h>
#include <shlobj.h>
#include <dbt.h>
#include <algorithm>
#include <filesystem>
#include <list>
#include <string>
#include <optional>
#include <format>

#include "Explorer.h"
#include "ExplorerResource.h"
#include "ContextMenu.h"
#include "NewDlg.h"
#include "IPluginContext.h"
#include "resource.h"
#include "ThemeRenderer.h"
#include "FileSystemService.h"
#include "TreeModelSynchronizer.h"

namespace {

template <typename... Args>
void DebugPrintf(std::wformat_string<Args...> format, Args&&... args)
{
    std::wstring message = std::format(format, std::forward<Args>(args)...);
    message.push_back('\n');
    OutputDebugString(message.c_str());
}


ToolBarButtonUnit toolBarIcons[] = {
    {IDM_EX_FAVORITES,      IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_TB_FAVES,       0},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
    {IDM_EX_PREV,           IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_PREV,        TBSTYLE_DROPDOWN},
    {IDM_EX_NEXT,           IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_NEXT,        TBSTYLE_DROPDOWN},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
    {IDM_EX_TOGGLE_WORKSPACE, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_WORKSPACE,  0},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
    {IDM_EX_FILE_NEW,       IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FILENEW,     0},
    {IDM_EX_FOLDER_NEW,     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERNEW,   0},
    {IDM_EX_SEARCH_FIND,    IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FIND,        0},
    {IDM_EX_TERMINAL,       IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_TERMINAL,    0},
    {IDM_EX_GO_TO_USER,     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERUSER,  0},
    {IDM_EX_GO_TO_FOLDER,   IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERGO,    0},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
    {IDM_EX_UPDATE,         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_UPDATE,      0},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
};


LPCTSTR GetNameStrFromCmd(UINT resourceId)
{
    if (resourceId == IDM_EX_TOGGLE_WORKSPACE) {
        return L"Toggle Workspace Mode";
    }

    LPCTSTR szToolTip[] = {
        L"Favorites",
        L"Previous Folder",
        L"Next Folder",
        L"New File...",
        L"New Folder...",
        L"Find in Files...",
        L"Open Command Window Here",
        L"Folder of Current File",
        L"User Folder",
        L"Refresh",
    };

    if ((IDM_EX_FAVORITES <= resourceId) && (resourceId <= IDM_EX_UPDATE)) {
        return szToolTip[resourceId - IDM_EX_FAVORITES];
    }
    return nullptr;
}

} // namespace



ExplorerDialog::ExplorerDialog()
    : DockingDlgInterface(IDD_EXPLORER_DLG)
    , _model(std::make_shared<ExplorerModel>())
    , _viewModel(std::make_shared<ExplorerViewModel>(_model, nullptr, &_workerThread))
    , _pendingSelectRootItem(nullptr)
    , _bStartupFinish(FALSE)
    , _hItemExpand(nullptr)
    , _hDefaultTreeProc(nullptr)
    , _hDefaultSplitterProc(nullptr)
    , _isSelNotifyEnable(TRUE)
    , _hListCtrl(nullptr)
    , _hHeader(nullptr)
    , _hSplitterCtrl(nullptr)
    , _hFilter(nullptr)
    , _FileList(_viewModel.get(), this)
    , _ptOldPos()
    , _ptOldPosHorizontal()
    , _isLeftButtonDown(FALSE)
    , _hSplitterCursorUpDown(nullptr)
    , _hSplitterCursorLeftRight(nullptr)
    , _pSettings(nullptr)
    , _isScrolling(FALSE)
    , _isDnDStarted(FALSE)
    , _iDockedPos(CONT_LEFT)
{
    _viewModel->AddObserver(this);
}

ExplorerDialog::~ExplorerDialog()
{
    _viewModel->RemoveObserver(this);
    _workerThread.Stop();
    if (_model) _model->RemoveObserver(this);
}


void ExplorerDialog::init(HINSTANCE hInst, HWND hParent, Settings *prop, IPluginContext* pluginContext)
{
    DockingDlgInterface::init(hInst, hParent);

    _pSettings = prop;
    _pluginContext = pluginContext;
    _viewModel->SetSettings(prop);
    _FileList.initProp(prop);
}

void ExplorerDialog::redraw()
{
    UpdateLayout();

    /* possible new imagelist -> update the window */
    _hTreeCtrl.SetImageList(GetSmallImageList(_pSettings->IsUseSystemIcons()));

    // Store the active path before clearing, as TVN_SELCHANGED events triggered
    // during tree deletion will otherwise overwrite the saved current directory.
    std::wstring savedDir = _pSettings->GetCurrentDir();

    // Clear all existing tree items recursively to ensure no stale cached files exist
    _hTreeCtrl.DeleteChildren(TVI_ROOT);
    // Re-populate drive roots so SelectItem has roots to traverse
    UpdateRoots();

    ::SetTimer(_hSelf, EXT_UPDATEDEVICE, 0, nullptr);
    _FileList.redraw();
    ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);

    Refresh();
};

void ExplorerDialog::doDialog(bool willBeShown)
{
    if (!isCreated()) {
        // define the default docking behaviour
        tTbData data{};
        create(&data);
        data.pszName = L"Explorer";
        data.dlgID = DOCKABLE_EXPLORER_INDEX;
        data.uMask = DWS_DF_CONT_LEFT | DWS_ADDINFO | DWS_ICONTAB;
        data.hIconTab = (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_EXPLORE), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
        data.pszModuleName = getPluginFileName();

        ThemeRenderer::Instance().Register(_hSelf);
        ::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);
    }
    else if (willBeShown) {
        if (_pSettings->IsAutoUpdate()) {
            ::KillTimer(_hSelf, EXT_UPDATEACTIVATE);
            ::SetTimer(_hSelf, EXT_UPDATEACTIVATE, 0, nullptr);
        } else {
            ::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
            ::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 0, nullptr);
        }
    }
    display(willBeShown);
}


INT_PTR CALLBACK ExplorerDialog::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* lpDrawItemStruct = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (lpDrawItemStruct && lpDrawItemStruct->CtlID == IDC_COMBO_FILTER) {
            _ComboFilter.DrawItem(lpDrawItemStruct);
            return TRUE;
        }
        break;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* lpMeasureItemStruct = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (lpMeasureItemStruct && lpMeasureItemStruct->CtlID == IDC_COMBO_FILTER) {
            _ComboFilter.MeasureItem(lpMeasureItemStruct);
            return TRUE;
        }
        break;
    }
    case WM_INITDIALOG: {
        InitialDialog();
        _model->AddObserver(this);

        // EID_INIT -> TaskInit
        _workerThread.Enqueue(std::make_unique<TaskInit>(_model, _pSettings));
        break;
    }
    case WM_COMMAND:  {
        if ((HWND)lParam == _hFilter) {
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                ::SendMessage(_hSelf, EXM_CHANGECOMBO, 0, 0);
                return TRUE;
            }
            else if (HIWORD(wParam) == CBN_KILLFOCUS) {
                std::wstring searchWords = _ComboFilter.GetText();
                if (!searchWords.empty()) {
                    _ComboFilter.AddText(searchWords);
                }
                ::SendMessage(_hSelf, EXM_CHANGECOMBO, 0, 0);
                return TRUE;
            }
        }
        if (HIWORD(wParam) == 0) {
            HandleToolBarCommand(LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    case WM_NOTIFY: {
        LPNMHDR nmhdr = (LPNMHDR)lParam;

        if (nmhdr->hwndFrom == _hParent) {
            switch (LOWORD(nmhdr->code)){
            case DMN_DOCK:
                _iDockedPos = HIWORD(nmhdr->code);
                break;
            default:
                break;
            }
        }
        else if (nmhdr->hwndFrom == _hTreeCtrl) {
            switch (nmhdr->code) {
            case NM_DBLCLK: {
                const DWORD pos = ::GetMessagePos();
                TVHITTESTINFO ht {
                    .pt {
                        .x = GET_X_LPARAM(pos),
                        .y = GET_Y_LPARAM(pos),
                    }
                };
                ::ScreenToClient(_hTreeCtrl, &ht.pt);

                const HTREEITEM item = _hTreeCtrl.HitTest(&ht);
                if (item != nullptr) {
                    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(item));
                    if (pShared != nullptr && *pShared != nullptr && !(*pShared)->FSEntry().IsDirectory()) {
                        _pluginContext->DoOpen((*pShared)->Path());
                    }
                }
                break;
            }
            case NM_RCLICK: {
                const DWORD pos = ::GetMessagePos();
                const POINT pt {
                    .x = GET_X_LPARAM(pos),
                    .y = GET_Y_LPARAM(pos),
                };
                TVHITTESTINFO ht {
                    .pt = pt
                };
                ::ScreenToClient(_hTreeCtrl, &ht.pt);

                const HTREEITEM item = _hTreeCtrl.HitTest(&ht);
                if (item != nullptr) {
                    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(item));
                    if (pShared != nullptr && *pShared != nullptr) {
                        ShowContextMenu(pt, {*pShared});
                    }
                }
                return TRUE;
            }
            case TVN_SELCHANGED: {
                HTREEITEM item = _hTreeCtrl.GetSelection();
                if (item != nullptr) {
                    std::filesystem::path path = GetPath(item);
                    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(item));
                    if (pShared != nullptr && *pShared != nullptr && (*pShared)->FSEntry().IsDirectory()) {
                        _pendingNavigateDir = path;
                    }
                    else {
                    }
                }
                if (_isSelNotifyEnable == TRUE) {
                    ::KillTimer(_hSelf, EXT_SELCHANGE);
                    ::SetTimer(_hSelf, EXT_SELCHANGE, 200, nullptr);
                }
                break;
            }
            case TVN_ITEMEXPANDING: {
                TVITEM tvi = (TVITEM)((LPNMTREEVIEW)lParam)->itemNew;

                if (tvi.hItem != _hItemExpand) {
                    if (!(tvi.state & TVIS_EXPANDED)) {
                        _hItemExpand = tvi.hItem;
                        FetchChildren(_hItemExpand);
                    }
                } else {
                    _hItemExpand = nullptr;
                }
                break;
            }
            case TVN_ITEMEXPANDED: {
                CheckVisibleFolderChildren();
                break;
            }
            case TVN_DELETEITEM: {
                LPNMTREEVIEW pnm = (LPNMTREEVIEW)lParam;
                if (pnm->itemOld.lParam != 0) {
                    delete reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(pnm->itemOld.lParam);
                }
                break;
            }
            case TVN_BEGINDRAG: {
                CIDropSource dropSrc;
                CIDataObject dataObj(&dropSrc);
                FolderExChange(&dropSrc, &dataObj, DROPEFFECT_COPY | DROPEFFECT_MOVE);
                break;
            }
            default:
                break;
            }
        }
        else if ((nmhdr->hwndFrom == _hListCtrl) || (nmhdr->hwndFrom == _hHeader)) {
            return _FileList.notify(wParam, lParam);
        }
        else if ((nmhdr->hwndFrom == _ToolBar.getHSelf()) && (nmhdr->code == TBN_DROPDOWN)) {
            HandleToolBarDropDown((LPNMTOOLBAR)lParam);
            return TBDDRET_NODEFAULT;
        }
        else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED)) {
            NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
            if (lpnm->wID == REBAR_BAR_TOOLBAR) {
                POINT pt;
                pt.x = lpnm->rc.left;
                pt.y = lpnm->rc.bottom;
                ClientToScreen(nmhdr->hwndFrom, &pt);
                HandleToolBarCommand(_ToolBar.doPopop(pt));
                return TRUE;
            }
            break;
        }
        else if (nmhdr->code == TTN_GETDISPINFO) {
            LPTOOLTIPTEXT lpttt;

            lpttt = (LPTOOLTIPTEXT)nmhdr;
            lpttt->hinst = _hInst;


            // Specify the resource identifier of the descriptive
            // text for the given button.
            int resourceId = int(lpttt->hdr.idFrom);
            lpttt->lpszText = const_cast<LPTSTR>(GetNameStrFromCmd(resourceId));
            return TRUE;
        }

        DockingDlgInterface::run_dlgProc(Message, wParam, lParam);

        return FALSE;
    }
    case WM_SIZE:
    case WM_MOVE:
        if (_bStartupFinish == FALSE) {
            return TRUE;
        }
        UpdateLayout();
        CheckVisibleFolderChildren();
        break;
    case WM_PAINT:
        ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
        break;
    case WM_DESTROY: {
        _pSettings->SetFilterHistory(_ComboFilter.GetComboList());
        std::wstring lastFilter = _ComboFilter.GetText();
        if (!lastFilter.empty()) {
            _pSettings->GetFileFilter().setFilter(lastFilter);
        }

        // Stop the worker thread first and wait for it to fully exit before
        // any destructors run. This prevents the thread from calling PostMessage
        // on a window that no longer exists.
        _workerThread.Stop();

        // Flush any EXM_ASYNCTASK_COMPLETED messages that were posted before
        // Stop() was called so the raw task pointers do not leak.
        {
            MSG msg{};
            while (::PeekMessage(&msg, _hSelf, EXM_ASYNCTASK_COMPLETED, EXM_ASYNCTASK_COMPLETED, PM_REMOVE)) {
                IAsyncTask* rawTask = reinterpret_cast<IAsyncTask*>(msg.wParam);
                delete rawTask;
            }
        }

        _ToolBar.destroy();

        /* unsubclass */
        if (_hDefaultTreeProc != nullptr) {
            ::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)_hDefaultTreeProc);
            _hDefaultTreeProc = nullptr;
        }
        if (_hDefaultSplitterProc != nullptr) {
            ::SetWindowLongPtr(_hSplitterCtrl, GWLP_WNDPROC, (LONG_PTR)_hDefaultSplitterProc);
            _hDefaultSplitterProc = nullptr;
        }

        break;
    }
    case EXM_CHANGECOMBO: {
        std::wstring searchWords = _ComboFilter.GetSelectedText();
        if (searchWords.empty()) {
            searchWords = _ComboFilter.GetText();
        }
        if (!searchWords.empty()) {
            _FileList.filterFiles(searchWords.c_str());
        }
        else {
            _FileList.filterFiles(L"*");
        }
        if (_pSettings->IsUseFullTree()) {
            RefreshTreeFilter();
        }
        return TRUE;
    }
    case EXM_OPENDIR:
        NavigateTo((LPTSTR)lParam);
        return TRUE;
    case EXM_USER_ICONBAR:
        HandleToolBarCommand(wParam);
        return TRUE;
    case EXM_ASYNCTASK_COMPLETED: {
        _viewModel->ProcessTaskCompleted(reinterpret_cast<IAsyncTask*>(wParam));
        return TRUE;
    }
    case WM_TIMER:
        if (wParam == EXT_UPDATEDEVICE) {
            ::KillTimer(_hSelf, EXT_UPDATEDEVICE);
            _workerThread.Enqueue(std::make_unique<TaskInit>(_model, _pSettings));
            return FALSE;
        }
        if (wParam == EXT_UPDATEACTIVATE) {
            ::KillTimer(_hSelf, EXT_UPDATEACTIVATE);
            UpdateAllExpandedItems();
            UpdatePath();
            return FALSE;
        }
        if (wParam == EXT_UPDATEACTIVATEPATH) {
            ::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
            {
                HTREEITEM hItem         = _hTreeCtrl.GetSelection();
                HTREEITEM hParentItem   = _hTreeCtrl.GetParent(hItem);

                if (hParentItem != nullptr) {
                    FetchChildren(hParentItem);
                }
                if (hItem != nullptr) {
                    FetchChildren(hItem);
                    UpdatePath();
                }
            }
            return FALSE;
        }
        if (wParam == EXT_AUTOGOTOFILE) {
            ::KillTimer(_hSelf, EXT_AUTOGOTOFILE);
            GotoCurrentFile();
            return FALSE;
        }
        if (wParam == EXT_SELCHANGE) {
            ::KillTimer(_hSelf, EXT_SELCHANGE);
            if (!_pendingNavigateDir.empty()) {
                _viewModel->NavigateTo(_pendingNavigateDir, true);
                _pendingNavigateDir.clear();
                updateDockingDlg();
            }
            return FALSE;
        }
        return TRUE;
    default:
        return DockingDlgInterface::run_dlgProc(Message, wParam, lParam);
    }

    return FALSE;
}

/****************************************************************************
 * Message handling of tree
 */
LRESULT ExplorerDialog::RunTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message){
    case WM_GETDLGCODE: {
        return DLGC_WANTALLKEYS | ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
    }
    case WM_CHAR: {
        /* do selection of items by user keyword typing or cut/copy/paste */
        switch (wParam) {
        case SHORTCUT_ALL:
            return TRUE;
        case SHORTCUT_CUT:
            OnCut();
            return TRUE;
        case SHORTCUT_COPY:
            OnCopy();
            return TRUE;
        case SHORTCUT_PASTE:
            OnPaste();
            return TRUE;
        case SHORTCUT_DELETE:
            OnDelete();
            return TRUE;
        case SHORTCUT_REFRESH:
            Refresh();
            return TRUE;
        case VK_RETURN: {
            /* toggle item on return */
            HTREEITEM hItem = _hTreeCtrl.GetSelection();
            auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
            if (pShared != nullptr && *pShared != nullptr) {
                if ((*pShared)->FSEntry().IsDirectory()) {
                    if (_hTreeCtrl.GetChild(hItem) == nullptr) {
                        FetchChildren(hItem);
                    }
                    _hTreeCtrl.Expand(hItem, TVE_TOGGLE);
                }
                else {
                    _pluginContext->DoOpen((*pShared)->Path());
                }
            }
            return TRUE;
        }
        case VK_TAB:
            if (!_pSettings->IsUseFullTree()) {
                if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
                    ::SetFocus(_hFilter);
                }
                else {
                    ::SetFocus(_hListCtrl);
                }
            }
            else {
                ::SetFocus(_hFilter);
            }
            return TRUE;
        default:
            break;
        }
        break;
    }
    case WM_KEYUP:
        if (VK_APPS == wParam) {
            HTREEITEM item = _hTreeCtrl.GetSelection();
            if (item != nullptr) {
                auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(item));
                if (pShared != nullptr && *pShared != nullptr) {
                    RECT rect{};
                    _hTreeCtrl.GetItemRect(item, &rect, TRUE);
                    ::ClientToScreen(_hTreeCtrl, &rect);
                    const POINT pt{
                        .x = rect.right,
                        .y = rect.bottom,
                    };
                    ShowContextMenu(pt, {*pShared});
                    return TRUE;
                }
            }
        }
        break;
    case WM_KEYDOWN: {
        if ((wParam == VK_DELETE) && !((0x8000 & ::GetKeyState(VK_CONTROL)) == 0x8000)) {
            OnDelete((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000);
            return TRUE;
        }
        if (wParam == VK_F5) {
            Refresh();
            return TRUE;
        }
        if (VK_ESCAPE == wParam) {
            _pluginContext->SetFocusToCurrentEdit();
            return TRUE;
        }
        break;
    }
    case WM_SYSKEYDOWN:
        if ((0x8000 & ::GetKeyState(VK_MENU)) == 0x8000) {
            if (wParam == VK_LEFT) {
                NavigateBack();
                return TRUE;
            }
            if (wParam == VK_RIGHT) {
                NavigateForward();
                return TRUE;
            }
        }
        break;
    case WM_SYSKEYUP:
        if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
            if (wParam == VK_F10) {
                HTREEITEM item = _hTreeCtrl.GetSelection();
                if (item != nullptr) {
                    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(item));
                    if (pShared != nullptr && *pShared != nullptr) {
                        RECT rect{};
                        _hTreeCtrl.GetItemRect(item, &rect, TRUE);
                        ::ClientToScreen(_hTreeCtrl, &rect);
                        const POINT pt{
                            .x = rect.right,
                            .y = rect.bottom,
                        };
                        ShowContextMenu(pt, {*pShared});
                        return TRUE;
                    }
                }
            }
        }
        break;
    case EXM_QUERYDROP: {
        TVHITTESTINFO ht = {};

        if (_isScrolling == FALSE) {
            ScDir scrDir = GetScrollDirection(_hTreeCtrl);

            if (scrDir == SCR_UP) {
                ::SetTimer(_hTreeCtrl, EXT_SCROLLLISTUP, 300, nullptr);
                _isScrolling = TRUE;
            } else if (scrDir == SCR_DOWN) {
                ::SetTimer(_hTreeCtrl, EXT_SCROLLLISTDOWN, 300, nullptr);
                _isScrolling = TRUE;
            }
        }

        /* select item */
        ::GetCursorPos(&ht.pt);
        ::ScreenToClient(_hTreeCtrl, &ht.pt);
        _hTreeCtrl.SelectDropTarget( _hTreeCtrl.HitTest(&ht));

        _isDnDStarted = TRUE;

        return TRUE;
    }
    case WM_MOUSEMOVE:
    case EXM_DRAGLEAVE: {
        /* unselect DnD highlight */
        if (_isDnDStarted == TRUE) {
            _hTreeCtrl.SelectDropTarget( NULL);
            _isDnDStarted = FALSE;
        }
        /* stop scrolling if still enabled while DnD */
        if (_isScrolling == TRUE) {
            ::KillTimer(_hTreeCtrl, EXT_SCROLLLISTUP);
            ::KillTimer(_hTreeCtrl, EXT_SCROLLLISTDOWN);
            _isScrolling = FALSE;
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == EXT_SCROLLLISTUP) {
            HTREEITEM   hItemHigh   = _hTreeCtrl.GetDropHilightItem();
            HTREEITEM   hItemRoot   = _hTreeCtrl.GetRoot();
            ScDir       scrDir      = GetScrollDirection(_hTreeCtrl);

            if ((scrDir != SCR_UP) || (hItemHigh == hItemRoot) || (!m_bAllowDrop)) {
                ::KillTimer(_hTreeCtrl, EXT_SCROLLLISTUP);
                _isScrolling = FALSE;
            } else {
                ::SendMessage(_hTreeCtrl, WM_VSCROLL, SB_LINEUP, NULL);
            }
            return FALSE;
        }
        if (wParam == EXT_SCROLLLISTDOWN)
        {
            HTREEITEM   hItemHigh   = _hTreeCtrl.GetDropHilightItem();
            HTREEITEM   hItemLast   = _hTreeCtrl.GetLastVisibleItem();
            ScDir       scrDir      = GetScrollDirection(_hTreeCtrl);

            if ((scrDir != SCR_DOWN) || (hItemHigh == hItemLast) || (!m_bAllowDrop)) {
                ::KillTimer(_hTreeCtrl, EXT_SCROLLLISTDOWN);
                _isScrolling = FALSE;
            } else {
                ::SendMessage(_hTreeCtrl, WM_VSCROLL, SB_LINEDOWN, NULL);
            }
            return FALSE;
        }
        return TRUE;
    }
    default:
        break;
    }

    LRESULT lResult = ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
    if (Message == WM_VSCROLL || Message == WM_HSCROLL || Message == WM_MOUSEWHEEL || 
        Message == WM_KEYUP || Message == WM_KEYDOWN) {
        CheckVisibleFolderChildren();
    }
    return lResult;
}

/****************************************************************************
 * Message handling of header
 */
LRESULT ExplorerDialog::RunSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message){
    case WM_LBUTTONDOWN: {
        _isLeftButtonDown = TRUE;

        /* set cursor */
        if (_iDockedPos < CONT_TOP) {
            ::GetCursorPos(&_ptOldPos);
            SetCursor(_hSplitterCursorUpDown);
        }
        else {
            ::GetCursorPos(&_ptOldPosHorizontal);
            SetCursor(_hSplitterCursorLeftRight);
        }
        break;
    }
    case WM_LBUTTONUP: {
        RECT rc;

        getClientRect(rc);
        _isLeftButtonDown = FALSE;

        /* set cursor */
        if ((_iDockedPos == CONT_LEFT) || (_iDockedPos == CONT_RIGHT)) {
            SetCursor(_hSplitterCursorUpDown);
            if (_pSettings->GetSplitterPos() < 50) {
                    _pSettings->SetSplitterPos(50);
            }
            else if (_pSettings->GetSplitterPos() > (rc.bottom - 100)) {
                    _pSettings->SetSplitterPos(rc.bottom - 100);
            }
        }
        else {
            SetCursor(_hSplitterCursorLeftRight);
            if (_pSettings->GetSplitterPosHorizontal() < 50) {
                    _pSettings->SetSplitterPosHorizontal(50);
            }
            else if (_pSettings->GetSplitterPosHorizontal() > (rc.right - 50)) {
                _pSettings->SetSplitterPosHorizontal(rc.right - 50);
            }
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (_isLeftButtonDown == TRUE) {
            POINT pt;
            ::GetCursorPos(&pt);

            if (_iDockedPos < CONT_TOP) {
                if (_ptOldPos.y != pt.y) {
                    _pSettings->SetSplitterPos(_pSettings->GetSplitterPos() - (_ptOldPos.y - pt.y));
                    ::SendMessage(_hSelf, WM_SIZE, 0, 0);
                }
                _ptOldPos = pt;
            }
            else {
                if (_ptOldPosHorizontal.x != pt.x) {
                    _pSettings->SetSplitterPosHorizontal(_pSettings->GetSplitterPosHorizontal() - (_ptOldPosHorizontal.x - pt.x));
                    ::SendMessage(_hSelf, WM_SIZE, 0, 0);
                }
                _ptOldPosHorizontal = pt;
            }
        }

        /* set cursor */
        if (_iDockedPos < CONT_TOP) {
            SetCursor(_hSplitterCursorUpDown);
        }
        else {
            SetCursor(_hSplitterCursorLeftRight);
        }
        break;
    }
    default:
        break;
    }

    return ::CallWindowProc(_hDefaultSplitterProc, hwnd, Message, wParam, lParam);
}

void ExplorerDialog::HandleToolBarCommand(WPARAM message)
{
    switch (message) {
    case IDM_EX_PREV:
        NavigateBack();
        break;
    case IDM_EX_NEXT:
        NavigateForward();
        break;
    case IDM_EX_FILE_NEW: {
        NewDlg  dlg;
        WCHAR   szFileName[MAX_PATH]{};
        WCHAR   szComment[] = L"New file";

        dlg.init(_hInst, _hParent);
        for (;;) {
            if (dlg.doDialog(szFileName, szComment) == TRUE) {
                /* test if is correct */
                if (IsValidFileName(szFileName)) {
                    std::filesystem::path newFilePath = GetPath(_hTreeCtrl.GetSelection());
                    if (std::filesystem::is_regular_file(newFilePath)) {
                        newFilePath = newFilePath.parent_path();
                    }
                    newFilePath /= szFileName;

                    if (FileSystemService::CreateNewFile(newFilePath.wstring())) {
                        ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)newFilePath.c_str());
                        RefreshActiveNode();
                    }
                    break;
                }
            }
            else {
                break;
            }
        }
        break;
    }
    case IDM_EX_FOLDER_NEW: {
        NewDlg  dlg;
        WCHAR   szFolderName[MAX_PATH];
        WCHAR   szComment[] = L"New folder";

        szFolderName[0] = '\0';

        dlg.init(_hInst, _hParent);
        for (;;) {
            if (dlg.doDialog(szFolderName, szComment) == TRUE) {
                /* test if is correct */
                if (IsValidFileName(szFolderName)) {
                    std::filesystem::path newFolderPath = GetPath(_hTreeCtrl.GetSelection());
                    if (std::filesystem::is_regular_file(newFolderPath)) {
                        newFolderPath = newFolderPath.parent_path();
                    }
                    newFolderPath /= szFolderName;

                    if (FileSystemService::CreateNewDirectory(newFolderPath.wstring()) == false) {
                        ::MessageBox(_hParent, L"Folder couldn't be created.", L"Error", MB_OK);
                    }
                    else {
                        RefreshActiveNode();
                    }
                    break;
                }
            }
            else {
                break;
            }
        }
        break;
    }
    case IDM_EX_SEARCH_FIND:
        _pluginContext->LaunchFindFileDialog(_pSettings->GetCurrentDir());
        break;
    case IDM_EX_GO_TO_USER:
        GotoUserFolder();
        break;
    case IDM_EX_GO_TO_FOLDER:
        GotoCurrentFile();
        break;
    case IDM_EX_TERMINAL:
        OpenTerminal();
        break;
    case IDM_EX_FAVORITES:
        ToggleFavesDialog();
        break;
    case IDM_EX_UPDATE:
        Refresh();
        break;
    case IDM_EX_TOGGLE_WORKSPACE:
        ToggleWorkspaceMode();
        break;
    default:
        break;
    }
}

void ExplorerDialog::HandleToolBarDropDown(LPNMTOOLBAR lpnmtb)
{
    INT iElements = 0;

    /* get element cnt */
    if (lpnmtb->iItem == IDM_EX_PREV) {
        iElements = _viewModel->GetBackHistory(nullptr);
    } else if (lpnmtb->iItem == IDM_EX_NEXT) {
        iElements = _viewModel->GetForwardHistory(nullptr);
    }

    if (iElements == 0) return;

    /* allocate elements */
    LPTSTR  *pszPathes = (LPTSTR*)new LPTSTR[iElements];
    for (size_t i = 0; i < iElements; i++) {
        pszPathes[i] = (LPTSTR)new WCHAR[MAX_PATH];
    }

    /* get directories */
    if (lpnmtb->iItem == IDM_EX_PREV) {
        _viewModel->GetBackHistory(pszPathes);
    } else if (lpnmtb->iItem == IDM_EX_NEXT) {
        _viewModel->GetForwardHistory(pszPathes);
    }

    POINT pt    = {0};
    HMENU hMenu = ::CreatePopupMenu();

    /* test are folder exist */
    for (INT i = 0; i < iElements; i++) {
        if (std::filesystem::exists(pszPathes[i])) {
            ::AppendMenu(hMenu, MF_STRING, i+1, pszPathes[i]);
        }
    }

    ::GetCursorPos(&pt);
    INT cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hSelf, nullptr);
    ::DestroyMenu(hMenu);
    _Rebar.redraw();

    /* select element */
    if (cmd) {
        int offset = (lpnmtb->iItem == IDM_EX_PREV ? -cmd : cmd);
        _viewModel->NavigateToHistoryOffset(offset);
    }

    for (size_t i = 0; i < iElements; i++) {
        delete [] pszPathes[i];
    }
    delete [] pszPathes;
}

void ExplorerDialog::InitialDialog()
{
    _viewModel->SetNotificationWindow(_hSelf);
    _workerThread.Start(_viewModel.get());

    /* get handle of dialogs */
    _hTreeCtrl.Attach(::GetDlgItem(_hSelf, IDC_TREE_FOLDER));
    _hListCtrl      = ::GetDlgItem(_hSelf, IDC_LIST_FILE);
    _hHeader        = ListView_GetHeader(_hListCtrl);
    _hSplitterCtrl  = ::GetDlgItem(_hSelf, IDC_BUTTON_SPLITTER);
    _hFilter        = ::GetDlgItem(_hSelf, IDC_COMBO_FILTER);

    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)_pSettings->GetDefaultFont(), TRUE);
    ::SendMessage(_hListCtrl, WM_SETFONT, (WPARAM)_pSettings->GetDefaultFont(), TRUE);

    /* subclass tree */
    ::SetWindowLongPtr(_hTreeCtrl, GWLP_USERDATA, (LONG_PTR)this);
    _hDefaultTreeProc = (WNDPROC)::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)WndDefaultTreeProc);

    /* subclass splitter */
    _hSplitterCursorUpDown      = ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_UPDOWN));
    _hSplitterCursorLeftRight   = ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_LEFTRIGHT));
    ::SetWindowLongPtr(_hSplitterCtrl, GWLP_USERDATA, (LONG_PTR)this);
    _hDefaultSplitterProc = (WNDPROC)::SetWindowLongPtr(_hSplitterCtrl, GWLP_WNDPROC, (LONG_PTR)WndDefaultSplitterProc);

    /* Load Image List */
    ::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)GetSmallImageList(_pSettings->IsUseSystemIcons()));

    /* initial file list */
    _FileList.init(_hInst, _hSelf, _hListCtrl);

    /* create toolbar */
    _ToolBar.init(_hInst, _hSelf, TB_STANDARD, toolBarIcons, sizeof(toolBarIcons)/sizeof(ToolBarButtonUnit));

    _Rebar.init(_hInst, _hSelf);
    _ToolBar.addToRebar(&_Rebar);
    _ToolBar.enable(IDM_EX_PREV, FALSE);
    _ToolBar.enable(IDM_EX_NEXT, FALSE);
    _Rebar.setIDVisible(REBAR_BAR_TOOLBAR, true);

    /* initial combo */
    _ComboFilter.Init(_hFilter, _hSelf);
    _ComboFilter.SetComboList(_pSettings->GetFilterHistory());
    if (_ComboFilter.GetComboList().empty()) {
        _ComboFilter.AddText(L"*.*");
    }
    _ComboFilter.AddText(_pSettings->GetFileFilter().getFilterString());

    /* initialize droping */
    ::RegisterDragDrop(_hTreeCtrl, this);

    /* create the supported formats */
    FORMATETC fmtetc = {
        .cfFormat   = CF_HDROP,
        .dwAspect   = DVASPECT_CONTENT,
        .lindex     = -1,
        .tymed      = TYMED_HGLOBAL,
    };
    AddSuportedFormat(_hTreeCtrl, fmtetc);

    // key binding
    _FileList.setDefaultOnCharHandler([this](UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */) -> BOOL {
        switch (nChar) {
        case VK_TAB:
            if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
                ::SetFocus(_hTreeCtrl);
            }
            else {
                ::SetFocus(_hFilter);
            }
            return TRUE;
        case VK_ESCAPE:
            _pluginContext->SetFocusToCurrentEdit();
            return TRUE;
        default:
            break;
        }
        return FALSE;
    });

    _ComboFilter.SetDefaultOnCharHandler([this](UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */) -> BOOL {
        switch (nChar) {
        case VK_TAB:
            if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
                if (_pSettings->IsUseFullTree()) {
                    ::SetFocus(_hTreeCtrl);
                }
                else {
                    ::SetFocus(_hListCtrl);
                }
            }
            else {
                ::SetFocus(_hTreeCtrl);
            }
            return TRUE;
        case VK_ESCAPE:
            _pluginContext->SetFocusToCurrentEdit();
            return TRUE;
        default:
            break;
        }
        return FALSE;
    });

    ::PostMessage(_hFilter, CB_SETEDITSEL, 0, MAKELPARAM(-1, -1));
}

void ExplorerDialog::EnqueueAsyncTask(std::unique_ptr<IAsyncTask> task)
{
    _workerThread.Enqueue(std::move(task));
}

void ExplorerDialog::ClearPendingTasks(std::optional<TaskCategory> category)
{
    _workerThread.ClearPendingTasks(category);
}

void ExplorerDialog::OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task)
{
    // Release ownership and transfer the raw pointer via the Windows message.
    // If PostMessage fails (e.g. the window is already destroyed), delete immediately
    // to avoid a memory leak.
    IAsyncTask* rawTask = task.release();
    if (!::PostMessage(_hSelf, EXM_ASYNCTASK_COMPLETED, reinterpret_cast<WPARAM>(rawTask), 0)) {
        delete rawTask;
    }
}

void ExplorerDialog::SetFont(HFONT font)
{
    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
    ::SendMessage(_hListCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

BOOL ExplorerDialog::SelectItem(const std::filesystem::path& path)
{
    BOOL        folderExists    = FALSE;

    auto longPath = [](const std::filesystem::path& path) -> std::filesystem::path {
        WCHAR szLongPath[MAX_PATH] = {};
        std::wstring remotePath;
        /* convert possible net path name and get the full path name for compare */
        if (FileSystemService::ConvertNetPathName(path.wstring(), remotePath)) {
            ::GetLongPathName(remotePath.c_str(), szLongPath, MAX_PATH);
        }
        else {
            ::GetLongPathName(path.c_str(), szLongPath, MAX_PATH);
        }
        return { szLongPath };
    }(path);

    /* test if folder exists */
    folderExists = std::filesystem::exists(longPath);
    if (!folderExists) {
        return FALSE;
    }

    /* disabled detection of TVN_SELCHANGED notification */
    _isSelNotifyEnable = FALSE;

    // Scan all root nodes in the tree control to check if the target path falls under one of them
    HTREEITEM hMatchedRoot = nullptr;
    std::wstring matchedRootPath;

    HTREEITEM hItem = _hTreeCtrl.GetRoot();
    while (hItem != nullptr) {
        std::wstring rootPath = GetPath(hItem);
        // Normalize trailing slashes for comparison
        std::wstring cleanRoot = rootPath;
        if (!cleanRoot.empty() && cleanRoot.back() == L'\\') cleanRoot.pop_back();
        std::wstring cleanTarget = longPath.wstring();
        if (!cleanTarget.empty() && cleanTarget.back() == L'\\') cleanTarget.pop_back();

        // Check if cleanTarget starts with cleanRoot
        bool isMatch = false;
        if (_wcsicmp(cleanTarget.c_str(), cleanRoot.c_str()) == 0) {
            isMatch = true;
        } else {
            std::wstring cleanRootWithSlash = cleanRoot + L"\\";
            if (cleanTarget.size() > cleanRootWithSlash.size() &&
                _wcsnicmp(cleanTarget.c_str(), cleanRootWithSlash.c_str(), cleanRootWithSlash.size()) == 0) {
                isMatch = true;
            }
        }

        if (isMatch) {
            if (hMatchedRoot == nullptr || rootPath.size() > matchedRootPath.size()) {
                hMatchedRoot = hItem;
                matchedRootPath = rootPath;
            }
        }
        hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXT);
    }

    std::vector<std::wstring> pathSegments;
    size_t skipSegments = 0;

    if (hMatchedRoot != nullptr) {
        _pendingSelectRootItem = hMatchedRoot;
        for (const auto& segment : std::filesystem::path(matchedRootPath)) {
            if (segment != L"" && segment != L"\\") {
                skipSegments++;
            }
        }
    }
    else {
        _pendingSelectRootItem = nullptr;

        // Fallback UNC check and mount
        HTREEITEM hFallbackItem = _hTreeCtrl.GetRoot();
        if (longPath.wstring().compare(0, 2, L"\\\\") == 0) {
            do {
                auto itemName = _hTreeCtrl.GetItemText(hFallbackItem);
                if (('A' <= itemName[0]) && (itemName[0] <= 'Z')) {
                    itemName.resize(2);
                }

                if (itemName.compare(0, 2, L"\\\\") == 0) {
                    std::wstring rootName = std::filesystem::path(longPath).root_name().wstring();
                    if (_wcsicmp(rootName.c_str(), itemName.c_str()) == 0) {
                        break;
                    }
                }
                else if (_tcsnicmp(longPath.c_str(), itemName.c_str(), itemName.size()) == 0) {
                    break;
                }
                hFallbackItem = _hTreeCtrl.GetNextItem(hFallbackItem, TVGN_NEXT);
                if (hFallbackItem == nullptr) {
                    std::wstring rootStr = std::filesystem::path(longPath).root_name().wstring();
                    auto rootEntry = std::make_shared<ExplorerEntry>(rootStr, FileSystemEntry(rootStr, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
                    InsertChildFolder(rootEntry, TVI_ROOT, TVI_LAST, TRUE, FALSE, TRUE);
                }
            } while (hFallbackItem != nullptr);
        }
    }

    size_t idx = 0;
    for (const auto& segment : longPath) {
        if (segment != L"" && segment != L"\\") {
            if (idx >= skipSegments) {
                pathSegments.push_back(segment.wstring());
            }
            idx++;
        }
    }
    _pendingSelectPathSegments = std::move(pathSegments);

    ResumePendingSelection();

    return TRUE;
}

void ExplorerDialog::ResumePendingSelection()
{
    if (_pendingSelectPathSegments.empty()) {
        if (_pendingSelectRootItem != nullptr && _pendingSelectRootItem != TVI_ROOT) {
            _hTreeCtrl.SelectItem(_pendingSelectRootItem);
            _hTreeCtrl.EnsureVisible(_pendingSelectRootItem);
            updateDockingDlg();
            _pendingSelectRootItem = nullptr;
            _isSelNotifyEnable = TRUE;
        }
        return;
    }

    HTREEITEM hParent = (_pendingSelectRootItem != nullptr) ? _pendingSelectRootItem : TVI_ROOT;
    HTREEITEM hItem = _hTreeCtrl.GetChild(hParent);

    if (hItem == nullptr) {
        bool isLoaded = false;
        if (hParent == TVI_ROOT) {
            isLoaded = true; // roots are loaded synchronously
        } else {
            auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hParent));
            isLoaded = (pShared != nullptr && *pShared != nullptr && (*pShared)->HasLoadedChildren());
        }

        if (!isLoaded) {
            _hTreeCtrl.SelectItem(hParent);
            FetchChildren(hParent);
            return;
        }
    }

    HTREEITEM hItemSel = nullptr;
    bool isRootSegment = (hParent == TVI_ROOT);
    size_t segmentIdx = 0;

    while (hItem != nullptr && segmentIdx < _pendingSelectPathSegments.size()) {
        auto itemName = _hTreeCtrl.GetItemText(hItem);

        /* truncate item name if we are in root */
        if (isRootSegment && (('A' <= itemName[0]) && (itemName[0] <= 'Z'))) {
            itemName.resize(2);
        }

        const std::wstring& segment = _pendingSelectPathSegments[segmentIdx];
        if (segment == itemName) {
            isRootSegment = false;
            hItemSel = hItem;
            segmentIdx++;

            // If we still have segments to go down, we must check if children are loaded
            if (segmentIdx < _pendingSelectPathSegments.size()) {
                HTREEITEM hChild = _hTreeCtrl.GetChild(hItem);

                // If it is a UNC server node, check if the child segment is present.
                // If not, dynamically insert it!
                if (FileSystemService::IsUncServerPath(GetPath(hItem))) {
                    const std::wstring& childSegment = _pendingSelectPathSegments[segmentIdx];
                    HTREEITEM hFoundChild = nullptr;
                    HTREEITEM hChildIter = hChild;
                    while (hChildIter != nullptr) {
                        if (_hTreeCtrl.GetItemText(hChildIter) == childSegment) {
                            hFoundChild = hChildIter;
                            break;
                        }
                        hChildIter = _hTreeCtrl.GetNextItem(hChildIter, TVGN_NEXT);
                    }

                    if (hFoundChild == nullptr) {
                        // Dynamically insert child segment (the share name)
                        std::wstring sharePath = FileSystemService::CombinePath(GetPath(hItem), childSegment);
                        auto shareEntry = std::make_shared<ExplorerEntry>(sharePath, FileSystemEntry(childSegment, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
                        HTREEITEM hShareItem = InsertChildFolder(shareEntry, hItem, TVI_LAST, TRUE, FALSE, TRUE);

                        // Add to parent ExplorerEntry's children
                        auto* pParentShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
                        if (pParentShared != nullptr && *pParentShared != nullptr) {
                            auto parentEntry = *pParentShared;
                            auto children = parentEntry->Children();
                            children.push_back(shareEntry);
                            parentEntry->SetChildren(children);
                        }

                        hChild = hShareItem;
                    } else {
                        hChild = hFoundChild;
                    }
                }

                if (hChild == nullptr) {
                    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
                    bool isLoaded = (pShared != nullptr && *pShared != nullptr && (*pShared)->HasLoadedChildren());
                    if (!isLoaded) {
                        // Not loaded yet! Request async fetch and stop.
                        _hTreeCtrl.SelectItem(hItem);
                        FetchChildren(hItem);
                        return;
                    }
                }
                // Go to child
                hItem = hChild;
            }
        } else {
            /* search for next item in list */
            hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXT);
        }
    }

    // If we finished all segments successfully:
    if (segmentIdx == _pendingSelectPathSegments.size() && hItemSel != nullptr) {
        _hTreeCtrl.SelectItem(hItemSel);
        _hTreeCtrl.EnsureVisible(hItemSel);
        updateDockingDlg();
    }

    // Failure / Success / Mismatch: clear pending state and restore notifications
    _pendingSelectPathSegments.clear();
    _pendingSelectRootItem = nullptr;
    _isSelNotifyEnable = TRUE;
}

BOOL ExplorerDialog::GotoPath()
{
    /* newDlg is exactly what I need */
    NewDlg  dlg;
    WCHAR   szFolderName[MAX_PATH];
    WCHAR   szComment[] = L"Go to Path";
    BOOL    bResult     = FALSE;

    szFolderName[0] = '\0';

    /* copy current path to show current position */
    wcscpy(szFolderName, _pSettings->GetCurrentDir().c_str());

    dlg.init(_hInst, _hParent);
    for (;;) {
        if (dlg.doDialog(szFolderName, szComment) == TRUE) {
            /* test if is correct */
            if (std::filesystem::exists(szFolderName)) {
                if (szFolderName[wcslen(szFolderName) - 1] != '\\') {
                    wcscat(szFolderName, L"\\");
                }
                _viewModel->NavigateTo(szFolderName);
                bResult = TRUE;
                break;
            }

            INT msgRet = ::MessageBox(_hParent, L"Path doesn't exist.", L"Error", MB_RETRYCANCEL);
            if (msgRet == IDCANCEL) {
                break;
            }
        }
        else {
            break;
        }
    }
    return bResult;
}

void ExplorerDialog::GotoUserFolder()
{
    WCHAR pathName[MAX_PATH];

    if (SHGetSpecialFolderPath(nullptr, pathName, CSIDL_PROFILE, FALSE) == TRUE) {
        _viewModel->NavigateTo(pathName);
    }
    SetFocusOnFile();
}

void ExplorerDialog::GotoCurrentFolder()
{
    std::wstring currentDir = _pluginContext->GetCurrentDirectory().wstring();
    if (!currentDir.empty()) {
        _viewModel->NavigateTo(currentDir);
    }
    SetFocusOnFile();
}

void ExplorerDialog::GotoCurrentFile()
{
    if (_pSettings->IsUseFullTree()) {
        std::filesystem::path currentPath = _pluginContext->GetFullCurrentPath();
        if (std::filesystem::exists(currentPath)) {
            _viewModel->NavigateTo(currentPath.wstring());
        }
    }
    else {
        std::wstring currentDir = _pluginContext->GetCurrentDirectory().wstring();
        if (!currentDir.empty()) {
            _viewModel->NavigateTo(currentDir);
            _FileList.SelectCurFile();
        }
    }
}

void ExplorerDialog::GotoFileLocation(const std::wstring& filePath)
{
    if (_pSettings->IsUseFullTree()) {
        _viewModel->NavigateTo(filePath);
        SetFocusOnFile();
    }
    else {
        std::filesystem::path path(filePath);
        std::wstring dirPath = path.parent_path().wstring();
        std::wstring fileName = path.filename().wstring();

        _viewModel->NavigateTo(dirPath);
        _FileList.SelectFile(fileName);
        SetFocusOnFile();
    }
}

void ExplorerDialog::SetFocusOnFolder()
{
    ::SetFocus(_hTreeCtrl);
}

void ExplorerDialog::SetFocusOnFile()
{
    if (!_pSettings->IsUseFullTree()) {
        ::SetFocus(_FileList.getHSelf());
    }
    else {
        ::SetFocus(_hTreeCtrl);
    }
}

void ExplorerDialog::ClearFilter()
{
    _pSettings->GetFilterHistory().clear();
    _pSettings->GetFileFilter().setFilter(L"*.*");
    _ComboFilter.ClearComboList();
    _ComboFilter.AddText(L"*.*");
    _ComboFilter.SetText(L"*.*");
    _FileList.filterFiles(L"*.*");
    if (_pSettings->IsUseFullTree()) {
        RefreshTreeFilter();
    }
}

/**************************************************************************
 * Shortcut functions
 */
void ExplorerDialog::OnDelete(bool immediate)
{
    HTREEITEM hItem = _hTreeCtrl.GetSelection();
    auto path = GetPath(hItem);
    if (path.empty()) {
        return;
    }
    if (path.back() == L'\\') {
        path.pop_back();
    }

    if (FileSystemService::DeleteFiles(_hParent, { path }, immediate)) {
        HTREEITEM hParentItem = _hTreeCtrl.GetParent(hItem);
        if (hParentItem != nullptr) {
            _hTreeCtrl.SelectItem(hParentItem);

            // Recursively remove from _checkedItems so we prevent leaking handles
            auto RemoveFromCheckedItemsRecursive = [&](auto& self, HTREEITEM item) -> void {
                if (item == nullptr) return;
                _checkedItems.erase(item);
                HTREEITEM child = _hTreeCtrl.GetChild(item);
                while (child != nullptr) {
                    self(self, child);
                    child = _hTreeCtrl.GetNextItem(child, TVGN_NEXT);
                }
            };
            RemoveFromCheckedItemsRecursive(RemoveFromCheckedItemsRecursive, hItem);

            _hTreeCtrl.DeleteItem(hItem);
            FetchChildren(hParentItem);
        } else {
            Refresh();
        }
    }
}

void ExplorerDialog::OnCut()
{
    CIDataObject dataObj(nullptr);
    FolderExChange(nullptr, &dataObj, DROPEFFECT_MOVE);
}

void ExplorerDialog::OnCopy()
{
    CIDataObject dataObj(nullptr);
    FolderExChange(nullptr, &dataObj, DROPEFFECT_COPY);
}

void ExplorerDialog::OnPaste()
{
    /* Insure desired format is there, and open clipboard */
    if (!::IsClipboardFormatAvailable(CF_HDROP)) {
        return;
    }
    if (!::OpenClipboard(nullptr)) {
        return;
    }

    /* Get handle to Dropped Filelist data, and number of files */
    LPDROPFILES hFiles = (LPDROPFILES)::GlobalLock(::GetClipboardData(CF_HDROP));
    if (hFiles == nullptr) {
        ErrorMessage(::GetLastError());
        return;
    }
    LPBYTE hEffect = (LPBYTE)::GlobalLock(::GetClipboardData(::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)));
    if (hEffect == nullptr) {
        ErrorMessage(::GetLastError());
        return;
    }

    /* get target */
    auto filesTo = GetPath(_hTreeCtrl.GetSelection());

    if (hEffect[0] == 2) {
        DoPaste(filesTo.c_str(), hFiles, DROPEFFECT_MOVE);
    } else if (hEffect[0] == 5) {
        DoPaste(filesTo.c_str(), hFiles, DROPEFFECT_COPY);
    }
    ::GlobalUnlock(hFiles);
    ::GlobalUnlock(hEffect);
    ::CloseClipboard();

    ::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
    ::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 200, nullptr);
}

/**
 * UpdateDevices()
 */
void ExplorerDialog::UpdateRoots()
{
    _checkedItems.clear();
    auto root = _model->Root();
    if (!root) return;

    _hTreeCtrl.DeleteChildren(TVI_ROOT);

    auto drives = root->Children();
    for (const auto& driveEntry : drives) {
        std::wstring volumeName = driveEntry->FSEntry().Name();
        std::wstring drivePath = driveEntry->Path();
        bool haveChildren = FileSystemService::HaveChildren(drivePath, _pSettings->IsUseFullTree(), _pSettings->IsShowHidden());
        HTREEITEM hItem = InsertChildFolder(driveEntry, TVI_ROOT, TVI_LAST, TRUE, FALSE, haveChildren);
        if (hItem != nullptr && std::find(_expandedPaths.begin(), _expandedPaths.end(), drivePath) != _expandedPaths.end()) {
            _hTreeCtrl.Expand(hItem, TVE_EXPAND);
        }
    }
}

void ExplorerDialog::RebuildRoots()
{
    _expandedPaths.clear();
    CollectExpandedPaths(TVI_ROOT);
    _workerThread.Enqueue(std::make_unique<TaskInit>(_model, _pSettings));
}

void ExplorerDialog::CollectExpandedPaths(HTREEITEM hItem)
{
    HTREEITEM hChild = (hItem == TVI_ROOT || hItem == nullptr) ? _hTreeCtrl.GetRoot() : _hTreeCtrl.GetChild(hItem);
    while (hChild != nullptr) {
        if (_hTreeCtrl.IsItemExpanded(hChild)) {
            _expandedPaths.push_back(GetPath(hChild));
            CollectExpandedPaths(hChild);
        }
        hChild = _hTreeCtrl.GetNextItem(hChild, TVGN_NEXT);
    }
}

void ExplorerDialog::UpdateAllExpandedItems()
{
    HTREEITEM hCurrentItem = _hTreeCtrl.GetRoot();

    while (hCurrentItem != nullptr) {
        if (_hTreeCtrl.IsItemExpanded(hCurrentItem)) {
            FetchChildren(hCurrentItem);
        }
        hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
    }
}

void ExplorerDialog::UpdatePath()
{
    if (!_pSettings->IsUseFullTree()) {
        auto path = GetPath(_hTreeCtrl.GetSelection());
        _viewModel->NavigateTo(path, false);
    }
}

HTREEITEM ExplorerDialog::InsertChildFolder(std::shared_ptr<ExplorerEntry> entry, HTREEITEM parentItem, HTREEITEM insertAfter, BOOL isDirectory, BOOL isHidden, BOOL haveChildren)
{
    DevType devType = (parentItem == TVI_ROOT ? DEVT_DRIVE : DEVT_DIRECTORY);
    std::wstring pathStr = entry->Path();
    std::wstring childFolderName = entry->FSEntry().Name();

    /* insert item */
    INT iIconNormal     = ICON_FOLDER;
    INT iIconSelected   = ICON_FOLDER;
    INT iIconOverlayed  = 0;

    /* get icons */
    if (_pSettings->IsUseSystemIcons()) {
        GetIcons(pathStr, entry->FSEntry().Attributes(), &iIconNormal, &iIconSelected, &iIconOverlayed);
    }

    auto* pSharedEntry = new std::shared_ptr<ExplorerEntry>(entry);
    return _hTreeCtrl.InsertItem(childFolderName, iIconNormal, iIconSelected, iIconOverlayed, isHidden, parentItem, insertAfter, haveChildren, pSharedEntry);
}

BOOL ExplorerDialog::FindFolderAfter(LPCTSTR itemName, HTREEITEM pAfterItem)
{
    WCHAR       pszItem[MAX_PATH];
    BOOL        isFound = FALSE;
    HTREEITEM   hCurrentItem = _hTreeCtrl.GetNextItem(pAfterItem, TVGN_NEXT);

    while (hCurrentItem != nullptr) {
        _hTreeCtrl.GetItemText(hCurrentItem, pszItem, MAX_PATH);
        if (_tcscmp(itemName, pszItem) == 0) {
            isFound = TRUE;
            hCurrentItem = nullptr;
        }
        else {
            hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
        }
    }

    return isFound;
}


void ExplorerDialog::FetchChildren(HTREEITEM parentItem)
{
    auto parentFolderPath = GetPath(parentItem);
    auto tempEntry = std::make_shared<ExplorerEntry>(parentFolderPath, FileSystemEntry(parentFolderPath, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
    // Pass path as an explicit value-copy so the worker thread never dereferences
    // tempEntry->Path() across thread boundaries.
    _workerThread.Enqueue(std::make_unique<TaskUpdateDirectory>(_model, tempEntry, parentFolderPath, _pSettings));
}



std::wstring ExplorerDialog::GetPath(HTREEITEM currentItem) const
{
    if (currentItem == nullptr) {
        return {};
    }

    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(currentItem));
    if (pShared != nullptr && *pShared != nullptr) {
        return (*pShared)->Path();
    }

    return {};
}

HTREEITEM ExplorerDialog::FindTreeItemByPath(const std::wstring& path)
{
    auto FindTreeItemByPathRecursive = [&](const auto& self, HTREEITEM item) -> HTREEITEM {
        while (item != nullptr) {
            if (GetPath(item) == path) {
                return item;
            }

            HTREEITEM hChildItem = _hTreeCtrl.GetChild(item);
            HTREEITEM hFoundItem = self(self, hChildItem);
            if (hFoundItem != nullptr) {
                return hFoundItem;
            }

            item = _hTreeCtrl.GetNextItem(item, TVGN_NEXT);
        }
        return nullptr;
    };

    return FindTreeItemByPathRecursive(FindTreeItemByPathRecursive, _hTreeCtrl.GetRoot());
}

void ExplorerDialog::NotifyNewFile()
{
    if (isCreated()) {
        WCHAR currentDirectory[MAX_PATH]{};
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)currentDirectory);
        _ToolBar.enable(IDM_EX_GO_TO_FOLDER, (wcslen(currentDirectory) != 0));
    }
}

void ExplorerDialog::UpdateLayout()
{
    RECT rc = { 0 };
    RECT rcBuff = { 0 };

    INT filterHeight = 0;
    if (_hFilter) {
        RECT filterRect;
        ::GetWindowRect(_hFilter, &filterRect);
        filterHeight = filterRect.bottom - filterRect.top;
    }
    if (filterHeight <= 0) {
        filterHeight = GetSystemMetrics(SM_CYSMSIZE);
    }

    getClientRect(rc);

    if ((_iDockedPos == CONT_LEFT) || (_iDockedPos == CONT_RIGHT)) {
        INT splitterPos = _pSettings->GetSplitterPos();

        if (splitterPos < 50) {
            splitterPos = 50;
        }
        else if (splitterPos > (rc.bottom - 100)) {
            splitterPos = rc.bottom - 100;
        }

        /* set position of toolbar */
        _ToolBar.reSizeTo(rc);
        _Rebar.reSizeTo(rc);

        auto toolBarHeight = _ToolBar.getHeight();

        if (_pSettings->IsUseFullTree()) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= (toolBarHeight + filterHeight);
            ::SetWindowPos(_hTreeCtrl,      nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl,  nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl,      nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);

            getClientRect(rc);
            rc.top = rc.bottom - filterHeight;
            rc.bottom = filterHeight;
            ::SetWindowPos(_hFilter,        nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
        else {
            /* set position of tree control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom = splitterPos;
            ::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set splitter */
            getClientRect(rc);
            rc.top = (splitterPos + toolBarHeight);
            rc.bottom = 6;
            ::SetWindowPos(_hSplitterCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of list control */
            getClientRect(rc);
            rc.top = (splitterPos + toolBarHeight + 6);
            rc.bottom -= (splitterPos + toolBarHeight + 6 + filterHeight);
            ::SetWindowPos(_hListCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of filter controls */
            getClientRect(rc);

            /* set position of combo */
            rc.top = rc.bottom - filterHeight;
            rc.bottom = filterHeight;
            ::SetWindowPos(_hFilter, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
    else {
        INT splitterPos = _pSettings->GetSplitterPosHorizontal();

        if (splitterPos < 50) {
            splitterPos = 50;
        }
        else if (splitterPos > (rc.right - 50)) {
            splitterPos = rc.right - 50;
        }

        /* set position of toolbar */
        _ToolBar.reSizeTo(rc);
        _Rebar.reSizeTo(rc);

        auto toolBarHeight = _ToolBar.getHeight();

        if (_pSettings->IsUseFullTree()) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= (toolBarHeight + filterHeight);
            ::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);

            getClientRect(rc);
            rc.top = rc.bottom - filterHeight;
            rc.bottom = filterHeight;
            ::SetWindowPos(_hFilter, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
        else {
            /* set position of tree control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight + filterHeight;
            rc.right = splitterPos;
            ::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of filter controls */
            getClientRect(rc);
            rcBuff = rc;

            /* set position of combo */
            rc.top = rcBuff.bottom - filterHeight;
            rc.bottom = filterHeight;
            rc.left = rcBuff.left;
            rc.right = splitterPos - rcBuff.left;
            ::SetWindowPos(_hFilter, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set splitter */
            getClientRect(rc);
            rc.left = splitterPos;
            rc.right = 6;
            ::SetWindowPos(_hSplitterCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of list control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.left = splitterPos + 6;
            rc.right -= rc.left;
            ::SetWindowPos(_hListCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
}

/***************************************************************************************
 *  Drag'n'Drop, Cut and Copy of folders
 */
void ExplorerDialog::FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect)
{
    SIZE_T      bufsz = sizeof(DROPFILES) + sizeof(WCHAR);
    HTREEITEM   hItem = nullptr;

    /* get selected item */
    if (dwEffect == (DROPEFFECT_COPY | DROPEFFECT_MOVE)) {
        TVHITTESTINFO ht = {};
        ::GetCursorPos(&ht.pt);
        ::ScreenToClient(_hTreeCtrl, &ht.pt);
        hItem = _hTreeCtrl.HitTest(&ht);
    }
    else {
        hItem = _hTreeCtrl.GetSelection();
    }

    /* get buffer size */
    auto path = GetPath(hItem);
    if (path.back() == L'\\') {
        path.pop_back();
    }
    bufsz += (path.size() + 1) * sizeof(WCHAR);

    /* allocate global resources */
    HDROP hDrop = (HDROP)GlobalAlloc(GHND | GMEM_SHARE, bufsz);
    if (nullptr == hDrop) {
        return;
    }

    LPDROPFILES lpDropFileStruct = (LPDROPFILES)::GlobalLock(hDrop);
    if (nullptr == lpDropFileStruct) {
        GlobalFree(hDrop);
        return;
    }
    ::ZeroMemory(lpDropFileStruct, bufsz);

    lpDropFileStruct->pFiles = sizeof(DROPFILES);
    lpDropFileStruct->pt.x = 0;
    lpDropFileStruct->pt.y = 0;
    lpDropFileStruct->fNC = FALSE;
    lpDropFileStruct->fWide = TRUE;

    /* add path to payload */
    wcscpy((LPTSTR)&lpDropFileStruct[1], path.c_str());
    GlobalUnlock(hDrop);

    /* Init the supported format */
    FORMATETC fmtetc = {
        .cfFormat   = CF_HDROP,
        .dwAspect   = DVASPECT_CONTENT,
        .lindex     = -1,
        .tymed      = TYMED_HGLOBAL,
    };

    /* Init the medium used */
    STGMEDIUM medium = {
        .tymed      = TYMED_HGLOBAL,
        .hGlobal    = hDrop,
    };

    /* Add it to DataObject */
    pdobj->SetData(&fmtetc, &medium, TRUE);

    if (pdsrc == nullptr) {
        hDrop = (HDROP)GlobalAlloc(GHND|GMEM_SHARE, 4);
        if (nullptr == hDrop) {
            return;
        }

        LPBYTE prefCopyData = (LPBYTE)::GlobalLock(hDrop);
        if (nullptr == prefCopyData) {
            GlobalFree(hDrop);
            return;
        }

        int eff = (dwEffect == DROPEFFECT_MOVE ? 2 : 5);
        ::ZeroMemory(prefCopyData, 4);
        CopyMemory(prefCopyData, &eff, 1);
        ::GlobalUnlock(hDrop);

        /* Init the supported format */
        fmtetc.cfFormat = ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
        /* Init the medium used */
        medium.hGlobal  = hDrop;

        pdobj->SetData(&fmtetc, &medium, TRUE);

        if (::OleSetClipboard(pdobj) == S_OK) {
            ::OleFlushClipboard();
        }
    }
    else {
        /* Initiate the Drag & Drop */
        DWORD dwEffectResp;
        ::DoDragDrop(pdobj, pdsrc, dwEffect, &dwEffectResp);
    }
}

bool ExplorerDialog::OnDrop(FORMATETC* /* pFmtEtc */, STGMEDIUM& medium, DWORD* pdwEffect)
{
    LPDROPFILES hFiles = (LPDROPFILES)::GlobalLock(medium.hGlobal);
    if (nullptr == hFiles) {
        return false;
    }

    /* get target */
    auto path = GetPath(_hTreeCtrl.GetDropHilightItem());

    DoPaste(path.c_str(), hFiles, *pdwEffect);
    ::CloseClipboard();

    _hTreeCtrl.SelectDropTarget( NULL);

    return true;
}

void ExplorerDialog::NavigateBack()
{
    _viewModel->NavigateBack();
}

void ExplorerDialog::NavigateForward()
{
    _viewModel->NavigateForward();
}

void ExplorerDialog::NavigateTo(const std::wstring &path)
{
    _viewModel->NavigateTo(path);
}

void ExplorerDialog::Open(const std::wstring &path)
{
    if (!path.empty()) {
        HTREEITEM hItem = _hTreeCtrl.GetSelection();

        /* get current folder path */
        auto filePath = GetPath(hItem);
        filePath = FileSystemService::CombinePath(filePath, path);

        /* open possible link */
        std::wstring resolvedPath;
        if (FileSystemService::ResolveShortCut(filePath, resolvedPath)) {
            if (std::filesystem::is_directory(resolvedPath)) {
                _viewModel->NavigateTo(resolvedPath);
            }
            else {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)resolvedPath.c_str());
            }
        }
        else {
            ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)filePath.c_str());
        }
    }
}

void ExplorerDialog::Refresh()
{
    UpdateRoots();
    UpdateAllExpandedItems();
    UpdatePath();
    _viewModel->Refresh();
}

bool ExplorerDialog::DoPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect)
{
    /* get files from and to, to fill struct */
    UINT    headerSize      = sizeof(DROPFILES);
    SIZE_T  payloadSize     = ::GlobalSize(hData) - headerSize;
    LPVOID  pPld            = (LPBYTE)hData + headerSize;
    LPTSTR  lpszFilesFrom   = nullptr;

    if (((LPDROPFILES)hData)->fWide == TRUE) {
        lpszFilesFrom = (LPWSTR)pPld;
    } else {
        lpszFilesFrom = new WCHAR[payloadSize];
        ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pPld, (int)payloadSize, lpszFilesFrom, (int)payloadSize);
    }

    if (lpszFilesFrom != nullptr)
    {
        UINT count = 0;
        SIZE_T length = payloadSize;
        if (((LPDROPFILES)hData)->fWide == TRUE) {
            length = payloadSize / 2;
        }
        for (SIZE_T i = 0; i < length-1; i++) {
            if (lpszFilesFrom[i] == '\0') {
                count++;
            }
        }

        const std::wstring message = (dwEffect == DROPEFFECT_MOVE)
            ? std::format(L"Move {} file(s)/folder(s) to:\n\n{}", count, pszTo)
            : std::format(L"Copy {} file(s)/folder(s) to:\n\n{}", count, pszTo);

        if (::MessageBox(_hSelf, message.c_str(), L"Explorer", MB_YESNO) == IDYES) {
            // TODO move or copy the file views into other window in dependency to keystate
            SHFILEOPSTRUCT fileOp = {
                .hwnd   = _hParent,
                .wFunc  = static_cast<UINT>((dwEffect == DROPEFFECT_MOVE) ? FO_MOVE : FO_COPY),
                .pFrom  = lpszFilesFrom,
                .pTo    = pszTo,
                .fFlags = FOF_RENAMEONCOLLISION,
            };
            SHFileOperation(&fileOp);

            ::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
            ::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 200, nullptr);
        }

        if (((LPDROPFILES)hData)->fWide == FALSE) {
            delete [] lpszFilesFrom;
        }
    }
    return true;
}

void ExplorerDialog::ShowContextMenu(POINT screenLocation, const std::vector<std::shared_ptr<ExplorerEntry>>& entries, bool hasStandardMenu)
{
    ContextMenu cm(_pluginContext);
    cm.SetObjects(entries);
    cm.ShowContextMenu(_hInst, _hParent, _hSelf, screenLocation, hasStandardMenu);
}

void ExplorerDialog::OnEntryUpdated(std::shared_ptr<ExplorerEntry> entry) {
    if (!isCreated()) return;

    if (entry == _model->Root()) {
        UpdateRoots();
        _viewModel->NavigateTo(_pSettings->GetCurrentDir(), true);
        OnCurrentDirectoryChanged(_pSettings->GetCurrentDir());
        NotifyNewFile();
    } else {
        HTREEITEM hItem = FindTreeItemByPath(entry->Path());

        if (hItem != nullptr) {
            auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
            if (pShared != nullptr) {
                *pShared = entry;
            }

            // Delegate the full insert/update/delete diff logic to the
            // dedicated synchronizer. ExplorerDialog retains responsibility
            // for the UI state that follows the structural sync.
            TreeModelSynchronizer::Synchronize(*this, _hTreeCtrl, hItem, entry, _pSettings, _workerThread);

            // Schedule has-children checks for all visible direct children
            HTREEITEM child = _hTreeCtrl.GetChild(hItem);
            while (child != nullptr) {
                auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(child));
                if (pShared != nullptr && *pShared != nullptr && (*pShared)->FSEntry().IsDirectory()) {
                    std::wstring childPath = (*pShared)->Path();
                    if (std::find(_expandedPaths.begin(), _expandedPaths.end(), childPath) != _expandedPaths.end()) {
                        _hTreeCtrl.Expand(child, TVE_EXPAND);
                    }
                    else if (!_hTreeCtrl.IsItemExpanded(child) && _hTreeCtrl.GetChild(child) == nullptr) {
                        RECT rect;
                        if (_hTreeCtrl.GetItemRect(child, &rect, FALSE)) {
                            if (_checkedItems.find(child) == _checkedItems.end()) {
                                _checkedItems.insert(child);
                                EnqueueAsyncTask(std::make_unique<TaskCheckFolderChildren>(this, child, childPath, _pSettings));
                            }
                        }
                    }
                }
                child = _hTreeCtrl.GetNextItem(child, TVGN_NEXT);
            }

            // Auto-expand a node that was pending expansion when FetchChildren was called
            if (hItem == _hItemExpand) {
                _hTreeCtrl.Expand(hItem, TVE_EXPAND);
                _hItemExpand = nullptr;
            }

            ResumePendingSelection();
            CheckVisibleFolderChildren();
        }
    }
}

void ExplorerDialog::OnFolderChildrenChecked(HTREEITEM hItem, const std::wstring& path, bool hasChildren)
{
    if (GetPath(hItem) == path) {
        if (FileSystemService::IsUncServerPath(path)) {
            _hTreeCtrl.SetItemHasChildren(hItem, TRUE);
        } else {
            _hTreeCtrl.SetItemHasChildren(hItem, hasChildren);
        }
    }
}

void ExplorerDialog::OnEntryRenamed(const std::wstring& oldPath, const std::wstring& newPath, const std::wstring& newName) {
    if (!isCreated()) return;

    HTREEITEM hItem = FindTreeItemByPath(oldPath);
    if (hItem != nullptr) {
        auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
        if (pShared != nullptr && *pShared != nullptr) {
            (*pShared)->Rename(newPath, newName);

            // Elegantly set only the item text! Win32 TreeView preserves all other states!
            _hTreeCtrl.SetItemText(hItem, newName);

            HTREEITEM hParentItem = _hTreeCtrl.GetParent(hItem);
            if (hParentItem != nullptr) {
                FetchChildren(hParentItem);
            }
        }
    }

    _viewModel->OnParentDirectoryRenamed(oldPath, newPath);
}

void ExplorerDialog::RefreshActiveNode()
{
    if (!isCreated()) return;

    HTREEITEM hItem = _hTreeCtrl.GetSelection();
    if (hItem != nullptr) {
        FetchChildren(hItem);
        HTREEITEM hParent = _hTreeCtrl.GetParent(hItem);
        if (hParent != nullptr) {
            FetchChildren(hParent);
        }
    }
    Refresh();
}

void ExplorerDialog::RefreshTreeFilter()
{
    if (!isCreated()) return;

    HTREEITEM hRoot = _hTreeCtrl.GetRoot();
    while (hRoot != nullptr) {
        RefreshTreeFilter(hRoot);
        hRoot = _hTreeCtrl.GetNextItem(hRoot, TVGN_NEXT);
    }
}

void ExplorerDialog::RefreshTreeFilter(HTREEITEM hItem)
{
    if (hItem == nullptr) {
        return;
    }

    auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
    if (pShared != nullptr && *pShared != nullptr && (*pShared)->HasLoadedChildren()) {
        TreeModelSynchronizer::Synchronize(*this, _hTreeCtrl, hItem, *pShared, _pSettings, _workerThread);
    }

    HTREEITEM hChild = _hTreeCtrl.GetChild(hItem);
    while (hChild != nullptr) {
        RefreshTreeFilter(hChild);
        hChild = _hTreeCtrl.GetNextItem(hChild, TVGN_NEXT);
    }
}


void ExplorerDialog::CheckVisibleFolderChildren()
{
    HTREEITEM hItem = _hTreeCtrl.GetNextItem(nullptr, TVGN_FIRSTVISIBLE);
    bool seenVisible = false;

    while (hItem != nullptr) {
        RECT rect;
        if (!_hTreeCtrl.GetItemRect(hItem, &rect, FALSE)) {
            if (seenVisible) {
                break;
            }
        }
        else {
            seenVisible = true;

            auto* pShared = reinterpret_cast<std::shared_ptr<ExplorerEntry>*>(_hTreeCtrl.GetParam(hItem));
            if (pShared != nullptr && *pShared != nullptr && (*pShared)->FSEntry().IsDirectory()) {
                if (!_hTreeCtrl.IsItemExpanded(hItem) && _hTreeCtrl.GetChild(hItem) == nullptr) {
                    if (_checkedItems.find(hItem) == _checkedItems.end()) {
                        _checkedItems.insert(hItem);
                        std::wstring childPath = GetPath(hItem);
                        EnqueueAsyncTask(std::make_unique<TaskCheckFolderChildren>(this, hItem, childPath, _pSettings));
                    }
                }
            }
        }

        hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXTVISIBLE);
    }
}

void ExplorerDialog::OnCurrentDirectoryChanged(const std::wstring& path)
{
    HTREEITEM hSel = _hTreeCtrl.GetSelection();
    if (hSel != nullptr) {
        const std::wstring selected_path = GetPath(hSel);
        // Already selected
        if (selected_path == path) {
            return; 
        }
    }

    SelectItem(path);
}

void ExplorerDialog::OnNavigationStateChanged()
{
    _ToolBar.enable(IDM_EX_PREV, _viewModel->CanNavigateBack());
    _ToolBar.enable(IDM_EX_NEXT, _viewModel->CanNavigateForward());
}
