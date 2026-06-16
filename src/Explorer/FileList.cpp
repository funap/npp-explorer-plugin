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

#include "FileList.h"
#include "ExplorerTasks.h"
#include "resource.h"
#include "FileFilter.h"
#include "FileSystemService.h"
#include "ExplorerModel.h"

#include <windows.h>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <format>
#include <array>
#include <cmath>
#include <cwctype>
#include <utility>

#define LVIS_SELANDFOC (LVIS_SELECTED|LVIS_FOCUSED)

namespace {

LPCWSTR cColumns[] = {
    L"Name",
    L"Ext.",
    L"Size",
    L"Date",
};

constexpr UINT_PTR LIST_SUBCLASS_ID = 0;
constexpr UINT_PTR HEADER_SUBCLASS_ID = 0;

namespace SubItem {
    constexpr int Name = 0;
    constexpr int Extension = 1;
    constexpr int Size = 2;
    constexpr int Date = 3;
}
}

FileList::FileList(ExplorerViewModel *viewModel)
    : _hHeader(nullptr)
    , _hImlListSys(nullptr)
    , _pSettings(nullptr)
    , _hImlParent(nullptr)
    , _cancelToken(nullptr)
    , _uMaxFolders(0)
    , _uMaxElements(0)
    , _uMaxElementsOld(0)
    , _bOldAddExtToName(FALSE)
    , _bOldViewLong(FALSE)
    , _isScrolling(FALSE)
    , _isDnDStarted(FALSE)
    , _onCharHandler(nullptr)
    , _viewModel(viewModel)
    , _focusAddressBarCallback(nullptr)
    , _showContextMenuCallback(nullptr)
{
    _viewModel->AddObserver(this);
}

FileList::~FileList()
{
    _viewModel->RemoveObserver(this);
}

