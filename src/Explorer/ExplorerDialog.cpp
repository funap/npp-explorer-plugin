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
#include <shlwapi.h>
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
#include "Editor.h"
#include "resource.h"
#include "ThemeRenderer.h"
#include "FileSystemService.h"

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
    {IDM_EX_FILE_NEW,       IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FILENEW,     0},
    {IDM_EX_FOLDER_NEW,     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERNEW,   0},
    {IDM_EX_SEARCH_FIND,    IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FIND,        0},
    {IDM_EX_TERMINAL,       IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_TERMINAL,    0},
    {IDM_EX_GO_TO_USER,     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERUSER,  0},
    {IDM_EX_GO_TO_FOLDER,   IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_FOLDERGO,    0},
    {0,                     IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
    {IDM_EX_UPDATE,         IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, IDB_EX_UPDATE,      0}
};


LPCTSTR GetNameStrFromCmd(UINT resourceId)
{
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
    , _bStartupFinish(FALSE)
    , _hExploreVolumeThread(nullptr)
    , _hItemExpand(nullptr)
    , _hDefaultTreeProc(nullptr)
    , _hDefaultSplitterProc(nullptr)
    , _bOldRectInitialized(FALSE)
    , _isSelNotifyEnable(TRUE)
    , _hListCtrl(nullptr)
    , _hHeader(nullptr)
    , _hSplitterCtrl(nullptr)
    , _hFilter(nullptr)
    , _FileList(this)
    , _ptOldPos()
    , _ptOldPosHorizontal()
    , _isLeftButtonDown(FALSE)
    , _hSplitterCursorUpDown(nullptr)
    , _hSplitterCursorLeftRight(nullptr)
    , _pSettings(nullptr)
    , _hCurWait(nullptr)
    , _isScrolling(FALSE)
    , _isDnDStarted(FALSE)
    , _iDockedPos(CONT_LEFT)
    , _model(std::make_shared<ExplorerModel>())
{
}

ExplorerDialog::~ExplorerDialog()
{
    _workerThread.Stop();
    if (_model) _model->RemoveObserver(this);
}


void ExplorerDialog::init(HINSTANCE hInst, HWND hParent, Settings *prop)
{
    DockingDlgInterface::init(hInst, hParent);

    _pSettings = prop;
    _FileList.initProp(prop);
}

void ExplorerDialog::redraw()
{
    UpdateLayout();

    /* possible new imagelist -> update the window */
    _hTreeCtrl.SetImageList(GetSmallImageList(_pSettings->IsUseSystemIcons()));
    ::SetTimer(_hSelf, EXT_UPDATEDEVICE, 0, nullptr);
    _FileList.redraw();
    ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);

    /* and only when dialog is visible, select item again */
    SelectItem(_pSettings->GetCurrentDir());

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
    case WM_INITDIALOG: {
        InitialDialog();
        _model->AddObserver(this);

        // EID_INIT -> TaskInit
        _workerThread.Enqueue(std::make_unique<TaskInit>(_model, _pSettings));
        break;
    }
    case WM_COMMAND:  {
        if (((HWND)lParam == _hFilter) && (HIWORD(wParam) == CBN_SELCHANGE)) {
            ::SendMessage(_hSelf, EXM_CHANGECOMBO, 0, 0);
            return TRUE;
        }
        if ((HWND)lParam == _ToolBar.getHSelf()) {
            tb_cmd(LOWORD(wParam));
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
                    const auto path = GetPath(item);
                    if (!PathIsDirectory(path.c_str())) {
                        Editor::Instance().DoOpen(path);
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
                    const auto path = GetPath(item);
                    ShowContextMenu(pt, {path});
                }
                return TRUE;
            }
            case TVN_SELCHANGED: {
                HTREEITEM item = _hTreeCtrl.GetSelection();
                if (item != nullptr) {
                    std::filesystem::path path = GetPath(item);
                    if (std::filesystem::is_regular_file(path)) {
                        path = path.parent_path();
                        path += "\\";
                    }
                    /* save current path */
                    _pSettings->SetCurrentDir(path.wstring());
                    DebugPrintf(L"pwd:{}", _pSettings->GetCurrentDir().c_str());
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
                        if (!_hTreeCtrl.GetChild(_hItemExpand)) {
                            FetchChildren(_hItemExpand);
                        } else {
                            const auto path = GetPath(_hItemExpand);
                            UpdateChildren(path, _hItemExpand);
                        }
                    }
                } else {
                    _hItemExpand = nullptr;
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
            tb_not((LPNMTOOLBAR)lParam);
            return TBDDRET_NODEFAULT;
        }
        else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED)) {
            NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
            if (lpnm->wID == REBAR_BAR_TOOLBAR) {
                POINT pt;
                pt.x = lpnm->rc.left;
                pt.y = lpnm->rc.bottom;
                ClientToScreen(nmhdr->hwndFrom, &pt);
                tb_cmd(_ToolBar.doPopop(pt));
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
        break;
    case WM_PAINT:
        ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
        break;
    case WM_DESTROY: {
        WCHAR szLastFilter[MAX_PATH];

        _pSettings->SetFilterHistory(_ComboFilter.getComboList());
        _ComboFilter.getText(szLastFilter, MAX_PATH);
        if (wcslen(szLastFilter) != 0) {
            _pSettings->GetFileFilter().setFilter(szLastFilter);
        }

        if (::WaitForSingleObject(_hExploreVolumeThread, 50) != WAIT_OBJECT_0) {
            ::Sleep(1);
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
        WCHAR searchWords[MAX_PATH] = {};
        if (_ComboFilter.getSelText(searchWords)) {
            _FileList.filterFiles(searchWords);
        }
        else {
            _FileList.filterFiles(L"*");
        }
        return TRUE;
    }
    case EXM_OPENDIR:
        NavigateTo((LPTSTR)lParam);
        return TRUE;
    case EXM_USER_ICONBAR:
        tb_cmd(wParam);
        return TRUE;
    case EXM_ASYNCTASK_COMPLETED: {
        std::unique_ptr<IAsyncTask> task(reinterpret_cast<IAsyncTask*>(wParam));
        task->OnCompleted();
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
            UpdateAllExpandedItems(); UpdatePath();
            return FALSE;
        }
        if (wParam == EXT_UPDATEACTIVATEPATH) {
            ::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
            {
        HTREEITEM hItem         = _hTreeCtrl.GetSelection();
        HTREEITEM hParentItem   = _hTreeCtrl.GetParent(hItem);

        if (hParentItem != nullptr) {
            auto path = GetPath(hParentItem);
            UpdateChildren(path, hParentItem, FALSE);
        }
        if (hItem != nullptr) {
            auto path = GetPath(hItem);
            UpdateChildren(path, hItem, FALSE);
            UpdatePath();
        }
    }
            return FALSE;
        }
        if (wParam == EXT_AUTOGOTOFILE) {
            ::KillTimer(_hSelf, EXT_AUTOGOTOFILE);
            gotoCurrentFile();
            return FALSE;
        }
        if (wParam == EXT_SELCHANGE) {
            ::KillTimer(_hSelf, EXT_SELCHANGE);
            _FileList.viewPath(_pSettings->GetCurrentDir(), TRUE);
            updateDockingDlg();
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
LRESULT ExplorerDialog::runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_GETDLGCODE: {
        return DLGC_WANTALLKEYS | ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
    }
    case WM_CHAR: {
        /* do selection of items by user keyword typing or cut/copy/paste */
        switch (wParam) {
        case SHORTCUT_CUT:
            onCut();
            return TRUE;
        case SHORTCUT_COPY:
            onCopy();
            return TRUE;
        case SHORTCUT_PASTE:
            onPaste();
            return TRUE;
        case SHORTCUT_DELETE:
            onDelete();
            return TRUE;
        case SHORTCUT_REFRESH:
            Refresh();
            return TRUE;
        case VK_RETURN: {
            /* toggle item on return */
            HTREEITEM hItem = _hTreeCtrl.GetSelection();
            const auto path = GetPath(hItem);
            if (PathIsDirectory(path.c_str())) {
                if (_hTreeCtrl.GetChild(hItem) == nullptr) {
                    FetchChildren(hItem);
                }
                _hTreeCtrl.Expand(hItem, TVE_TOGGLE);
            }
            else {
                Editor::Instance().DoOpen(path);
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
            return TRUE;
        default:
            break;
        }
        break;
    }
    case WM_KEYUP:
        if (VK_APPS == wParam) {
            HTREEITEM item = _hTreeCtrl.GetSelection();
            RECT rect{};
            _hTreeCtrl.GetItemRect(item, &rect, TRUE);
            ::ClientToScreen(_hTreeCtrl, &rect);
            const POINT pt{
                .x = rect.right,
                .y = rect.bottom,
            };
            const auto path = GetPath(item);
            ShowContextMenu(pt, {path});
            return TRUE;
        }
        break;
    case WM_KEYDOWN: {
        if ((wParam == VK_DELETE) && !((0x8000 & ::GetKeyState(VK_CONTROL)) == 0x8000)) {
            onDelete((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000);
            return TRUE;
        }
        if (wParam == VK_F5) {
            Refresh();
            return TRUE;
        }
        if (VK_ESCAPE == wParam) {
            Editor::Instance().SetFocusToCurrentEdit();
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
                RECT rect{};
                _hTreeCtrl.GetItemRect(item, &rect, TRUE);
                ::ClientToScreen(_hTreeCtrl, &rect);
                const POINT pt{
                    .x = rect.right,
                    .y = rect.bottom,
                };
                const auto path = GetPath(item);
                ShowContextMenu(pt, {path});
                return TRUE;
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

    return ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
}

/****************************************************************************
 * Message handling of header
 */
LRESULT ExplorerDialog::runSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
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

void ExplorerDialog::tb_cmd(WPARAM message)
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
        Editor::Instance().LaunchFindFileDialog(_pSettings->GetCurrentDir());
        break;
    case IDM_EX_GO_TO_USER:
        gotoUserFolder();
        break;
    case IDM_EX_GO_TO_FOLDER:
        gotoCurrentFile();
        break;
    case IDM_EX_TERMINAL:
        openTerminal();
        break;
    case IDM_EX_FAVORITES:
        toggleFavesDialog();
        break;
    case IDM_EX_UPDATE:
        Refresh();
        break;
    default:
        break;
    }
}

void ExplorerDialog::tb_not(LPNMTOOLBAR lpnmtb)
{
    INT iElements = 0;

    _FileList.ToggleStackRec();

    /* get element cnt */
    if (lpnmtb->iItem == IDM_EX_PREV) {
        iElements = _FileList.GetPrevDirs(nullptr);
    } else if (lpnmtb->iItem == IDM_EX_NEXT) {
        iElements = _FileList.GetNextDirs(nullptr);
    }

    /* allocate elements */
    LPTSTR  *pszPathes = (LPTSTR*)new LPTSTR[iElements];
    for (size_t i = 0; i < iElements; i++) {
        pszPathes[i] = (LPTSTR)new WCHAR[MAX_PATH];
    }

    /* get directories */
    if (lpnmtb->iItem == IDM_EX_PREV) {
        _FileList.GetPrevDirs(pszPathes);
    } else if (lpnmtb->iItem == IDM_EX_NEXT) {
        _FileList.GetNextDirs(pszPathes);
    }

    POINT pt    = {0};
    HMENU hMenu = ::CreatePopupMenu();

    /* test are folder exist */
    for (INT i = 0; i < iElements; i++) {
        if (::PathFileExists(pszPathes[i])) {
            ::AppendMenu(hMenu, MF_STRING, i+1, pszPathes[i]);
        }
    }

    ::GetCursorPos(&pt);
    INT cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hSelf, nullptr);
    ::DestroyMenu(hMenu);
    _Rebar.redraw();

    /* select element */
    if (cmd) {
        std::vector<std::wstring> vStrItems;

        SelectItem(pszPathes[cmd-1]);
        _FileList.OffsetItr(lpnmtb->iItem == IDM_EX_PREV ? -cmd : cmd, vStrItems);

        if (!vStrItems.empty()) {
            _FileList.SetItems(vStrItems);
        }
    }

    for (size_t i = 0; i < iElements; i++) {
        delete [] pszPathes[i];
    }
    delete [] pszPathes;

    _FileList.ToggleStackRec();
}

void ExplorerDialog::InitialDialog()
{
    _workerThread.Start(this);

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
    _hDefaultTreeProc = (WNDPROC)::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)wndDefaultTreeProc);

    /* subclass splitter */
    _hSplitterCursorUpDown      = ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_UPDOWN));
    _hSplitterCursorLeftRight   = ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_LEFTRIGHT));
    ::SetWindowLongPtr(_hSplitterCtrl, GWLP_USERDATA, (LONG_PTR)this);
    _hDefaultSplitterProc = (WNDPROC)::SetWindowLongPtr(_hSplitterCtrl, GWLP_WNDPROC, (LONG_PTR)wndDefaultSplitterProc);

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
    _ComboFilter.init(_hFilter, _hSelf);

    /* load cursor */
    _hCurWait = ::LoadCursor(nullptr, IDC_WAIT);

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
            Editor::Instance().SetFocusToCurrentEdit();
            return TRUE;
        default:
            break;
        }
        return FALSE;
    });

    _ComboFilter.setDefaultOnCharHandler([this](UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */) -> BOOL {
        switch (nChar) {
        case VK_TAB:
            if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
                ::SetFocus(_hListCtrl);
            }
            else {
                ::SetFocus(_hTreeCtrl);
            }
            return TRUE;
        case VK_ESCAPE:
            Editor::Instance().SetFocusToCurrentEdit();
            return TRUE;
        default:
            break;
        }
        return FALSE;
    });
}

