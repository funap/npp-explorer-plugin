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
#include "resource.h"
#include "FileFilter.h"

#include <windows.h>
#include <algorithm>
#include <sstream>
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

DWORD WINAPI FileOverlayThread(LPVOID lpParam)
{
    FileList* pFileList = (FileList*)lpParam;
    pFileList->UpdateOverlayIcon();
    return 0;
}
}

FileList::FileList(ExplorerContext *context)
    : _hHeader(nullptr)
    , _hImlListSys(nullptr)
    , _pExProp(nullptr)
    , _hImlParent(nullptr)
    , _hEvent{}
    , _hOverThread(nullptr)
    , _hSemaphore(nullptr)
    , _uMaxFolders(0)
    , _uMaxElements(0)
    , _uMaxElementsOld(0)
    , _bOldAddExtToName(FALSE)
    , _bOldViewLong(FALSE)
    , _isStackRec(TRUE)
    , _itrPos()
    , _pToolBar(nullptr)
    , _idRedo(0)
    , _idUndo(0)
    , _isScrolling(FALSE)
    , _isDnDStarted(FALSE)
    , _onCharHandler(nullptr)
    , _context(context)
{
}

FileList::~FileList()
{
}

void FileList::init(HINSTANCE hInst, HWND hParent, HWND hParentList)
{
    /* this is the list element */
    Window::init(hInst, hParent);
    _hSelf = hParentList;

    /* create semaphore for thead */
    _hSemaphore = ::CreateSemaphore(nullptr, 1, 1, nullptr);

    /* create events for thread */
    for (INT i = 0; i < FL_EVT_MAX; i++) {
        _hEvent[i] = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    /* create thread */
    DWORD dwFlags = 0;
    _hOverThread = ::CreateThread(nullptr, 0, FileOverlayThread, this, 0, &dwFlags);

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
    _hImlListSys = GetSmallImageList(_pExProp->bUseSystemIcons);
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

void FileList::initProp(ExProp* prop)
{
    /* set properties */
    _pExProp = prop;
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
            _context->Refresh();
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
            _context->Refresh();
            return TRUE;
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
                _context->NavigateBack();
                return TRUE;
            }
            if (wParam == VK_RIGHT) {
                _context->NavigateForward();
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

        if (_hSemaphore) {
            ::SetEvent(_hEvent[FL_EVT_EXIT]);

            ::CloseHandle(_hOverThread);
            _hOverThread = nullptr;

            for (INT i = 0; i < FL_EVT_MAX; i++) {
                ::CloseHandle(_hEvent[i]);
                _hEvent[i] = nullptr;
            }

            ::CloseHandle(_hSemaphore);
            _hSemaphore = nullptr;
        }

        _vDirStack.clear();
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
    case EXM_UPDATE_OVERICON:
    {
        INT         iIcon       = 0;
        INT         iSelected   = 0;
        RECT        rcIcon      = {0};
        UINT        iPos        = (UINT)wParam;
        DevType     type        = (DevType)lParam;

        if (iPos < _uMaxElements) {
            /* test if overlay icon is need to be updated and if it's changed do a redraw */
            if (_vFileList[iPos].iOverlay == 0) {
                ExtractIcons(_pExProp->currentDir.c_str(), _vFileList[iPos].strNameExt.c_str(),
                    type, &iIcon, &iSelected, &_vFileList[iPos].iOverlay);
            }

            if (_vFileList[iPos].iOverlay != 0) {
                ListView_GetSubItemRect(_hSelf, iPos, 0, LVIR_ICON, &rcIcon);
                ::RedrawWindow(_hSelf, &rcIcon, NULL, TRUE);
            }

            ::SetEvent(_hEvent[FL_EVT_NEXT]);
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
        _pExProp->iColumnPosName = ListView_GetColumnWidth(_hSelf, 0);
        _pExProp->iColumnPosExt  = ListView_GetColumnWidth(_hSelf, 1);
        if (_pExProp->bViewLong == TRUE) {
            _pExProp->iColumnPosSize = ListView_GetColumnWidth(_hSelf, 2);
            _pExProp->iColumnPosDate = ListView_GetColumnWidth(_hSelf, 3);
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

            if (lvItem.mask & LVIF_TEXT) {
                /* must be a cont array */
                static WCHAR str[MAX_PATH];

                ReadArrayToList(str, lvItem.iItem ,lvItem.iSubItem);
                lvItem.pszText      = str;
                lvItem.cchTextMax   = (int)wcslen(str);
            }
            if (lvItem.mask & LVIF_IMAGE && !_vFileList[lvItem.iItem].isParent) {
                INT iIcon;
                INT iOverlay;
                BOOL isHidden;
                ReadIconToList(lvItem.iItem, &iIcon, &iOverlay, &isHidden);
                lvItem.iImage = iIcon;
                lvItem.state = INDEXTOOVERLAYMASK(iOverlay);
            }
            if (lvItem.mask & LVIF_STATE && _vFileList[lvItem.iItem].isHidden) {
                lvItem.state |= LVIS_CUT;
            }
            break;
        }
        case LVN_COLUMNCLICK: {
            /* store the marked items */
            for (UINT i = 0; i < _uMaxElements; i++) {
                _vFileList[i].state = ListView_GetItemState(_hSelf, i, LVIS_FOCUSED | LVIS_SELECTED);
            }

            INT iPos  = ((LPNMLISTVIEW)lParam)->iSubItem;

            if (iPos != _pExProp->iSortPos) {
                _pExProp->iSortPos = iPos;
            }
            else {
                _pExProp->bAscending ^= TRUE;
            }
            SetOrder();
            UpdateList();

            /* mark old items */
            for (UINT i = 0; i < _uMaxElements; i++) {
                ListView_SetItemState(_hSelf, i, _vFileList[i].state, LVIS_FOCUSED | LVIS_SELECTED);
            }
            break;
        }
        case LVN_KEYDOWN: {
            switch (((LPNMLVKEYDOWN)lParam)->wVKey) {
            case VK_RETURN: {
                UINT i = ListView_GetSelectionMark(_hSelf);
                if (i != -1) {
                    if (i < _uMaxFolders) {
                        _context->NavigateTo(_vFileList[i].strName);
                    } else {
                        _context->Open(_vFileList[i].strNameExt);
                    }
                }
                break;
            }
            case VK_BACK:
                _context->NavigateTo(L"..");
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
                if (index >= static_cast<INT>(_uMaxFolders)) {
                    std::wstring strFilePath = _pExProp->currentDir + _vFileList[index].strNameExt;
                    if (IsFileOpen(strFilePath) == TRUE) {
                        ::SelectObject(lpCD->nmcd.hdc, _pExProp->underlineFont);
                        ::SetWindowLongPtr(_hParent, DWLP_MSGRESULT, CDRF_NEWFONT);
                    }
                }
                if (_vFileList[index].isParent) {
                    ::SetWindowLongPtr(_hParent, DWLP_MSGRESULT, CDRF_NOTIFYPOSTPAINT);
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

void FileList::UpdateOverlayIcon()
{
    SIZE_T i = 0;

    while (true) {
        DWORD dwCase = ::WaitForMultipleObjects(FL_EVT_MAX, _hEvent, FALSE, INFINITE);

        switch (dwCase) {
        case FL_EVT_EXIT:
            LIST_UNLOCK();
            return;
        case FL_EVT_INT:
            i = _uMaxElements;
            LIST_UNLOCK();
            break;
        case FL_EVT_START:
            i = 0;
            if (_vFileList.empty()) {
                break;
            }
            LIST_LOCK();

            /* step over parent icon */
            if ((_uMaxFolders != 0) && (_vFileList[0].isParent == FALSE)) {
                i = 1;
            }

            ::SetEvent(_hEvent[FL_EVT_NEXT]);
            break;
        case FL_EVT_NEXT:
            if (::WaitForSingleObject(_hEvent[FL_EVT_INT], 1) == WAIT_TIMEOUT) {
                if (i < _uMaxFolders) {
                    ::PostMessage(_hSelf, EXM_UPDATE_OVERICON, i, (LPARAM)DEVT_DIRECTORY);
                }
                else if (i < _uMaxElements) {
                    ::PostMessage(_hSelf, EXM_UPDATE_OVERICON, i, (LPARAM)DEVT_FILE);
                }
                else {
                    LIST_UNLOCK();
                }
                i++;
            }
            else {
                ::SetEvent(_hEvent[FL_EVT_INT]);
            }
            break;
        default:
            break;
        }
    }
}

void FileList::ReadIconToList(UINT iItem, LPINT piIcon, LPINT piOverlay, LPBOOL pbHidden)
{
    INT     iIconSelected   = 0;
    DevType type            = (iItem < _uMaxFolders ? DEVT_DIRECTORY : DEVT_FILE);

    if (_vFileList[iItem].iIcon == -1) {
        ExtractIcons(_pExProp->currentDir.c_str(), _vFileList[iItem].strNameExt.c_str(),
            type, &_vFileList[iItem].iIcon, &iIconSelected, NULL);
    }
    *piIcon     = _vFileList[iItem].iIcon;
    *piOverlay  = _vFileList[iItem].iOverlay;
    *pbHidden   = _vFileList[iItem].isHidden;
}

void FileList::ReadArrayToList(LPTSTR szItem, INT iItem ,INT iSubItem)
{
    /* copy into temp */
    switch (iSubItem) {
    case SubItem::Name:
        if ((iItem < (INT)_uMaxFolders) && (_pExProp->bViewBraces == TRUE)) {
            swprintf(szItem, L"[%ls]", _vFileList[iItem].strName.c_str());
        }
        else {
            swprintf(szItem, L"%ls", _vFileList[iItem].strName.c_str());
        }
        break;
    case SubItem::Extension:
        if ((iItem < (INT)_uMaxFolders) || (_pExProp->bAddExtToName == FALSE)) {
            wcscpy(szItem, _vFileList[iItem].strExt.c_str());
        }
        else {
            szItem[0] = '\0';
        }
        break;
    case SubItem::Size:
        wcscpy(szItem, _vFileList[iItem].strSize.c_str());
        break;
    case SubItem::Date:
    default:
        wcscpy(szItem, _vFileList[iItem].strDate.c_str());
        break;
    }
}

void FileList::viewPath(const std::wstring& currentDir, BOOL redraw)
{
    std::wstring findName = currentDir;
    if (findName.empty()) {
        return;
    }

    /* add backslash if necessary */
    if (findName.back() != L'\\') {
        findName.push_back(L'\\');
    }
    findName.push_back(L'*');

    /* end thread if it is in run mode */
    ::SetEvent(_hEvent[FL_EVT_INT]);

    /* clear data */
    _uMaxElementsOld = _uMaxElements;

    /* find every element in folder */
    WIN32_FIND_DATA Find{};
    auto *hFind = ::FindFirstFile(findName.c_str(), &Find);
    std::vector<FileListData> vFoldersTemp;
    std::vector<FileListData> vFilesTemp;
    if (hFind != INVALID_HANDLE_VALUE) {
        /* get current filters */
        FileListData tempData;
        do {
            if (IsValidFolder(Find) == TRUE) {
                /* get data in order of list elements */
                tempData.isParent       = FALSE;
                tempData.iIcon          = -1;
                tempData.iOverlay       = 0;
                tempData.isDirectory    = TRUE;
                tempData.isHidden       = ((Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
                tempData.strName        = Find.cFileName;
                tempData.strNameExt     = Find.cFileName;
                tempData.strExt         = L"";

                if (_pExProp->bViewLong == TRUE) {
                    tempData.strSize    = L"<DIR>";
                    tempData.i64Size    = 0;
                    GetDate(Find.ftLastWriteTime, tempData.strDate);
                    tempData.i64Date    = 0;
                }

                vFoldersTemp.push_back(tempData);
            }
            else if ((IsValidFile(Find) == TRUE) && (_pExProp->fileFilter.match(Find.cFileName) == TRUE)) {
                /* store for correct sorting the complete name (with extension) */
                tempData.strNameExt     = Find.cFileName;
                tempData.isParent       = FALSE;
                tempData.iIcon          = -1;
                tempData.iOverlay       = 0;
                tempData.isDirectory    = FALSE;
                tempData.isHidden       = ((Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);

                /* extract name and extension */
                LPTSTR extBeg = wcsrchr(&Find.cFileName[1], '.');

                if (extBeg != NULL) {
                    *extBeg = '\0';
                    tempData.strExt     = &extBeg[1];
                }
                else {
                    tempData.strExt     = L"";
                }

                if ((_pExProp->bAddExtToName == TRUE) && (extBeg != NULL)) {
                    *extBeg = '.';
                }
                tempData.strName        = Find.cFileName;


                if (_pExProp->bViewLong == TRUE) {
                    tempData.i64Size    = (((INT64)Find.nFileSizeHigh) << 32) + Find.nFileSizeLow;
                    GetSize(tempData.i64Size, tempData.strSize);
                    tempData.i64Date    = (((INT64)Find.ftLastWriteTime.dwHighDateTime) << 32) + Find.ftLastWriteTime.dwLowDateTime;
                    GetDate(Find.ftLastWriteTime, tempData.strDate);
                }

                vFilesTemp.push_back(tempData);
            }
            else if ((IsValidParentFolder(Find) == TRUE) && !PathIsRoot(currentDir.c_str())) {
                /* if 'Find' is not a folder but a parent one */
                tempData.isParent       = TRUE;
                tempData.isHidden       = FALSE;
                tempData.isDirectory    = TRUE;
                tempData.strName        = Find.cFileName;
                tempData.strNameExt     = Find.cFileName;
                tempData.strExt         = L"";

                if (_pExProp->bViewLong == TRUE) {
                    tempData.strSize    = L"<DIR>";
                    tempData.i64Size    = 0;
                    GetDate(Find.ftLastWriteTime, tempData.strDate);
                    tempData.i64Date    = 0;
                }

                vFoldersTemp.push_back(tempData);
            }
        } while (FindNextFile(hFind, &Find));

        /* close file search */
        ::FindClose(hFind);
    }

    /* add current dir to stack */
    PushDir(currentDir);

    LIST_LOCK();

    /* delete old global list */
    _vFileList.clear();

    /* set temporal list as global */
    for (const auto &folder : vFoldersTemp) {
        _vFileList.push_back(folder);
    }
    for (const auto &file : vFilesTemp) {
        _vFileList.push_back(file);
    }

    /* set max elements in list */
    _uMaxFolders    = vFoldersTemp.size();
    _uMaxElements   = _uMaxFolders + vFilesTemp.size();
    vFoldersTemp.clear();
    vFilesTemp.clear();

    /* update list content */
    UpdateList();

    /* select first entry */
    if (redraw == TRUE) {
        SetFocusItem(0);
    }

    LIST_UNLOCK();

    /* start with update of overlay icons */
    ::SetEvent(_hEvent[FL_EVT_START]);
}

void FileList::filterFiles(LPCTSTR currentFilter)
{
    _pExProp->fileFilter.setFilter(currentFilter);
    viewPath(_pExProp->currentDir, TRUE);
}

void FileList::SelectFolder(LPCTSTR filePath)
{
    for (UINT uFolder = 0; uFolder < _uMaxFolders; uFolder++) {
        if (_wcsicmp(_vFileList[uFolder].strName.c_str(), filePath) == 0) {
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
    for (SIZE_T i = _uMaxFolders; i < _uMaxElements; i++) {
        if (fileName == _vFileList[i].strNameExt) {
            SetFocusItem(i);
            return;
        }
    }
}

void FileList::UpdateList()
{
    std::sort(_vFileList.begin(), _vFileList.end(), [&](const FileListData& lhs, const FileListData& rhs) {
        if (lhs.isParent != rhs.isParent) {
            return lhs.isParent > rhs.isParent;
        }
        if (lhs.isDirectory != rhs.isDirectory) {
            return lhs.isDirectory > rhs.isDirectory;
        }

        const int resultNameExt = ::StrCmpLogicalW(lhs.strNameExt.c_str(), rhs.strNameExt.c_str());
        INT64 result = 0;

        if (lhs.isDirectory && rhs.isDirectory) {
            return resultNameExt < 0;
        }

        switch (_pExProp->iSortPos) {
        case SubItem::Name:
            result = resultNameExt;
            break;
        case SubItem::Extension:
            result = lhs.strExt.compare(rhs.strExt);
            break;
        case SubItem::Size:
            result = lhs.i64Size - rhs.i64Size;
            break;
        case SubItem::Date:
            result = lhs.i64Date - rhs.i64Date;
            break;
        default:
            break;
        }

        if (result == 0) {
            result = resultNameExt;
        }

        if (!_pExProp->bAscending) {
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

    if ((_bOldAddExtToName != _pExProp->bAddExtToName) && (_pExProp->iSortPos > 0)) {
        if (_pExProp->bAddExtToName == FALSE) {
            _pExProp->iSortPos++;
        }
        else {
            _pExProp->iSortPos--;
        }

        _bOldAddExtToName = _pExProp->bAddExtToName;
    }
    if (_bOldViewLong != _pExProp->bViewLong) {
        if ((_pExProp->bViewLong == FALSE) &&
            (((_pExProp->iSortPos > 0) && (_pExProp->bAddExtToName == TRUE)) ||
                ((_pExProp->iSortPos > 1) && (_pExProp->bAddExtToName == FALSE)))) {
            _pExProp->iSortPos = 0;
        }
        _bOldViewLong = _pExProp->bViewLong;
    }

    ColSetup.mask       = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
    ColSetup.fmt        = LVCFMT_LEFT;
    ColSetup.pszText    = const_cast<LPTSTR>(cColumns[0]);
    ColSetup.cchTextMax = (int)wcslen(cColumns[0]);
    ColSetup.cx         = _pExProp->iColumnPosName;
    ListView_InsertColumn(_hSelf, 0, &ColSetup);
    ColSetup.pszText    = const_cast<LPTSTR>(cColumns[1]);
    ColSetup.cchTextMax = (int)wcslen(cColumns[1]);
    ColSetup.cx         = _pExProp->iColumnPosExt;
    ListView_InsertColumn(_hSelf, 1, &ColSetup);

    if (_pExProp->bViewLong)
    {
        ColSetup.fmt        = LVCFMT_RIGHT;
        ColSetup.pszText    = const_cast<LPTSTR>(cColumns[2]);
        ColSetup.cchTextMax = (int)wcslen(cColumns[2]);
        ColSetup.cx         = _pExProp->iColumnPosSize;
        ListView_InsertColumn(_hSelf, 2, &ColSetup);
        ColSetup.fmt        = LVCFMT_LEFT;
        ColSetup.pszText    = const_cast<LPTSTR>(cColumns[3]);
        ColSetup.cchTextMax = (int)wcslen(cColumns[3]);
        ColSetup.cx         = _pExProp->iColumnPosDate;
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
        if (i == _pExProp->iSortPos) {
            hdItem.fmt |= _pExProp->bAscending ? HDF_SORTUP : HDF_SORTDOWN;
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
    std::vector<std::wstring> paths;

    /* create data */
    for (UINT uList = 0; uList < _uMaxElements; uList++) {
        if (ListView_GetItemState(_hSelf, uList, LVIS_SELECTED) == LVIS_SELECTED) {
            if (uList == 0) {
                if (_vFileList[0].strName == L"..") {
                    ListView_SetItemState(_hSelf, uList, 0, 0xFF);
                    isParent = true;
                    continue;
                }
            }

            if (uList < _uMaxFolders) {
                paths.push_back(_pExProp->currentDir + _vFileList[uList].strName + L"\\");
            }
            else {
                paths.push_back(_pExProp->currentDir + _vFileList[uList].strNameExt);
            }
        }
    }

    if (paths.empty()) {
        paths.push_back(_pExProp->currentDir);
    }

    const auto hasStandardMenu = (!isParent || (paths.size() != 1));
    _context->ShowContextMenu(screenLocation.value(), paths, hasStandardMenu);
}

void FileList::onLMouseBtnDbl()
{
    UINT selRow = ListView_GetSelectionMark(_hSelf);

    if (selRow != -1) {
        if (selRow < _uMaxFolders) {
            _context->NavigateTo(_vFileList[selRow].strName);
        }
        else {
            _context->Open(_vFileList[selRow].strNameExt);
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
        firstRow = (_vFileList[0].isParent ? 0 : -1);
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
        doPaste(_pExProp->currentDir.c_str(), hFiles, DROPEFFECT_MOVE);
    }
    else if (hEffect[0] == 5) {
        doPaste(_pExProp->currentDir.c_str(), hFiles, DROPEFFECT_COPY);
    }
    ::GlobalUnlock(hFiles);
    ::GlobalUnlock(hEffect);
    ::CloseClipboard();

    ::KillTimer(_hParent, EXT_UPDATEACTIVATEPATH);
    ::SetTimer(_hParent, EXT_UPDATEACTIVATEPATH, 200, nullptr);
}

void FileList::onDelete(bool immediate)
{
    SIZE_T  lengthPath  = _pExProp->currentDir.size();
    UINT    bufSize     = ListView_GetSelectedCount(_hSelf) * MAX_PATH;
    LPTSTR  lpszFiles   = new WCHAR[bufSize];
    ::ZeroMemory(lpszFiles, bufSize);

    /* add files to payload and seperate with "\0" */
    SIZE_T offset = 0;
    for (SIZE_T i = 0; i < _uMaxElements; i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            if ((i == 0) && (_vFileList[i].isParent == TRUE)) {
                continue;
            }
            wcscpy(&lpszFiles[offset], _pExProp->currentDir.c_str());
            wcscpy(&lpszFiles[offset+lengthPath], _vFileList[i].strNameExt.c_str());
            offset += lengthPath + _vFileList[i].strNameExt.size() + 1;
        }
    }

    /* delete folder into recycle bin */
    SHFILEOPSTRUCT fileOp   = {0};
    fileOp.hwnd             = _hParent;
    fileOp.pFrom            = lpszFiles;
    fileOp.fFlags           = (immediate ? 0 : FOF_ALLOWUNDO);
    fileOp.wFunc            = FO_DELETE;
    SHFileOperation(&fileOp);
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

        std::wstring lowerFileName = _vFileList[i].strName;
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


void FileList::GetSize(INT64 fileSize, std::wstring & str)
{
    constexpr std::array<const WCHAR*, 4> SIZE_UNITS{ L"bytes", L"KB", L"MB", L"GB"};

    auto displayFileSize = static_cast<double>(fileSize);
    size_t iSizeIndex = 0;

    switch (_pExProp->fmtSize) {
    case SFMT_BYTES:
        iSizeIndex = 0;
        break;
    case SFMT_KBYTE:
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
    if (_pExProp->fmtSize == SFMT_DYNAMIC_EX) {
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

void FileList::GetDate(FILETIME ftLastWriteTime, std::wstring & str)
{
    FILETIME    ftLocalTime;
    SYSTEMTIME  sysTime;
    WCHAR       TEMP[18];

    FileTimeToLocalFileTime(&ftLastWriteTime, &ftLocalTime);
    FileTimeToSystemTime(&ftLocalTime, &sysTime);

    if (_pExProp->fmtDate == DFMT_ENG) {
        swprintf(TEMP, L"%02d/%02d/%02d %02d:%02d", sysTime.wYear % 100, sysTime.wMonth, sysTime.wDay, sysTime.wHour, sysTime.wMinute);
    }
    else {
        swprintf(TEMP, L"%02d.%02d.%04d %02d:%02d", sysTime.wDay, sysTime.wMonth, sysTime.wYear, sysTime.wHour, sysTime.wMinute);
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
void FileList::SetToolBarInfo(ToolBar *toolBar, UINT idUndo, UINT idRedo)
{
    _pToolBar   = toolBar;
    _idUndo     = idUndo;
    _idRedo     = idRedo;
    ResetDirStack();
}

void FileList::ResetDirStack()
{
    _vDirStack.clear();
    _itrPos = _vDirStack.end();
    UpdateToolBarElements();
}

void FileList::ToggleStackRec()
{
    _isStackRec ^= TRUE;
}

void FileList::PushDir(const std::wstring& path)
{
    if (_isStackRec == TRUE) {
        StaInfo StackInfo {
            .strPath = path,
        };

        if (_itrPos != _vDirStack.end()) {
            _vDirStack.erase(_itrPos + 1, _vDirStack.end());

            if (_wcsicmp(path.c_str(), _itrPos->strPath.c_str()) != 0) {
                _vDirStack.push_back(StackInfo);
            }
        }
        else {
            _vDirStack.push_back(StackInfo);
        }

        while (_pExProp->maxHistorySize + 1 < _vDirStack.size()) {
            _vDirStack.erase(_vDirStack.begin());
        }
        _itrPos = _vDirStack.end() - 1;

    }

    UpdateToolBarElements();
}

bool FileList::GetPrevDir(LPTSTR pszPath, std::vector<std::wstring> & vStrItems)
{
    if (_vDirStack.size() > 1) {
        if (_itrPos != _vDirStack.begin()) {
            _itrPos--;
            wcscpy(pszPath, _itrPos->strPath.c_str());
            vStrItems = _itrPos->vStrItems;
            return true;
        }
    }
    return false;
}

bool FileList::GetNextDir(LPTSTR pszPath, std::vector<std::wstring> & vStrItems)
{
    if (_vDirStack.size() > 1) {
        if (_itrPos != _vDirStack.end() - 1) {
            _itrPos++;
            wcscpy(pszPath, _itrPos->strPath.c_str());
            vStrItems = _itrPos->vStrItems;
            return true;
        }
    }
    return false;
}

INT FileList::GetPrevDirs(LPTSTR *pszPathes)
{
    INT i = 0;
    std::vector<StaInfo>::iterator itr = _itrPos;

    if (_vDirStack.size() > 1) {
        while (1) {
            if (itr != _vDirStack.begin()) {
                itr--;
                if (pszPathes) {
                    wcscpy(pszPathes[i], itr->strPath.c_str());
                }
            } else {
                break;
            }
            i++;
        }
    }
    return i;
}

INT FileList::GetNextDirs(LPTSTR *pszPathes)
{
    INT i = 0;
    std::vector<StaInfo>::iterator itr = _itrPos;

    if (_vDirStack.size() > 1) {
        while (1) {
            if (itr != _vDirStack.end() - 1) {
                itr++;
                if (pszPathes) {
                    wcscpy(pszPathes[i], itr->strPath.c_str());
                }
            } else {
                break;
            }
            i++;
        }
    }
    return i;
}

void FileList::OffsetItr(INT offsetItr, std::vector<std::wstring> & vStrItems)
{
    _itrPos += offsetItr;
    vStrItems = _itrPos->vStrItems;
    UpdateToolBarElements();
}

void FileList::UpdateToolBarElements()
{
    bool canRedo = !_vDirStack.empty() && (_itrPos != _vDirStack.end() - 1);
    _pToolBar->enable(_idRedo, canRedo);
    _pToolBar->enable(_idUndo, _itrPos != _vDirStack.begin());
}

void FileList::UpdateSelItems()
{
    if (_isStackRec == TRUE) {
        _itrPos->vStrItems.clear();

        for (UINT i = 0; i < _uMaxElements; i++) {
            if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
                _itrPos->vStrItems.push_back(_vFileList[i].strNameExt);
            }
        }
    }
}

void FileList::SetItems(std::vector<std::wstring> vStrItems)
{
    UINT selType = LVIS_SELANDFOC;

    for (UINT i = 0; i < _uMaxElements; i++) {
        for (UINT itemPos = 0; itemPos < vStrItems.size(); itemPos++) {
            if (_vFileList[i].strNameExt == vStrItems[itemPos]) {
                ListView_SetItemState(_hSelf, i, selType, 0xFF);

                /* set first item in view */
                if (selType == LVIS_SELANDFOC) {
                    ListView_EnsureVisible(_hSelf, _uMaxElements - 1, FALSE);
                    ListView_EnsureVisible(_hSelf, i, FALSE);
                }

                /* delete last found item to be faster in compare */
                vStrItems.erase(vStrItems.begin() + itemPos);

                /* if last item were delete return from function */
                if (vStrItems.empty()) {
                    return;
                }

                selType = LVIS_SELECTED;
                break;
            }
            ListView_SetItemState(_hSelf, i, 0, 0xFF);
        }
    }
}


/***************************************************************************************
 *  Drag'n'Drop, Cut and Copy of folders
 */
void FileList::FolderExChange(CIDropSource* pdsrc, CIDataObject* pdobj, UINT dwEffect)
{
    SIZE_T parsz = _pExProp->currentDir.size();
    SIZE_T bufsz = sizeof(DROPFILES) + sizeof(WCHAR);

    /* get buffer size */
    for (SIZE_T i = 0; i < _uMaxElements; i++) {
        if (ListView_GetItemState(_hSelf, i, LVIS_SELECTED) == LVIS_SELECTED) {
            if ((i == 0) && (_vFileList[i].isParent == TRUE)) {
                continue;
            }
            bufsz += (parsz + _vFileList[i].strNameExt.size() + 1) * sizeof(WCHAR);
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
            if ((i == 0) && (_vFileList[i].isParent == TRUE)) {
                continue;
            }
            wcscpy(&szPath[offset], _pExProp->currentDir.c_str());
            wcscpy(&szPath[offset+parsz], _vFileList[i].strNameExt.c_str());
            offset += parsz + _vFileList[i].strNameExt.size() + 1;
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
    wcscpy(pszFilesTo, _pExProp->currentDir.c_str());

    /* get position */
    LVHITTESTINFO hittest = {};
    ::GetCursorPos(&hittest.pt);
    ScreenToClient(_hSelf, &hittest.pt);
    ::SendMessage(_hSelf, LVM_SUBITEMHITTEST, 0, (LPARAM)&hittest);

    if ((UINT)hittest.iItem < _uMaxFolders) {
        if (_vFileList[hittest.iItem].isParent == TRUE) {
            ::PathRemoveFileSpec(pszFilesTo);
            ::PathRemoveFileSpec(pszFilesTo);
        }
        else {
            ::PathAppend(pszFilesTo, _vFileList[hittest.iItem].strNameExt.c_str());
        }
    }

    doPaste(pszFilesTo, hDrop, *pdwEffect);

    return true;
}

bool FileList::doPaste(LPCTSTR pszTo, LPDROPFILES hData, const DWORD & dwEffect)
{
    /* get files from and to, to fill struct */
    UINT    headerSize      = sizeof(DROPFILES);
    SIZE_T  payloadSize     = ::GlobalSize(hData) - headerSize;
    LPVOID  pPld            = (LPBYTE)hData + headerSize;
    LPTSTR  lpszFilesFrom   = nullptr;

    if (((LPDROPFILES)hData)->fWide == TRUE) {
        lpszFilesFrom = (LPWSTR)pPld;
    }
    else {
        lpszFilesFrom = new WCHAR[payloadSize];
        ::MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pPld, (int)payloadSize, lpszFilesFrom, (int)payloadSize);
    }

    if (lpszFilesFrom != NULL) {
        UINT count = 0;
        SIZE_T length = payloadSize;
        if (((LPDROPFILES)hData)->fWide == TRUE) {
            length = payloadSize / 2;
        }
        for (UINT i = 0; i < length-1; i++) {
            if (lpszFilesFrom[i] == '\0') {
                count++;
            }
        }

        WCHAR text[MAX_PATH + 32];
        if (dwEffect == DROPEFFECT_MOVE) {
            swprintf(text, L"Move %d file(s)/folder(s) to:\n\n%ls", count, pszTo);
        }
        else {// dwEffect == DROPEFFECT_COPY
            swprintf(text, L"Copy %d file(s)/folder(s) to:\n\n%ls", count, pszTo);
        }

        if (::MessageBox(_hSelf, text, L"Explorer", MB_YESNO) == IDYES) {
            // TODO move or copy the file views into other window in dependency to keystate
            SHFILEOPSTRUCT fileOp = {};
            fileOp.hwnd         = _hParent;
            fileOp.pFrom        = lpszFilesFrom;
            fileOp.pTo          = pszTo;
            fileOp.fFlags       = FOF_RENAMEONCOLLISION;
            if (dwEffect == DROPEFFECT_MOVE) {
                fileOp.wFunc    = FO_MOVE;
            }
            else {
                fileOp.wFunc    = FO_COPY;
            }
            SHFileOperation(&fileOp);

            ::KillTimer(_hParent, EXT_UPDATEACTIVATEPATH);
            ::SetTimer(_hParent, EXT_UPDATEACTIVATEPATH, 200, NULL);
        }

        if (((LPDROPFILES)hData)->fWide == FALSE) {
            delete [] lpszFilesFrom;
        }
    }
    return true;
}
