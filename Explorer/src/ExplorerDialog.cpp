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

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "ExplorerResource.h"
#include "ContextMenu.h"
#include "NewDlg.h"
#include "Scintilla.h"
#include "ToolTip.h"
#include "resource.h"

#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <dbt.h>


ToolTip		toolTip;


BOOL	DEBUG_ON		= FALSE;
#define DEBUG_FLAG(x)	if(DEBUG_ON == TRUE) DEBUG(x);

#ifndef CSIDL_PROFILE
#define CSIDL_PROFILE (0x0028)
#endif

HANDLE g_hEvent[EID_MAX]	= {NULL};
HANDLE g_hThread			= NULL;



DWORD WINAPI UpdateThread(LPVOID lpParam)
{
	BOOL			bRun			= TRUE;
    DWORD			dwWaitResult    = EID_INIT;
	ExplorerDialog*	dlg             = (ExplorerDialog*)lpParam;

	dlg->NotifyEvent(dwWaitResult);

	while (bRun)
	{
		dwWaitResult = ::WaitForMultipleObjects(EID_MAX_THREAD, g_hEvent, FALSE, INFINITE);

		if (dwWaitResult == EID_THREAD_END)
		{
			bRun = FALSE;
		}
		else if (dwWaitResult < EID_MAX)
		{
			dlg->NotifyEvent(dwWaitResult);
		}
	}
	return 0;
}


DWORD WINAPI GetVolumeInformationTimeoutThread(LPVOID lpParam)
{
	DWORD			serialNr		= 0;
	DWORD			space			= 0;
	DWORD			flags			= 0;
	tGetVolumeInfo*	volInfo         = (tGetVolumeInfo*)lpParam;

	if (volInfo->maxSize < MAX_PATH+1)
		*volInfo->pIsValidDrive = GetVolumeInformation(volInfo->pszDrivePathName,
			volInfo->pszVolumeName, volInfo->maxSize, &serialNr, &space, &flags, NULL, 0);

	::SetEvent(g_hEvent[EID_GET_VOLINFO]);
	::ExitThread(0);
	return 0;
}


static ToolBarButtonUnit toolBarIcons[] = {
	
	{IDM_EX_PREV,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_PREV, TBSTYLE_DROPDOWN},
	{IDM_EX_NEXT,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_NEXT, TBSTYLE_DROPDOWN},	 
	 
	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,	IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//
	 
    {IDM_EX_FILE_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FILENEW, 0},
    {IDM_EX_FOLDER_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERNEW, 0},
	{IDM_EX_SEARCH_FIND,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FIND, 0},
	{IDM_EX_GO_TO_FOLDER,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERGO, 0},

	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//
	 
	{IDM_EX_UPDATE,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_UPDATE, 0}
};
					