void ExplorerDialog::OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task)
{
    PostMessage(_hSelf, EXM_ASYNCTASK_COMPLETED, reinterpret_cast<WPARAM>(task.release()), 0);
}

void ExplorerDialog::SetFont(HFONT font)
{
    ::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
    ::SendMessage(_hListCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

BOOL ExplorerDialog::SelectItem(const std::filesystem::path& path)
{
    BOOL        folderExists    = FALSE;
    BOOL        isRoot          = TRUE;
    HTREEITEM   hItem           = _hTreeCtrl.GetRoot();
    HTREEITEM   hItemSel        = nullptr;
    HTREEITEM   hItemUpdate     = nullptr;

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

    std::list<std::wstring> pathSegments;
    for (const auto& segment : longPath) {
        if (segment != L"" && segment != L"\\") {
            pathSegments.push_back(segment.wstring());
        }
    }

    if (PathIsNetworkPath(longPath.c_str()) && pathSegments.size() >= 2) {
        auto it = pathSegments.begin();
        std::wstring root = *it++;
        root.append(L"\\");
        root.append(*it);
        pathSegments.erase(it);
        pathSegments.front() = root;
    }

    if (pathSegments.empty()) {
        return folderExists;
    }

    /* test if folder exists */
    folderExists = ::PathFileExists(longPath.c_str());
    if (!folderExists) {
        return folderExists;
    }

    /* disabled detection of TVN_SELCHANGED notification */
    _isSelNotifyEnable = FALSE;

    // mount the root path if it is unmounted
    if (PathIsNetworkPath(longPath.c_str())) {
        do {
            auto itemName = _hTreeCtrl.GetItemText(hItem);

            // truncate item name if we are in root
            if (('A' <= itemName[0]) && (itemName[0] <= 'Z')) {
                itemName.resize(2);
            }

            // compare path names
            if (_tcsnicmp(longPath.c_str(), itemName.c_str(), itemName.size()) == 0) {
                // already mounted
                break;
            }
            hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXT);
            if (hItem == nullptr) {
                // longPath is not mounted, add root item
                WCHAR root[MAX_PATH] = {};
                wcsncpy_s(root, longPath.c_str(), MAX_PATH);
                ::PathStripToRoot(root);
                InsertChildFolder(root, TVI_ROOT, TVI_LAST, 1);
            }
        } while (hItem != nullptr);
    }

    // expand select item
    hItem = _hTreeCtrl.GetRoot();
    do {
        if (pathSegments.empty()) {
            break;
        }
        auto itemName = _hTreeCtrl.GetItemText(hItem);

        /* truncate item name if we are in root */
        if (isRoot == TRUE && (('A' <= itemName[0]) && (itemName[0] <= 'Z'))) {
            itemName.resize(2);
        }

        /* compare path names */
        const std::wstring &segment = pathSegments.front();
        if (segment == itemName) {
            /* only on first case it is a root */
            isRoot = FALSE;
            pathSegments.pop_front();

            /* found -> store item for correct selection */
            hItemSel = hItem;

            /* expand, if possible and get child item */
            if (_hTreeCtrl.GetChild(hItem) == nullptr) {
                /* if no child item available, draw them */
                _hTreeCtrl.SelectItem(hItem);
                FetchChildren(hItem);
            }
            hItem = _hTreeCtrl.GetChild(hItem);
        } else {
            /* search for next item in list */
            hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXT);
        }

        /* try again, maybe there is only an update needed */
        if ((hItem == nullptr) && (hItemUpdate != hItemSel)) {
            _hTreeCtrl.Expand(hItemSel, TVE_EXPAND);
            hItemUpdate = hItemSel;
            auto selectedPath = GetPath(hItemSel);
            UpdateChildren(selectedPath, hItemSel, FALSE);
            hItem = _hTreeCtrl.GetChild(hItemSel);
        }
    } while (hItem != nullptr);

    /* view path */
    if (hItemSel != nullptr) {
        /* select last selected item */
        _hTreeCtrl.SelectItem(hItemSel);
        _hTreeCtrl.EnsureVisible(hItemSel);

        _FileList.viewPath(_pSettings->GetCurrentDir(), TRUE);
        updateDockingDlg();
    }

    /* enable detection of TVN_SELCHANGED notification */
    _isSelNotifyEnable = TRUE;

    return folderExists;
}

