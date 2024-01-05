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

#include "Explorer.h"
#include "ExplorerResource.h"
#include "ContextMenu.h"
#include "NewDlg.h"
#include "NppInterface.h"
#include "StringUtil.h"
#include "resource.h"
#include "ThemeRenderer.h"

namespace {

template <typename... Args>
void DebugPrintf(std::wformat_string<Args...> format, Args&&... args)
{
    std::wstring message = std::format(format, std::forward<Args>(args)...);
    message.push_back('\n');
    OutputDebugString(message.c_str());
}

HANDLE g_hEvent[EID_MAX]	= {NULL};
HANDLE g_hThread			= NULL;



DWORD WINAPI UpdateThread(LPVOID lpParam)
{
	BOOL			bRun			= TRUE;
    DWORD			dwWaitResult    = EID_INIT;
	ExplorerDialog*	dlg             = (ExplorerDialog*)lpParam;

	CoInitialize(nullptr);
	dlg->NotifyEvent(dwWaitResult);

	while (bRun)
	{
		dwWaitResult = ::WaitForMultipleObjects(EID_MAX_THREAD, g_hEvent, FALSE, INFINITE);

		if (dwWaitResult == EID_THREAD_END)
		{
			DebugPrintf(L"UpdateThread() : EID_THREAD_END");
			bRun = FALSE;
		}
		else if (dwWaitResult < EID_MAX)
		{
            DebugPrintf(L"UpdateThread() : NotifyEvent({})", dwWaitResult);
			dlg->NotifyEvent(dwWaitResult);
		}
	}

	CoUninitialize();
	return 0;
}


DWORD WINAPI GetVolumeInformationTimeoutThread(LPVOID lpParam)
{
	DWORD			serialNr		= 0;
	DWORD			space			= 0;
	DWORD			flags			= 0;
	GetVolumeInfo*	volInfo         = (GetVolumeInfo*)lpParam;

	if (volInfo->maxSize < MAX_PATH+1)
		*volInfo->pIsValidDrive = GetVolumeInformation(volInfo->pszDrivePathName,
			volInfo->pszVolumeName, volInfo->maxSize, &serialNr, &space, &flags, NULL, 0);

	::SetEvent(g_hEvent[EID_GET_VOLINFO]);
	return 0;
}


static ToolBarButtonUnit toolBarIcons[] = {
	{IDM_EX_FAVORITES,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_TB_FAVES, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

	{IDM_EX_PREV,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_PREV, TBSTYLE_DROPDOWN},
	{IDM_EX_NEXT,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_NEXT, TBSTYLE_DROPDOWN},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

	{IDM_EX_FILE_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FILENEW, 0},
	{IDM_EX_FOLDER_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERNEW, 0},
	{IDM_EX_SEARCH_FIND,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FIND, 0},
	{IDM_EX_TERMINAL,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_TERMINAL, 0},
	{IDM_EX_GO_TO_USER,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERUSER, 0},
	{IDM_EX_GO_TO_FOLDER,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERGO, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//

	{IDM_EX_UPDATE,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_UPDATE, 0}
};

/**
 *	Note: On change, keep sure to change order of IDM_EX_... also
 */
static LPCTSTR szToolTip[23] = {
	_T("Favorites"),
	_T("Previous Folder"),
	_T("Next Folder"),
	_T("New File..."),
	_T("New Folder..."),
	_T("Find in Files..."),
	_T("Open Command Window Here"),
	_T("Folder of Current File"),
	_T("User Folder"),
	_T("Refresh")
};

} // namespace

struct FileSystemEntry {
    std::wstring	name;
    DWORD			attributes;
    FileSystemEntry(const WCHAR* fileName, DWORD fileAttributes)
        : name(fileName)
        , attributes(fileAttributes)
    {
    }
};


LPCTSTR ExplorerDialog::GetNameStrFromCmd(UINT resID)
{
	if ((IDM_EX_FAVORITES <= resID) && (resID <= IDM_EX_UPDATE)) {
		return szToolTip[resID - IDM_EX_FAVORITES];
	}
	return nullptr;
}





ExplorerDialog::ExplorerDialog(void)
    : DockingDlgInterface(IDD_EXPLORER_DLG)
    , _bStartupFinish(FALSE)
    , _hExploreVolumeThread(nullptr)
    , _hItemExpand(nullptr)
    , _hDefaultTreeProc(nullptr)
    , _hDefaultSplitterProc(nullptr)
    , _bOldRectInitilized(FALSE)
    , _isSelNotifyEnable(TRUE)
    , _hListCtrl(nullptr)
    , _hHeader(nullptr)
    , _hSplitterCtrl(nullptr)
    , _hFilter(nullptr)
    , _FileList(this)
    , _ComboFilter()
    , _ToolBar()
    , _Rebar()
    , _ptOldPos()
    , _ptOldPosHorizontal()
    , _isLeftButtonDown(FALSE)
    , _hSplitterCursorUpDown(nullptr)
    , _hSplitterCursorLeftRight(nullptr)
    , _pExProp(nullptr)
    , _hCurWait(nullptr)
    , _isScrolling(FALSE)
    , _isDnDStarted(FALSE)
    , _iDockedPos(CONT_LEFT)
{
}

ExplorerDialog::~ExplorerDialog(void)
{
}


void ExplorerDialog::init(HINSTANCE hInst, HWND hParent, ExProp *prop)
{
	DockingDlgInterface::init(hInst, hParent);

	_pExProp = prop;
	_FileList.initProp(prop);
}

void ExplorerDialog::redraw()
{
    UpdateLayout();

    /* possible new imagelist -> update the window */
    ::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)GetSmallImageList(_pExProp->bUseSystemIcons));
    ::SetTimer(_hSelf, EXT_UPDATEDEVICE, 0, NULL);
    _FileList.redraw();
    ::RedrawWindow(_ToolBar.getHSelf(), NULL, NULL, TRUE);

    /* and only when dialog is visible, select item again */
    SelectItem(_pExProp->currentDir);

    ::SetEvent(g_hEvent[EID_UPDATE_USER]);
};

void ExplorerDialog::doDialog(bool willBeShown)
{
    if (!isCreated())
	{
        // define the default docking behaviour
        tTbData data{};
        create(&data);
        data.pszName = _T("Explorer");
        data.dlgID = DOCKABLE_EXPLORER_INDEX;
        data.uMask = DWS_DF_CONT_LEFT | DWS_ADDINFO | DWS_ICONTAB;
        data.hIconTab = (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_EXPLORE), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
        data.pszModuleName = getPluginFileName();

        ThemeRenderer::Instance().Register(_hSelf);
		::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&data);
	}
	else if (willBeShown)
	{
		if (_pExProp->bAutoUpdate == TRUE) {
			::KillTimer(_hSelf, EXT_UPDATEACTIVATE);
			::SetTimer(_hSelf, EXT_UPDATEACTIVATE, 0, NULL);
		} else {
			::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
			::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 0, NULL);
		}
	}
	display(willBeShown);
}