static ToolBarButtonUnit toolBarIconsNT[] = {
	
	{IDM_EX_PREV,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_PREV, TBSTYLE_DROPDOWN},
	{IDM_EX_NEXT,			IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_NEXT, TBSTYLE_DROPDOWN},	 
	 
	//-------------------------------------------------------------------------------------//
	{0,						IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDI_SEPARATOR_ICON, 0},
	//-------------------------------------------------------------------------------------//
	 
    {IDM_EX_FILE_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FILENEW, 0},
    {IDM_EX_FOLDER_NEW,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FOLDERNEW, 0},
	{IDM_EX_SEARCH_FIND,	IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON,		IDI_SEPARATOR_ICON, IDB_EX_FIND, 0},
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
static LPTSTR szToolTip[23] = {
	_T("Previous Folder"),
	_T("Next Folder"),
	_T("New File..."),
	_T("New Folder..."),
	_T("Find in Files..."),
	_T("Folder of Current File"),
	_T("User Folder"),
	_T("Refresh")
};


LPTSTR ExplorerDialog::GetNameStrFromCmd(UINT resID)
{
	if ((IDM_EX_PREV <= resID) && (resID <= IDM_EX_UPDATE)) {
		return szToolTip[resID - IDM_EX_PREV];
	}
	return nullptr;
}




ExplorerDialog::ExplorerDialog(void) : DockingDlgInterface(IDD_EXPLORER_DLG)
{
	_hDefaultTreeProc		= NULL;
	_hDefaultSplitterProc	= NULL;
	_hTreeCtrl				= NULL;
	_hListCtrl				= NULL;
	_hHeader				= NULL;
	_hFilter				= NULL;
	_hCurWait				= NULL;
	_isScrolling			= FALSE;
	_isDnDStarted			= FALSE;
	_isSelNotifyEnable		= TRUE;
	_isLeftButtonDown		= FALSE;
	_hSplitterCursorUpDown	= NULL;
	_bStartupFinish			= FALSE;
	_hFilterButton			= NULL;
	_bOldRectInitilized		= FALSE;
	_hExploreVolumeThread	= NULL;
	_hItemExpand			= NULL;
	TreeHelper::UseOverlayThreading();
}

ExplorerDialog::~ExplorerDialog(void)
{
}


void ExplorerDialog::init(HINSTANCE hInst, NppData nppData, tExProp *prop)
{
	_nppData = nppData;
	DockingDlgInterface::init(hInst, nppData._nppHandle);

	_pExProp = prop;
	_FileList.initProp(prop);
}


void ExplorerDialog::doDialog(bool willBeShown)
{
    if (!isCreated())
	{
		create(&_data);

		// define the default docking behaviour
		_data.uMask			= DWS_DF_CONT_LEFT | DWS_ADDINFO | DWS_ICONTAB;
		if (!NLGetText(_hInst, _nppData._nppHandle, _T("Explorer"), _data.pszName, MAX_PATH)) {
			_tcscpy(_data.pszName, _T("Explorer"));
		}
		_data.pszAddInfo	= _pExProp->szCurrentPath;
		_data.hIconTab		= (HICON)::LoadImage(_hInst, MAKEINTRESOURCE(IDI_EXPLORE), IMAGE_ICON, 0, 0, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT);
		_data.pszModuleName	= getPluginFileName();
		_data.dlgID			= DOCKABLE_EXPLORER_INDEX;
		::SendMessage(_hParent, NPPM_DMMREGASDCKDLG, 0, (LPARAM)&_data);
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


BOOL CALLBACK ExplorerDialog::run_dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
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

			/* Only used on non NT based systems */
			if ((HWND)lParam == _hFilterButton)
			{
				TCHAR	TEMP[MAX_PATH];

				_ComboFilter.getText(TEMP);
				_ComboFilter.addText(TEMP);
				_FileList.filterFiles(TEMP);
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

			if (nmhdr->hwndFrom == _hTreeCtrl)
			{
				switch (nmhdr->code)
				{
					case NM_RCLICK:
					{
						ContextMenu		cm;
						POINT			pt		= {0};
						TVHITTESTINFO	ht		= {0};
						DWORD			dwpos	= ::GetMessagePos();
						HTREEITEM		hItem	= NULL;

						pt.x = GET_X_LPARAM(dwpos);
						pt.y = GET_Y_LPARAM(dwpos);

						ht.pt = pt;
						::ScreenToClient(_hTreeCtrl, &ht.pt);

						hItem = TreeView_HitTest(_hTreeCtrl, &ht);
						if (hItem != NULL)
						{
							TCHAR	strPathName[MAX_PATH];

							GetFolderPathName(hItem, strPathName);

							cm.SetObjects(strPathName);
							cm.ShowContextMenu(_hInst, _nppData._nppHandle, _hSelf, pt);
						}
						return TRUE;
					}
					case TVN_SELCHANGED:
					{
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
				lpttt->lpszText = GetNameStrFromCmd(resId);
				break;
			}

			DockingDlgInterface::run_dlgProc(hWnd, Message, wParam, lParam);

			return FALSE;
		}
		case WM_SIZE:
		case WM_MOVE:
		{
			if (_bStartupFinish == FALSE)
				return TRUE;

			HWND	hWnd		= NULL;
			RECT	rc			= {0};
			RECT	rcWnd		= {0};
			RECT	rcBuff		= {0};

			getClientRect(rc);

			if ((_iDockedPos == CONT_LEFT) || (_iDockedPos == CONT_RIGHT))
			{
				INT		splitterPos	= _pExProp->iSplitterPos;

				if (splitterPos < 50)
					splitterPos = 50;
				else if (splitterPos > (rc.bottom - 100))
					splitterPos = rc.bottom - 100;

				/* set position of toolbar */
				_ToolBar.reSizeTo(rc);
				_Rebar.reSizeTo(rc);

				/* set position of tree control */
				getClientRect(rc);
				rc.top    += 26;
				rc.bottom  = splitterPos;
				::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set splitter */
				getClientRect(rc);
				rc.top	   = (splitterPos + 26);
				rc.bottom  = 6;
				::SetWindowPos(_hSplitterCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set position of list control */
				getClientRect(rc);
				rc.top	   = (splitterPos + 32);
				rc.bottom -= (splitterPos + 32 + 22);
				::SetWindowPos(_hListCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set position of filter controls */
				getClientRect(rc);
				rcBuff = rc;

				/* set position of static text */
				if (_hFilterButton == NULL)
				{
					hWnd = ::GetDlgItem(_hSelf, IDC_STATIC_FILTER);
					::GetWindowRect(hWnd, &rcWnd);
					rc.top	     = rcBuff.bottom - 18;
					rc.bottom    = 12;
					rc.left     += 2;
					rc.right     = rcWnd.right - rcWnd.left;
					::SetWindowPos(hWnd, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
				}
				else
				{
					rc.top	     = rcBuff.bottom - 21;
					rc.bottom    = 20;
					rc.left     += 2;
					rc.right     = 35;
					::SetWindowPos(_hFilterButton, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
				}
				rcBuff.left = rc.right + 4;

				/* set position of combo */
				rc.top		 = rcBuff.bottom - 21;
				rc.bottom	 = 20;
				rc.left		 = rcBuff.left;
				rc.right	 = rcBuff.right - rcBuff.left;
				::SetWindowPos(_hFilter, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);				
			}
			else
			{
				INT		splitterPos	= _pExProp->iSplitterPosHorizontal;

				if (splitterPos < 50)
					splitterPos = 50;
				else if (splitterPos > (rc.right - 50))
					splitterPos = rc.right - 50;

				/* set position of toolbar */
				rc.right   = splitterPos;
				_ToolBar.reSizeTo(rc);
				_Rebar.reSizeTo(rc);

				/* set position of tree control */
				getClientRect(rc);
				rc.top    += 26;
				rc.bottom -= 26 + 22;
				rc.right   = splitterPos;
				::SetWindowPos(_hTreeCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set position of filter controls */
				getClientRect(rc);
				rcBuff = rc;

				/* set position of static text */
				if (_hFilterButton == NULL)
				{
					hWnd = ::GetDlgItem(_hSelf, IDC_STATIC_FILTER);
					::GetWindowRect(hWnd, &rcWnd);
					rc.top	     = rcBuff.bottom - 18;
					rc.bottom    = 12;
					rc.left     += 2;
					rc.right     = rcWnd.right - rcWnd.left;
					::SetWindowPos(hWnd, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
				}
				else
				{
					rc.top	     = rcBuff.bottom - 21;
					rc.bottom    = 20;
					rc.left     += 2;
					rc.right     = 35;
					::SetWindowPos(_hFilterButton, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
				}
				rcBuff.left = rc.right + 4;

				/* set position of combo */
				rc.top		 = rcBuff.bottom - 21;
				rc.bottom	 = 20;
				rc.left		 = rcBuff.left;
				rc.right	 = splitterPos - rcBuff.left;
				::SetWindowPos(_hFilter, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set splitter */
				getClientRect(rc);
				rc.left		 = splitterPos;
				rc.right     = 6;
				::SetWindowPos(_hSplitterCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);

				/* set position of list control */
				getClientRect(rc);
				rc.left      = splitterPos + 6;
				rc.right    -= rc.left;
				::SetWindowPos(_hListCtrl, NULL, rc.left, rc.top, rc.right, rc.bottom, SWP_NOZORDER | SWP_SHOWWINDOW);
			}
			break;
		}
		case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT*	pDrawItemStruct	= (DRAWITEMSTRUCT *)lParam;

			if (pDrawItemStruct->hwndItem == _hSplitterCtrl)
			{
				RECT		rc		= pDrawItemStruct->rcItem;
				HDC			hDc		= pDrawItemStruct->hDC;
				HBRUSH		bgbrush	= ::CreateSolidBrush(::GetSysColor(COLOR_BTNFACE));

				/* fill background */
				::FillRect(hDc, &rc, bgbrush);

				::DeleteObject(bgbrush);
				return TRUE;
			}
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

			::DestroyIcon(_data.hIconTab);
			_ComboFilter.getComboList(_pExProp->vStrFilterHistory);
			_ComboFilter.getText(szLastFilter, MAX_PATH);
			if (_tcslen(szLastFilter) != 0)
				_pExProp->strLastFilter = szLastFilter;

			::SetEvent(g_hEvent[EID_THREAD_END]);
			if (::WaitForSingleObject(_hExploreVolumeThread, 50) != WAIT_OBJECT_0)
				Sleep(1);
			if (::WaitForSingleObject(g_hThread, 50) != WAIT_OBJECT_0)
				Sleep(1);

			/* destroy events */
			for (int i = 0; i < EID_MAX; i++)
				::CloseHandle(g_hEvent[i]);

			/* destroy thread */
			::CloseHandle(g_hThread);

			/* unsubclass */
			if (_hDefaultTreeProc != NULL)
				::SetWindowLongPtr(_hTreeCtrl, GWLP_WNDPROC, (LONG_PTR)_hDefaultTreeProc);
			if (_hDefaultSplitterProc != NULL)
				::SetWindowLongPtr(_hSplitterCtrl, GWLP_WNDPROC, (LONG_PTR)_hDefaultSplitterProc);

			break;
		}
		case EXM_CHANGECOMBO:
		{
			TCHAR	TEMP[MAX_PATH];
			if (_ComboFilter.getSelText(TEMP))
				_FileList.filterFiles(TEMP);
			return TRUE;
		}
		case EXM_OPENDIR:
		{
			if (_tcslen((LPTSTR)lParam) != 0)
			{
				TCHAR		folderPathName[MAX_PATH]	= _T("\0");
				TCHAR		folderChildPath[MAX_PATH]	= _T("\0");

				LPTSTR		szInList		= NULL;
				HTREEITEM	hItem			= TreeView_GetSelection(_hTreeCtrl);

				_tcscpy(folderPathName, (LPTSTR)lParam);

				if (((folderPathName[1] != ':') && (folderPathName[2] != '\\')) &&
					((folderPathName[0] != '\\') &&(folderPathName[1] != '\\')))
				{
					/* get current folder path */
					GetFolderPathName(hItem, folderPathName);
					_stprintf(folderPathName, _T("%s%s\\"), folderPathName, (LPTSTR)lParam);

					/* test if selected parent folder */
					if (_tcscmp((LPTSTR)lParam, _T("..")) == 0)
					{
						/* if so get the parent folder name and the current one */
						*_tcsrchr(folderPathName, '\\') = '\0';
						*_tcsrchr(folderPathName, '\\') = '\0';
						szInList = _tcsrchr(folderPathName, '\\');

						/* 
						 * if pointer of szInList is smaller as pointer of 
						 * folderPathName, it seems to be a root folder and break
						 */
						if (szInList < folderPathName)
							break;

						_tcscpy(folderChildPath, &szInList[1]);
						szInList[1] = '\0';
					}
				}

				/* if last char no backslash, add one */
				if (folderPathName[_tcslen(folderPathName)-1] != '\\')
					_tcscat(folderPathName, _T("\\"));

				/* select item */
				SelectItem(folderPathName);

				/* set position of selection */
				if (folderChildPath[0] != '\0') {
					_FileList.SelectFolder(folderChildPath);
				} else {
					_FileList.SelectFolder(_T(".."));
				}
			}
			return TRUE;
		}
		case EXM_OPENFILE:
		{
			if (_tcslen((LPTSTR)lParam) != 0)
			{
				TCHAR		pszFilePath[MAX_PATH];
				TCHAR		pszShortcutPath[MAX_PATH];
				HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);

				/* get current folder path */
				GetFolderPathName(hItem, pszFilePath);
				_stprintf(pszShortcutPath, _T("%s%s"), pszFilePath, (LPTSTR)lParam);

				/* open possible link */
				if (ResolveShortCut(pszShortcutPath, pszFilePath, MAX_PATH) == S_OK) {
					if (::PathIsDirectory(pszFilePath) != FALSE) {
						SelectItem(pszFilePath);
					} else {
						::SendMessage(_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)pszFilePath);
					}
				} else {
					::SendMessage(_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)pszShortcutPath);
				}
			}
			return TRUE;
		}
		case EXM_RIGHTCLICK:
		{
			ContextMenu		cm;
			POINT			pt				= {0};
			vector<string>	files			= *((vector<string>*)lParam);
			HTREEITEM		hItem			= TreeView_GetSelection(_hTreeCtrl);
			DWORD			dwpos			= ::GetMessagePos();
			TCHAR			folderPathName[MAX_PATH];

			GetFolderPathName(hItem, folderPathName);

			pt.x = GET_X_LPARAM(dwpos);
			pt.y = GET_Y_LPARAM(dwpos);

			cm.SetObjects(files);
			cm.ShowContextMenu(_hInst, _nppData._nppHandle, _hSelf, pt, wParam == FALSE);

			return TRUE;
		}
		case EXM_UPDATE_PATH :
		{
			UpdatePath();
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
			else if (wParam == EXT_SELCHANGE)
			{
				::KillTimer(_hSelf, EXT_SELCHANGE);

				TCHAR		strPathName[MAX_PATH];
				HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);

				if (hItem != NULL)
				{
					GetFolderPathName(hItem, strPathName);
					_FileList.viewPath(strPathName, TRUE);
					UPDATE_CAPTION();
				}
				return FALSE;
			}
			return TRUE;
		}
		default:
			return DockingDlgInterface::run_dlgProc(hWnd, Message, wParam, lParam);
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
					HTREEITEM	hItem = TreeView_GetSelection(_hTreeCtrl);
					if (TreeView_GetChild(_hTreeCtrl, hItem) == NULL)
						DrawChildren(hItem);
					TreeView_Expand(_hTreeCtrl, hItem, TVE_TOGGLE);
					return TRUE;
				}
				case VK_TAB:
				{
					if ((0x80 & ::GetKeyState(VK_SHIFT)) == 0x80) {
						::SetFocus(_hFilter);
					} else {
						::SetFocus(_hListCtrl);
					}
					return TRUE;
				}
				default:
					break;
			}
			break;
		}
		case WM_KEYDOWN:
		{
			if ((wParam == VK_DELETE) && !((0x80 & ::GetKeyState(VK_CONTROL)) == 0x80))
			{
				onDelete((0x80 & ::GetKeyState(VK_SHIFT)) == 0x80);
				return TRUE;
			}
			if (wParam == VK_F5)
			{
				::SetEvent(g_hEvent[EID_UPDATE_USER]);
				return TRUE;
			}
			break;
		}
		case WM_SYSKEYDOWN:
		{
			if ((0x80 & ::GetKeyState(VK_MENU)) == 0x80)
			{
				if (wParam == VK_LEFT) {
					tb_cmd(IDM_EX_PREV);
					return TRUE;
				} else if (wParam == VK_RIGHT) {
					tb_cmd(IDM_EX_NEXT);
					return TRUE;
				}
			}
			break;
		}
		case EXM_QUERYDROP:
		{
			TVHITTESTINFO	ht		= {0};

			if (_isScrolling == FALSE)
			{
				eScDir scrDir = GetScrollDirection(_hTreeCtrl);

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
				eScDir		scrDir		= GetScrollDirection(_hTreeCtrl);

				if ((scrDir != SCR_UP) || (hItemHigh == hItemRoot) || (!m_bAllowDrop)) {
					::KillTimer(_hTreeCtrl, EXT_SCROLLLISTUP);
					_isScrolling = FALSE;
				} else {
					::SendMessage(_hTreeCtrl, 277, 0, NULL);				
				}
				return FALSE;
			}
			else if (wParam == EXT_SCROLLLISTDOWN)
			{
				HTREEITEM	hItemHigh	= TreeView_GetDropHilight(_hTreeCtrl);
				HTREEITEM	hItemLast	= TreeView_GetLastVisible(_hTreeCtrl);
				eScDir		scrDir		= GetScrollDirection(_hTreeCtrl);

				if ((scrDir != SCR_DOWN) || (hItemHigh == hItemLast) || (!m_bAllowDrop)) {
					::KillTimer(_hTreeCtrl, EXT_SCROLLLISTDOWN);
					_isScrolling = FALSE;
				} else {
					::SendMessage(_hTreeCtrl, 277, 1, NULL);
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
		{
			TCHAR			pszPath[MAX_PATH];
			BOOL			dirValid	= TRUE;
			BOOL			selected	= TRUE;
			vector<string>	vStrItems;

			_FileList.ToggleStackRec();

			do {
				if (dirValid = _FileList.GetPrevDir(pszPath, vStrItems))
					selected = SelectItem(pszPath);
			} while (dirValid && (selected == FALSE));

			if (selected == FALSE)
				_FileList.GetNextDir(pszPath, vStrItems);

			if (vStrItems.size() != 0)
				_FileList.SetItems(vStrItems);

			_FileList.ToggleStackRec();
			break;
		}
		case IDM_EX_NEXT:
		{
			TCHAR			pszPath[MAX_PATH];
			BOOL			dirValid	= TRUE;
			BOOL			selected	= TRUE;
			vector<string>	vStrItems;

			_FileList.ToggleStackRec();

			do {
				if (dirValid = _FileList.GetNextDir(pszPath, vStrItems))
					selected = SelectItem(pszPath);
			} while (dirValid && (selected == FALSE));

			if (selected == FALSE)
				_FileList.GetPrevDir(pszPath, vStrItems);

			if (vStrItems.size() != 0)
				_FileList.SetItems(vStrItems);

			_FileList.ToggleStackRec();
			break;
		}
		case IDM_EX_FILE_NEW:
		{
			NewDlg		dlg;
			TCHAR		szFileName[MAX_PATH];
			TCHAR		szComment[MAX_PATH];
			BOOL		bLeave		= FALSE;

			szFileName[0] = '\0';

			/* rename comment */
			if (NLGetText(_hInst, _nppData._nppHandle, _T("New file"), szComment, MAX_PATH) == 0) {
				_tcscpy(szComment, _T("New file"));
			}

			dlg.init(_hInst, _hParent);
			while (bLeave == FALSE)
			{
				if (dlg.doDialog(szFileName, szComment) == TRUE)
				{
					/* test if is correct */
					if (IsValidFileName(szFileName))
					{
						TCHAR	pszNewFile[MAX_PATH];

						GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszNewFile);
						_tcscat(pszNewFile, szFileName);
						
						::CloseHandle(::CreateFile(pszNewFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
						::SendMessage(_hParent, NPPM_DOOPEN, 0, (LPARAM)pszNewFile);

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
			TCHAR		szComment[MAX_PATH];
			BOOL		bLeave			= FALSE;

			szFolderName[0] = '\0';

			/* rename comment */
			if (NLGetText(_hInst, _nppData._nppHandle, _T("New folder"), szComment, MAX_PATH) == 0) {
				_tcscpy(szComment, _T("New folder"));
			}

			dlg.init(_hInst, _hParent);
			while (bLeave == FALSE)
			{
				if (dlg.doDialog(szFolderName, szComment) == TRUE)
				{
					/* test if is correct */
					if (IsValidFileName(szFolderName))
					{
						TCHAR	pszNewFolder[MAX_PATH];

						GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszNewFolder);
						_tcscat(pszNewFolder, szFolderName);
						
						if (::CreateDirectory(pszNewFolder, NULL) == FALSE) {
							if (NLMessageBox(_hInst, _hParent, _T("MsgBox FolderCreateError"), MB_OK) == FALSE)
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
			TCHAR	pszPath[MAX_PATH];
			GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszPath);
			::SendMessage(_hParent, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)pszPath, NULL);
			break;
		}
		case IDM_EX_GO_TO_USER:
		{
			TCHAR	pathName[MAX_PATH];

			if (SHGetSpecialFolderPath(_hSelf, pathName, CSIDL_PROFILE, FALSE) == TRUE) {
				_tcscat(pathName, _T("\\"));
				SelectItem(pathName);
			}
			break;
		}
		case IDM_EX_GO_TO_FOLDER:
		{
			TCHAR	pathName[MAX_PATH];
			::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)pathName);
			_tcscat(pathName, _T("\\"));
			SelectItem(pathName);
			_FileList.SelectCurFile();
			break;
		}
		case IDM_EX_UPDATE:
		{
			::SetEvent(g_hEvent[EID_UPDATE_USER]);
			break;
		}
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
		vector<string>	vStrItems;

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
			_ComboFilter.setText((LPTSTR)_pExProp->strLastFilter.c_str());

			/* initilize file list */
			_FileList.SetToolBarInfo(&_ToolBar , IDM_EX_PREV, IDM_EX_NEXT);

			/* initial tree */
			UpdateDevices();

			/* set data */
			SelectItem(_pExProp->szCurrentPath);

			/* Update "Go to Folder" icon */
			NotifyNewFile();
			break;
		}
		case EID_UPDATE_DEVICE :
		{
			UpdateDevices();
			break;
		}
		case EID_UPDATE_USER :
		{
			/* No break!! */
			UpdateDevices();
		}
		case EID_UPDATE_ACTIVATE :
		{
			UpdateFolders();
			UpdatePath();
			break;
		}
		case EID_UPDATE_ACTIVATEPATH :
		{
			TCHAR		strPathName[MAX_PATH];
			HTREEITEM	hItem		= TreeView_GetSelection(_hTreeCtrl);
			HTREEITEM	hParentItem = TreeView_GetParent(_hTreeCtrl, hItem);

			if (hParentItem != NULL)
			{
				GetFolderPathName(hParentItem, strPathName);
				UpdateChildren(strPathName, hParentItem, FALSE);
			}
			if (hItem != NULL)
			{
				GetFolderPathName(hItem, strPathName);
				UpdateChildren(strPathName, hItem, FALSE);
				UpdatePath();
			}
			break;
		}
		case EID_EXPAND_ITEM:
		{
			if (!TreeView_GetChild(_hTreeCtrl, _hItemExpand)) {
				DrawChildren(_hItemExpand);
			} else {
				/* set cursor back before tree is updated for faster access */
				::SetClassLongPtr(_hSelf, GCLP_HCURSOR, oldCur);
				::EnableWindow(_hSelf, TRUE);
				::GetCursorPos(&pt);
				::SetCursorPos(pt.x, pt.y);

				TCHAR	strPathName[MAX_PATH];
				GetFolderPathName(_hItemExpand, strPathName);
				strPathName[_tcslen(strPathName)-1] = '\0';
				UpdateChildren(strPathName, _hItemExpand);

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

	if (gWinVersion < WV_NT) {
		_hFilterButton = ::GetDlgItem(_hSelf, IDC_BUTTON_FILTER);
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_STATIC_FILTER));
	} else {
		::DestroyWindow(::GetDlgItem(_hSelf, IDC_BUTTON_FILTER));
	}

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
	if (gWinVersion < WV_NT) {
		_ToolBar.init(_hInst, _hSelf, TB_STANDARD, toolBarIcons, sizeof(toolBarIcons)/sizeof(ToolBarButtonUnit));
	} else {
		_ToolBar.init(_hInst, _hSelf, TB_STANDARD, toolBarIconsNT, sizeof(toolBarIconsNT)/sizeof(ToolBarButtonUnit));
	}

	_Rebar.init(_hInst, _hSelf);
	_ToolBar.addToRebar(&_Rebar);
	_ToolBar.enable(IDM_EX_PREV, FALSE);
	_ToolBar.enable(IDM_EX_NEXT, FALSE);
	_Rebar.setIDVisible(REBAR_BAR_TOOLBAR, true);

	/* initial combo */
	_ComboFilter.init(_hFilter);

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

	/* change language */
	NLChangeDialog(_hInst, _nppData._nppHandle, _hSelf, _T("Explorer"));
	NLChangeHeader(_hInst, _nppData._nppHandle, _hHeader, _T("FileList"));
}

BOOL ExplorerDialog::SelectItem(LPCTSTR path)
{
	BOOL				folderExists	= FALSE;

	TCHAR				TEMP[MAX_PATH];
	TCHAR				szItemName[MAX_PATH];
	TCHAR				szLongPath[MAX_PATH];
	TCHAR				szCurrPath[MAX_PATH];
	TCHAR				szRemotePath[MAX_PATH];
	SIZE_T				iPathLen		= 0;
	SIZE_T				iTempLen		= 0;
	BOOL				isRoot			= TRUE;
	HTREEITEM			hItem			= TreeView_GetRoot(_hTreeCtrl);
	HTREEITEM			hItemSel		= NULL;
	HTREEITEM			hItemUpdate		= NULL;

	iPathLen = _tcslen(path);

	/* convert possible net path name and get the full path name for compare */
	if (ConvertNetPathName(path, szRemotePath, MAX_PATH) == TRUE) {
		::GetLongPathName(szRemotePath, szLongPath, MAX_PATH);
	} else {
		::GetLongPathName(path, szLongPath, MAX_PATH);
	}

	/* test if folder exists */
	folderExists = ::PathFileExists(szLongPath);

	if (folderExists == TRUE)
	{
		/* empty szCurrPath */
		szCurrPath[0] = '\0';

		/* disabled detection of TVN_SELCHANGED notification */
		_isSelNotifyEnable = FALSE;

		do
		{
			GetItemText(hItem, szItemName, MAX_PATH);

			/* truncate item name if we are in root */
			if (isRoot == TRUE)
				szItemName[2] = '\0';

			/* compare path names */
			_stprintf(TEMP, _T("%s%s\\"), szCurrPath, szItemName);
			iTempLen = _tcslen(TEMP);

			if (_tcsnicmp(szLongPath, TEMP, iTempLen) == 0) 
			{
				/* set current selected path */
				_tcscpy(szCurrPath, TEMP);

				/* found -> store item for correct selection */
				hItemSel = hItem;

				/* expand, if possible and get child item */
				if (TreeView_GetChild(_hTreeCtrl, hItem) == NULL)
				{
					/* if no child item available, draw them */
					TreeView_SelectItem(_hTreeCtrl, hItem);
					DrawChildren(hItem);
				}
				hItem = TreeView_GetChild(_hTreeCtrl, hItem);

				/* only on first case it is a root */
				isRoot = FALSE;

				/* leave loop if last element is reached */
				if (iTempLen == iPathLen)
					break;
			} else {
				/* search for next item in list */
				hItem = TreeView_GetNextItem(_hTreeCtrl, hItem, TVGN_NEXT);
			}

			/* try again, maybe there is only an update needed */
			if ((hItem == NULL) && (hItemUpdate != hItemSel))
			{
				TreeView_Expand(_hTreeCtrl, hItemSel, TVE_EXPAND);
				hItemUpdate = hItemSel;
				GetFolderPathName(hItemSel, TEMP);
				UpdateChildren(TEMP , hItemSel, FALSE);
				hItem = TreeView_GetChild(_hTreeCtrl, hItemSel);
			}
		} while (hItem != NULL);

		/* view path */
		if (szCurrPath[0] != '\0')
		{
			/* select last selected item */
			TreeView_SelectItem(_hTreeCtrl, hItemSel);
			TreeView_EnsureVisible(_hTreeCtrl, hItemSel);

			_FileList.viewPath(szCurrPath, TRUE);
			UPDATE_CAPTION();
		}

		/* enable detection of TVN_SELCHANGED notification */
		_isSelNotifyEnable = TRUE;
	}

	return folderExists;
}

void ExplorerDialog::gotoPath(void)
{
	/* newDlg is exactly what I need */
	NewDlg		dlg;
	TCHAR		szFolderName[MAX_PATH];
	TCHAR		szComment[MAX_PATH];
	BOOL		bLeave			= FALSE;

	szFolderName[0] = '\0';

	/* rename comment */
	if (NLGetText(_hInst, _nppData._nppHandle, _T("Go to Path"), szComment, MAX_PATH) == 0) {
		_tcscpy(szComment, _T("Go to Path"));
	}

	/* copy current path to show current position */
	_tcscpy(szFolderName, _pExProp->szCurrentPath);

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
			}
			else
			{
				INT msgRet = NLMessageBox(_hInst, _hParent, _T("MsgBox FolderDoesNotExist"), MB_RETRYCANCEL);
				if (msgRet == FALSE)
					msgRet = ::MessageBox(_hParent, _T("Path doesn't exist."), _T("Error"), MB_RETRYCANCEL);

				if (msgRet == IDCANCEL)
					bLeave = TRUE;
			}
		}
		else
			bLeave = TRUE;
	}
}

void ExplorerDialog::clearFilter(void)
{
	_pExProp->vStrFilterHistory.clear();
	_pExProp->strLastFilter = _T("*.*");
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
	TCHAR		pszPath[MAX_PATH];

	/* get buffer size */
	GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszPath);
	PathRemoveBackslash(pszPath);

	/* delete folder into recycle bin */
	SHFILEOPSTRUCT	fileOp	= {0};
	fileOp.hwnd				= _hParent;
	fileOp.pFrom			= pszPath;
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
	TCHAR	pszFilesTo[MAX_PATH];
	GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszFilesTo);

	if (hEffect[0] == 2) { 
		doPaste(pszFilesTo, hFiles, DROPEFFECT_MOVE);
	} else if (hEffect[0] == 5) {
		doPaste(pszFilesTo, hFiles, DROPEFFECT_COPY);
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
void ExplorerDialog::UpdateDevices(void)
{
	BOOL			bDefaultDevice  = FALSE;
	DWORD			driveList		= ::GetLogicalDrives();
	BOOL			isValidDrive	= FALSE;
	BOOL			haveChildren	= FALSE;

	HTREEITEM		hCurrentItem	= TreeView_GetNextItem(_hTreeCtrl, TVI_ROOT, TVGN_CHILD);

	TCHAR			drivePathName[]	= _T(" :\\\0\0");	// it is longer for function 'HaveChildren()'
	TCHAR			TEMP[MAX_PATH]	= {0};
	TCHAR			volumeName[MAX_PATH];

	for (int i = 1; i < 32; i++)
	{
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
				/* correct modified drivePathName */
				drivePathName[3]			= '\0';
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

void ExplorerDialog::UpdateFolders(void)
{
	TCHAR			pszPath[MAX_PATH];
	HTREEITEM		hCurrentItem	= TreeView_GetChild(_hTreeCtrl, TVI_ROOT);

	while (hCurrentItem != NULL)
	{
		if (TreeView_GetItemState(_hTreeCtrl, hCurrentItem, TVIS_EXPANDED) & TVIS_EXPANDED)
		{
			GetItemText(hCurrentItem, pszPath, MAX_PATH);
			pszPath[2] = '\0';
			UpdateChildren(pszPath, hCurrentItem);
		}
		hCurrentItem = TreeView_GetNextItem(_hTreeCtrl, hCurrentItem, TVGN_NEXT);
	}
}

void ExplorerDialog::UpdatePath(void)
{
	TCHAR	pszPath[MAX_PATH];

	_FileList.ToggleStackRec();
	GetFolderPathName(TreeView_GetSelection(_hTreeCtrl), pszPath);
	_FileList.viewPath(pszPath);
	_FileList.ToggleStackRec();
}

void ExplorerDialog::GetFolderPathName(HTREEITEM currentItem, LPTSTR folderPathName)
{
	/* return if current folder is root folder */
	if (currentItem == TVI_ROOT)
		return;

	/* delete folder path name */
	folderPathName[0]	= '\0';

	/* create temp resources */
	TCHAR	TEMP[MAX_PATH] = {0};
	TCHAR	szName[MAX_PATH] = {0};;

	/* join elements together */
	while (currentItem != NULL)
	{
		GetItemText(currentItem, szName, MAX_PATH);
		_stprintf(TEMP, _T("%s\\%s"), szName, folderPathName);
		_tcscpy(folderPathName, TEMP);
		currentItem = TreeView_GetNextItem(_hTreeCtrl, currentItem, TVGN_PARENT);
	}

	if (_tcslen(folderPathName) > 3)
	{
		/* remove drive name */
		_tcscpy(TEMP, folderPathName);
		int i;
		for (i = 3; TEMP[i] != '\\'; i++);

		_stprintf(folderPathName, _T("%c:%s"), TEMP[0], &TEMP[i]);
	}
}


void ExplorerDialog::NotifyNewFile(void)
{
	if (isCreated())
	{
		TCHAR	TEMP[MAX_PATH];
		::SendMessage(_hParent, NPPM_GETCURRENTDIRECTORY, 0, (LPARAM)TEMP);
		_ToolBar.enable(IDM_EX_GO_TO_FOLDER, (_tcslen(TEMP) != 0));
	}
}


BOOL ExplorerDialog::ExploreVolumeInformation(LPCTSTR pszDrivePathName, LPTSTR pszVolumeName, UINT maxSize)
{
	tGetVolumeInfo	volInfo;
	DWORD			dwThreadId		= 0;
	BOOL			isValidDrive	= FALSE;
	HANDLE			hThread			= NULL;

	volInfo.pszDrivePathName	= pszDrivePathName;
	volInfo.pszVolumeName		= pszVolumeName;
	volInfo.maxSize				= maxSize;
	volInfo.pIsValidDrive		= &isValidDrive;


	_hExploreVolumeThread = ::CreateThread(NULL, 0, GetVolumeInformationTimeoutThread, &volInfo, 0, &dwThreadId);

	if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_hEvent[EID_GET_VOLINFO], _pExProp->uTimeout))
	{
		::TerminateThread(_hExploreVolumeThread, 0);
		isValidDrive = FALSE;
	}
	::CloseHandle(_hExploreVolumeThread);
	_hExploreVolumeThread = NULL;

	return isValidDrive;
}

/***************************************************************************************
 *  Drag'n'Drop, Cut and Copy of folders
 */
void ExplorerDialog::FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect)
{
	TCHAR		pszPath[MAX_PATH];
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
	GetFolderPathName(hItem, pszPath);
	PathRemoveBackslash(pszPath);
	bufsz += (_tcslen(pszPath) + 1) * sizeof(TCHAR);

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
#ifdef _UNICODE
	lpDropFileStruct->fWide = TRUE;
#else
	lpDropFileStruct->fWide = FALSE;
#endif

	/* add path to payload */
	_tcscpy((LPTSTR)&lpDropFileStruct[1], pszPath);
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

bool ExplorerDialog::OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect)
{
	LPDROPFILES hFiles	= (LPDROPFILES)::GlobalLock(medium.hGlobal);
	if (NULL == hFiles)
		return false;

	/* get target */
	TCHAR	pszFilesTo[MAX_PATH];
	GetFolderPathName(TreeView_GetDropHilight(_hTreeCtrl), pszFilesTo);

	doPaste(pszFilesTo, hFiles, *pdwEffect);
	::CloseClipboard();

	TreeView_SelectDropTarget(_hTreeCtrl, NULL);

	return true;
}

bool ExplorerDialog::doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect)
{
	/* get files from and to, to fill struct */
	UINT		headerSize				= sizeof(DROPFILES);
	SIZE_T		payloadSize				= ::GlobalSize(hData) - headerSize;
	LPVOID		pPld					= (LPBYTE)hData + headerSize;
	LPTSTR		lpszFilesFrom			= NULL;

#ifdef _UNICODE
	if (((LPDROPFILES)hData)->fWide == TRUE) {
		lpszFilesFrom = (LPWSTR)pPld;
	} else {
		lpszFilesFrom = new TCHAR[payloadSize];
		::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pPld, (int)payloadSize, lpszFilesFrom, (int)payloadSize);
	}
#else
	if (((LPDROPFILES)hData)->fWide == TRUE) {
		lpszFilesFrom = new CHAR[payloadSize/2];
		::WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)pPld, payloadSize, lpszFilesFrom, payloadSize/2, NULL, NULL);
	} else {
		lpszFilesFrom = (LPSTR)pPld;
	}
#endif

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

		TCHAR	text[MAX_PATH + 32];
		if (dwEffect == DROPEFFECT_MOVE) {
			_stprintf(text, _T("Move %d file(s)/folder(s) to:\n\n%s"), count, pszTo);
		} else if (dwEffect == DROPEFFECT_COPY) {
			_stprintf(text, _T("Copy %d file(s)/folder(s) to:\n\n%s"), count, pszTo);
		}

		if (::MessageBox(_hSelf, text, _T("Explorer"), MB_YESNO) == IDYES)
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

#ifdef _UNICODE
		if (((LPDROPFILES)hData)->fWide == FALSE) {
#else
		if (((LPDROPFILES)hData)->fWide == TRUE) {
#endif
			delete [] lpszFilesFrom;
		}
	}
	return true;
}

void ExplorerDialog::UpdateColors()
{
	COLORREF bgColor = (COLORREF)::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR, 0, 0);
	COLORREF fgColor = (COLORREF)::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR, 0, 0);

	TreeView_SetBkColor(_hTreeCtrl, bgColor);
	TreeView_SetTextColor(_hTreeCtrl, fgColor);

	ListView_SetBkColor(_hListCtrl, bgColor);
	ListView_SetTextColor(_hListCtrl, fgColor);
	ListView_SetTextBkColor(_hListCtrl, CLR_NONE);

	// also look at listview's custom draw code (NM_CUSTOMDRAW) in FileList.cpp
	// the code there was modified to use ListView_GetBkColor and ListView_GetTextColor
	// so that we get a consistent look.

	::InvalidateRect(_hTreeCtrl, NULL, TRUE);
	::InvalidateRect(_hListCtrl, NULL, TRUE);
}