BOOL ExplorerDialog::gotoPath()
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
            if (::PathFileExists(szFolderName)) {
                if (szFolderName[wcslen(szFolderName) - 1] != '\\') {
                    wcscat(szFolderName, L"\\");
                }
                SelectItem(szFolderName);
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

void ExplorerDialog::gotoUserFolder()
{
    WCHAR pathName[MAX_PATH];

    if (SHGetSpecialFolderPath(nullptr, pathName, CSIDL_PROFILE, FALSE) == TRUE) {
        wcscat(pathName, L"\\");
        SelectItem(pathName);
    }
    setFocusOnFile();
}

void ExplorerDialog::gotoCurrentFolder()
{
    std::wstring currentDir = Editor::Instance().GetCurrentDirectory().wstring();
    if (!currentDir.empty()) {
        if (currentDir.back() != '\\') currentDir.push_back('\\');
        SelectItem(currentDir);
    }
    setFocusOnFile();
}

void ExplorerDialog::gotoCurrentFile()
{
    if (_pSettings->IsUseFullTree()) {
        std::filesystem::path currentPath = Editor::Instance().GetFullCurrentPath();
        if (PathFileExists(currentPath.c_str())) {
            SelectItem(currentPath.wstring());
        }
    }
    else {
        std::wstring currentDir = Editor::Instance().GetCurrentDirectory().wstring();
        if (!currentDir.empty()) {
            if (currentDir.back() != '\\') currentDir.push_back('\\');
            SelectItem(currentDir);
            _FileList.SelectCurFile();
            setFocusOnFile();
        }
    }
}

void ExplorerDialog::gotoFileLocation(const std::wstring& filePath)
{
    SelectItem(filePath);

    std::wstring fileName = filePath.substr(filePath.find_last_of(L'\\') + 1);
    _FileList.SelectFile(fileName);

    setFocusOnFile();
}

void ExplorerDialog::setFocusOnFolder()
{
    ::SetFocus(_hTreeCtrl);
}

void ExplorerDialog::setFocusOnFile()
{
    if (!_pSettings->IsUseFullTree()) {
        ::SetFocus(_FileList.getHSelf());
    }
    else {
        ::SetFocus(_hTreeCtrl);
    }
}

void ExplorerDialog::clearFilter()
{
    _pSettings->GetFilterHistory().clear();
    _pSettings->GetFileFilter().setFilter(L"*.*");
    _ComboFilter.clearComboList();
    _ComboFilter.addText(L"*.*");
    _ComboFilter.setText(L"*.*");
    _FileList.filterFiles(L"*.*");
}

/**************************************************************************
 * Shortcut functions
 */
void ExplorerDialog::onDelete(bool immediate)
{
    auto path = GetPath(_hTreeCtrl.GetSelection());
    if (path.empty()) {
        return;
    }
    if (path.back() == L'\\') {
        path.pop_back();
    }

    FileSystemService::DeleteFiles(_hParent, { path }, immediate);
}

void ExplorerDialog::onCut()
{
    CIDataObject dataObj(nullptr);
    FolderExChange(nullptr, &dataObj, DROPEFFECT_MOVE);
}

void ExplorerDialog::onCopy()
{
    CIDataObject dataObj(nullptr);
    FolderExChange(nullptr, &dataObj, DROPEFFECT_COPY);
}

void ExplorerDialog::onPaste()
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
        doPaste(filesTo.c_str(), hFiles, DROPEFFECT_MOVE);
    } else if (hEffect[0] == 5) {
        doPaste(filesTo.c_str(), hFiles, DROPEFFECT_COPY);
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
    auto root = _model->Root();
    if (!root) return;

    HTREEITEM hCurrentItem = _hTreeCtrl.GetNextItem(TVI_ROOT, TVGN_CHILD);

    auto drives = root->Children();

    for (const auto& driveEntry : drives) {
        std::wstring volumeName = driveEntry->FSEntry().Name();
        std::wstring drivePath = driveEntry->Path();
        wchar_t driveLetter = drivePath[0];

        bool haveChildren = FileSystemService::HaveChildren(drivePath, _pSettings->IsUseFullTree(), _pSettings->IsShowHidden());

        if (hCurrentItem != nullptr) {
            auto currentItemName = _hTreeCtrl.GetItemText(hCurrentItem);
            if (volumeName == currentItemName) {
                // if names are equal, go to next item in tree
                int iIconNormal = 0;
                int iIconSelected = 0;
                int iIconOverlayed = 0;
                ExtractIcons(drivePath.c_str(), nullptr, DEVT_DRIVE, &iIconNormal, &iIconSelected);
                _hTreeCtrl.UpdateItem(hCurrentItem, volumeName, iIconNormal, iIconSelected, 0, 0, haveChildren);
                _workerThread.Enqueue(std::make_unique<TaskGetCompleteIconTree>(_hSelf, this, hCurrentItem, drivePath, DEVT_DRIVE));
                hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
            }
            else if (!currentItemName.empty() && (driveLetter == currentItemName.front())) {
                // if names are not the same but the drive letter are equal, rename item
                int iIconNormal = 0;
                int iIconSelected = 0;
                int iIconOverlayed = 0;
                ExtractIcons(drivePath.c_str(), nullptr, DEVT_DRIVE, &iIconNormal, &iIconSelected);
                _hTreeCtrl.UpdateItem(hCurrentItem, volumeName, iIconNormal, iIconSelected, 0, 0, haveChildren);
                _workerThread.Enqueue(std::make_unique<TaskGetCompleteIconTree>(_hSelf, this, hCurrentItem, drivePath, DEVT_DRIVE));
                _hTreeCtrl.DeleteChildren(hCurrentItem);
                hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
            }
            else {
                // insert the device when new and not present before
                HTREEITEM hItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_PREVIOUS);
                InsertChildFolder(volumeName, TVI_ROOT, hItem, true);
            }
        }
        else {
            InsertChildFolder(volumeName, TVI_ROOT, TVI_LAST, true);
        }
    }

}

