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
#include <mutex>
#include <atomic>


struct StaInfo {
    std::wstring                strPath;
    std::vector<std::wstring>   vStrItems;
};


/* pattern for column resize by mouse */
static const WORD DotPattern[] =
{
    0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF
};

#include "FileSystemService.h"

struct IconWorkItem {
    UINT index;
    std::wstring name;
    DevType type;
};

struct FileListItem {
    FileSystemEntry fsEntry;
    std::wstring fullPath;
    mutable int icon{-1};
    mutable int overlay{0};
    mutable unsigned int state{0};

    FileListItem(const FileSystemEntry& entry, const std::wstring& parentDir)
        : fsEntry(entry)
    {
        fullPath = FileSystemService::CombinePath(parentDir, entry.Name());
    }

    const std::wstring& Name() const { return fsEntry.Name(); }
    unsigned int Attributes() const { return fsEntry.Attributes(); }
    size_t FileSize() const { return fsEntry.FileSize(); }
    time_t LastWriteTime() const { return fsEntry.LastWriteTime(); }
    bool IsDirectory() const { return fsEntry.IsDirectory(); }
    bool IsHidden() const { return fsEntry.IsHidden(); }
    bool IsParent() const { return fsEntry.IsParent(); }

    int Icon() const { return icon; }
    void SetIcon(int i) const { icon = i; }
    int Overlay() const { return overlay; }
    void SetOverlay(int o) const { overlay = o; }
    unsigned int State() const { return state; }
    void SetState(unsigned int s) const { state = s; }
};

struct IconResult {
    std::wstring workDir;
    UINT index;
    int icon;
    int overlay;
    uint64_t generation;
    std::wstring fileName;
};

#include "ExplorerViewModel.h"

class FileList : public Window, public CIDropTarget, public IExplorerViewModelObserver
{
public:
    FileList() = delete;
    FileList(ExplorerViewModel* viewModel);
    ~FileList();

    void init(HINSTANCE hInst, HWND hParent, HWND hParentList);
    void initProp(Settings* prop);

    // IExplorerViewModelObserver implementation
    void OnCurrentDirectoryChanged(const std::wstring& path) override;
    void OnDirectoryEntriesLoaded(const std::wstring& path, const std::vector<FileSystemEntry>& entries) override;
    void OnNavigationStateChanged() override {};
    void OnOpenFileRequested(const std::wstring& filePath) override {}
    void OnCommandExecutionFailed(const std::wstring& command) override {}
    void OnToggleWorkspaceModeRequested() override {}

    void SetFocusAddressBarCallback(std::function<void()> callback) { _focusAddressBarCallback = callback; }
    void SetShowContextMenuCallback(std::function<void(POINT, const std::vector<std::shared_ptr<ExplorerEntry>>&, bool)> callback) { _showContextMenuCallback = callback; }

    BOOL notify(WPARAM wParam, LPARAM lParam);

    void filterFiles(LPCTSTR currentFilter);
    void SelectCurFile();
    void SelectFile(const std::wstring& fileName);
    void SelectFolder(LPCTSTR filePath);

    virtual void destroy() {};
    virtual void redraw() {
        _hImlListSys = GetSmallImageList(_pSettings->IsUseSystemIcons());
        ListView_SetImageList(_hSelf, _hImlListSys, LVSIL_SMALL);
        SetColumns();
        Window::redraw();
    };

    void UpdateSelItems();
    void SetItems(const std::vector<std::wstring>& vStrItems);

    void setDefaultOnCharHandler(std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)> onCharHandler);

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

    void UpdateList();
    void SetColumns();
    void SetOrder();

    BOOL FindNextItemInList(LPUINT puPos);

    void ShowContextMenu(std::optional<POINT> screenLocation = std::nullopt);
    void onLMouseBtnDbl();

    void onSelectItem(WCHAR charkey);
    void onSelectAll();
    void onDelete(bool immediate = false);
    void onCopy();
    void onPaste();
    void onCut();

    void FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect);
    bool doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect);

    void SetFocusItem(SIZE_T item) {
        /* select first entry */
        SIZE_T dataSize = _uMaxElements;

        /* at first unselect all */
        for (SIZE_T iItem = 0; iItem < dataSize; iItem++) {
            ListView_SetItemState(_hSelf, iItem, 0, 0xFF);
        }

        ListView_SetItemState(_hSelf, item, LVIS_SELECTED|LVIS_FOCUSED, 0xFF);
        ListView_EnsureVisible(_hSelf, item, TRUE);
        ListView_SetSelectionMark(_hSelf, item);
    };

    void GetSize(size_t size, std::wstring & str);
    void GetDate(time_t lastWriteTime, std::wstring & str);

private:    /* for thread */

    HWND                            _hHeader;
    HIMAGELIST                      _hImlListSys;

    Settings*                       _pSettings;

    /* file list owner drawn */
    HIMAGELIST                      _hImlParent;

    std::shared_ptr<std::atomic<bool>> _cancelToken;
    uint64_t                        _currentGeneration{0};

    /* stores the path here for sorting        */
    /* Note: _vFolder will not be sorted    */
    SIZE_T                          _uMaxFolders;
    SIZE_T                          _uMaxElements;
    SIZE_T                          _uMaxElementsOld;
    std::vector<FileListItem>       _vFileList;

    /* search in list by typing of characters */
    std::wstring                    _searchQuery;

    bool                            _bOldAddExtToName;
    bool                            _bOldViewLong;

    /* scrolling on DnD */
    BOOL                            _isScrolling;
    BOOL                            _isDnDStarted;

    std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)> _onCharHandler;
    ExplorerViewModel*              _viewModel;
    std::function<void()>           _focusAddressBarCallback;
    std::function<void(POINT, const std::vector<std::shared_ptr<ExplorerEntry>>&, bool)> _showContextMenuCallback;
    std::wstring                    _pendingLoadDir;
    BOOL                            _pendingRedraw;
    std::wstring                    _pendingSelectFile;
};