void FileList::init(HINSTANCE hInst, HWND hParent, HWND hParentList)
{
    /* this is the list element */
    Window::init(hInst, hParent);
    _hSelf = hParentList;

    /* keep sure to support virtual list with icons */
    LONG_PTR style = ::GetWindowLongPtr(_hSelf, GWL_STYLE);
    ::SetWindowLongPtr(_hSelf, GWL_STYLE, style | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS);

    /* enable full row select */
    ListView_SetExtendedListViewStyle(_hSelf, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ListView_SetCallbackMask(_hSelf, LVIS_OVERLAYMASK);

    /* subclass list control */
    SetWindowSubclass(_hSelf, wndDefaultListProc, LIST_SUBCLASS_ID, (DWORD_PTR)this);

    /* set image list and icon */
    _hImlParent = GetSmallImageList(FALSE);
    _hImlListSys = GetSmallImageList(_pSettings->IsUseSystemIcons());
    ListView_SetImageList(_hSelf, _hImlListSys, LVSIL_SMALL);

    /* get header control and subclass it */
    _hHeader = ListView_GetHeader(_hSelf);
    SetWindowSubclass(_hHeader, wndDefaultHeaderProc, HEADER_SUBCLASS_ID, (DWORD_PTR)this);

    /* set here the columns */
    SetColumns();

    /* initialize droping */
    ::RegisterDragDrop(_hSelf, this);

    /* create the supported formats */
    FORMATETC fmtetc {
        .cfFormat   = CF_HDROP,
        .dwAspect   = DVASPECT_CONTENT,
        .lindex     = -1,
        .tymed      = TYMED_HGLOBAL,
    };
    AddSuportedFormat(_hSelf, fmtetc);
}

void FileList::initProp(Settings* prop)
{
    /* set properties */
    _pSettings = prop;
}


/****************************************************************************
 * Draw header list
 */
LRESULT FileList::runListProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | ::DefSubclassProc(hwnd, Message, wParam, lParam);
    case WM_CHAR:
    {
        WCHAR charkey = std::towlower((WCHAR)wParam);

        /* do selection of items by user keyword typing or cut/copy/paste */
        switch (charkey) {
        case SHORTCUT_CUT:
            onCut();
            return TRUE;
        case SHORTCUT_COPY:
            onCopy();
            return TRUE;
        case SHORTCUT_PASTE:
            onPaste();
            return TRUE;
        case SHORTCUT_ALL:
            onSelectAll();
            return TRUE;
        case SHORTCUT_DELETE:
            onDelete();
            return TRUE;
        case SHORTCUT_REFRESH:
            _viewModel->Refresh();
            return TRUE;
        default:
            if (_onCharHandler) {
                BOOL handled = _onCharHandler(static_cast<UINT>(wParam), LOWORD(lParam), HIWORD(lParam));
                if (handled) {
                    return TRUE;
                }
            }
            onSelectItem(charkey);
            break;
        }
        return TRUE;
    }
    case WM_KEYDOWN:
    {
        switch (wParam) {
        case VK_DELETE:
            if ((0x8000 & ::GetKeyState(VK_CONTROL)) != 0x8000) {
                onDelete((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000);
                return TRUE;
            }
            break;
        case VK_F5:
            _viewModel->Refresh();
            return TRUE;
        case 'L':
            if ((0x8000 & ::GetKeyState(VK_CONTROL)) == 0x8000) {
                if (_focusAddressBarCallback) {
                    _focusAddressBarCallback();
                }
                return TRUE;
            }
            break;
        default:

            break;
        }
        break;
    }
    case WM_KEYUP:
        if (VK_APPS == wParam) {
            int index = ListView_GetSelectionMark(_hSelf);
            RECT rect;
            ListView_GetItemRect(_hSelf, index, &rect, LVIR_ICON);
            ClientToScreen(_hSelf, &rect);
            POINT screenLocation = {
                .x = rect.right,
                .y = rect.bottom,
            };
            ShowContextMenu(screenLocation);
            return TRUE;
        }
        break;
    case WM_SYSKEYDOWN:
        if ((0x8000 & ::GetKeyState(VK_MENU)) == 0x8000) {
            if (wParam == VK_LEFT) {
                _viewModel->NavigateBack();
                return TRUE;
            }
            if (wParam == VK_RIGHT) {
                _viewModel->NavigateForward();
                return TRUE;
            }
        }
        break;
    case WM_SYSKEYUP:
        if ((0x8000 & ::GetKeyState(VK_SHIFT)) == 0x8000) {
            if (wParam == VK_F10) {
                ShowContextMenu();
                return TRUE;
            }
        }
        break;
    case WM_MOUSEMOVE: {
        LVHITTESTINFO hittest = {};

        /* get position */
        ::GetCursorPos(&hittest.pt);
        ScreenToClient(_hSelf, &hittest.pt);
        ::SendMessage(_hSelf, LVM_SUBITEMHITTEST, 0, (LPARAM)&hittest);

        if (_isDnDStarted == TRUE) {
            for (UINT i = 0; i < _uMaxElements; i++) {
                ListView_SetItemState(_hSelf, i, 0, LVIS_DROPHILITED);
            }
            _isDnDStarted = FALSE;
        }
        break;
    }
    case WM_DESTROY:
    {
        ImageList_Destroy(_hImlParent);

        if (_cancelToken) {
            _cancelToken->store(true);
        }

        _vFileList.clear();
        ::RemoveWindowSubclass(hwnd, wndDefaultListProc, LIST_SUBCLASS_ID);
        break;
    }
    case WM_TIMER:
    {
        if (wParam == EXT_SEARCHFILE) {
            ::KillTimer(_hSelf, EXT_SEARCHFILE);
            _searchQuery.clear();
            return FALSE;
        }
        if (wParam == EXT_SCROLLLISTUP) {
            RECT rc = {0};
            Header_GetItemRect(_hHeader, 0, &rc);

            UINT    iItem   = ListView_GetTopIndex(_hSelf);
            ScDir   scrDir  = GetScrollDirection(_hSelf, rc.bottom - rc.top);

            if ((scrDir != SCR_UP) || (iItem == 0) || (!m_bAllowDrop)) {
                ::KillTimer(_hSelf, EXT_SCROLLLISTUP);
                _isScrolling = FALSE;
            }
            else {
                ListView_Scroll(_hSelf, 0, -12);
            }
            return FALSE;
        }
        if (wParam == EXT_SCROLLLISTDOWN) {
            UINT    iItem   = ListView_GetTopIndex(_hSelf) + ListView_GetCountPerPage(_hSelf) - 1;
            ScDir   scrDir  = GetScrollDirection(_hSelf);

            if ((scrDir != SCR_DOWN) || (iItem >= _uMaxElements) || (!m_bAllowDrop)) {
                ::KillTimer(_hSelf, EXT_SCROLLLISTDOWN);
                _isScrolling = FALSE;
            }
            else {
                ListView_Scroll(_hSelf, 0, 12);
            }
            return FALSE;
        }
        break;
    }
    case EXM_UPDATE_ICON_RESULT:
    {
        IconResult* result = reinterpret_cast<IconResult*>(lParam);
        if (result) {
            auto normalizePath = [](std::wstring p) {
                if (!p.empty() && p.back() == '\\') {
                    p.pop_back();
                }
                return p;
            };
            if (result->generation == _currentGeneration &&
                _wcsicmp(normalizePath(result->workDir).c_str(), normalizePath(_pSettings->GetCurrentDir()).c_str()) == 0) {
                UINT iPos = result->index;
                if (iPos < _uMaxElements && iPos < _vFileList.size() && _vFileList[iPos].Name() == result->fileName) {
                    _vFileList[iPos].SetIcon(result->icon);
                    _vFileList[iPos].SetOverlay(result->overlay);

                    RECT rcIcon = {0};
                    ListView_GetSubItemRect(_hSelf, iPos, 0, LVIR_ICON, &rcIcon);
                    ::RedrawWindow(_hSelf, &rcIcon, NULL, TRUE);
                }
            }
            delete result;
        }
        break;
    }
    case EXM_QUERYDROP:
    {
        if (_isScrolling == FALSE) {
            /* get hight of header */
            RECT rc = {0};
            Header_GetItemRect(_hHeader, 0, &rc);
            ScDir scrDir = GetScrollDirection(_hSelf, rc.bottom - rc.top);

            if (scrDir == SCR_UP) {
                ::SetTimer(_hSelf, EXT_SCROLLLISTUP, 300, NULL);
                _isScrolling = TRUE;
            }
            else if (scrDir == SCR_DOWN) {
                ::SetTimer(_hSelf, EXT_SCROLLLISTDOWN, 300, NULL);
                _isScrolling = TRUE;
            }
        }

        /* select item */
        LVHITTESTINFO hittest = {};
        ::GetCursorPos(&hittest.pt);
        ScreenToClient(_hSelf, &hittest.pt);
        ListView_SubItemHitTest(_hSelf, &hittest);

        for (UINT i = 0; i < _uMaxFolders; i++) {
            ListView_SetItemState(_hSelf, i,
                i == hittest.iItem ? LVIS_DROPHILITED : 0,
                LVIS_DROPHILITED);
        }

        _isDnDStarted = TRUE;

        return TRUE;
    }
    case EXM_DRAGLEAVE:
    {
        /* stop scrolling if still enabled while DnD */
        /* unselect DnD highlight */
        if (_isDnDStarted == TRUE) {
            for (UINT i = 0; i < _uMaxElements; i++)
                ListView_SetItemState(_hSelf, i, 0, LVIS_DROPHILITED);
            _isDnDStarted = FALSE;
        }
        if (_isScrolling == TRUE) {
            ::KillTimer(_hSelf, EXT_SCROLLLISTUP);
            ::KillTimer(_hSelf, EXT_SCROLLLISTDOWN);
            _isScrolling = FALSE;
        }
        return TRUE;
    }
    default:
        break;
    }

    return ::DefSubclassProc(hwnd, Message, wParam, lParam);
}

/****************************************************************************
 * Message handling of header
 */
LRESULT FileList::runHeaderProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_LBUTTONUP: {
        /* update here the header column width */
        _pSettings->SetColumnPosName(ListView_GetColumnWidth(_hSelf, 0));
        _pSettings->SetColumnPosExt(ListView_GetColumnWidth(_hSelf, 1));
        if (_pSettings->IsViewLong()) {
            _pSettings->SetColumnPosSize(ListView_GetColumnWidth(_hSelf, 2));
            _pSettings->SetColumnPosDate(ListView_GetColumnWidth(_hSelf, 3));
        }
        break;
    }
    case WM_DESTROY:
        ::RemoveWindowSubclass(hwnd, wndDefaultHeaderProc, HEADER_SUBCLASS_ID);
        break;
    default:
        break;
    }

    return ::DefSubclassProc(hwnd, Message, wParam, lParam);
}

