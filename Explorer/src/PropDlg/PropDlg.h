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


#ifndef PROP_DLG_DEFINE_H
#define PROP_DLG_DEFINE_H

#include <windows.h>
#include <commctrl.h>
#include "StaticDialog.h"
#include "Explorer.h"
#include "FileDlg.h"
#include "ExplorerResource.h"
#include "TreeHelperClass.h"
#include <vector>

enum LinkDlg {
	LINK_DLG_NONE,
	LINK_DLG_FOLDER,
	LINK_DLG_FILE
};

class PropDlg : public StaticDialog, public TreeHelper
{

public:
	PropDlg(void);
    
    void init(HINSTANCE hInst, HWND hWnd) {
		Window::init(hInst, hWnd);
	};

	INT_PTR doDialog(LPTSTR pName, LPTSTR pLink, LPTSTR pDesc, LinkDlg linkDlg = LINK_DLG_NONE, BOOL fileMustExist = FALSE);

    virtual void destroy() {};

	void setTreeElements(PELEM pElem, INT iUserImagePos, BOOL bWithLink = FALSE);
	LPCTSTR getGroupName(void);

protected :
	INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

	void DrawChildrenOfItem(HTREEITEM hParentItem);

public:
	void GetFolderPathName(HTREEITEM hItem, LPTSTR name);

private:
	LPTSTR			_pName;
	LPTSTR			_pLink;
	LPTSTR			_pDesc;
	LinkDlg			_linkDlg;
	BOOL			_fileMustExist;
	BOOL			_bWithLink;
	BOOL			_seeDetails;
	PELEM			_pElem;
	INT				_iUImgPos;
	std::wstring	_strGroupName;

	TCHAR			_szDetails[20];
};



#endif // PROP_DLG_DEFINE_H
