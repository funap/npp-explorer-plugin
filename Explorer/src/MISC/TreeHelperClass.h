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


#ifndef TREEHELPERCLASS_H
#define TREEHELPERCLASS_H

#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>

enum {
	ICON_UPDATE_EVT_START,
	ICON_UPDATE_EVT_RESP,
	ICON_UPDATE_EVT_END,
	ICON_UPDATE_EVT_MAX
};

struct TreeIconUpdate {
	std::wstring		strLastPath;
	HTREEITEM			hLastItem;
};



#ifndef TreeView_GetItemState
#define TVM_GETITEMSTATE (TV_FIRST+39)
#define TreeView_GetItemState(hwndTV, hti, mask) \
(UINT)::SendMessage((hwndTV), TVM_GETITEMSTATE, (WPARAM)(hti), (LPARAM)(mask))
#endif


class TreeHelper
{
public:
	TreeHelper() : _hTreeCtrl(NULL), _hSemaphore(NULL) {};
	~TreeHelper() {
		if (_hSemaphore)
		{
			::SetEvent(_hEvent[ICON_UPDATE_EVT_END]);
			::WaitForSingleObject(_hEvent[ICON_UPDATE_EVT_RESP], INFINITE);

			for (UINT i = 0; i < ICON_UPDATE_EVT_MAX; i++) {
				::CloseHandle(_hEvent[i]);
				_hEvent[i] = nullptr;
			}
			::CloseHandle(_hSemaphore);
			_hSemaphore = nullptr;

			_vIconUpdate.clear();
		}
	};

	void UpdateOverlayIcon(void);

protected:

	void UseOverlayThreading(void);

	std::vector<std::wstring> GetItemPathFromRoot(HTREEITEM currentItem) const;
	void GetFolderPathName(HTREEITEM currentItem, LPTSTR folderPathName) const;
	std::wstring GetFolderPathName(HTREEITEM currentItem) const;
	void DrawChildren(HTREEITEM parentItem);
	void UpdateChildren(LPCTSTR pszParentPath, HTREEITEM pCurrentItem, BOOL doRecursive = TRUE );
	HTREEITEM InsertChildFolder(LPCTSTR childFolderName, HTREEITEM parentItem, HTREEITEM insertAfter = TVI_LAST, BOOL bChildrenTest = TRUE);
	HTREEITEM InsertItem(const std::wstring &itemName, INT nImage, INT nSelectedIamage, INT nOverlayedImage, BOOL bHidden, HTREEITEM hParent, HTREEITEM hInsertAfter = TVI_LAST, BOOL haveChildren = FALSE, LPARAM lParam = NULL);
	BOOL UpdateItem(HTREEITEM hItem, const std::wstring &itemName, INT nImage, INT nSelectedIamage, INT nOverlayedImage, BOOL bHidden, BOOL haveChildren = FALSE, LPARAM lParam = NULL, BOOL delChildren = TRUE);
	void DeleteChildren(HTREEITEM parentItem);
	BOOL GetItemText(HTREEITEM hItem, LPTSTR szBuf, INT bufSize);
	std::wstring GetItemText(HTREEITEM hItem) const;
	LPARAM GetParam(HTREEITEM hItem);
	void SetParam(HTREEITEM hItem, LPARAM lParam);
	BOOL GetItemIcons(HTREEITEM hItem, LPINT iIcon, LPINT piSelected, LPINT iOverlay);
	void SetItemIcons(HTREEITEM hItem, INT icon, INT selected, INT overlay);
	BOOL IsItemExpanded(HTREEITEM hItem);
	INT GetChildrenCount(HTREEITEM item);

private:

	struct ItemList {
		std::wstring	strName;
		DWORD			dwAttributes;
	};
	void QuickSortItems(std::vector<ItemList>* vList, INT d, INT h);
	BOOL FindFolderAfter(LPCTSTR itemName, HTREEITEM pAfterItem);

private:	/* for thread */

	void SetOverlayIcon(HTREEITEM hItem, INT iOverlayIcon);
	void TREE_LOCK(void) {
		while (_hSemaphore) {
			if (::WaitForSingleObject(_hSemaphore, INFINITE) == WAIT_OBJECT_0)
				return;
		}
	};
	void TREE_UNLOCK(void) {
		if (_hSemaphore) {
			::ReleaseSemaphore(_hSemaphore, 1, NULL);
		}
	};

protected:
	HWND				_hTreeCtrl;

private:
	/* member var for overlay update thread */
	std::vector<TreeIconUpdate>		_vIconUpdate;
	HANDLE						_hSemaphore;
	HANDLE						_hEvent[ICON_UPDATE_EVT_MAX];
	HANDLE						_hOverThread;
};

#endif // TREEHELPERCLASS_H