/****************************************************************************
 * Parent notification
 */
BOOL FileList::notify(WPARAM wParam, LPARAM lParam)
{
    LPNMHDR  nmhdr = (LPNMHDR)lParam;

    if (nmhdr->hwndFrom == _hSelf) {
        switch (nmhdr->code) {
        case LVN_GETDISPINFO: {
            LV_ITEM &lvItem = reinterpret_cast<LV_DISPINFO*>((LV_DISPINFO FAR *)lParam)->item;

            if (lvItem.iItem >= 0 && lvItem.iItem < static_cast<INT>(_vFileList.size())) {
                if (lvItem.mask & LVIF_TEXT) {
                    /* must be a cont array */
                    static WCHAR str[MAX_PATH];

                    ReadArrayToList(str, lvItem.iItem ,lvItem.iSubItem);
                    lvItem.pszText      = str;
                    lvItem.cchTextMax   = (int)wcslen(str);
                }
                if (lvItem.mask & LVIF_IMAGE && !_vFileList[lvItem.iItem].IsParent()) {
                    INT iIcon;
                    INT iOverlay;
                    BOOL isHidden;
                    ReadIconToList(lvItem.iItem, &iIcon, &iOverlay, &isHidden);
                    lvItem.iImage = iIcon;
                    lvItem.state = INDEXTOOVERLAYMASK(iOverlay);
                }
                if (lvItem.mask & LVIF_STATE && _vFileList[lvItem.iItem].IsHidden()) {
                    lvItem.state |= LVIS_CUT;
                }
            }
            break;
        }
        case LVN_COLUMNCLICK: {
            /* store the marked items */
            for (UINT i = 0; i < _uMaxElements; i++) {
                _vFileList[i].SetState(ListView_GetItemState(_hSelf, i, LVIS_FOCUSED | LVIS_SELECTED));
            }

            INT iPos  = ((LPNMLISTVIEW)lParam)->iSubItem;

            if (iPos != _pSettings->GetSortPos()) {
                _pSettings->SetSortPos(iPos);
            }
            else {
                _pSettings->SetAscending(!_pSettings->IsAscending());
            }
            SetOrder();
            UpdateList();

            /* mark old items */
            for (UINT i = 0; i < _uMaxElements; i++) {
                ListView_SetItemState(_hSelf, i, _vFileList[i].State(), LVIS_FOCUSED | LVIS_SELECTED);
            }
            break;
        }
        case LVN_KEYDOWN: {
            switch (((LPNMLVKEYDOWN)lParam)->wVKey) {
            case VK_RETURN: {
                UINT i = ListView_GetSelectionMark(_hSelf);
                if (i != -1) {
                    if (i < _uMaxFolders) {
                        _viewModel->NavigateTo(_vFileList[i].fullPath);
                    } else {
                        _viewModel->OpenFile(_vFileList[i].fullPath);
                    }
                }
                break;
            }
            case VK_BACK:
                _viewModel->NavigateTo(L"..");
                break;
            default:
                break;
            }
            break;
        }
        case NM_CLICK:
            break;
        case NM_RCLICK: {
            DWORD pos = ::GetMessagePos();
            POINT screenLocation = {
                .x = GET_X_LPARAM(pos),
                .y = GET_Y_LPARAM(pos),
            };
            ShowContextMenu(screenLocation);
            break;
        }
        case LVN_BEGINDRAG: {
            CIDropSource dropSrc;
            CIDataObject dataObj(&dropSrc);
            FolderExChange(&dropSrc, &dataObj, DROPEFFECT_COPY | DROPEFFECT_MOVE);
            break;
        }
        case NM_DBLCLK:
            onLMouseBtnDbl();
            break;
        case LVN_ITEMCHANGED:
            UpdateSelItems();
            break;
        case NM_CUSTOMDRAW: {
            LPNMLVCUSTOMDRAW lpCD = (LPNMLVCUSTOMDRAW)lParam;
            switch (lpCD->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                ::SetWindowLongPtr(_hParent, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                return TRUE;
            case CDDS_ITEMPREPAINT: {
                auto index = static_cast<INT>(lpCD->nmcd.dwItemSpec);
                if (index >= 0 && index < static_cast<INT>(_vFileList.size())) {
                    if (index >= static_cast<INT>(_uMaxFolders)) {
                        std::wstring strFilePath = _vFileList[index].fullPath;
                        if (IsFileOpen(strFilePath) == TRUE) {
                            ::SelectObject(lpCD->nmcd.hdc, _pSettings->GetUnderlineFont());
                            ::SetWindowLongPtr(_hParent, DWLP_MSGRESULT, CDRF_NEWFONT);
                        }
                    }
                    if (_vFileList[index].IsParent()) {
                        ::SetWindowLongPtr(_hParent, DWLP_MSGRESULT, CDRF_NOTIFYPOSTPAINT);
                    }
                }
                return TRUE;
            }
            case CDDS_ITEMPOSTPAINT: {
                auto index = static_cast<INT>(lpCD->nmcd.dwItemSpec);
                RECT rc {};
                ListView_GetSubItemRect(_hSelf, index, lpCD->iSubItem, LVIR_ICON, &rc);
                UINT state = ListView_GetItemState(_hSelf, index, LVIS_SELECTED);
                bool isSelected = ((state & LVIS_SELECTED) ? (::GetFocus() == _hSelf) : ((state & LVIS_DROPHILITED) == LVIS_DROPHILITED));
                ImageList_Draw(_hImlParent, ICON_PARENT, lpCD->nmcd.hdc, rc.left, rc.top, ILD_NORMAL | (isSelected ? ILD_SELECTED : 0));
                return TRUE;
            }
            default:
                return FALSE;
            }
            break;
        }
        default:
            break;
        }
    }

    return FALSE;
}

void FileList::ReadIconToList(UINT iItem, LPINT piIcon, LPINT piOverlay, LPBOOL pbHidden)
{
    DevType type            = (iItem < _uMaxFolders ? DEVT_DIRECTORY : DEVT_FILE);

    if (_vFileList[iItem].Icon() == -1) {
        _vFileList[iItem].SetIcon(type == DEVT_DIRECTORY ? ICON_FOLDER : ICON_FILE);
        _vFileList[iItem].SetOverlay(0);
        if (_pSettings->IsUseSystemIcons()) {
            INT iIcon = -1;
            std::wstring pathStr = _vFileList[iItem].fullPath;
            GetIcons(pathStr, _vFileList[iItem].Attributes(), &iIcon);
            if (iIcon != -1) {
                _vFileList[iItem].SetIcon(iIcon);
            }
        }
    }
    *piIcon     = _vFileList[iItem].Icon();
    *piOverlay  = _vFileList[iItem].Overlay();
    *pbHidden   = _vFileList[iItem].IsHidden();
}

void FileList::ReadArrayToList(LPTSTR szItem, INT iItem ,INT iSubItem)
{
    /* copy into temp */
    switch (iSubItem) {
    case SubItem::Name: {
        std::wstring name = _vFileList[iItem].Name();
        if (!_vFileList[iItem].IsDirectory()) {
            size_t extBegPos = name.find_last_of(L'.');
            if (extBegPos != std::wstring::npos && extBegPos > 0 && !_pSettings->IsAddExtToName()) {
                name = name.substr(0, extBegPos);
            }
        }

        if ((iItem < (INT)_uMaxFolders) && (_pSettings->IsViewBraces()) && !_vFileList[iItem].IsParent()) {
            swprintf(szItem, L"[%ls]", name.c_str());
        }
        else {
            swprintf(szItem, L"%ls", name.c_str());
        }
        break;
    }
    case SubItem::Extension: {
        if ((iItem < (INT)_uMaxFolders) || (_pSettings->IsAddExtToName())) {
            szItem[0] = '\0';
        }
        else {
            std::wstring name = _vFileList[iItem].Name();
            size_t extBegPos = name.find_last_of(L'.');
            if (extBegPos != std::wstring::npos && extBegPos > 0) {
                wcscpy(szItem, name.substr(extBegPos + 1).c_str());
            }
            else {
                szItem[0] = '\0';
            }
        }
        break;
    }
    case SubItem::Size: {
        if (_vFileList[iItem].IsDirectory()) {
            wcscpy(szItem, L"<DIR>");
        }
        else {
            std::wstring strSize;
            GetSize(_vFileList[iItem].FileSize(), strSize);
            wcscpy(szItem, strSize.c_str());
        }
        break;
    }
    case SubItem::Date:
    default: {
        std::wstring strDate;
        GetDate(_vFileList[iItem].LastWriteTime(), strDate);
        wcscpy(szItem, strDate.c_str());
        break;
    }
    }
}

void FileList::OnCurrentDirectoryChanged(const std::wstring& path)
{
    if (_cancelToken) {
        _cancelToken->store(true);
    }
    _cancelToken = std::make_shared<std::atomic<bool>>(false);
    _currentGeneration++;

    /* clear data */
    _uMaxElementsOld = _uMaxElements;
    _pendingLoadDir = path;
    _pendingRedraw = TRUE;
}

void FileList::OnDirectoryEntriesLoaded(const std::wstring& currentDir, const std::vector<FileSystemEntry>& entries)
{
    auto normalizePath = [](std::wstring p) {
        if (!p.empty() && p.back() == '\\') {
            p.pop_back();
        }
        return p;
    };
    if (_wcsicmp(normalizePath(currentDir).c_str(), normalizePath(_pendingLoadDir).c_str()) != 0) {
        return;
    }

    std::vector<FileSystemEntry> vFoldersTemp;
    std::vector<FileSystemEntry> vFilesTemp;

    for (const auto& entry : entries) {
        if (entry.IsDirectory()) {
            if (_pSettings->IsHideFoldersInFileList()) {
                continue; // Skip directories if hideFoldersInContentView is true
            }

            if (entry.IsParent()) {
                if (PathIsRoot(currentDir.c_str())) {
                    continue;
                }
                vFoldersTemp.push_back(entry);
            }
            else {
                /* regular folder */
                vFoldersTemp.push_back(entry);
            }
        }
        else if (_pSettings->GetFileFilter().match(entry.Name().c_str())) {
            /* file */
            vFilesTemp.push_back(entry);
        }
    }

    /* delete old global list */
    _vFileList.clear();

    /* set temporal list as global */
    for (const auto& folder : vFoldersTemp) {
        _vFileList.push_back(FileListItem{ folder, currentDir });
    }
    for (const auto &file : vFilesTemp) {
        _vFileList.push_back(FileListItem{ file, currentDir });
    }

    /* set max elements in list */
    _uMaxFolders    = vFoldersTemp.size();
    _uMaxElements   = _uMaxFolders + vFilesTemp.size();
    vFoldersTemp.clear();
    vFilesTemp.clear();

    /* update list content */
    UpdateList();

    /* select first entry or the pending file */
    if (_pendingRedraw == TRUE) {
        bool selected = false;
        if (!_pendingSelectFile.empty()) {
            for (SIZE_T i = _uMaxFolders; i < _uMaxElements; i++) {
                if (_pendingSelectFile == _vFileList[i].Name()) {
                    SetFocusItem(i);
                    selected = true;
                    break;
                }
            }
            _pendingSelectFile.clear();
        }
        if (!selected) {
            SetFocusItem(0);
        }
        _pendingRedraw = FALSE;
    }

    // Restore previous selection
    auto prevSel = _viewModel->GetCurrentSelection();
    if (!prevSel.empty()) {
        for (UINT iItem = 0; iItem < _uMaxElements; iItem++) {
            ListView_SetItemState(_hSelf, iItem, 0, 0xFF);
        }
        for (const auto& name : prevSel) {
            for (SIZE_T i = 0; i < _uMaxElements; i++) {
                if (_vFileList[i].Name() == name) {
                    ListView_SetItemState(_hSelf, i, LVIS_SELECTED | LVIS_FOCUSED, 0xFF);
                }
            }
        }
    }

    std::vector<IconWorkItem> workItems;
    for (UINT i = 0; i < _uMaxElements; ++i) {
        if (_vFileList[i].IsParent()) {
            continue;
        }
        IconWorkItem item;
        item.index = i;
        item.name = _vFileList[i].Name();
        item.type = (i < _uMaxFolders ? DEVT_DIRECTORY : DEVT_FILE);
        workItems.push_back(item);
    }

    _viewModel->EnqueueAsyncTask(std::make_unique<TaskFetchIcons>(this, _hSelf, currentDir, std::move(workItems), _cancelToken, _currentGeneration));
}

void FileList::filterFiles(LPCTSTR currentFilter)
{
    _viewModel->SetFilter(currentFilter);
}

void FileList::SelectFolder(LPCTSTR filePath)
{
    for (UINT uFolder = 0; uFolder < _uMaxFolders; uFolder++) {
        if (_wcsicmp(_vFileList[uFolder].Name().c_str(), filePath) == 0) {
            SetFocusItem(uFolder);
            return;
        }
    }
}

void FileList::SelectCurFile()
{
    extern WCHAR g_currentFile[MAX_PATH];

    std::wstring fileName = std::wstring(g_currentFile);
    fileName = fileName.substr(fileName.find_last_of(L'\\') + 1);
    SelectFile(fileName);
}

void FileList::SelectFile(const std::wstring &fileName)
{
    if (_pendingRedraw == TRUE) {
        _pendingSelectFile = fileName;
        return;
    }

    for (SIZE_T i = _uMaxFolders; i < _uMaxElements; i++) {
        if (fileName == _vFileList[i].Name()) {
            SetFocusItem(i);
            return;
        }
    }
}

void FileList::UpdateList()
{
    std::sort(_vFileList.begin(), _vFileList.end(), [&](const FileListItem& lhs, const FileListItem& rhs) {
        if (lhs.IsParent() != rhs.IsParent()) {
            return lhs.IsParent() > rhs.IsParent();
        }
        if (lhs.IsDirectory() != rhs.IsDirectory()) {
            return lhs.IsDirectory() > rhs.IsDirectory();
        }

        const int resultNameExt = ::StrCmpLogicalW(lhs.Name().c_str(), rhs.Name().c_str());
        INT64 result = 0;

        if (lhs.IsDirectory() && rhs.IsDirectory()) {
            return resultNameExt < 0;
        }

        switch (_pSettings->GetSortPos()) {
        case SubItem::Name:
            result = resultNameExt;
            break;
        case SubItem::Extension: {
            std::wstring lhsExt, rhsExt;
            size_t lhsExtPos = lhs.Name().find_last_of(L'.');
            if (lhsExtPos != std::wstring::npos && lhsExtPos > 0) lhsExt = lhs.Name().substr(lhsExtPos + 1);
            size_t rhsExtPos = rhs.Name().find_last_of(L'.');
            if (rhsExtPos != std::wstring::npos && rhsExtPos > 0) rhsExt = rhs.Name().substr(rhsExtPos + 1);
            result = lhsExt.compare(rhsExt);
            break;
        }
        case SubItem::Size:
            result = lhs.FileSize() - rhs.FileSize();
            break;
        case SubItem::Date: {
            time_t lhsDate = lhs.LastWriteTime();
            time_t rhsDate = rhs.LastWriteTime();
            result = static_cast<INT64>(lhsDate - rhsDate);
            break;
        }
        default:
            break;
        }

        if (result == 0) {
            result = resultNameExt;
        }

        if (!_pSettings->IsAscending()) {
            result *= -1;
        }

        return result < 0;
    });

    /* avoid flickering */
    if (_uMaxElementsOld != _uMaxElements) {
        ListView_SetItemCountEx(_hSelf, _uMaxElements, LVSICF_NOSCROLL);
    }
    else {
        ::RedrawWindow(_hSelf, NULL, NULL, TRUE);
    }
}

void FileList::SetColumns()
{
    LVCOLUMN ColSetup = {0};

    ListView_DeleteColumn(_hSelf, 3);
    ListView_DeleteColumn(_hSelf, 2);
    ListView_DeleteColumn(_hSelf, 1);
    ListView_DeleteColumn(_hSelf, 0);

    if ((_bOldAddExtToName != _pSettings->IsAddExtToName()) && (_pSettings->GetSortPos() > 0)) {
        if (!_pSettings->IsAddExtToName()) {
            _pSettings->SetSortPos(_pSettings->GetSortPos() + 1);
        }
        else {
            _pSettings->SetSortPos(_pSettings->GetSortPos() - 1);
        }

        _bOldAddExtToName = _pSettings->IsAddExtToName();
    }
    if (_bOldViewLong != _pSettings->IsViewLong()) {
        if (!_pSettings->IsViewLong() &&
            (((_pSettings->GetSortPos() > 0) && (_pSettings->IsAddExtToName())) ||
                ((_pSettings->GetSortPos() > 1) && (!_pSettings->IsAddExtToName())))) {
            _pSettings->SetSortPos(0);
        }
        _bOldViewLong = _pSettings->IsViewLong();
    }

    ColSetup.mask       = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
    ColSetup.fmt        = LVCFMT_LEFT;
    ColSetup.pszText    = const_cast<LPTSTR>(cColumns[0]);
    ColSetup.cchTextMax = (int)wcslen(cColumns[0]);
    ColSetup.cx         = _pSettings->GetColumnPosName();
    ListView_InsertColumn(_hSelf, 0, &ColSetup);
    ColSetup.pszText    = const_cast<LPTSTR>(cColumns[1]);
    ColSetup.cchTextMax = (int)wcslen(cColumns[1]);
    ColSetup.cx         = _pSettings->GetColumnPosExt();
    ListView_InsertColumn(_hSelf, 1, &ColSetup);

    if (_pSettings->IsViewLong())
    {
        ColSetup.fmt        = LVCFMT_RIGHT;
        ColSetup.pszText    = const_cast<LPTSTR>(cColumns[2]);
        ColSetup.cchTextMax = (int)wcslen(cColumns[2]);
        ColSetup.cx         = _pSettings->GetColumnPosSize();
        ListView_InsertColumn(_hSelf, 2, &ColSetup);
        ColSetup.fmt        = LVCFMT_LEFT;
        ColSetup.pszText    = const_cast<LPTSTR>(cColumns[3]);
        ColSetup.cchTextMax = (int)wcslen(cColumns[3]);
        ColSetup.cx         = _pSettings->GetColumnPosDate();
        ListView_InsertColumn(_hSelf, 3, &ColSetup);
    }
    SetOrder();
}

void FileList::SetOrder()
{
    HDITEM  hdItem      = {0};
    UINT    uMaxHeader  = Header_GetItemCount(_hHeader);

    for (UINT i = 0; i < uMaxHeader; i++) {
        hdItem.mask = HDI_FORMAT;
        Header_GetItem(_hHeader, i, &hdItem);

        hdItem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == _pSettings->GetSortPos()) {
            hdItem.fmt |= _pSettings->IsAscending() ? HDF_SORTUP : HDF_SORTDOWN;
        }
        Header_SetItem(_hHeader, i, &hdItem);
    }
}