INT_PTR CALLBACK ExplorerDialog::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_INITDIALOG:
		{
			InitialDialog();

			DWORD   dwThreadId   = 0;

			/* create events */
			for (int i = 0; i < EID_MAX; i++)
				g_hEvent[i] = ::CreateEvent(NULL, FALSE, FALSE, NULL);

			/* create thread */
			g_hThread = ::CreateThread(NULL, 0, UpdateThread, this, 0, &dwThreadId);
			break;
		}
		case WM_COMMAND:
		{
			if (((HWND)lParam == _hFilter) && (HIWORD(wParam) == CBN_SELCHANGE))
			{
				::SendMessage(_hSelf, EXM_CHANGECOMBO, 0, 0);
				return TRUE;
			}

			if ((HWND)lParam == _ToolBar.getHSelf())
			{
				tb_cmd(LOWORD(wParam));
				return TRUE;
			}
			break;
		}
		case WM_NOTIFY:
		{
			LPNMHDR		nmhdr = (LPNMHDR)lParam;

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
                    const POINT pt{
                        .x = GET_X_LPARAM(pos),
                        .y = GET_Y_LPARAM(pos),
                    };
                    TVHITTESTINFO ht{
                        .pt = pt
                    };
                    ::ScreenToClient(_hTreeCtrl, &ht.pt);

                    const HTREEITEM item = TreeView_HitTest(_hTreeCtrl, &ht);
                    if (item != nullptr) {
                        const auto path = GetPath(item);
                        if (!PathIsDirectory(path.c_str())) {
                            NppInterface::doOpen(path);
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

                    const HTREEITEM item = TreeView_HitTest(_hTreeCtrl, &ht);
                    if (item != nullptr) 	{
                        const auto path = GetPath(item);
                        ShowContextMenu(pt, {path});
                    }
                    return TRUE;
                }
                case TVN_SELCHANGED:
                {
                    HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);
                    if (hItem != NULL) {
                        const auto path = GetPath(hItem);
                        /* save current path */
                        if (PathIsDirectory(path.c_str())) {
                            _pExProp->currentDir = path;
                        }
                        else {
                            std::filesystem::path currentDir(path);
                            _pExProp->currentDir = currentDir.parent_path().wstring() + L"\\";
                        }
                    }
                    if (_isSelNotifyEnable == TRUE)
                    {
                        ::KillTimer(_hSelf, EXT_SELCHANGE);
                        ::SetTimer(_hSelf, EXT_SELCHANGE, 200, NULL);
                    }
                    break;
                }
                case TVN_ITEMEXPANDING:
                {
                    TVITEM	tvi	= (TVITEM)((LPNMTREEVIEW)lParam)->itemNew;

                    if (tvi.hItem != _hItemExpand) {
                        if (!(tvi.state & TVIS_EXPANDED))
                        {
                            _hItemExpand = tvi.hItem;
                            SetEvent(g_hEvent[EID_EXPAND_ITEM]);
                        }
                    } else {
                        _hItemExpand = NULL;
                    }
                    break;
                }
                case TVN_BEGINDRAG:
                {
                    CIDropSource	dropSrc;
                    CIDataObject	dataObj(&dropSrc);
                    FolderExChange(&dropSrc, &dataObj, DROPEFFECT_COPY | DROPEFFECT_MOVE);
                    break;
                }
                default:
                    break;
                }
            }
			else if ((nmhdr->hwndFrom == _hListCtrl) || (nmhdr->hwndFrom == _hHeader))
			{
				return _FileList.notify(wParam, lParam);
			}
			else if ((nmhdr->hwndFrom == _ToolBar.getHSelf()) && (nmhdr->code == TBN_DROPDOWN))
			{
				tb_not((LPNMTOOLBAR)lParam);
				return TBDDRET_NODEFAULT;
			}
			else if ((nmhdr->hwndFrom == _Rebar.getHSelf()) && (nmhdr->code == RBN_CHEVRONPUSHED))
			{
				NMREBARCHEVRON * lpnm = (NMREBARCHEVRON*)nmhdr;
				if (lpnm->wID == REBAR_BAR_TOOLBAR)
				{
					POINT pt;
					pt.x = lpnm->rc.left;
					pt.y = lpnm->rc.bottom;
					ClientToScreen(nmhdr->hwndFrom, &pt);
					tb_cmd(_ToolBar.doPopop(pt));
					return TRUE;
				}
				break;
			}
			else if (nmhdr->code == TTN_GETDISPINFO)
			{
				LPTOOLTIPTEXT lpttt;

				lpttt = (LPTOOLTIPTEXT)nmhdr;
				lpttt->hinst = _hInst;


				// Specify the resource identifier of the descriptive
				// text for the given button.
				int resId = int(lpttt->hdr.idFrom);
				lpttt->lpszText = const_cast<LPTSTR>(GetNameStrFromCmd(resId));
				return TRUE;
			}

			DockingDlgInterface::run_dlgProc(Message, wParam, lParam);

			return FALSE;
		}
		case WM_SIZE:
		case WM_MOVE:
		{
			if (_bStartupFinish == FALSE)
				return TRUE;
            UpdateLayout();
			break;
		}
		case WM_PAINT:
		{
			::RedrawWindow(_ToolBar.getHSelf(), NULL, NULL, TRUE);
			break;
		}
		case WM_DESTROY:
		{
			TCHAR	szLastFilter[MAX_PATH];

			_pExProp->vStrFilterHistory = _ComboFilter.getComboList();
			_ComboFilter.getText(szLastFilter, MAX_PATH);
			if (_tcslen(szLastFilter) != 0)
				_pExProp->fileFilter.setFilter(szLastFilter);

			::SetEvent(g_hEvent[EID_THREAD_END]);
			if (::WaitForSingleObject(_hExploreVolumeThread, 50) != WAIT_OBJECT_0) {
				::Sleep(1);
			}
			if (::WaitForSingleObject(g_hThread, 300) != WAIT_OBJECT_0) {
				DebugPrintf(L"ExplorerDialog::run_dlgProc() => WM_DESTROY => TerminateThread!!");
				// https://github.com/funap/npp-explorer-plugin/issues/4
				// Failsafe for [Bug] Incompatibility with NppMenuSearch plugin #4
				// TreeView_xxx function in TreeHelper class won't return.
				// Force thread termination because the thread was deadlock.
				::TerminateThread(g_hThread, 0);
				::Sleep(1);
			}


			/* destroy events */
			for (int i = 0; i < EID_MAX; i++) {
				::CloseHandle(g_hEvent[i]);
				g_hEvent[i] = nullptr;
			}

			/* destroy thread */
			if (g_hThread) {
				::CloseHandle(g_hThread);
				g_hThread = nullptr;
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
		case EXM_CHANGECOMBO:
		{
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
		{
            NavigateTo((LPTSTR)lParam);
			return TRUE;
		}
		case EXM_USER_ICONBAR:
		{
			tb_cmd(wParam);
			return TRUE;
		}
		case WM_TIMER:
		{
			if (wParam == EXT_UPDATEDEVICE)
			{
				::KillTimer(_hSelf, EXT_UPDATEDEVICE);
				::SetEvent(g_hEvent[EID_UPDATE_DEVICE]);
				return FALSE;
			}
			else if (wParam == EXT_UPDATEACTIVATE)
			{
				::KillTimer(_hSelf, EXT_UPDATEACTIVATE);
				::SetEvent(g_hEvent[EID_UPDATE_ACTIVATE]);
				return FALSE;
			}
			else if (wParam == EXT_UPDATEACTIVATEPATH)
			{
				::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
				::SetEvent(g_hEvent[EID_UPDATE_ACTIVATEPATH]);
				return FALSE;
			}
			else if (wParam == EXT_AUTOGOTOFILE)
			{
				::KillTimer(_hSelf, EXT_AUTOGOTOFILE);
				::SetEvent(g_hEvent[EID_UPDATE_GOTOCURRENTFILE]);
				return FALSE;
			}
			else if (wParam == EXT_SELCHANGE)
			{
                ::KillTimer(_hSelf, EXT_SELCHANGE);
                _FileList.viewPath(_pExProp->currentDir, TRUE);
                updateDockingDlg();
				return FALSE;
			}
			return TRUE;
		}
		default:
			return DockingDlgInterface::run_dlgProc(Message, wParam, lParam);
	}

	return FALSE;
}

/****************************************************************************
 *	Message handling of tree
 */
LRESULT ExplorerDialog::runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_GETDLGCODE:
		{
			return DLGC_WANTALLKEYS | ::CallWindowProc(_hDefaultTreeProc, hwnd, Message, wParam, lParam);
		}
		case WM_CHAR:
		{
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
					::SetEvent(g_hEvent[EID_UPDATE_USER]);
					return TRUE;
				case VK_RETURN:
				{
					/* toggle item on return */
					HTREEITEM hItem = TreeView_GetSelection(_hTreeCtrl);
                    const auto path = GetPath(hItem);
                    if (PathIsDirectory(path.c_str())) {
                        if (TreeView_GetChild(_hTreeCtrl, hItem) == NULL)
                            FetchChildren(hItem);
                        TreeView_Expand(_hTreeCtrl, hItem, TVE_TOGGLE);
                    }
                    else {
                        NppInterface::doOpen(path);
                    }
					return TRUE;
				}
				case VK_TAB:
                    if (!_pExProp->useFullTree) {
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
                HTREEITEM item = TreeView_GetSelection(_hTreeCtrl);
                RECT rect{};
                TreeView_GetItemRect(_hTreeCtrl, item, &rect, TRUE);
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
		case WM_KEYDOWN:
		{
			if ((wParam == VK_DELETE) && !((0x8000 & ::GetKeyState(VK_CONTROL)) == 0x8000))
			{
				onDelete((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000);
				return TRUE;
			}
			if (wParam == VK_F5)
			{
				::SetEvent(g_hEvent[EID_UPDATE_USER]);
				return TRUE;
			}
			if (VK_ESCAPE == wParam) {
				NppInterface::setFocusToCurrentEdit();
				return TRUE;
			}
			break;
		}
		case WM_SYSKEYDOWN:
            if ((0x8000 & ::GetKeyState(VK_MENU)) == 0x8000) {
                if (wParam == VK_LEFT) {
                    NavigateBack();
                    return TRUE;
                } else if (wParam == VK_RIGHT) {
                    NavigateForward();
                    return TRUE;
                }
            }
            break;
        case WM_SYSKEYUP:
            if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
                if (wParam == VK_F10) {
                    HTREEITEM item = TreeView_GetSelection(_hTreeCtrl);
                    RECT rect{};
                    TreeView_GetItemRect(_hTreeCtrl, item, &rect, TRUE);
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
		case EXM_QUERYDROP:
		{
			TVHITTESTINFO	ht		= {0};

			if (_isScrolling == FALSE)
			{
				ScDir scrDir = GetScrollDirection(_hTreeCtrl);

				if (scrDir == SCR_UP) {
					::SetTimer(_hTreeCtrl, EXT_SCROLLLISTUP, 300, NULL);
					_isScrolling = TRUE;
				} else if (scrDir == SCR_DOWN) {
					::SetTimer(_hTreeCtrl, EXT_SCROLLLISTDOWN, 300, NULL);
					_isScrolling = TRUE;
				}
			}

			/* select item */
			::GetCursorPos(&ht.pt);
			::ScreenToClient(_hTreeCtrl, &ht.pt);
			TreeView_SelectDropTarget(_hTreeCtrl, TreeView_HitTest(_hTreeCtrl, &ht));

			_isDnDStarted = TRUE;

			return TRUE;
		}
		case WM_MOUSEMOVE:
		case EXM_DRAGLEAVE:
		{
			/* unselect DnD highlight */
			if (_isDnDStarted == TRUE) {
				TreeView_SelectDropTarget(_hTreeCtrl, NULL);
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
		case WM_TIMER:
		{
			if (wParam == EXT_SCROLLLISTUP)
			{
				HTREEITEM	hItemHigh	= TreeView_GetDropHilight(_hTreeCtrl);
				HTREEITEM	hItemRoot	= TreeView_GetRoot(_hTreeCtrl);
				ScDir		scrDir		= GetScrollDirection(_hTreeCtrl);

				if ((scrDir != SCR_UP) || (hItemHigh == hItemRoot) || (!m_bAllowDrop)) {
					::KillTimer(_hTreeCtrl, EXT_SCROLLLISTUP);
					_isScrolling = FALSE;
				} else {
					::SendMessage(_hTreeCtrl, WM_VSCROLL, SB_LINEUP, NULL);
				}
				return FALSE;
			}
			else if (wParam == EXT_SCROLLLISTDOWN)
			{
				HTREEITEM	hItemHigh	= TreeView_GetDropHilight(_hTreeCtrl);
				HTREEITEM	hItemLast	= TreeView_GetLastVisible(_hTreeCtrl);
				ScDir		scrDir		= GetScrollDirection(_hTreeCtrl);

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
 *	Message handling of header
 */
LRESULT ExplorerDialog::runSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_LBUTTONDOWN:
		{
			_isLeftButtonDown = TRUE;

			/* set cursor */
			if (_iDockedPos < CONT_TOP)
			{
				::GetCursorPos(&_ptOldPos);
				SetCursor(_hSplitterCursorUpDown);
			}
			else
			{
				::GetCursorPos(&_ptOldPosHorizontal);
				SetCursor(_hSplitterCursorLeftRight);
			}
			break;
		}
		case WM_LBUTTONUP:
		{
			RECT	rc;

			getClientRect(rc);
			_isLeftButtonDown = FALSE;

			/* set cursor */
			if ((_iDockedPos == CONT_LEFT) || (_iDockedPos == CONT_RIGHT))
			{
				SetCursor(_hSplitterCursorUpDown);
				if (_pExProp->iSplitterPos < 50)
				{
					_pExProp->iSplitterPos = 50;
				}
				else if (_pExProp->iSplitterPos > (rc.bottom - 100))
				{
					_pExProp->iSplitterPos = rc.bottom - 100;
				}
			}
			else
			{
				SetCursor(_hSplitterCursorLeftRight);
				if (_pExProp->iSplitterPosHorizontal < 50)
				{
					_pExProp->iSplitterPosHorizontal = 50;
				}
				else if (_pExProp->iSplitterPosHorizontal > (rc.right - 50))
				{
					_pExProp->iSplitterPosHorizontal = rc.right - 50;
				}
			}
			break;
		}
		case WM_MOUSEMOVE:
		{
			if (_isLeftButtonDown == TRUE)
			{
				POINT	pt;

				::GetCursorPos(&pt);

				if (_iDockedPos < CONT_TOP)
				{
					if (_ptOldPos.y != pt.y)
					{
						_pExProp->iSplitterPos -= _ptOldPos.y - pt.y;
						::SendMessage(_hSelf, WM_SIZE, 0, 0);
					}
					_ptOldPos = pt;
				}
				else
				{
					if (_ptOldPosHorizontal.x != pt.x)
					{
						_pExProp->iSplitterPosHorizontal -= _ptOldPosHorizontal.x - pt.x;
						::SendMessage(_hSelf, WM_SIZE, 0, 0);
					}
					_ptOldPosHorizontal = pt;
				}
			}

			/* set cursor */
			if (_iDockedPos < CONT_TOP)
				SetCursor(_hSplitterCursorUpDown);
			else
				SetCursor(_hSplitterCursorLeftRight);
			break;
		}
		default:
			break;
	}

	return ::CallWindowProc(_hDefaultSplitterProc, hwnd, Message, wParam, lParam);
}

void ExplorerDialog::tb_cmd(WPARAM message)
{
	switch (message)
	{
        case IDM_EX_PREV:
            NavigateBack();
            break;
        case IDM_EX_NEXT:
            NavigateForward();
            break;
		case IDM_EX_FILE_NEW:
		{
			NewDlg		dlg;
			TCHAR		szFileName[MAX_PATH];
			TCHAR		szComment[] = L"New file";
			BOOL		bLeave		= FALSE;

			szFileName[0] = '\0';

			dlg.init(_hInst, _hParent);
			while (bLeave == FALSE)
			{
				if (dlg.doDialog(szFileName, szComment) == TRUE)
				{
					/* test if is correct */
					if (IsValidFileName(szFileName)) {
						auto newFilePath = GetPath(TreeView_GetSelection(_hTreeCtrl));
                        newFilePath += szFileName;

						::CloseHandle(::CreateFile(newFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
						::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)newFilePath.c_str());

						bLeave = TRUE;
					}
				}
				else
					bLeave = TRUE;
			}
			break;
		}
		case IDM_EX_FOLDER_NEW:
		{
			NewDlg		dlg;
			TCHAR		szFolderName[MAX_PATH];
			TCHAR		szComment[] = L"New folder";
			BOOL		bLeave			= FALSE;

			szFolderName[0] = '\0';

			dlg.init(_hInst, _hParent);
			while (bLeave == FALSE)
			{
				if (dlg.doDialog(szFolderName, szComment) == TRUE)
				{
					/* test if is correct */
					if (IsValidFileName(szFolderName)) {
						auto newFolderPath = GetPath(TreeView_GetSelection(_hTreeCtrl));
                        newFolderPath.append(szFolderName);

						if (::CreateDirectory(newFolderPath.c_str(), NULL) == FALSE) {
							::MessageBox(_hParent, _T("Folder couldn't be created."), _T("Error"), MB_OK);
						}
						bLeave = TRUE;
					}
				}
				else
					bLeave = TRUE;
			}
			break;
		}
		case IDM_EX_SEARCH_FIND:
		{
			::SendMessage(_hParent, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)_pExProp->currentDir.c_str(), NULL);
			break;
		}
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
	INT			i = 0;
	LPTSTR		*pszPathes;
	INT			iElements	= 0;

	_FileList.ToggleStackRec();

	/* get element cnt */
	if (lpnmtb->iItem == IDM_EX_PREV) {
		iElements = _FileList.GetPrevDirs(NULL);
	} else if (lpnmtb->iItem == IDM_EX_NEXT) {
		iElements = _FileList.GetNextDirs(NULL);
	}

	/* allocate elements */
	pszPathes	= (LPTSTR*)new LPTSTR[iElements];
	for (i = 0; i < iElements; i++)
		pszPathes[i] = (LPTSTR)new TCHAR[MAX_PATH];

	/* get directories */
	if (lpnmtb->iItem == IDM_EX_PREV) {
		_FileList.GetPrevDirs(pszPathes);
	} else if (lpnmtb->iItem == IDM_EX_NEXT) {
		_FileList.GetNextDirs(pszPathes);
	}

	POINT	pt		= {0};
	HMENU	hMenu	= ::CreatePopupMenu();

	/* test are folder exist */
	for (i = 0; i < iElements; i++) {
		if (::PathFileExists(pszPathes[i]))
			::AppendMenu(hMenu, MF_STRING, i+1, pszPathes[i]);
	}

	::GetCursorPos(&pt);
	INT cmd = ::TrackPopupMenu(hMenu, TPM_RETURNCMD, pt.x, pt.y, 0, _hSelf, NULL);
	::DestroyMenu(hMenu);
	_Rebar.redraw();

	/* select element */
	if (cmd) {
		std::vector<std::wstring>	vStrItems;

		SelectItem(pszPathes[cmd-1]);
		_FileList.OffsetItr(lpnmtb->iItem == IDM_EX_PREV ? -cmd : cmd, vStrItems);

		if (vStrItems.size() != 0)
			_FileList.SetItems(vStrItems);
	}

	for (i = 0; i < iElements; i++)
		delete [] pszPathes[i];
	delete [] pszPathes;

	_FileList.ToggleStackRec();
}

void ExplorerDialog::NotifyEvent(DWORD event)
{
	POINT	pt		= {0};
	LONG_PTR	oldCur	= NULL;

	oldCur = ::SetClassLongPtr(_hSelf, GCLP_HCURSOR, (LONG_PTR)_hCurWait);
	::EnableWindow(_hSelf, FALSE);
	::GetCursorPos(&pt);
	::SetCursorPos(pt.x, pt.y);

	switch(event)
	{
		case EID_INIT :
		{
			/* initilize combo */
			_ComboFilter.setComboList(_pExProp->vStrFilterHistory);
			_ComboFilter.addText(_T("*.*"));
			_ComboFilter.setText(_pExProp->fileFilter.getFilterString());

			/* initilize file list */
			_FileList.SetToolBarInfo(&_ToolBar , IDM_EX_PREV, IDM_EX_NEXT);

			/* initial tree */
			UpdateRoots();

			/* set data */
			SelectItem(_pExProp->currentDir);

			/* Update "Go to Folder" icon */
			NotifyNewFile();
			break;
		}
		case EID_UPDATE_DEVICE :
			UpdateRoots();
			break;
		case EID_UPDATE_USER :
			UpdateRoots();
			UpdateAllExpandedItems();
			UpdatePath();
			break;
		case EID_UPDATE_ACTIVATE :
			UpdateAllExpandedItems();
			UpdatePath();
			break;
		case EID_UPDATE_ACTIVATEPATH :
		{
			HTREEITEM	hItem		= TreeView_GetSelection(_hTreeCtrl);
			HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);

			if (hParentItem != NULL) {
				auto path = GetPath(hParentItem);
                UpdateChildren(path, hParentItem, FALSE);
			}
			if (hItem != NULL) {
                auto path = GetPath(hItem);
                UpdateChildren(path, hItem, FALSE);
                UpdatePath();
			}
			break;
		}
		case EID_UPDATE_GOTOCURRENTFILE :
		{
			gotoCurrentFile();
			break;
		}
		case EID_EXPAND_ITEM:
		{
			if (!TreeView_GetChild(_hTreeCtrl, _hItemExpand)) {
				FetchChildren(_hItemExpand);
			} else {
				/* set cursor back before tree is updated for faster access */
				::SetClassLongPtr(_hSelf, GCLP_HCURSOR, oldCur);
				::EnableWindow(_hSelf, TRUE);
				::GetCursorPos(&pt);
				::SetCursorPos(pt.x, pt.y);

				const auto path = GetPath(_hItemExpand);
				UpdateChildren(path, _hItemExpand);

				TreeView_Expand(_hTreeCtrl, _hItemExpand, TVE_EXPAND);
				return;
			}
			TreeView_Expand(_hTreeCtrl, _hItemExpand, TVE_EXPAND);
			break;
		}
		default:
			break;
	}

	::SetClassLongPtr(_hSelf, GCLP_HCURSOR, oldCur);
	::EnableWindow(_hSelf, TRUE);
	::GetCursorPos(&pt);
	::SetCursorPos(pt.x, pt.y);
}


void ExplorerDialog::InitialDialog(void)
{
	/* get handle of dialogs */
	_hTreeCtrl		= ::GetDlgItem(_hSelf, IDC_TREE_FOLDER);
	_hListCtrl		= ::GetDlgItem(_hSelf, IDC_LIST_FILE);
	_hHeader		= ListView_GetHeader(_hListCtrl);
	_hSplitterCtrl	= ::GetDlgItem(_hSelf, IDC_BUTTON_SPLITTER);
	_hFilter		= ::GetDlgItem(_hSelf, IDC_COMBO_FILTER);

	::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)_pExProp->defaultFont, TRUE);
	::SendMessage(_hListCtrl, WM_SETFONT, (WPARAM)_pExProp->defaultFont, TRUE);

	/* subclass tree */
	::SetWindowLongPtr(_hTreeCtrl, GWLP_USERDATA, (LONG_PTR)this);
	_hDefaultTreeProc = (WNDPROC)::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)wndDefaultTreeProc);

	/* subclass splitter */
	_hSplitterCursorUpDown		= ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_UPDOWN));
	_hSplitterCursorLeftRight	= ::LoadCursor(_hInst, MAKEINTRESOURCE(IDC_LEFTRIGHT));
	::SetWindowLongPtr(_hSplitterCtrl, GWLP_USERDATA, (LONG_PTR)this);
	_hDefaultSplitterProc = (WNDPROC)::SetWindowLongPtr(_hSplitterCtrl, GWLP_WNDPROC, (LONG_PTR)wndDefaultSplitterProc);

	/* Load Image List */
	::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)GetSmallImageList(_pExProp->bUseSystemIcons));

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
	_hCurWait = ::LoadCursor(NULL, IDC_WAIT);

	/* initialize droping */
	::RegisterDragDrop(_hTreeCtrl, this);

	/* create the supported formats */
	FORMATETC fmtetc	= {0};
	fmtetc.cfFormat		= CF_HDROP;
	fmtetc.dwAspect		= DVASPECT_CONTENT;
	fmtetc.lindex		= -1;
	fmtetc.tymed		= TYMED_HGLOBAL;
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
			NppInterface::setFocusToCurrentEdit();
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
			NppInterface::setFocusToCurrentEdit();
			return TRUE;
		default:
			break;
		}
		return FALSE;
	});
}