void ExplorerDialog::UpdateAllExpandedItems()
{
    HTREEITEM hCurrentItem = _hTreeCtrl.GetChild(TVI_ROOT);

    while (hCurrentItem != nullptr) {
        if (_hTreeCtrl.IsItemExpanded(hCurrentItem)) {
            auto path = _hTreeCtrl.GetItemText(hCurrentItem);
            if (!path.empty() && (L'A' <= path.front() && path.front() <= L'Z')) {
                path.resize(2);
            }
            UpdateChildren(path, hCurrentItem);
        }
        hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
    }
}

void ExplorerDialog::UpdatePath()
{
    if (!_pSettings->IsUseFullTree()) {
        auto path = GetPath(_hTreeCtrl.GetSelection());
        _FileList.ToggleStackRec();
        _FileList.viewPath(path);
        _FileList.ToggleStackRec();
    }
}

HTREEITEM ExplorerDialog::InsertChildFolder(const std::wstring& childFolderName, HTREEITEM parentItem, HTREEITEM insertAfter, BOOL bChildrenTest)
{
    /* We search if it already exists */
    BOOL    bHidden = FALSE;
    DevType devType = (parentItem == TVI_ROOT ? DEVT_DRIVE : DEVT_DIRECTORY);

    /* get name of parent path and merge it */
    auto path = GetPath(parentItem);
    path.append(childFolderName);

    if (parentItem == TVI_ROOT) {
        if (('A' <= path.front()) && (path.front() <= 'Z')) {
            path.resize(2);
        }
    }
    else {
        /* get only hidden icon when folder is not a device */
        WIN32_FIND_DATA Find{};
        HANDLE hFind = ::FindFirstFile(path.c_str(), &Find);
        bHidden = ((Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
        ::FindClose(hFind);
    }

    /* look if children test id allowed */
    BOOL haveChildren = FALSE;
    if (bChildrenTest == TRUE) {
        haveChildren = FileSystemService::HaveChildren(path, _pSettings->IsUseFullTree(), _pSettings->IsShowHidden());
    }

    /* insert item */
    INT iIconNormal     = 0;
    INT iIconSelected   = 0;
    INT iIconOverlayed  = 0;

    /* get icons */
    ExtractIcons(path.c_str(), nullptr, devType, &iIconNormal, &iIconSelected);

    /* set item */
    HTREEITEM hNewItem = _hTreeCtrl.InsertItem(childFolderName, iIconNormal, iIconSelected, 0, bHidden, parentItem, insertAfter, haveChildren);

    _workerThread.Enqueue(std::make_unique<TaskGetCompleteIconTree>(_hSelf, this, hNewItem, path, devType));
    return hNewItem;
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

void ExplorerDialog::UpdateChildren(const std::wstring& path, HTREEITEM parentItem, BOOL doRecursive)
{
    // For now, this is just bridging the old style call.
    // The model-driven approach would be to find the entry and enqueue a TaskUpdateDirectory.
    // However, since we need to do it by finding an entry:

    // We can just keep the legacy update via FileSystemService for synchronous parts like SelectItem where we wait,
    // but the plan says "When an ExplorerEntry is updated, update the TreeView directly from the UI thread".

    // For a real composite pattern, we'd traverse the model to find the entry for `path`.
    // In this interim step, we enqueue a TaskUpdateDirectory for a temporary entry, or we do a full model match.
    // Given the complexity of retrofitting TreeView hit-test without storing Entry pointers in lParam, we can store
    // the ExplorerEntry pointer in the TreeView lParam!

    // Let's rely on standard logic for now if we don't have it in model, OR we'll implement it shortly.
    HTREEITEM hCurrentItem = _hTreeCtrl.GetNextItem(parentItem, TVGN_CHILD);

    if (path.empty()) return;

    auto entries = FileSystemService::GetDirectoryEntries(path, _pSettings->IsShowHidden());

    if (!entries.empty()) {
        std::vector<FileSystemEntry> folders;
        std::vector<FileSystemEntry> files;

        for (const auto& entry : entries) {
            if (entry.IsDirectory()) {
                folders.push_back(entry);
            }
            else if (_pSettings->IsUseFullTree()) {
                files.push_back(entry);
            }
        }

        std::sort(folders.begin(), folders.end(), [](const auto& lhs, const auto& rhs) {
            return ::StrCmpLogicalW(lhs.Name().c_str(), rhs.Name().c_str()) < 0;
        });
        std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
            return ::StrCmpLogicalW(lhs.Name().c_str(), rhs.Name().c_str()) < 0;
        });

        for (const auto* entries_ptr : { &folders, &files }) {
            for (const auto& entry : *entries_ptr) {
                std::wstring name = _hTreeCtrl.GetItemText(hCurrentItem);
                if (!name.empty()) {
                    while ((name != entry.Name()) && (hCurrentItem != nullptr)) {
                        if (FindFolderAfter(entry.Name().c_str(), hCurrentItem) == TRUE) {
                            HTREEITEM pPrevItem = hCurrentItem;
                            hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
                            _hTreeCtrl.DeleteItem(pPrevItem);
                        }
                        else {
                            HTREEITEM pPrevItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_PREVIOUS);
                            if (pPrevItem == nullptr) {
                                hCurrentItem = InsertChildFolder(entry.Name(), parentItem, TVI_FIRST);
                            }
                            else {
                                hCurrentItem = InsertChildFolder(entry.Name(), parentItem, pPrevItem);
                            }
                        }

                        if (hCurrentItem != nullptr) {
                            name = _hTreeCtrl.GetItemText(hCurrentItem);
                        }
                    }

                    std::wstring currentPath = GetPath(hCurrentItem);
                    BOOL haveChildren = FileSystemService::HaveChildren(currentPath, _pSettings->IsUseFullTree(), _pSettings->IsShowHidden());

                    INT iIconNormal = 0, iIconSelected = 0, iIconOverlayed = 0;
                    ExtractIcons(currentPath.c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected);

                    BOOL bHidden = entry.IsHidden();
                    _hTreeCtrl.UpdateItem(hCurrentItem, name, iIconNormal, iIconSelected, 0, bHidden, haveChildren);

                    _workerThread.Enqueue(std::make_unique<TaskGetCompleteIconTree>(_hSelf, this, hCurrentItem, currentPath, DEVT_DIRECTORY));

                    if ((doRecursive) && _hTreeCtrl.IsItemExpanded(hCurrentItem)) {
                        UpdateChildren(currentPath, hCurrentItem);
                    }

                    hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
                }
                else {
                    hCurrentItem = InsertChildFolder(entry.Name(), parentItem);
                    hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
                }
            }
        }

        while (hCurrentItem != nullptr) {
            HTREEITEM pPrevItem = hCurrentItem;
            hCurrentItem = _hTreeCtrl.GetNextItem(hCurrentItem, TVGN_NEXT);
            _hTreeCtrl.DeleteItem(pPrevItem);
        }
    }
}