/*********************************************************************************
 * User interactions
 */
void FileList::ShowContextMenu(std::optional<POINT> screenLocation)
{
    if (!screenLocation.has_value()) {
        int index = ListView_GetSelectionMark(_hSelf);
        RECT rect;
        ListView_GetItemRect(_hSelf, index, &rect, LVIR_ICON);
        ClientToScreen(_hSelf, &rect);
        screenLocation = {
            .x = rect.right,
            .y = rect.bottom,
        };
    }

    bool isParent = false;
    std::vector<std::shared_ptr<ExplorerEntry>> entries;

    /* create data */
    for (UINT uList = 0; uList < _uMaxElements && uList < _vFileList.size(); uList++) {
        if (ListView_GetItemState(_hSelf, uList, LVIS_SELECTED) == LVIS_SELECTED) {
            if (uList == 0) {
                if (_vFileList[0].IsParent()) {
                    ListView_SetItemState(_hSelf, uList, 0, 0xFF);
                    isParent = true;
                    continue;
                }
            }

            entries.push_back(std::make_shared<ExplorerEntry>(_vFileList[uList].fullPath, _vFileList[uList].fsEntry));
        }
    }

    if (entries.empty()) {
        FileSystemEntry currentDirFsEntry(_pSettings->GetCurrentDir(), FILE_ATTRIBUTE_DIRECTORY, 0, 0, false);
        entries.push_back(std::make_shared<ExplorerEntry>(_pSettings->GetCurrentDir(), currentDirFsEntry));
    }

    const auto hasStandardMenu = (!isParent || (entries.size() != 1));
    if (_showContextMenuCallback) {
        _showContextMenuCallback(screenLocation.value(), entries, hasStandardMenu);
    }
}