void ExplorerDialog::SetFont(const HFONT font)
{
	::SendMessage(_hTreeCtrl, WM_SETFONT, (WPARAM)font, TRUE);
	::SendMessage(_hListCtrl, WM_SETFONT, (WPARAM)font, TRUE);
}

BOOL ExplorerDialog::SelectItem(const std::filesystem::path& path)
{
    BOOL				folderExists	= FALSE;
    SIZE_T				iPathLen		= 0;
    BOOL				isRoot			= TRUE;
    HTREEITEM			hItem			= TreeView_GetRoot(_hTreeCtrl);
    HTREEITEM			hItemSel		= NULL;
    HTREEITEM			hItemUpdate		= NULL;

    auto longPath = [](const std::filesystem::path& path) -> std::filesystem::path {
        TCHAR szLongPath[MAX_PATH] = {};
        TCHAR szRemotePath[MAX_PATH] = {};
        /* convert possible net path name and get the full path name for compare */
        if (ConvertNetPathName(path.c_str(), szRemotePath, MAX_PATH) == TRUE) {
            ::GetLongPathName(szRemotePath, szLongPath, MAX_PATH);
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
            auto itemName = GetItemText(hItem);

            // truncate item name if we are in root
            if (('A' <= itemName[0]) && (itemName[0] <= 'Z')) {
                itemName.resize(2);
            }

            // compare path names
            if (_tcsnicmp(longPath.c_str(), itemName.c_str(), itemName.size()) == 0) {
                // allready mounted
                break;
            }
            hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
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
    hItem = TreeView_GetRoot(_hTreeCtrl);
    do {
        if (pathSegments.empty()) {
            break;
        }
        auto itemName = GetItemText(hItem);

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
            if (TreeView_GetChild(_hTreeCtrl, hItem) == NULL) {
                /* if no child item available, draw them */
                TreeView_SelectItem(_hTreeCtrl, hItem);
                FetchChildren(hItem);
            }
            hItem = TreeView_GetChild(_hTreeCtrl, hItem);
        } else {
            /* search for next item in list */
            hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
        }

        /* try again, maybe there is only an update needed */
        if ((hItem == NULL) && (hItemUpdate != hItemSel)) {
            TreeView_Expand(_hTreeCtrl, hItemSel, TVE_EXPAND);
            hItemUpdate = hItemSel;
            auto path = GetPath(hItemSel);
            UpdateChildren(path, hItemSel, FALSE);
            hItem = TreeView_GetChild(_hTreeCtrl, hItemSel);
        }
    } while (hItem != NULL);

    /* view path */
    if (hItemSel != NULL) {
        /* select last selected item */
        TreeView_SelectItem(_hTreeCtrl, hItemSel);
        TreeView_EnsureVisible(_hTreeCtrl, hItemSel);

        _FileList.viewPath(_pExProp->currentDir, TRUE);
        updateDockingDlg();
    }

    /* enable detection of TVN_SELCHANGED notification */
    _isSelNotifyEnable = TRUE;

    return folderExists;
}

BOOL ExplorerDialog::gotoPath(void)
{
	/* newDlg is exactly what I need */
	NewDlg		dlg;
	TCHAR		szFolderName[MAX_PATH];
	TCHAR		szComment[] = L"Go to Path";
	BOOL		bLeave			= FALSE;
	BOOL		bResult			= FALSE;

	szFolderName[0] = '\0';

	/* copy current path to show current position */
	_tcscpy(szFolderName, _pExProp->currentDir.c_str());

	dlg.init(_hInst, _hParent);
	while (bLeave == FALSE)
	{
		if (dlg.doDialog(szFolderName, szComment) == TRUE)
		{
			/* test if is correct */
			if (::PathFileExists(szFolderName))
			{
				if (szFolderName[_tcslen(szFolderName) - 1] != '\\')
					_tcscat(szFolderName, _T("\\"));
				SelectItem(szFolderName);
				bLeave = TRUE;
				bResult = TRUE;
			}
			else
			{
				INT msgRet = ::MessageBox(_hParent, _T("Path doesn't exist."), _T("Error"), MB_RETRYCANCEL);

				if (msgRet == IDCANCEL)
					bLeave = TRUE;
			}
		}
		else
			bLeave = TRUE;
	}
	return bResult;
}

void ExplorerDialog::gotoUserFolder(void)
{
	TCHAR	pathName[MAX_PATH];

	if (SHGetSpecialFolderPath(nullptr, pathName, CSIDL_PROFILE, FALSE) == TRUE) {
		_tcscat(pathName, _T("\\"));
		SelectItem(pathName);
	}
	setFocusOnFile();
}

void ExplorerDialog::gotoCurrentFolder(void)
{
	TCHAR	pathName[MAX_PATH];
	::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)pathName);
	_tcscat(pathName, _T("\\"));
	SelectItem(pathName);
	setFocusOnFile();
}

