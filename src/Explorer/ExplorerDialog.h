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
#include <filesystem>

#include "ComboOrgi.h"
#include "Explorer.h"
#include "ExplorerContext.h"
#include "FileList.h"
#include "TreeView.h"
#include "ToolBar.h"
#include "../NppPlugin/DockingFeature/DockingDlgInterface.h"

enum EventID {
    EID_INIT = 0,
    EID_UPDATE_USER,
    EID_UPDATE_DEVICE,
    EID_UPDATE_ACTIVATE,
    EID_UPDATE_ACTIVATEPATH,
    EID_UPDATE_GOTOCURRENTFILE,
    EID_EXPAND_ITEM,
    EID_THREAD_END,
    EID_MAX_THREAD,
    EID_MAX,
};

struct GetVolumeInfo {
    LPCTSTR     pszDrivePathName;
    LPTSTR      pszVolumeName;
    UINT        maxSize;
    LPBOOL      pIsValidDrive;
};

class ExplorerDialog : public DockingDlgInterface, public CIDropTarget, public ExplorerContext
{
public:
    ExplorerDialog();
    ~ExplorerDialog();

    void init(HINSTANCE hInst, HWND hParent, ExProp *prop);
    void redraw();
    void destroy() override {};
    void doDialog(bool willBeShown = true);
    BOOL gotoPath();
    void gotoUserFolder();
    void gotoCurrentFolder();
    void gotoCurrentFile();
    void gotoFileLocation(const std::wstring& filePath);
    void clearFilter();
    void setFocusOnFolder();
    void setFocusOnFile();
    void NotifyNewFile();
    void initFinish() {
        _bStartupFinish = TRUE;
        ::SendMessage(_hSelf, WM_SIZE, 0, 0);
    };
    void NotifyEvent(DWORD event);
    void SetFont(HFONT font);
    bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect) override;
    void NavigateBack() override;
    void NavigateForward() override;
    void NavigateTo(const std::wstring& path) override;
    void Open(const std::wstring& path) override;
    void Refresh() override;
    void ShowContextMenu(POINT screenLocation, const std::vector<std::wstring>& paths, bool hasStandardMenu = true) override;
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

    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    void InitialDialog();

    void UpdateRoots();
    void UpdateAllExpandedItems();
    void UpdatePath();

    BOOL SelectItem(const std::filesystem::path& path);

    void onDelete(bool immediate = false);
    void onCopy();
    void onPaste();
    void onCut();

    void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
    bool doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect);

    void tb_cmd(WPARAM message);
    void tb_not(LPNMTOOLBAR lpnmtb);

    BOOL FindFolderAfter(LPCTSTR itemName, HTREEITEM pAfterItem);
    void UpdateChildren(const std::wstring& path, HTREEITEM parentItem, BOOL doRecursive = TRUE);
    HTREEITEM InsertChildFolder(const std::wstring& childFolderName, HTREEITEM parentItem, HTREEITEM insertAfter = TVI_LAST, BOOL bChildrenTest = TRUE);
    void FetchChildren(HTREEITEM parentItem);
    std::wstring GetPath(HTREEITEM currentItem) const;
    void UpdateLayout();
private:
    /* Handles */
    BOOL        _bStartupFinish;
    HANDLE      _hExploreVolumeThread;
    HTREEITEM   _hItemExpand;

    /* control process */
    WNDPROC     _hDefaultTreeProc;
    WNDPROC     _hDefaultSplitterProc;

    /* some status values */
    BOOL        _bOldRectInitialized;
    BOOL        _isSelNotifyEnable;

    /* handles of controls */
    HWND        _hListCtrl;
    HWND        _hHeader;
    HWND        _hSplitterCtrl;
    HWND        _hFilter;

    /* classes */
    FileList    _FileList;
    ComboOrgi   _ComboFilter;
    ToolBar     _ToolBar;
    ReBar       _Rebar;

    /* splitter values */
    POINT       _ptOldPos;
    POINT       _ptOldPosHorizontal;
    BOOL        _isLeftButtonDown;
    HCURSOR     _hSplitterCursorUpDown;
    HCURSOR     _hSplitterCursorLeftRight;
    ExProp*     _pExProp;

    /* thread variable */
    HCURSOR     _hCurWait;

    /* drag and drop values */
    BOOL        _isScrolling;
    BOOL        _isDnDStarted;

    INT         _iDockedPos;
    TreeView    _hTreeCtrl;
};