void FileList::onLMouseBtnDbl()
{
    UINT selRow = ListView_GetSelectionMark(_hSelf);

    if (selRow != -1 && selRow < _vFileList.size()) {
        if (selRow < _uMaxFolders) {
            _viewModel->NavigateTo(_vFileList[selRow].fullPath);
        }
        else {
            _viewModel->OpenFile(_vFileList[selRow].fullPath);
        }
    }
}

void FileList::onSelectItem(WCHAR charkey)
{
    UINT selRow = ListView_GetSelectionMark(_hSelf);

    /* restart timer */
    ::KillTimer(_hSelf, EXT_SEARCHFILE);
    ::SetTimer(_hSelf, EXT_SEARCHFILE, 1000, NULL);

    /* initilize again if error previous occured */
    if (selRow < 0) {
        selRow = 0;
    }

    /* on first call start searching on next element */
    if (_searchQuery.empty()) {
        selRow++;
    }

    /* add character to string */
    _searchQuery.append(1, charkey);

    BOOL found = FindNextItemInList(&selRow);
    if (!found) {
        _searchQuery.pop_back();
        if (!_searchQuery.empty()) {
            selRow++;
            found = FindNextItemInList(&selRow);
        }
    }

    if (found) {
        /* select only one item */
        for (UINT i = 0; i < _uMaxElements; i++) {
            ListView_SetItemState(_hSelf, i, (selRow == i ? LVIS_SELANDFOC : 0), 0xFF);
        }
        ListView_SetSelectionMark(_hSelf, selRow);
        ListView_EnsureVisible(_hSelf, selRow, TRUE);
    }
}