void ExplorerDialog::gotoCurrentFile(void)
{
    TCHAR	pathName[MAX_PATH];
    if (_pExProp->useFullTree) {
        ::SendMessage(_hParent, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)pathName);
        if (PathFileExists(pathName)) {
            SelectItem(pathName);
        }
    }
    else {
        ::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)pathName);
        _tcscat(pathName, _T("\\"));
        SelectItem(pathName);
        _FileList.SelectCurFile();
        setFocusOnFile();
    }
}

void ExplorerDialog::gotoFileLocation(const std::wstring& filePath)
{
	SelectItem(filePath);

	std::wstring fileName = filePath.substr(filePath.find_last_of(L"\\") + 1);
	_FileList.SelectFile(fileName);

	setFocusOnFile();
}

void ExplorerDialog::setFocusOnFolder(void)
{
	::SetFocus(_hTreeCtrl);
}

void ExplorerDialog::setFocusOnFile(void)
{
    if (!_pExProp->useFullTree) {
        ::SetFocus(_FileList.getHSelf());
    }
    else {
        ::SetFocus(_hTreeCtrl);
    }
}

void ExplorerDialog::clearFilter(void)
{
	_pExProp->vStrFilterHistory.clear();
	_pExProp->fileFilter.setFilter(L"*.*");
	_ComboFilter.clearComboList();
	_ComboFilter.addText(_T("*.*"));
	_ComboFilter.setText(_T("*.*"));
	_FileList.filterFiles(_T("*.*"));
}

