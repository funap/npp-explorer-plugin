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


#ifndef FILELIST_DEFINE_H
#define FILELIST_DEFINE_H

#include "Explorer.h"
#include "ExplorerResource.h"
#include "ToolBar.h"
#include "ToolTip.h"
#include "window.h"
#include "DragDropImpl.h"
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>

#ifdef _UNICODE
#define string	wstring
#endif

typedef struct {
	string			strPath;
	vector<string>	vStrItems;
} tStaInfo;


static class FileList*	lpFileListClass	= NULL;

/* pattern for column resize by mouse */
static const WORD DotPattern[] = 
{
	0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF
};

typedef struct {
	BOOL		bParent;
	INT			iIcon;
	INT			iOverlay;
	BOOL		bHidden;
	string		strName;
	string		strExt;
	string		strSize;
	string		strDate;
	/* not visible, only for sorting */
	string		strNameExt;
	__int64		i64Size;
	__int64		i64Date;
	/* not visible, remember state */
	UINT		state;
} tFileListData;


class FileList : public Window, public CIDropTarget
{
public:
	FileList(void);
	~FileList(void);
	
	void init(HINSTANCE hInst, HWND hParent, HWND hParentList);
	void initProp(tExProp* prop);

	void viewPath(LPCTSTR currentPath, BOOL redraw = FALSE);

	BOOL notify(WPARAM wParam, LPARAM lParam);

	void filterFiles(LPCTSTR currentFilter);
	void SelectCurFile(void);
	void SelectFolder(LPCTSTR selFolder);

	virtual void destroy() {};
	virtual void redraw(void) {
		_hImlListSys = GetSmallImageList(_pExProp->bUseSystemIcons);
		SetColumns();
		Window::redraw();
	};

	void ToggleStackRec(void);					// enables/disable trace of viewed directories
	void ResetDirStack(void);					// resets the stack
	void SetToolBarInfo(ToolBar *ToolBar, UINT idRedo, UINT idUndo);	// set dependency to a toolbar element
	bool GetPrevDir(LPTSTR pszPath, vector<string> & vStrItems);			// get previous directory
	bool GetNextDir(LPTSTR pszPath, vector<string> & vStrItems);			// get next directory
	INT  GetPrevDirs(LPTSTR *pszPathes);		// get previous directorys
	INT  GetNextDirs(LPTSTR *pszPathes);		// get next directorys
	void OffsetItr(INT offsetItr, vector<string> & vStrItems);			// get offset directory
	void UpdateSelItems(void);
	void SetItems(vector<string> vStrItems);

	void UpdateOverlayIcon(void);

public:
	virtual bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect);

protected:

	/* Subclassing list control */
	LRESULT runListProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultListProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (lpFileListClass->runListProc(hwnd, Message, wParam, lParam));
	};

	/* Subclassing header control */
	LRESULT runHeaderProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultHeaderProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (lpFileListClass->runHeaderProc(hwnd, Message, wParam, lParam));
	};

	void ReadIconToList(UINT iItem, LPINT piIcon, LPINT piOverlayed, LPBOOL pbHidden);
	void ReadArrayToList(LPTSTR szItem, INT iItem ,INT iSubItem);

	void ShowToolTip(const LVHITTESTINFO & hittest);

	void DrawDivider(UINT x);
	void UpdateList(void);
	void SetColumns(void);
	void SetOrder(void);
	void SetFilter(LPCTSTR pszNewFilter);

	BOOL FindNextItemInList(SIZE_T maxFolder, SIZE_T maxData, LPUINT puPos);

	void QuickSortRecursiveCol(INT d, INT h, INT column, BOOL bAscending);
	void QuickSortRecursiveColEx(INT d, INT h, INT column, BOOL bAscending);

	void onRMouseBtn();
	void onLMouseBtnDbl();

	void onSelectItem(TCHAR charkey);
	void onSelectAll(void);
	void onDelete(bool immediate = false);
	void onCopy(void);
	void onPaste(void);
	void onCut(void);

	void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
	bool doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect);

	void PushDir(LPCTSTR str);
	void UpdateToolBarElements(void);

	void SetFocusItem(SIZE_T item) {
		/* select first entry */
		SIZE_T	dataSize	= _uMaxElements;

		/* at first unselect all */
		for (SIZE_T iItem = 0; iItem < dataSize; iItem++) {
			ListView_SetItemState(_hSelf, iItem, 0, 0xFF);
		}

		ListView_SetItemState(_hSelf, item, LVIS_SELECTED|LVIS_FOCUSED, 0xFF);
		ListView_EnsureVisible(_hSelf, item, TRUE);
		ListView_SetSelectionMark(_hSelf, item);
	};

	void GetSize(__int64 size, string & str);
	void GetDate(FILETIME ftLastWriteTime, string & str);

	void GetFilterLists(LPTSTR pszFilter, LPTSTR pszExFilter);
	BOOL DoFilter(LPCTSTR pszFileName, LPTSTR pszFilter, LPTSTR pszExFilter);
	INT  WildCmp(LPCTSTR string, LPCTSTR wild);

private:	/* for thread */

	void LIST_LOCK(void) {
		while (_hSemaphore) {
			if (::WaitForSingleObject(_hSemaphore, 50) == WAIT_OBJECT_0)
				return;
		}
	};
	void LIST_UNLOCK(void) {
		if (_hSemaphore) {
			::ReleaseSemaphore(_hSemaphore, 1, NULL);
		}
	};

private:
	HWND						_hHeader;
	HIMAGELIST					_hImlListSys;
	WNDPROC						_hDefaultListProc;
	WNDPROC						_hDefaultHeaderProc;

	tExProp*					_pExProp;

	/* file list owner drawn */
	HFONT						_hFont;
	HFONT						_hFontUnder;
	HIMAGELIST					_hImlParent;

	enum eOverThEv { FL_EVT_EXIT, FL_EVT_INT, FL_EVT_START, FL_EVT_NEXT, FL_EVT_MAX };
	HANDLE						_hEvent[FL_EVT_MAX];
	HANDLE						_hOverThread;
	HANDLE						_hSemaphore;

	/* header values */
	HBITMAP						_bmpSortUp;
	HBITMAP						_bmpSortDown;
	INT							_iMouseTrackItem;
	LONG						_lMouseTrackPos;
	INT							_iBltPos;

	/* tooltip values */
	ToolTip						_pToolTip;
	UINT						_iItem;
	UINT						_iSubItem;

	/* current file filter */
	TCHAR						_szFileFilter[MAX_PATH];

	/* stores the path here for sorting		*/
	/* Note: _vFolder will not be sorted    */
	SIZE_T						_uMaxFolders;
	SIZE_T						_uMaxElements;
	SIZE_T						_uMaxElementsOld;
	vector<tFileListData>		_vFileList;

	/* search in list by typing of characters */
	BOOL						_bSearchFile;
	TCHAR						_strSearchFile[MAX_PATH];

	BOOL						_bOldAddExtToName;
	BOOL						_bOldViewLong;

	/* stack for prev and next dir */
	BOOL						_isStackRec;
	vector<tStaInfo>			_vDirStack;
	vector<tStaInfo>::iterator	_itrPos;
    
	ToolBar*					_pToolBar;
	UINT						_idRedo;
	UINT						_idUndo;

	/* scrolling on DnD */
	BOOL						_isScrolling;
	BOOL						_isDnDStarted;
};


#endif	//	FILELIST_DEFINE_H