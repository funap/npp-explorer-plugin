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

#include <vector>

#include <windows.h>
#include <commctrl.h>

#include "../NppPlugin/DockingFeature/StaticDialog.h"

#include "Explorer.h"
#include "FileDlg.h"
#include "ExplorerResource.h"
#include "TreeHelperClass.h"
#include "FavesModel.h"

enum class LinkDlg {
    NONE = 0,
    FOLDER,
    FILE
};

class PropDlg : public StaticDialog, public TreeHelper
{

public:
    PropDlg();
    ~PropDlg();

    void init(HINSTANCE hInst, HWND hWnd) {
        Window::init(hInst, hWnd);
    };
    virtual void destroy() {};

    INT_PTR doDialog(LPTSTR pName, LPTSTR pLink, LPTSTR pDesc, LinkDlg linkDlg = LinkDlg::NONE, BOOL fileMustExist = FALSE);
    void setRoot(FavesItemPtr pElem, INT iUserImagePos, BOOL bWithLink = FALSE);
    FavesItemPtr getSelectedGroup() const;
    void setSelectedGroup(FavesItemPtr group);

protected :
    INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

    void ExpandTreeView(HTREEITEM hParentItem);

private:
    LPTSTR          _pName;
    LPTSTR          _pLink;
    LPTSTR          _pDesc;
    LinkDlg         _linkDlg;
    BOOL            _fileMustExist;
    BOOL            _bWithLink;
    BOOL            _seeDetails;
    FavesItemPtr    _root;
    INT             _iUImgPos;
    FavesItemPtr    _selectedGroup;
};