/**************************************************************************
 *	Shortcut functions
 */
void ExplorerDialog::onDelete(bool immediate)
{
	auto path = GetPath(TreeView_GetSelection(_hTreeCtrl));
    if (path.empty()) {
        return;
    }
    if (path.back() == L'\\') {
        path.pop_back();
    }

	/* delete folder into recycle bin */
	SHFILEOPSTRUCT	fileOp	= {0};
	fileOp.hwnd				= _hParent;
	fileOp.pFrom			= path.data();
	fileOp.fFlags			= (immediate ? 0 : FOF_ALLOWUNDO);
	fileOp.wFunc			= FO_DELETE;
	SHFileOperation(&fileOp);
}

void ExplorerDialog::onCut(void)
{
	CIDataObject	dataObj(NULL);
	FolderExChange(NULL, &dataObj, DROPEFFECT_MOVE);
}

void ExplorerDialog::onCopy(void)
{
	CIDataObject	dataObj(NULL);
	FolderExChange(NULL, &dataObj, DROPEFFECT_COPY);
}

void ExplorerDialog::onPaste(void)
{
	/* Insure desired format is there, and open clipboard */
	if (::IsClipboardFormatAvailable(CF_HDROP) == TRUE) {
		if (::OpenClipboard(NULL) == FALSE)
			return;
	} else {
		return;
	}

	/* Get handle to Dropped Filelist data, and number of files */
	LPDROPFILES hFiles	= (LPDROPFILES)::GlobalLock(::GetClipboardData(CF_HDROP));
	if (hFiles == NULL) {
		ErrorMessage(::GetLastError());
		return;
	}
	LPBYTE	hEffect	= (LPBYTE)::GlobalLock(::GetClipboardData(::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)));
	if (hEffect == NULL) {
		ErrorMessage(::GetLastError());
		return;
	}

	/* get target */
	auto filesTo = GetPath(TreeView_GetSelection(_hTreeCtrl));

	if (hEffect[0] == 2) {
		doPaste(filesTo.c_str(), hFiles, DROPEFFECT_MOVE);
	} else if (hEffect[0] == 5) {
		doPaste(filesTo.c_str(), hFiles, DROPEFFECT_COPY);
	}
	::GlobalUnlock(hFiles);
	::GlobalUnlock(hEffect);
	::CloseClipboard();

	::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
	::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 200, NULL);
}