void ExplorerDialog::FetchChildren(HTREEITEM parentItem)
{
    // To properly use ExplorerModel here, we would:
    // 1. Get the path for parentItem.
    // 2. Find the ExplorerEntry for it.
    // 3. Enqueue a TaskUpdateDirectory for it.
    // 4. Return immediately.
    // 5. In OnEntryUpdated, we'd actually add the children.
    // Since tree view needs it instantly for expanding, we might have a placeholder.

    // For now we will create a temporary entry and enqueue it, and handle it async.
    auto parentFolderPath = GetPath(parentItem);
    auto tempEntry = std::make_shared<ExplorerEntry>(parentFolderPath, FileSystemEntry(parentFolderPath, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
    _workerThread.Enqueue(std::make_unique<TaskUpdateDirectory>(_model, tempEntry, _pSettings));
}



std::wstring ExplorerDialog::GetPath(HTREEITEM currentItem) const
{
    std::wstring result;
    std::vector<std::wstring> paths = _hTreeCtrl.GetItemPathFromRoot(currentItem);

    bool firstLoop = true;
    for (const auto &path : paths) {
        if (path.empty()) {
            continue;
        }
        if (firstLoop) {
            if (::PathIsUNC(path.c_str())) {
                result = path;
            }
            else {
                result = path.front();
                result += L":";
            }
            firstLoop = false;
        }
        else {
            result += L"\\";
            result += path;
        }
    }

    if (!result.empty()) {
        if (::PathIsDirectory(result.c_str())) {
            if ('\\' != result.back()) {
                result += L"\\";
            }
        }
    }
    return result;
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
        auto filterHeight = GetSystemMetrics(SM_CYSMSIZE);

        if (_pSettings->IsUseFullTree()) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight;
            ::SetWindowPos(_hTreeCtrl,      nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl,  nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl,      nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hFilter,        nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
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
            rc.top = rc.bottom - filterHeight + 1;
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
        auto filterHeight = GetSystemMetrics(SM_CYSMSIZE);

        if (_pSettings->IsUseFullTree()) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight;
            ::SetWindowPos(_hTreeCtrl, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hFilter, nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW);
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
            rc.top = rcBuff.bottom - filterHeight + 6;
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

    doPaste(path.c_str(), hFiles, *pdwEffect);
    ::CloseClipboard();

    _hTreeCtrl.SelectDropTarget( NULL);

    return true;
}

void ExplorerDialog::NavigateBack()
{
    WCHAR   pszPath[MAX_PATH];
    bool    dirValid = true;
    bool    selected = true;
    std::vector<std::wstring>   vStrItems;

    _FileList.ToggleStackRec();

    do {
        dirValid = _FileList.GetPrevDir(pszPath, vStrItems);
        if (dirValid) {
            selected = SelectItem(pszPath);
        }
    } while (dirValid && !selected);

    if (!selected) {
        _FileList.GetNextDir(pszPath, vStrItems);
    }

    if (!vStrItems.empty()) {
        _FileList.SetItems(vStrItems);
    }

    _FileList.ToggleStackRec();
}

void ExplorerDialog::NavigateForward()
{
    WCHAR   pszPath[MAX_PATH];
    bool    dirValid = true;
    bool    selected = true;
    std::vector<std::wstring> vStrItems;

    _FileList.ToggleStackRec();

    do {
        dirValid = _FileList.GetNextDir(pszPath, vStrItems);
        if (dirValid) {
            selected = SelectItem(pszPath);
        }
    } while (dirValid && !selected);

    if (!selected) {
        _FileList.GetPrevDir(pszPath, vStrItems);
    }

    if (!vStrItems.empty()) {
        _FileList.SetItems(vStrItems);
    }

    _FileList.ToggleStackRec();
}

void ExplorerDialog::NavigateTo(const std::wstring &path)
{
    if (!path.empty()) {
        std::filesystem::path navigatePath(path);

        std::filesystem::path lastPath;
        if (navigatePath.is_relative()) {
            HTREEITEM item = _hTreeCtrl.GetSelection();
            lastPath       = GetPath(item);
            navigatePath   = lastPath;
            navigatePath   = navigatePath.concat(path).lexically_normal().concat(L"\\");
        }

        SelectItem(navigatePath);

        if (path == L"..") {
            _FileList.SelectFolder(lastPath.parent_path().filename().c_str());
        }
        else {
            _FileList.SelectFolder(L"..");
        }
    }

}

void ExplorerDialog::Open(const std::wstring &path)
{
    if (!path.empty()) {
        HTREEITEM hItem = _hTreeCtrl.GetSelection();

        /* get current folder path */
        auto filePath = GetPath(hItem);
        filePath += path;

        /* open possible link */
        std::wstring resolvedPath;
        if (FileSystemService::ResolveShortCut(filePath, resolvedPath)) {
            if (::PathIsDirectory(resolvedPath.c_str()) != FALSE) {
                SelectItem(resolvedPath);
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
    UpdateRoots(); UpdateAllExpandedItems(); UpdatePath();
}

bool ExplorerDialog::doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect)
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

void ExplorerDialog::ShowContextMenu(POINT screenLocation, const std::vector<std::wstring>& paths, bool hasStandardMenu)
{
    ContextMenu cm;
    cm.SetObjects(paths);
    cm.ShowContextMenu(_hInst, _hParent, _hSelf, screenLocation, hasStandardMenu);
}

void ExplorerDialog::OnEntryUpdated(std::shared_ptr<ExplorerEntry> entry) {
    if (!isCreated()) return;

    if (entry == _model->Root()) {
        UpdateRoots();
        _FileList.SetToolBarInfo(&_ToolBar , IDM_EX_PREV, IDM_EX_NEXT);
        SelectItem(_pSettings->GetCurrentDir());
        NotifyNewFile();
    } else {
        // Find HTREEITEM by path matching (as we don't store pointers in lParam yet)
        // Since we know the path, we can traverse down the tree to find it, or simply:
        auto pathSegments = [&]() {
            std::vector<std::wstring> segments;
            std::wstring path = entry->Path();
            size_t pos = 0;
            while ((pos = path.find(L"\\")) != std::wstring::npos) {
                if (pos > 0) segments.push_back(path.substr(0, pos));
                path.erase(0, pos + 1);
            }
            if (!path.empty()) segments.push_back(path);
            return segments;
        }();

        HTREEITEM hItem = _hTreeCtrl.GetRoot();
        bool isRoot = true;

        for (const auto& segment : pathSegments) {
            bool found = false;
            while (hItem != nullptr) {
                auto itemName = _hTreeCtrl.GetItemText(hItem);
                if (isRoot && ('A' <= itemName[0] && itemName[0] <= 'Z')) {
                    itemName.resize(2);
                }

                if (segment == itemName || (isRoot && segment.size() >= 2 && segment[0] == itemName[0] && segment[1] == ':')) {
                    found = true;
                    isRoot = false;
                    break;
                }
                hItem = _hTreeCtrl.GetNextItem(hItem, TVGN_NEXT);
            }
            if (!found) {
                hItem = nullptr;
                break;
            }
            // we found the current segment, if it's not the last we go to its child
            if (segment != pathSegments.back()) {
                hItem = _hTreeCtrl.GetChild(hItem);
            }
        }

        if (hItem != nullptr) {
            // We found the HTREEITEM, insert its children from ExplorerEntry
            HTREEITEM hCurrentChild = _hTreeCtrl.GetNextItem(hItem, TVGN_CHILD);

            auto children = entry->Children();
            std::vector<std::shared_ptr<ExplorerEntry>> folders;
            std::vector<std::shared_ptr<ExplorerEntry>> files;

            for (const auto& child : children) {
                if (child->FSEntry().IsDirectory()) {
                    folders.push_back(child);
                } else if (_pSettings->IsUseFullTree()) {
                    files.push_back(child);
                }
            }

            std::sort(folders.begin(), folders.end(), [](const auto& lhs, const auto& rhs) {
                return ::StrCmpLogicalW(lhs->FSEntry().Name().c_str(), rhs->FSEntry().Name().c_str()) < 0;
            });
            std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
                return ::StrCmpLogicalW(lhs->FSEntry().Name().c_str(), rhs->FSEntry().Name().c_str()) < 0;
            });

            for (const auto* entries_ptr : { &folders, &files }) {
                for (const auto& childEntry : *entries_ptr) {
                    if (InsertChildFolder(childEntry->FSEntry().Name(), hItem) == nullptr) {
                        break;
                    }
                }
            }

            if (hItem == _hItemExpand) {
                _hTreeCtrl.Expand(hItem, TVE_EXPAND);
                _hItemExpand = nullptr;
            }
        }
    }
}

void ExplorerDialog::UpdateTreeItemCompleteIcon(HTREEITEM hItem, int icon, int selIcon, int overlay)
{
    int iIconNormal, iIconSelected, iIconOverlayed;
    if (_hTreeCtrl.GetItemIcons(hItem, &iIconNormal, &iIconSelected, &iIconOverlayed)) {
        _hTreeCtrl.SetItemIcons(hItem, icon, selIcon, overlay);
    }
}
