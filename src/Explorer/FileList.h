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

#pragma once

#include "Explorer.h"
#include "ExplorerContext.h"
#include "ExplorerResource.h"
#include "ToolBar.h"
#include "../NppPlugin/DockingFeature/Window.h"
#include "DragDropImpl.h"

#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <functional>
#include <optional>

struct StaInfo {
	std::wstring				strPath;
	std::vector<std::wstring>	vStrItems;
};


/* pattern for column resize by mouse */
static const WORD DotPattern[] =
{
	0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF
};

struct FileListData {
	BOOL			isParent{};
	INT				iIcon{};
	INT				iOverlay{};
	BOOL			isHidden{};
	BOOL			isDirectory{};
	std::wstring	strName{};
	std::wstring	strExt{};
	std::wstring	strSize{};
	std::wstring	strDate{};
	/* not visible, only for sorting */
	std::wstring	strNameExt{};
	INT64			i64Size{};
	INT64			i64Date{};
	/* not visible, remember state */
	UINT			state{};
};


class FileList : public Window, public CIDropTarget
{
public:
	FileList(void) = delete;
    explicit FileList(ExplorerContext* context);
	~FileList(void);

	void init(HINSTANCE hInst, HWND hParent, HWND hParentList);
	void initProp(ExProp* prop);

	void viewPath(const std::wstring& currendDir, BOOL redraw = FALSE);

	BOOL notify(WPARAM wParam, LPARAM lParam);

	void filterFiles(LPCTSTR currentFilter);
	void SelectCurFile(void);
	void SelectFile(const std::wstring& fileName);
	void SelectFolder(LPCTSTR selFolder);

	virtual void destroy() {};
	virtual void redraw(void) {
		_hImlListSys = GetSmallImageList(_pExProp->bUseSystemIcons);
		ListView_SetImageList(_hSelf, _hImlListSys, LVSIL_SMALL);
		SetColumns();
		Window::redraw();
	};

	void ToggleStackRec(void);					// enables/disable trace of viewed directories
	void ResetDirStack(void);					// resets the stack
	void SetToolBarInfo(ToolBar *ToolBar, UINT idRedo, UINT idUndo);	// set dependency to a toolbar element
	bool GetPrevDir(LPTSTR pszPath, std::vector<std::wstring> & vStrItems);			// get previous directory
	bool GetNextDir(LPTSTR pszPath, std::vector<std::wstring> & vStrItems);			// get next directory
	INT  GetPrevDirs(LPTSTR *pszPathes);		// get previous directorys
	INT  GetNextDirs(LPTSTR *pszPathes);		// get next directorys
	void OffsetItr(INT offsetItr, std::vector<std::wstring> & vStrItems);			// get offset directory
	void UpdateSelItems(void);
	void SetItems(std::vector<std::wstring> vStrItems);

	void UpdateOverlayIcon(void);
	void setDefaultOnCharHandler(std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)> onCharHandler);

public:
	virtual bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect);

protected:

	/* Subclassing list control */
	LRESULT runListProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultListProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
		auto* target = reinterpret_cast<FileList*>(dwRefData);
		return (target->runListProc(hwnd, Message, wParam, lParam));
	};

	/* Subclassing header control */
	LRESULT runHeaderProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultHeaderProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
		auto* target = reinterpret_cast<FileList*>(dwRefData);
		return (target->runHeaderProc(hwnd, Message, wParam, lParam));
	};

	void ReadIconToList(UINT iItem, LPINT piIcon, LPINT piOverlayed, LPBOOL pbHidden);
	void ReadArrayToList(LPTSTR szItem, INT iItem ,INT iSubItem);

	void UpdateList(void);
	void SetColumns(void);
	void SetOrder(void);

	BOOL FindNextItemInList(LPUINT puPos);


	void ShowContextMenu(std::optional<POINT> screenLocation = std::nullopt);
	void onLMouseBtnDbl();

	void onSelectItem(WCHAR charkey);
	void onSelectAll(void);
	void onDelete(bool immediate = false);
	void onCopy(void);
	void onPaste(void);
	void onCut(void);

	void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
	bool doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect);

	void PushDir(const std::wstring& str);
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

	void GetSize(INT64 size, std::wstring & str);
	void GetDate(FILETIME ftLastWriteTime, std::wstring & str);

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

	ExProp*						_pExProp;

	/* file list owner drawn */
	HIMAGELIST					_hImlParent;

	enum eOverThEv { FL_EVT_EXIT, FL_EVT_INT, FL_EVT_START, FL_EVT_NEXT, FL_EVT_MAX };
	HANDLE						_hEvent[FL_EVT_MAX];
	HANDLE						_hOverThread;
	HANDLE						_hSemaphore;

	/* stores the path here for sorting		*/
	/* Note: _vFolder will not be sorted    */
	SIZE_T						_uMaxFolders;
	SIZE_T						_uMaxElements;
	SIZE_T						_uMaxElementsOld;
	std::vector<FileListData>	_vFileList;

	/* search in list by typing of characters */
    std::wstring                _searchQuery;

	BOOL						_bOldAddExtToName;
	BOOL						_bOldViewLong;

	/* stack for prev and next dir */
	BOOL							_isStackRec;
	std::vector<StaInfo>			_vDirStack;
	std::vector<StaInfo>::iterator	_itrPos;

	ToolBar*					_pToolBar;
	UINT						_idRedo;
	UINT						_idUndo;

	/* scrolling on DnD */
	BOOL						_isScrolling;
	BOOL						_isDnDStarted;

	std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)>		_onCharHandler;
    ExplorerContext*            _context;
};