/**
 *	UpdateDevices()
 */
void ExplorerDialog::UpdateRoots(void)
{
	DWORD			driveList		= ::GetLogicalDrives();
	BOOL			isValidDrive	= FALSE;
	BOOL			haveChildren	= FALSE;

	HTREEITEM		hCurrentItem	= TreeView_GetNextItem(_hTreeCtrl, TVI_ROOT, TVGN_CHILD);

	TCHAR			drivePathName[]	= _T(" :\\");
	TCHAR			TEMP[MAX_PATH]	= {0};
	TCHAR			volumeName[MAX_PATH];

	for (INT i = 0; i < 26; i++) {
		drivePathName[0] = 'A' + i;

		if (0x01 & (driveList >> i))
		{
			/* create volume name */
			isValidDrive = ExploreVolumeInformation(drivePathName, TEMP, MAX_PATH);
			_stprintf(volumeName, _T("%c:"), 'A' + i);

			if (isValidDrive == TRUE)
			{
				_stprintf(volumeName, _T("%c: [%s]"), 'A' + i, TEMP);

				/* have children */
				haveChildren = HaveChildren(drivePathName);
			}

			if (hCurrentItem != NULL)
			{
				int			iIconNormal		= 0;
				int			iIconSelected	= 0;
				int			iIconOverlayed	= 0;

				/* get current volume name in list and test if name is changed */
				GetItemText(hCurrentItem, TEMP, MAX_PATH);

				if (_tcscmp(volumeName, TEMP) == 0)
				{
					/* if names are equal, go to next item in tree */
					ExtractIcons(drivePathName, NULL, DEVT_DRIVE, &iIconNormal, &iIconSelected, &iIconOverlayed);
					UpdateItem(hCurrentItem, volumeName, iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren);
					hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
				}
				else if (volumeName[0] == TEMP[0])
				{
					/* if names are not the same but the drive letter are equal, rename item */

					/* get icons */
					ExtractIcons(drivePathName, NULL, DEVT_DRIVE, &iIconNormal, &iIconSelected, &iIconOverlayed);
					UpdateItem(hCurrentItem, volumeName, iIconNormal, iIconSelected, iIconOverlayed, 0, haveChildren);
					DeleteChildren(hCurrentItem);
					hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
				}
				else
				{
					/* insert the device when new and not present before */
					HTREEITEM	hItem	= TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_PREVIOUS);
					InsertChildFolder(volumeName, TVI_ROOT, hItem, isValidDrive);
				}
			}
			else
			{
				InsertChildFolder(volumeName, TVI_ROOT, TVI_LAST, isValidDrive);
			}
		}
		else
		{
			if (hCurrentItem != NULL)
			{
				/* get current volume name in list and test if name is changed */
				GetItemText(hCurrentItem, TEMP, MAX_PATH);

				if (drivePathName[0] == TEMP[0])
				{
					HTREEITEM	pPrevItem	= hCurrentItem;
					hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
					TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
				}
			}
		}
	}
}

void ExplorerDialog::UpdateAllExpandedItems(void)
{
	HTREEITEM		hCurrentItem	= TreeView_GetChild(_hTreeCtrl, TVI_ROOT);

	while (hCurrentItem != NULL)
	{
		if (TreeView_GetItemState(_hTreeCtrl, hCurrentItem, TVIS_EXPANDED) & TVIS_EXPANDED)
		{
			auto path = GetItemText(hCurrentItem);
            if (!path.empty() && (L'A' <= path.front() && path.front() <= L'Z')) {
                path.resize(2);
            }
			UpdateChildren(path, hCurrentItem);
		}
		hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
	}
}

void ExplorerDialog::UpdatePath(void)
{
    if (!_pExProp->useFullTree) {
        auto path = GetPath(TreeView_GetSelection(_hTreeCtrl));
        _FileList.ToggleStackRec();
        _FileList.viewPath(path.c_str());
        _FileList.ToggleStackRec();
    }
}

HTREEITEM ExplorerDialog::InsertChildFolder(const std::wstring& childFolderName, HTREEITEM parentItem, HTREEITEM insertAfter, BOOL bChildrenTest)
{
	/* We search if it already exists */
	HTREEITEM			pCurrentItem	= TreeView_GetNextItem(_hTreeCtrl, parentItem, TVGN_CHILD);
	BOOL				bHidden			= FALSE;
	WIN32_FIND_DATA		Find			= {0};
	HANDLE				hFind			= NULL;
	DevType				devType			= (parentItem == TVI_ROOT ? DEVT_DRIVE : DEVT_DIRECTORY);

	pCurrentItem = NULL;

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
		hFind = ::FindFirstFile(path.c_str(), &Find);
		bHidden = ((Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
		::FindClose(hFind);
	}

	/* look if children test id allowed */
	BOOL	haveChildren = FALSE;
	if (bChildrenTest == TRUE) {
		haveChildren = HaveChildren(path.c_str());
	}

	/* insert item */
	INT					iIconNormal		= 0;
	INT					iIconSelected	= 0;
	INT					iIconOverlayed	= 0;

	/* get icons */
	ExtractIcons(path.c_str(), NULL, devType, &iIconNormal, &iIconSelected, &iIconOverlayed);

	/* set item */
	return InsertItem(childFolderName, iIconNormal, iIconSelected, iIconOverlayed, bHidden, parentItem, insertAfter, haveChildren);
}

BOOL ExplorerDialog::FindFolderAfter(LPCTSTR itemName, HTREEITEM pAfterItem)
{
	TCHAR		pszItem[MAX_PATH];
	BOOL		isFound = FALSE;
	HTREEITEM	hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, pAfterItem, TVGN_NEXT);

	while (hCurrentItem != NULL) {
		GetItemText(hCurrentItem, pszItem, MAX_PATH);
		if (_tcscmp(itemName, pszItem) == 0) {
			isFound = TRUE;
			hCurrentItem = NULL;
		}
		else {
			hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
		}
	}

	return isFound;
}

