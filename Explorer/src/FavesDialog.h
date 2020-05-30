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


#ifndef FAVESDLG_DEFINE_H
#define FAVESDLG_DEFINE_H

#include "DockingDlgInterface.h"
#include "TreeHelperClass.h"
#include "FileList.h"
#include "ComboOrgi.h"
#include "Toolbar.h"
#include "PropDlg.h"
#include <string>
#include <vector>
#include <algorithm>
#include <shlwapi.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "Explorer.h"
#include "ExplorerResource.h"


enum MenuID {
	FM_NEWLINK = 1,
	FM_NEWGROUP,
	FM_ADDSESSION,
	FM_SAVESESSION,
	FM_COPY,
	FM_CUT,
	FM_PASTE,
	FM_DELETE,
	FM_PROPERTIES,
	FM_OPEN,
	FM_OPENOTHERVIEW,
	FM_OPENNEWINSTANCE,
	FM_GOTO_FILE_LOCATION,
	FM_ADDTOSESSION
};


class FavesDialog : public DockingDlgInterface, public TreeHelper
{
public:
	FavesDialog(void);
	~FavesDialog(void);

    void init(HINSTANCE hInst, NppData nppData, LPTSTR pCurrentPath, ExProp *prop);

	virtual void redraw(void) {
		::RedrawWindow(_ToolBar.getHSelf(), NULL, NULL, TRUE);
		ExpandElementsRecursive(TVI_ROOT);
	};

	void destroy(void)
	{
		/* save settings and destroy the resources */
		SaveSettings();
	};

   	void doDialog(bool willBeShown = true);

	void AddToFavorties(BOOL isFolder, LPTSTR szLink);
	void SaveSession(void);
	void NotifyNewFile(void);

	void initFinish(void) {
		::SendMessage(_hSelf, WM_SIZE, 0, 0);
		UpdateColors();
	};
	void UpdateColors();

protected:

	virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

	LPCTSTR GetNameStrFromCmd(UINT idButton);
	void tb_cmd(UINT message);

	void InitialDialog(void);

	HTREEITEM GetTreeItem(LPCTSTR pszGroupName);
	PELEM GetElementPointer(LPCTSTR pszGroupName);
	void CopyItem(HTREEITEM hItem);
	void CutItem(HTREEITEM hItem);
	void PasteItem(HTREEITEM hItem);

	void AddSaveSession(HTREEITEM hItem, BOOL bSave);

	void NewItem(HTREEITEM hItem);
	void EditItem(HTREEITEM hItem);
	void DeleteItem(HTREEITEM hItem);

	void DuplicateRecursive(PELEM pTarget, PELEM pSource);
	void DeleteRecursive(PELEM pElem);

	void OpenContext(HTREEITEM hItem, POINT pt);
	BOOL DoesNameNotExist(HTREEITEM hItem, HTREEITEM hCurrItem, LPTSTR name);
	BOOL DoesLinkExist(LPTSTR link, int root);
	void OpenLink(PELEM pElem);
	void UpdateLink(HTREEITEM hItem);
	void UpdateNode(HTREEITEM hItem, BOOL haveChildren);

	void SortElementList(std::vector<ItemElement> & parentElement);
	void SortElementsRecursive(std::vector<ItemElement> & parentElement, int d, int h);

	void DrawSessionChildren(HTREEITEM hItem);

	void ReadSettings(void);
	void ReadElementTreeRecursive(ELEM_ITR elem_itr, LPTSTR* ptr);

	void SaveSettings(void);
	void SaveElementTreeRecursive(PELEM pElem, HANDLE hFile);

	void ExpandElementsRecursive(HTREEITEM hItem);

	LinkDlg MapPropDlg(int root) {
		switch (root) {
			case FAVES_FOLDERS:		return LINK_DLG_FOLDER;
			case FAVES_FILES:		return LINK_DLG_FILE;
			case FAVES_SESSIONS:	return LINK_DLG_FILE;
			default: return LINK_DLG_NONE;
		}
	};


public:
	void GetFolderPathName(HTREEITEM currentItem, LPTSTR folderPathName) {};

protected:

	/* Subclassing tree */
	LRESULT runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (((FavesDialog *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->runTreeProc(hwnd, Message, wParam, lParam));
	};

private:
	/* Handles */
	NppData					_nppData;
	tTbData					_data;

	/* control process */
	WNDPROC					_hDefaultTreeProc;

	/* Current active font in [Files] */
	HFONT					_hFont;
	HFONT					_hFontUnder;

	/* different imagelists */
	HIMAGELIST				_hImageList;
	HIMAGELIST				_hImageListSys;

	BOOL					_isCut;
	HTREEITEM				_hTreeCutCopy;
	
	LPTSTR					_pCurrentElement;

	ToolBar					_ToolBar;
	ReBar					_Rebar;

	BOOL					_addToSession;
	PELEM					_peOpenLink;
	ExProp*					_pExProp;

	/* database */
	std::vector<ItemElement>	_vDB;
};




#endif // FAVESDLG_DEFINE_H