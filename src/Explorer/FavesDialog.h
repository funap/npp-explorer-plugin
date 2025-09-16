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

#include <string>
#include <vector>
#include <shlwapi.h>

#include "Explorer.h"
#include "FavesModel.h"
#include "TreeView.h"
#include "ToolBar.h"
#include "../NppPlugin/DockingFeature/DockingDlgInterface.h"

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
    FM_ADDTOSESSION,
};


class FavesDialog : public DockingDlgInterface
{
public:
    FavesDialog();
    ~FavesDialog();

    void init(HINSTANCE hInst, HWND hParent, ExProp *prop);

    virtual void redraw() {
        ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
        ExpandElementsRecursive(TVI_ROOT);
    };

    void destroy() override
    {
        /* save settings and destroy the resources */
        SaveSettings();
    };

    void doDialog(bool willBeShown = true);

    void AddToFavorties(BOOL isFolder, LPTSTR szLink);
    void AddToFavorties(BOOL isFolder, std::vector<std::wstring>&& paths);
    void SaveSession();
    void NotifyNewFile();

    void initFinish() {
        ::SendMessage(_hSelf, WM_SIZE, 0, 0);
    };
    void SetFont(HFONT font);
protected:

    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    void tb_cmd(UINT message);

    void InitialDialog();

    void CopyItem(HTREEITEM hItem);
    void CutItem(HTREEITEM hItem);
    void PasteItem(HTREEITEM hItem);

    void AddSaveSession(HTREEITEM hItem, BOOL bSave);

    void NewItem(HTREEITEM hItem);
    void EditItem(HTREEITEM hItem);
    void DeleteItem(HTREEITEM hItem);

    void RefreshTree(HTREEITEM parentItem);

    void OpenContext(HTREEITEM hItem, POINT pt);
    BOOL DoesLinkExist(LPTSTR link, FavesType type);
    void OpenLink(FavesItemPtr pElem);
    void UpdateLink(HTREEITEM hParentItem);

    void DrawSessionChildren(HTREEITEM hItem);

    void ReadSettings();
    void SaveSettings();

    void ExpandElementsRecursive(HTREEITEM hItem);

    BOOL OpenTreeViewItem(HTREEITEM hItem);

    /* Subclassing tree */
    LRESULT runTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK wndDefaultTreeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        return reinterpret_cast<FavesDialog*>(dwRefData)->runTreeProc(hwnd, message, wParam, lParam);
    };

private:
    /* control process */
    WNDPROC         _hDefaultTreeProc;

    /* different imagelists */
    HIMAGELIST      _hImageList;
    HIMAGELIST      _hImageListSys;

    BOOL            _isCut;
    HTREEITEM       _hTreeCutCopy;

    ToolBar         _ToolBar;
    ReBar           _Rebar;

    BOOL            _addToSession;
    FavesItemPtr    _peOpenLink;
    ExProp*         _pExProp;

    /* database */
    FavesModel      _model;
    TreeView        _hTreeCtrl;
};