void ExplorerDialog::UpdateChildren(const std::wstring& parentPath, HTREEITEM hParentItem, BOOL doRecursive)
{
	std::wstring findPath = parentPath;
	HTREEITEM    hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hParentItem, TVGN_CHILD);

	if (findPath.empty()) {
		return;
	}
	if (findPath.back() != '\\') {
		findPath.push_back(L'\\');
	}
	findPath.push_back(L'*');

	WIN32_FIND_DATA			findData = { 0 };
	HANDLE					hFind = nullptr;
	if ((hFind = ::FindFirstFile(findPath.c_str(), &findData)) != INVALID_HANDLE_VALUE) {
		std::vector<FileSystemEntry> folders;
        std::vector<FileSystemEntry> files;

		/* find folders */
		do {
            if (::IsValidFolder(findData) == TRUE) {
                folders.emplace_back(findData.cFileName, findData.dwFileAttributes);
			}
            else if (_pExProp->useFullTree && ::IsValidFile(findData) == TRUE) {
                files.emplace_back(findData.cFileName, findData.dwFileAttributes);
            }
		} while (::FindNextFile(hFind, &findData));
		::FindClose(hFind);

		/* sort data */
        std::sort(folders.begin(), folders.end(), [](const auto& lhs, const auto& rhs) {
            return ::StrCmpLogicalW(lhs.name.c_str(), rhs.name.c_str()) < 0;
        });
        std::sort(files.begin(), files.end(), [](const auto &lhs, const auto &rhs) {
            return ::StrCmpLogicalW(lhs.name.c_str(), rhs.name.c_str()) < 0;
		});

		/* update tree */
        for (const auto entries : { &folders, &files }) {
            for (const auto& entry : *entries) {
                std::wstring name = GetItemText(hCurrentItem);
                if (!name.empty()) {
                    /* compare current item and the current folder name */
                    while ((name != entry.name) && (hCurrentItem != nullptr)) {
                        /* if it's not equal delete or add new item */
                        if (FindFolderAfter(entry.name.c_str(), hCurrentItem) == TRUE) {
                            HTREEITEM pPrevItem = hCurrentItem;
                            hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
                            TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
                        }
                        else {
                            HTREEITEM pPrevItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_PREVIOUS);

                            /* Note: If hCurrentItem is the first item in the list pPrevItem is nullptr */
                            if (pPrevItem == nullptr) {
                                hCurrentItem = InsertChildFolder(entry.name, hParentItem, TVI_FIRST);
                            }
                            else {
                                hCurrentItem = InsertChildFolder(entry.name, hParentItem, pPrevItem);
                            }
                        }

                        if (hCurrentItem != nullptr) {
                            name = GetItemText(hCurrentItem);
                        }
                    }

                    /* update icons and expandable information */
                    std::wstring currentPath = GetPath(hCurrentItem);
                    BOOL	haveChildren = HaveChildren(currentPath);

                    /* get icons and update item */
                    INT		iIconNormal = 0;
                    INT		iIconSelected = 0;
                    INT		iIconOverlayed = 0;
                    ExtractIcons(currentPath.c_str(), nullptr, DEVT_DIRECTORY, &iIconNormal, &iIconSelected, &iIconOverlayed);

                    BOOL bHidden = ((entry.attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
                    UpdateItem(hCurrentItem, name, iIconNormal, iIconSelected, iIconOverlayed, bHidden, haveChildren);

                    /* update recursive */
                    if ((doRecursive) && IsItemExpanded(hCurrentItem)) {
                        UpdateChildren(currentPath, hCurrentItem);
                    }

                    /* select next item */
                    hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
                }
                else {
                    hCurrentItem = InsertChildFolder(entry.name, hParentItem);
                    hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
                }
            }
        }

		/* delete possible not existed items */
		while (hCurrentItem != nullptr) {
			HTREEITEM	pPrevItem = hCurrentItem;
			hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
			TreeView_DeleteItem(_hTreeCtrl, pPrevItem);
		}
	}
}

void ExplorerDialog::FetchChildren(HTREEITEM parentItem)
{
	WIN32_FIND_DATA				findData = { 0 };
	HANDLE						hFind = NULL;
    std::vector<FileSystemEntry> folders;
    std::vector<FileSystemEntry> files;

	auto parentFolderPath = GetPath(parentItem);

	/* add wildcard */
    parentFolderPath.append(L"*");

    /* find first file */
	hFind = ::FindFirstFile(parentFolderPath.c_str(), &findData);

	/* if not found -> exit */
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
            if (::IsValidFolder(findData) == TRUE) {
                folders.emplace_back(findData.cFileName, findData.dwFileAttributes);
            }
            else if (_pExProp->useFullTree && ::IsValidFile(findData) == TRUE) {
                files.emplace_back(findData.cFileName, findData.dwFileAttributes);
            }
		} while (FindNextFile(hFind, &findData));

		::FindClose(hFind);

        /* sort data */
        std::sort(folders.begin(), folders.end(), [](const auto& lhs, const auto& rhs) {
            return ::StrCmpLogicalW(lhs.name.c_str(), rhs.name.c_str()) < 0;
        });
        std::sort(files.begin(), files.end(), [](const auto& lhs, const auto& rhs) {
            return ::StrCmpLogicalW(lhs.name.c_str(), rhs.name.c_str()) < 0;
        });

        for (const auto entries : { &folders, &files }) {
            for (const auto& entry : *entries) {
                if (InsertChildFolder(entry.name, parentItem) == NULL) {
                    break;
                }
            }
        }
	}
}

std::wstring ExplorerDialog::GetPath(HTREEITEM currentItem) const
{
	std::wstring result;
	std::vector<std::wstring> paths = GetItemPathFromRoot(currentItem);

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

void ExplorerDialog::NotifyNewFile(void)
{
	if (isCreated())
	{
        TCHAR	TEMP[MAX_PATH]{};
		::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
		_ToolBar.enable(IDM_EX_GO_TO_FOLDER, (_tcslen(TEMP) != 0));
	}
}


BOOL ExplorerDialog::ExploreVolumeInformation(LPCTSTR pszDrivePathName, LPTSTR pszVolumeName, UINT maxSize)
{
	GetVolumeInfo	volInfo;
	DWORD			dwThreadId		= 0;
	BOOL			isValidDrive	= FALSE;

	volInfo.pszDrivePathName	= pszDrivePathName;
	volInfo.pszVolumeName		= pszVolumeName;
	volInfo.maxSize				= maxSize;
	volInfo.pIsValidDrive		= &isValidDrive;


	_hExploreVolumeThread = ::CreateThread(NULL, 0, GetVolumeInformationTimeoutThread, &volInfo, 0, &dwThreadId);

	if (_hExploreVolumeThread) {
		if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_hEvent[EID_GET_VOLINFO], _pExProp->uTimeout)) {
			::TerminateThread(_hExploreVolumeThread, 0);
			isValidDrive = FALSE;
		}

		::CloseHandle(_hExploreVolumeThread);
		_hExploreVolumeThread = nullptr;
	}

	return isValidDrive;
}