void FileList::onSelectAll()
{
    INT firstRow = 0;
    if (_uMaxFolders != 0) {
        firstRow = (_vFileList[0].IsParent() ? 0 : -1);
    }

    for (UINT i = 0; i < _uMaxElements; i++) {
        ListView_SetItemState(_hSelf, i, (i == firstRow) ? 0 : LVIS_SELANDFOC, 0xFF);
    }
    ListView_SetSelectionMark(_hSelf, firstRow);
    ListView_EnsureVisible(_hSelf, firstRow, TRUE);
}

void FileList::onCut()
{
    CIDataObject dataObj(NULL);
    FolderExChange(NULL, &dataObj, DROPEFFECT_MOVE);
}

void FileList::onCopy()
{
    CIDataObject dataObj(NULL);
    FolderExChange(NULL, &dataObj, DROPEFFECT_COPY);
}

void FileList::onPaste()
{
    /* Insure desired format is there, and open clipboard */
    if (::IsClipboardFormatAvailable(CF_HDROP) == TRUE) {
        if (::OpenClipboard(NULL) == FALSE) {
            return;
        }
    }
    else {
        return;
    }

    /* Get handle to Dropped Filelist data, and number of files */
    LPDROPFILES hFiles = (LPDROPFILES)::GlobalLock(::GetClipboardData(CF_HDROP));
    if (hFiles == NULL) {
        ErrorMessage(::GetLastError());
        return;
    }
    LPBYTE hEffect = (LPBYTE)::GlobalLock(::GetClipboardData(::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT)));
    if (hEffect == NULL) {
        ErrorMessage(::GetLastError());
        return;
    }
    if (hEffect[0] == 2) {
        doPaste(_pSettings->GetCurrentDir().c_str(), hFiles, DROPEFFECT_MOVE);
    }
    else if (hEffect[0] == 5) {
        doPaste(_pSettings->GetCurrentDir().c_str(), hFiles, DROPEFFECT_COPY);
    }
    ::GlobalUnlock(hFiles);
    ::GlobalUnlock(hEffect);
    ::CloseClipboard();

    ::KillTimer(_hParent, EXT_UPDATEACTIVATEPATH);
    ::SetTimer(_hParent, EXT_UPDATEACTIVATEPATH, 200, nullptr);
}

