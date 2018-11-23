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


#ifndef EXPLORERDLG_DEFINE_H
#define EXPLORERDLG_DEFINE_H

#include "DockingDlgInterface.h"
#include "TreeHelperClass.h"
#include "FileList.h"
#include "ComboOrgi.h"
#include "Toolbar.h"
#include <string>
#include <vector>
#include <algorithm>
#include <shlwapi.h>

#include "Explorer.h"
#include "ExplorerResource.h"

using namespace std;


typedef enum {
	EID_INIT = 0,
	EID_UPDATE_USER,
	EID_UPDATE_DEVICE,
	EID_UPDATE_ACTIVATE,
	EID_UPDATE_ACTIVATEPATH,
	EID_EXPAND_ITEM,
	EID_THREAD_END,
	EID_MAX_THREAD,
	EID_GET_VOLINFO,
	EID_MAX
} eEventID;

typedef struct {
	LPCTSTR		pszDrivePathName;
	LPTSTR		pszVolumeName;
	UINT		maxSize;
	LPBOOL		pIsValidDrive;
} tGetVolumeInfo;


class ExplorerDialog : public DockingDlgInterface, public TreeHelper, public CIDropTarget
{
public:
	ExplorerDialog(void);
	~ExplorerDialog(void);

    void init(HINSTANCE hInst, NppData nppData, tExProp *prop);

	virtual void redraw(void) {
		/* possible new imagelist -> update the window */
		::SendMessage(_hTreeCtrl, TVM_SETIMAGELIST, TVSIL_NORMAL, (LPARAM)GetSmallImageList(_pExProp->bUseSystemIcons));
		::SetTimer(_hSelf, EXT_UPDATEDEVICE, 0, NULL);
		_FileList.redraw();
		::RedrawWindow(_ToolBar.getHSelf(), NULL, NULL, TRUE);
		/* and only when dialog is visible, select item again */
		SelectItem(_pExProp->szCurrentPath);
	};

	void destroy(void) {};

   	void doDialog(bool willBeShown = true);

	void gotoPath(void);
	void clearFilter(void);

	void NotifyNewFile(void);

	void initFinish(void) {
		_bStartupFinish = TRUE;
		::SendMessage(_hSelf, WM_SIZE, 0, 0);
		UpdateColors();
	};

	void NotifyEvent(DWORD event);

	void UpdateColors();

public:
	virtual bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect);

protected:

	/* Subclassing tree */
	LRESULT runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (((ExplorerDialog *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->runTreeProc(hwnd, Message, wParam, lParam));
	};

	/* Subclassing splitter */
	LRESULT runSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (((ExplorerDialog *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->runSplitterProc(hwnd, Message, wParam, lParam));
	};

	virtual BOOL CALLBACK run_dlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	LPTSTR GetNameStrFromCmd(UINT idButton);

	void InitialDialog(void);

	void UpdateDevices(void);
	void UpdateFolders(void);
	void UpdatePath(void);

	void SelectItem(POINT pt);
	BOOL SelectItem(LPCTSTR path);

	void onDelete(bool immediate = false);
	void onCopy(void);
	void onPaste(void);
	void onCut(void);

	void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
	bool doPaste(LPCTSTR pszTo, LPDROPFILES hPaste, const DWORD & dwEffect);

	void tb_cmd(WPARAM message);
	void tb_not(LPNMTOOLBAR lpnmtb);

	void GetFolderPathName(HTREEITEM currentItem, LPTSTR folderPathName);
	BOOL ExploreVolumeInformation(LPCTSTR pszDrivePathName, LPTSTR pszVolumeName, UINT maxSize);

private:
	/* Handles */
	NppData					_nppData;
	tTbData					_data;
	BOOL					_bStartupFinish;
	HANDLE					_hExploreVolumeThread;
	HTREEITEM				_hItemExpand;

	/* control process */
	WNDPROC					_hDefaultTreeProc;
	WNDPROC					_hDefaultSplitterProc;
	
	/* some status values */
	BOOL					_bOldRectInitilized;
	BOOL					_isSelNotifyEnable;

	/* handles of controls */
	HWND					_hListCtrl;
	HWND					_hHeader;
	HWND					_hSplitterCtrl;
	HWND					_hFilter;
	HWND					_hFilterButton;

	/* classes */
	FileList				_FileList;
	ComboOrgi				_ComboFilter;
	ToolBar					_ToolBar;
	ReBar					_Rebar;

	/* splitter values */
	POINT					_ptOldPos;
	POINT					_ptOldPosHorizontal;
	BOOL					_isLeftButtonDown;
	HCURSOR					_hSplitterCursorUpDown;
	HCURSOR					_hSplitterCursorLeftRight;
	tExProp*				_pExProp;

	/* thread variable */
	HCURSOR					_hCurWait;

	/* drag and drop values */
	BOOL					_isScrolling;
	BOOL					_isDnDStarted;
};




#endif // EXPLORERDLG_DEFINE_H