void ExplorerDialog::UpdateLayout()
{
    RECT	rc = { 0 };
    RECT	rcWnd = { 0 };
    RECT	rcBuff = { 0 };

    getClientRect(rc);

    if ((_iDockedPos == CONT_LEFT) || (_iDockedPos == CONT_RIGHT)) {
        INT splitterPos = _pExProp->iSplitterPos;

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

        if (_pExProp->useFullTree) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight;
            ::SetWindowPos(_hTreeCtrl,      NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl,  NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl,      NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hFilter,        NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
        }
        else {
            /* set position of tree control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom = splitterPos;
            ::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set splitter */
            getClientRect(rc);
            rc.top = (splitterPos + toolBarHeight);
            rc.bottom = 6;
            ::SetWindowPos(_hSplitterCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of list control */
            getClientRect(rc);
            rc.top = (splitterPos + toolBarHeight + 6);
            rc.bottom -= (splitterPos + toolBarHeight + 6 + filterHeight);
            ::SetWindowPos(_hListCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of filter controls */
            getClientRect(rc);

            /* set position of combo */
            rc.top = rc.bottom - filterHeight + 1;
            rc.bottom = filterHeight;
            ::SetWindowPos(_hFilter, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
    else {
        INT splitterPos = _pExProp->iSplitterPosHorizontal;

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

        if (_pExProp->useFullTree) {
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight;
            ::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
            ::SetWindowPos(_hSplitterCtrl, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hListCtrl, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
            ::SetWindowPos(_hFilter, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
        }
        else {
            /* set position of tree control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.bottom -= toolBarHeight + filterHeight;
            rc.right = splitterPos;
            ::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of filter controls */
            getClientRect(rc);
            rcBuff = rc;

            /* set position of combo */
            rc.top = rcBuff.bottom - filterHeight + 6;
            rc.bottom = filterHeight;
            rc.left = rcBuff.left;
            rc.right = splitterPos - rcBuff.left;
            ::SetWindowPos(_hFilter, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set splitter */
            getClientRect(rc);
            rc.left = splitterPos;
            rc.right = 6;
            ::SetWindowPos(_hSplitterCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

            /* set position of list control */
            getClientRect(rc);
            rc.top += toolBarHeight;
            rc.left = splitterPos + 6;
            rc.right -= rc.left;
            ::SetWindowPos(_hListCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
        }
    }
}

/***************************************************************************************
 *  Drag'n'Drop, Cut and Copy of folders
 */
void ExplorerDialog::FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect)
{
	SIZE_T		bufsz	= sizeof(DROPFILES) + sizeof(TCHAR);
	HTREEITEM	hItem	= NULL;

	/* get selected item */
	if (dwEffect == (DROPEFFECT_COPY | DROPEFFECT_MOVE))
	{
		TVHITTESTINFO	ht		= {0};
		::GetCursorPos(&ht.pt);
		::ScreenToClient(_hTreeCtrl, &ht.pt);
		hItem = TreeView_HitTest(_hTreeCtrl, &ht);
	}
	else
	{
		hItem = TreeView_GetSelection(_hTreeCtrl);
	}

	/* get buffer size */
	auto path = GetPath(hItem);
    if (path.back() == L'\\') {
        path.pop_back();
    }
	bufsz += (path.size() + 1) * sizeof(WCHAR);

	/* allocate global resources */
	HDROP hDrop = (HDROP)GlobalAlloc(GHND | GMEM_SHARE, bufsz);
	if (NULL == hDrop)
		return;

	LPDROPFILES lpDropFileStruct = (LPDROPFILES)::GlobalLock(hDrop);
	if (NULL == lpDropFileStruct) {
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
	_tcscpy((LPTSTR)&lpDropFileStruct[1], path.c_str());
	GlobalUnlock(hDrop);

	/* Init the supported format */
	FORMATETC fmtetc	= {0};
	fmtetc.cfFormat		= CF_HDROP;
	fmtetc.dwAspect		= DVASPECT_CONTENT;
	fmtetc.lindex		= -1;
	fmtetc.tymed		= TYMED_HGLOBAL;

	/* Init the medium used */
	STGMEDIUM medium = {0};
	medium.tymed	= TYMED_HGLOBAL;
	medium.hGlobal	= hDrop;

	/* Add it to DataObject */
	pdobj->SetData(&fmtetc, &medium, TRUE);

	if (pdsrc == NULL)
	{
		hDrop = (HDROP)GlobalAlloc(GHND|GMEM_SHARE, 4);
		if (NULL == hDrop)
			return;

		LPBYTE prefCopyData = (LPBYTE)::GlobalLock(hDrop);
		if (NULL == prefCopyData) {
			GlobalFree(hDrop);
			return;
		}

		int	eff = (dwEffect == DROPEFFECT_MOVE ? 2 : 5);
		::ZeroMemory(prefCopyData, 4);
		CopyMemory(prefCopyData, &eff, 1);
		::GlobalUnlock(hDrop);

		/* Init the supported format */
		fmtetc.cfFormat		= ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
		/* Init the medium used */
		medium.hGlobal		= hDrop;

		pdobj->SetData(&fmtetc, &medium, TRUE);

		if (::OleSetClipboard(pdobj) == S_OK) {
			::OleFlushClipboard();
		}
	}
	else
	{
		/* Initiate the Drag & Drop */
		DWORD	dwEffectResp;
		::DoDragDrop(pdobj, pdsrc, dwEffect, &dwEffectResp);
	}
}

bool ExplorerDialog::OnDrop(FORMATETC* /* pFmtEtc */, STGMEDIUM& medium, DWORD* pdwEffect)
{
	LPDROPFILES hFiles	= (LPDROPFILES)::GlobalLock(medium.hGlobal);
	if (NULL == hFiles)
		return false;

	/* get target */
	auto path = GetPath(TreeView_GetDropHilight(_hTreeCtrl));

	doPaste(path.c_str(), hFiles, *pdwEffect);
	::CloseClipboard();

	TreeView_SelectDropTarget(_hTreeCtrl, NULL);

	return true;
}

void ExplorerDialog::NavigateBack()
{
    TCHAR			pszPath[MAX_PATH];
    BOOL			dirValid	= TRUE;
    BOOL			selected	= TRUE;
    std::vector<std::wstring>   vStrItems;

    _FileList.ToggleStackRec();

    do {
        dirValid = _FileList.GetPrevDir(pszPath, vStrItems);
        if (dirValid) {
            selected = SelectItem(pszPath);
        }
    } while (dirValid && (selected == FALSE));

    if (selected == FALSE) {
        _FileList.GetNextDir(pszPath, vStrItems);
    }

    if (vStrItems.size() != 0) {
        _FileList.SetItems(vStrItems);
    }

    _FileList.ToggleStackRec();
}

void ExplorerDialog::NavigateForward()
{
    TCHAR			pszPath[MAX_PATH];
    BOOL			dirValid	= TRUE;
    BOOL			selected	= TRUE;
    std::vector<std::wstring>	vStrItems;

    _FileList.ToggleStackRec();

    do {
        dirValid = _FileList.GetNextDir(pszPath, vStrItems);
        if (dirValid) {
            selected = SelectItem(pszPath);
        }
    } while (dirValid && (selected == FALSE));

    if (selected == FALSE) {
        _FileList.GetPrevDir(pszPath, vStrItems);
    }

    if (vStrItems.size() != 0) {
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
            HTREEITEM item = TreeView_GetSelection(_hTreeCtrl);
            lastPath = GetPath(item);
            navigatePath = lastPath;
            navigatePath = navigatePath.concat(path).lexically_normal().concat(L"\\");
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
    if (!path.empty())
    {
        HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);

        /* get current folder path */
        auto filePath = GetPath(hItem);
        filePath += path;

        /* open possible link */
        TCHAR resolvedPath[MAX_PATH];
        if (ResolveShortCut(filePath.c_str(), resolvedPath, MAX_PATH) == S_OK) {
            if (::PathIsDirectory(resolvedPath) != FALSE) {
                SelectItem(resolvedPath);
            }
            else {
                ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)resolvedPath);
            }
        }
        else {
            ::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)filePath.c_str());
        }
    }
}

void ExplorerDialog::Refresh()
{
    ::SetEvent(g_hEvent[EID_UPDATE_USER]);
}

bool ExplorerDialog::doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect)
{
	/* get files from and to, to fill struct */
	UINT		headerSize				= sizeof(DROPFILES);
	SIZE_T		payloadSize				= ::GlobalSize(hData) - headerSize;
	LPVOID		pPld					= (LPBYTE)hData + headerSize;
	LPTSTR		lpszFilesFrom			= NULL;

	if (((LPDROPFILES)hData)->fWide == TRUE) {
		lpszFilesFrom = (LPWSTR)pPld;
	} else {
		lpszFilesFrom = new TCHAR[payloadSize];
		::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pPld, (int)payloadSize, lpszFilesFrom, (int)payloadSize);
	}

	if (lpszFilesFrom != NULL)
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
			? StringUtil::format(L"Move %d file(s)/folder(s) to:\n\n%s", count, pszTo)
			: StringUtil::format(L"Copy %d file(s)/folder(s) to:\n\n%s", count, pszTo);

		if (::MessageBox(_hSelf, message.c_str(), _T("Explorer"), MB_YESNO) == IDYES)
		{
			// TODO move or copy the file views into other window in dependency to keystate
			SHFILEOPSTRUCT	fileOp	= {0};
			fileOp.hwnd				= _hParent;
			fileOp.pFrom			= lpszFilesFrom;
			fileOp.pTo				= pszTo;
			fileOp.fFlags			= FOF_RENAMEONCOLLISION;
			if (dwEffect == DROPEFFECT_MOVE) {
				fileOp.wFunc		= FO_MOVE;
			} else {
				fileOp.wFunc		= FO_COPY;
			}
			SHFileOperation(&fileOp);

			::KillTimer(_hSelf, EXT_UPDATEACTIVATEPATH);
			::SetTimer(_hSelf, EXT_UPDATEACTIVATEPATH, 200, NULL);
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