void FileList::onDelete(bool immediate)
{
    std::vector<std::wstring> filesToDelete;

    for (SIZE_T i = 0; i < _uMaxElements && i < _vFileList.size(); i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            if ((i == 0) && (_vFileList[i].IsParent())) {
                continue;
            }
            filesToDelete.push_back(_vFileList[i].fullPath);
        }
    }

    if (!filesToDelete.empty()) {
        if (FileSystemService::DeleteFiles(_hParent, filesToDelete, immediate)) {
            ::KillTimer(_hParent, EXT_UPDATEACTIVATEPATH);
            ::SetTimer(_hParent, EXT_UPDATEACTIVATEPATH, 200, nullptr);
        }
    }
}

BOOL FileList::FindNextItemInList(LPUINT puPos)
{
    BOOL bRet       = FALSE;
    UINT iStartPos  = *puPos;

    /* search in list */
    for (UINT i = iStartPos; i != (iStartPos-1); i++) {
        /* if max data is reached, set iterator to zero */
        if (i == _vFileList.size()) {
            if (iStartPos <= 1) {
                break;
            }
            i = 0;
        }

        std::wstring lowerFileName = _vFileList[i].Name();
        std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), towlower);
        if (0 == lowerFileName.compare(0, _searchQuery.size(), _searchQuery)) {
            /* string found in any following case */
            bRet    = TRUE;
            *puPos  = i;
            break;
        }
    }

    return bRet;
}


void FileList::GetSize(size_t fileSize, std::wstring & str)
{
    constexpr std::array<const WCHAR*, 4> SIZE_UNITS{ L"bytes", L"KB", L"MB", L"GB"};

    auto displayFileSize = static_cast<double>(fileSize);
    size_t iSizeIndex = 0;

    switch (_pSettings->GetFmtSize()) {
    case SizeFmt::SFMT_BYTES:
        iSizeIndex = 0;
        break;
    case SizeFmt::SFMT_KBYTE:
        iSizeIndex = 1;
        if (fileSize != 0) {
            displayFileSize += 1023.0;
            displayFileSize /= 1024.0;
        }
        break;
    default:
        while ((iSizeIndex < SIZE_UNITS.size() - 1) && (displayFileSize / 1024.0) >= 1) {
            displayFileSize /= 1024.0;
            iSizeIndex++;
        }
        break;
    }

    int precision = 0;
    if (_pSettings->GetFmtSize() == SizeFmt::SFMT_DYNAMIC_EX) {
        if (iSizeIndex == 0) {
            precision = 0;
        }
        else {
            if (displayFileSize < 10) {
                precision = 2;
            }
            else if (displayFileSize < 100) {
                precision = 1;
            }
            else {
                precision = 0;
            }
        }
    }

    // If a precision is set, the value is automatically rounded.
    // Therefore, if the least significant digit to be rounded is greater than 0.5, the value should be less than 0.5.
    auto least = static_cast<int>((displayFileSize - static_cast<int>(displayFileSize)) * std::pow(10.0, precision + 1));
    if (least >= 5) {
        displayFileSize -= 5.0 * std::pow(10.0, -(precision + 1));
    }

    std::wstringstream ss;
    ss.imbue(std::locale(""));
    ss.precision(precision);
    ss << std::fixed << displayFileSize << L" " << SIZE_UNITS.at(iSizeIndex);

    str = ss.str();
}

void FileList::GetDate(time_t lastWriteTime, std::wstring & str)
{
    struct tm   tm_time;
    WCHAR       TEMP[18];

    if (localtime_s(&tm_time, &lastWriteTime) != 0) {
        str = L"";
        return;
    }

    if (_pSettings->GetFmtDate() == DateFmt::DFMT_ENG) {
        swprintf(TEMP, L"%02d/%02d/%02d %02d:%02d", (tm_time.tm_year + 1900) % 100, tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min);
    }
    else {
        swprintf(TEMP, L"%02d.%02d.%04d %02d:%02d", tm_time.tm_mday, tm_time.tm_mon + 1, tm_time.tm_year + 1900, tm_time.tm_hour, tm_time.tm_min);
    }

    str = TEMP;
}

void FileList::setDefaultOnCharHandler(std::function<BOOL(UINT, UINT, UINT)> onCharHandler)
{
    _onCharHandler = std::move(onCharHandler);
}



/**************************************************************************************
 * Stack functions
 */
void FileList::UpdateSelItems()
{
    std::vector<std::wstring> selected;
    for (UINT i = 0; i < _uMaxElements; i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            selected.push_back(_vFileList[i].Name());
        }
    }
    _viewModel->UpdateSelection(selected);
}

void FileList::SetItems(const std::vector<std::wstring>& vStrItems)
{
    std::vector<std::wstring> itemsCopy = vStrItems;
    UINT selType = LVIS_SELANDFOC;

    for (UINT i = 0; i < _uMaxElements; i++) {
        bool match = false;
        for (UINT itemPos = 0; itemPos < itemsCopy.size(); itemPos++) {
            if (_vFileList[i].Name() == itemsCopy[itemPos]) {
                ListView_SetItemState(_hSelf, i, selType, 0xFF);

                /* set first item in view */
                if (selType == LVIS_SELANDFOC) {
                    ListView_EnsureVisible(_hSelf, _uMaxElements - 1, FALSE);
                    ListView_EnsureVisible(_hSelf, i, FALSE);
                }

                /* delete last found item to be faster in compare */
                itemsCopy.erase(itemsCopy.begin() + itemPos);

                match = true;
                selType = LVIS_SELECTED;
                break;
            }
        }
        if (!match) {
            ListView_SetItemState(_hSelf, i, 0, 0xFF);
        }
        if (itemsCopy.empty()) {
            return;
        }
    }
}


/***************************************************************************************
 *  Drag'n'Drop, Cut and Copy of folders
 */
