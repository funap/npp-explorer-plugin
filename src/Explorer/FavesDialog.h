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
#include <windows.h>
#include <commctrl.h>

#include "Explorer.h"
#include "FavesModel.h"
#include "TreeView.h"
#include "ToolBar.h"
#include "../NppPlugin/DockingFeature/DockingDlgInterface.h"

class IPluginContext;

enum class MenuID : UINT {
    NewLink = 1,
    NewGroup,
    AddSession,
    SaveSession,
    Copy,
    Cut,
    Paste,
    Delete,
    Properties,
    Open,
    OpenOtherView,
    OpenNewInstance,
    GotoFileLocation,
    AddToSession,
};


class FavesDialog : public DockingDlgInterface
{
public:
    FavesDialog();
    ~FavesDialog() = default;

    void init(HINSTANCE hInst, HWND hParent, Settings *prop, IPluginContext* pluginContext);

    virtual void redraw() {
        ::RedrawWindow(_ToolBar.getHSelf(), nullptr, nullptr, TRUE);
        ExpandElementsRecursive(TVI_ROOT);
    };

    void UpdateTheme();

    void destroy() override
    {
        /* save settings and destroy the resources */
        SaveSettings();
    };

    void doDialog(bool willBeShown = true);

    void AddToFavorites(bool isFolder, const std::wstring& link);
    void AddToFavorites(bool isFolder, std::vector<std::wstring>&& paths);
    void SaveSession();
    void NotifyNewFile();

    void InitFinish() {
        ::SendMessage(_hSelf, WM_SIZE, 0, 0);
    };
    void SetFont(HFONT font);

protected:
    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    void HandleToolBarCommand(UINT message);

    void InitialDialog();

    void CopyItem(HTREEITEM hItem);
    void CutItem(HTREEITEM hItem);
    void PasteItem(HTREEITEM hItem);

    void AddSaveSession(HTREEITEM hItem, bool bSave);

    void NewItem(HTREEITEM hItem);
    void EditItem(HTREEITEM hItem);
    void DeleteItem(HTREEITEM hItem);

    void RefreshTree(HTREEITEM parentItem);

    void OpenContext(HTREEITEM hItem, POINT pt);
    void OpenGroupContext(HTREEITEM hItem, POINT pt, FavesItem* pElem);
    void OpenLinkContext(HTREEITEM hItem, POINT pt, FavesItem* pElem);

    bool DoesLinkExist(const std::wstring& link, FavesType type);
    void OpenLink(FavesItem* pElem);
    void UpdateLink(HTREEITEM hParentItem);

    void DrawSessionChildren(HTREEITEM hItem);

    void ReadSettings();
    void SaveSettings();

    void ExpandElementsRecursive(HTREEITEM hItem);

    bool OpenTreeViewItem(HTREEITEM hItem);

    /* Subclassing tree */
    LRESULT RunTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndDefaultTreeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        return reinterpret_cast<FavesDialog*>(dwRefData)->RunTreeProc(hwnd, message, wParam, lParam);
    };

private:
    INT_PTR HandleCustomDraw(LPNMTVCUSTOMDRAW cd, LPNMHDR nmhdr);

    /* control process */
    WNDPROC         _hDefaultTreeProc = nullptr;

    /* different imagelists */
    HIMAGELIST      _hImageList = nullptr;
    HIMAGELIST      _hImageListSys = nullptr;

    bool            _isCut = false;
    HTREEITEM       _hTreeCutCopy = nullptr;

    ToolBar         _ToolBar;
    ReBar           _Rebar;

    bool            _addToSession = false;
    FavesItem*      _peOpenLink = nullptr;
    Settings*       _pSettings = nullptr;
    IPluginContext* _pluginContext = nullptr;

    /* database */
    FavesModel      _model;
    TreeView        _hTreeCtrl;
};
