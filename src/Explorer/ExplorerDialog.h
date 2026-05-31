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
#include <set>
#include <filesystem>
#include <optional>

#include "ComboOrgi.h"
#include "Explorer.h"
#include "ExplorerContext.h"
#include "FileList.h"
#include "TreeView.h"
#include "ToolBar.h"
#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "ExplorerTasks.h"
#include "../NppPlugin/DockingFeature/DockingDlgInterface.h"

// Forward declaration only: avoids circular include with TreeModelSynchronizer.h
class TreeModelSynchronizer;

struct GetVolumeInfo {
    LPCTSTR     pszDrivePathName;
    LPTSTR      pszVolumeName;
    UINT        maxSize;
    LPBOOL      pIsValidDrive;
};

#include "ExplorerViewModel.h"

class ExplorerDialog : public DockingDlgInterface, public CIDropTarget, public ExplorerContext, public IAsyncTaskCallback, public IExplorerModelObserver, public IExplorerViewModelObserver
{
public:
    ExplorerDialog();
    ~ExplorerDialog();

    void init(HINSTANCE hInst, HWND hParent, Settings *prop);
    void redraw();
    void destroy() override {};
    void doDialog(bool willBeShown = true);
    BOOL GotoPath();
    void GotoUserFolder();
    void GotoCurrentFolder();
    void GotoCurrentFile();
    void GotoFileLocation(const std::wstring& filePath);
    void ClearFilter();
    void SetFocusOnFolder();
    void SetFocusOnFile();
    void NotifyNewFile();
    void InitFinish() {
        _bStartupFinish = TRUE;
        ::SendMessage(_hSelf, WM_SIZE, 0, 0);
    };
    void SetFont(HFONT font);
    bool OnDrop(FORMATETC* pFmtEtc, STGMEDIUM& medium, DWORD *pdwEffect) override;
    void NavigateBack() override;
    void NavigateForward() override;
    void NavigateTo(const std::wstring& path) override;
    void Open(const std::wstring& path) override;
    void Refresh() override;
    void ShowContextMenu(POINT screenLocation, const std::vector<std::shared_ptr<ExplorerEntry>>& entries, bool hasStandardMenu = true) override;
    void EnqueueAsyncTask(std::unique_ptr<IAsyncTask> task) override;
    void ClearPendingTasks(std::optional<TaskCategory> category = std::nullopt) override;
    void OnFolderChildrenChecked(HTREEITEM hItem, const std::wstring& path, bool hasChildren);
    void OnEntryRenamed(const std::wstring& oldPath, const std::wstring& newPath, const std::wstring& newName);
    void RefreshActiveNode();

    // The following helpers are also used by TreeModelSynchronizer:
    BOOL FindFolderAfter(LPCTSTR itemName, HTREEITEM pAfterItem);
    HTREEITEM InsertChildFolder(std::shared_ptr<ExplorerEntry> entry, HTREEITEM parentItem, HTREEITEM insertAfter = TVI_LAST, BOOL isDirectory = TRUE, BOOL isHidden = FALSE, BOOL haveChildren = TRUE);
    std::wstring GetPath(HTREEITEM currentItem) const;
protected:
    /* Subclassing tree */
    LRESULT RunTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndDefaultTreeProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
        return (((ExplorerDialog *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->RunTreeProc(hwnd, Message, wParam, lParam));
    };

    /* Subclassing splitter */
    LRESULT RunSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndDefaultSplitterProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
        return (((ExplorerDialog *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->RunSplitterProc(hwnd, Message, wParam, lParam));
    };

    virtual INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    void InitialDialog();
    void OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task) override;
    void OnEntryUpdated(std::shared_ptr<ExplorerEntry> entry) override;

    // IExplorerViewModelObserver overrides
    void OnCurrentDirectoryChanged(const std::wstring& path) override;
    void OnDirectoryEntriesLoaded(const std::wstring& path, const std::vector<FileSystemEntry>& entries) override {};
    void OnNavigationStateChanged() override;

    void UpdateRoots();
    void UpdateAllExpandedItems();
    void UpdatePath();

    BOOL SelectItem(const std::filesystem::path& path);

    void OnDelete(bool immediate = false);
    void OnCopy();
    void OnPaste();
    void OnCut();

    void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
    bool DoPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect);

    void HandleToolBarCommand(WPARAM message);
    void HandleToolBarDropDown(LPNMTOOLBAR lpnmtb);

    HTREEITEM FindTreeItemByPath(const std::wstring& path);
    void FetchChildren(HTREEITEM parentItem);
    void UpdateLayout();
    void ResumePendingSelection();
private:
    std::shared_ptr<ExplorerModel> _model;
    std::shared_ptr<ExplorerViewModel> _viewModel;
    std::vector<std::wstring> _pendingSelectPathSegments;
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
    TreeView    _hTreeCtrl;
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
    Settings*   _pSettings;

    /* thread variable */
    HCURSOR     _hCurWait;

    /* drag and drop values */
    BOOL        _isScrolling;
    BOOL        _isDnDStarted;

    INT         _iDockedPos;

    WorkerThread _workerThread;
    std::set<HTREEITEM> _checkedItems;
    std::wstring _pendingNavigateDir;

    void CheckVisibleFolderChildren();
};