void FileList::FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect)
{
    SIZE_T bufsz = sizeof(DROPFILES) + sizeof(WCHAR);

    /* get buffer size */
    for (SIZE_T i = 0; i < _uMaxElements; i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            if ((i == 0) && (_vFileList[i].IsParent())) {
                continue;
            }
            bufsz += (_vFileList[i].fullPath.size() + 1) * sizeof(WCHAR);
        }
    }

    HDROP hDrop = (HDROP)GlobalAlloc(GHND|GMEM_SHARE, bufsz);

    if (nullptr == hDrop) {
        return;
    }

    LPDROPFILES lpDropFileStruct = (LPDROPFILES)::GlobalLock(hDrop);
    if (NULL == lpDropFileStruct) {
        GlobalFree(hDrop);
        return;
    }
    ::ZeroMemory(lpDropFileStruct, bufsz);

    lpDropFileStruct->pFiles = sizeof(DROPFILES);
    lpDropFileStruct->pt.x = 0;
    lpDropFileStruct->pt.y = 0;
    lpDropFileStruct->fNC = FALSE;
    lpDropFileStruct->fWide = TRUE;

    /* add files to payload and seperate with "\0" */
    SIZE_T offset   = 0;
    LPTSTR szPath   = (LPTSTR)&lpDropFileStruct[1];
    for (SIZE_T i = 0; i < _uMaxElements; i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            if ((i == 0) && (_vFileList[i].IsParent())) {
                continue;
            }
            wcscpy(&szPath[offset], _vFileList[i].fullPath.c_str());
            offset += _vFileList[i].fullPath.size() + 1;
        }
    }

    GlobalUnlock(hDrop);

    /* Init the supported format */
    FORMATETC fmtetc    = {0};
    fmtetc.cfFormat     = CF_HDROP;
    fmtetc.dwAspect     = DVASPECT_CONTENT;
    fmtetc.lindex       = -1;
    fmtetc.tymed        = TYMED_HGLOBAL;

    /* Init the medium used */
    STGMEDIUM medium = {0};
    medium.tymed    = TYMED_HGLOBAL;
    medium.hGlobal  = hDrop;

    /* Add it to DataObject */
    pdobj->SetData(&fmtetc, &medium, TRUE);

    if (pdsrc == nullptr) {
        hDrop = (HDROP)GlobalAlloc(GHND|GMEM_SHARE, 4);
        if (nullptr == hDrop) {
            return;
        }

        LPBYTE prefCopyData = (LPBYTE)::GlobalLock(hDrop);
        if (nullptr == prefCopyData) {
            GlobalFree(hDrop);
            return;
        }

        int eff = (dwEffect == DROPEFFECT_MOVE ? 2 : 5);
        ::ZeroMemory(prefCopyData, 4);
        CopyMemory(prefCopyData, &eff, 1);
        ::GlobalUnlock(hDrop);

        /* Init the supported format */
        fmtetc.cfFormat = ::RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
        /* Init the medium used */
        medium.hGlobal  = hDrop;

        pdobj->SetData(&fmtetc, &medium, TRUE);

        if (::OleSetClipboard(pdobj) == S_OK) {
            ::OleFlushClipboard();
        }
    }
    else {
        /* Initiate the Drag & Drop */
        DWORD dwEffectResp;
        ::DoDragDrop(pdobj, pdsrc, dwEffect, &dwEffectResp);
    }
}

bool FileList::OnDrop(FORMATETC* /* pFmtEtc*/, STGMEDIUM& medium, DWORD* pdwEffect)
{
    LPDROPFILES   hDrop = (LPDROPFILES)::GlobalLock(medium.hGlobal);
    if (nullptr == hDrop) {
        return false;
    }

    /* get target */
    WCHAR pszFilesTo[MAX_PATH];
    wcscpy(pszFilesTo, _pSettings->GetCurrentDir().c_str());

    /* get position */
    LVHITTESTINFO hittest = {};
    ::GetCursorPos(&hittest.pt);
    ScreenToClient(_hSelf, &hittest.pt);
    ::SendMessage(_hSelf, LVM_SUBITEMHITTEST, 0, (LPARAM)&hittest);

    if ((UINT)hittest.iItem < _uMaxFolders) {
        if (_vFileList[hittest.iItem].IsParent()) {
            ::PathRemoveFileSpec(pszFilesTo);
            ::PathRemoveFileSpec(pszFilesTo);
        }
        else {
            ::PathAppend(pszFilesTo, _vFileList[hittest.iItem].Name().c_str());
        }
    }

    doPaste(pszFilesTo, hDrop, *pdwEffect);

    return true;
}

bool FileList::doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD& dwEffect)
{
    /* get files from and to, to fill struct */
    UINT    headerSize = sizeof(DROPFILES);
    SIZE_T  payloadSize = ::GlobalSize(hData) - headerSize;
    LPVOID  pPld = (LPBYTE)hData + headerSize;
    std::vector<std::wstring> filesFrom;

    if (((LPDROPFILES)hData)->fWide == TRUE) {
        LPCWSTR p = (LPCWSTR)pPld;
        while (*p) {
            filesFrom.push_back(p);
            p += wcslen(p) + 1;
        }
    }
    else {
        LPCSTR p = (LPCSTR)pPld;
        while (*p) {
            int len = ::MultiByteToWideChar(CP_ACP, 0, p, -1, nullptr, 0);
            std::wstring wstr(len, L'\0');
            ::MultiByteToWideChar(CP_ACP, 0, p, -1, &wstr[0], len);
            wstr.resize(len - 1);
            filesFrom.push_back(wstr);
            p += strlen(p) + 1;
        }
    }

    if (!filesFrom.empty()) {
        const std::wstring message = (dwEffect == DROPEFFECT_MOVE)
            ? std::format(L"Move {} file(s)/folder(s) to:\n\n{}", filesFrom.size(), pszTo)
            : std::format(L"Copy {} file(s)/folder(s) to:\n\n{}", filesFrom.size(), pszTo);

        if (::MessageBox(_hSelf, message.c_str(), L"Explorer", MB_YESNO) == IDYES) {
            if (dwEffect == DROPEFFECT_MOVE) {
                FileSystemService::MoveFiles(_hParent, filesFrom, pszTo);
            }
            else {
                FileSystemService::CopyFiles(_hParent, filesFrom, pszTo);
            }

            ::KillTimer(_hParent, EXT_UPDATEACTIVATEPATH);
            ::SetTimer(_hParent, EXT_UPDATEACTIVATEPATH, 200, NULL);
        }
    }
    return true;
}
