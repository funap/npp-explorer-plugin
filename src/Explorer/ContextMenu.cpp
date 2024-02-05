/***********************************************************\
*   Original in MFC by Roman Engels     Copyright 2003      *
*                                                           *
*   http://www.codeproject.com/shell/shellcontextmenu.asp   *
\***********************************************************/

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

#include "ContextMenu.h"

#include <Shlwapi.h>

#include "Explorer.h"
#include "ExplorerDialog.h"
#include "FavesDialog.h"
#include "NppInterface.h"
#include "QuickOpenDialog.h"
#include "../NppPlugin/menuCmdID.h"
#include "../NppPlugin/nppexec_msgs.h"

/* global explorer params */
extern ExProp exProp;

namespace {
    constexpr UINT_PTR CONTEXT_MENU_SUBCLASS_ID = 1;
    constexpr int CTX_MIN = 1;
    constexpr int CTX_MAX = 10000;

    enum eContextMenuID {
        CTX_DELETE = 18,
        CTX_RENAME = 19,
        CTX_CUT = 25,
        CTX_COPY = 26,
        CTX_PASTE = 27,
        CTX_NEW_FILE = CTX_MAX,
        CTX_NEW_FOLDER,
        CTX_FIND_IN_FILES,
        CTX_OPEN,
        CTX_OPEN_DIFF_VIEW,
        CTX_OPEN_NEW_INST,
        CTX_OPEN_CMD,
        CTX_SET_AS_ROOT_FOLDER,
        CTX_GO_TO_ROOT_FOLDER,
        CTX_CLEAR_ROOT_FOLDER,
        CTX_ADD_TO_FAVES,
        CTX_RELATIVE_PATH,
        CTX_FULL_PATH,
        CTX_FULL_FILES,
        CTX_GOTO_SCRIPT_PATH,
        CTX_START_SCRIPT,
        CTX_QUICK_OPEN
    };

    struct ObjectData {
        WCHAR   *pszFullPath;
        WCHAR   szFileName[MAX_PATH];
        WCHAR   szTypeName[MAX_PATH];
        UINT64  u64FileSize;
        DWORD   dwFileAttributes;
        int     iIcon;
        FILETIME ftLastModified;
    };
}

ContextMenu::ContextMenu()
    : _hInst(nullptr)
    , _hWndNpp(nullptr)
    , _hWndParent(nullptr)
    , _nItems(0)
    , _bDelete(FALSE)
    , _psfFolder(nullptr)
    , _pidlArray(nullptr)
    , _contextMenu2(nullptr)
    , _contextMenu3(nullptr)
{
}

ContextMenu::~ContextMenu()
{
    /* free all allocated datas */
    if (_psfFolder && _bDelete) {
        _psfFolder->Release ();
    }
    _psfFolder = nullptr;
    FreePIDLArray(_pidlArray);
    _pidlArray = nullptr;

}


// this functions determines which version of IContextMenu is avaibale for those objects (always the highest one)
// and returns that interface
LPCONTEXTMENU ContextMenu::GetContextMenu()
{
    LPCONTEXTMENU contextMenu = nullptr;
    LPCONTEXTMENU contextMenu1 = nullptr;

    // first we retrieve the normal IContextMenu interface (every object should have it)
    _psfFolder->GetUIObjectOf(nullptr, (UINT)_nItems, (LPCITEMIDLIST *) _pidlArray, IID_IContextMenu, nullptr, (void**) &contextMenu1);

    if (contextMenu1) {
        // since we got an IContextMenu interface we can now obtain the higher version interfaces via that
        if (SUCCEEDED(contextMenu1->QueryInterface(IID_IContextMenu3, (void**)&_contextMenu3))) {
            contextMenu = _contextMenu3;
            contextMenu1->Release();
        }
        else if (SUCCEEDED(contextMenu1->QueryInterface(IID_IContextMenu2, (void**)&_contextMenu2))) {
            contextMenu = _contextMenu2;
            contextMenu1->Release();
        }
        else {
            // since no higher versions were found
            // redirect ppContextMenu to version 1 interface
            contextMenu = contextMenu1;
        }
    }

    return contextMenu;
}

LRESULT CALLBACK ContextMenu::defaultHookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto* cm = reinterpret_cast<ContextMenu*>(dwRefData);
    return cm->HookWndProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK ContextMenu::HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_MENUCHAR: // only supported by IContextMenu3
        if (_contextMenu3) {
            LRESULT lResult = 0;
            _contextMenu3->HandleMenuMsg2 (message, wParam, lParam, &lResult);
            return (lResult);
        }
        break;
    case WM_DRAWITEM:
    case WM_MEASUREITEM:
    case WM_INITMENUPOPUP: {
        HRESULT hr;
        if (_contextMenu2) {
            hr = _contextMenu2->HandleMenuMsg(message, wParam, lParam);
        }
        else {  // version 3
            hr = _contextMenu3->HandleMenuMsg2(message, wParam, lParam, nullptr);
        }

        if (SUCCEEDED(hr)) {
            if (message == WM_INITMENUPOPUP) {
                return FALSE;
            }
            return TRUE;
        }
        break;
    }
    default:
        break;
    }

    // call original WndProc of window to prevent undefined bevhaviour of window
    return ::DefSubclassProc(hWnd, message, wParam, lParam);
}


UINT ContextMenu::ShowContextMenu(HINSTANCE hInst, HWND hWndNpp, HWND hWndParent, POINT pt, bool normal)
{
    /* store notepad handle */
    _hInst = hInst;
    _hWndNpp = hWndNpp;
    _hWndParent = hWndParent;

    if (!_pidlArray) {
        return 0;
    }

    HMENU hShellMenu = ::CreatePopupMenu();
    if (nullptr == hShellMenu) {
        return 0;
    }

    // common pointer to IContextMenu and higher version interface
    LPCONTEXTMENU pContextMenu = GetContextMenu();
    if (nullptr == pContextMenu) {
        return 0;
    }

    if (nullptr != _pidlArray) {
        UINT uFlags = CMF_EXPLORE;
        if (!::PathIsRoot(_strFirstElement.c_str())) {
            uFlags |= CMF_CANRENAME;
        }
        pContextMenu->QueryContextMenu(hShellMenu, 0, CTX_MIN, CTX_MAX, uFlags);
    }

    // only subclass if its version 2 or 3
    BOOL bWindowSubclassed = FALSE;
    if ((nullptr != _contextMenu2) || (nullptr != _contextMenu3)) {
        bWindowSubclassed = SetWindowSubclass(hWndParent, defaultHookWndProc, CONTEXT_MENU_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
    }

    /************************************* modification for notepad ***********************************/
    HMENU   hMainMenu       = ::CreatePopupMenu();
    HMENU   hMenuNppExec    = ::CreatePopupMenu();
    BOOL    isFolder        = ('\\' == _strFirstElement.back());
    DWORD   dwExecVer       = 0;
    DWORD   dwExecState     = 0;
    WCHAR   szPath[MAX_PATH];
    ::GetModuleFileName((HMODULE)hInst, szPath, MAX_PATH);

    /* get version information */
    CommunicationInfo ci{
        .internalMsg      = NPEM_GETVERDWORD,
        .srcModuleName    = PathFindFileName(szPath),
        .info             = &dwExecVer,
    };
    ::SendMessage(hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)exProp.nppExecProp.szAppName, (LPARAM)&ci);

    /* get acivity state of NppExec */
    ci.internalMsg      = NPEM_GETSTATE;
    ci.srcModuleName    = PathFindFileName(szPath);
    ci.info             = &dwExecState;
    ::SendMessage(hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)exProp.nppExecProp.szAppName, (LPARAM)&ci);

    /* Add notepad menu items */
    if (isFolder) {
        ::AppendMenu(hMainMenu, MF_STRING, CTX_NEW_FILE, L"New File...");
        ::AppendMenu(hMainMenu, MF_STRING, CTX_NEW_FOLDER, L"New Folder...");
        ::AppendMenu(hMainMenu, MF_STRING, CTX_FIND_IN_FILES, L"Find in Files...");
    }
    else {
        ::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN, L"Open");
        ::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_DIFF_VIEW, L"Open in Other View");
        ::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_NEW_INST, L"Open in New Instance");
    }

    if (dwExecVer >= 0x02F5) {
        WCHAR           TEMP[MAX_PATH];
        WIN32_FIND_DATA Find    = {0};
        HANDLE          hFind   = nullptr;

        /* initialize scripts */
        _strNppScripts.clear();

        /* add backslash if necessary */
        if ((exProp.nppExecProp.szScriptPath[0] == '.') &&
            (exProp.nppExecProp.szScriptPath[1] == '.')) {
            /* module path of notepad */
            GetModuleFileName(hInst, TEMP, sizeof(TEMP));
            PathRemoveFileSpec(TEMP);
            PathAppend(TEMP, exProp.nppExecProp.szScriptPath);
        } else {
            _tcsncpy(TEMP, exProp.nppExecProp.szScriptPath, MAX_PATH-1);
        }
        if (TEMP[wcslen(TEMP) - 1] != '\\') {
            wcscat(TEMP, L"\\");
        }

        /* find every element in folder */
        wcscat(TEMP, L"*.exec");
        hFind = ::FindFirstFile(TEMP, &Find);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                ::AppendMenu(hMenuNppExec, MF_STRING | (dwExecState == NPE_STATEREADY ? 0 : MF_DISABLED), CTX_START_SCRIPT + _strNppScripts.size(), Find.cFileName);
                _strNppScripts.push_back(Find.cFileName);
            } while (FindNextFile(hFind, &Find));

            /* close file search */
            ::FindClose(hFind);
        }
        if (!_strNppScripts.empty()) {
            ::AppendMenu(hMenuNppExec, MF_SEPARATOR, 0, nullptr);
        }
        ::AppendMenu(hMenuNppExec, MF_STRING, CTX_GOTO_SCRIPT_PATH, L"Go to script folder");
        ::AppendMenu(hMainMenu, MF_STRING | MF_POPUP, (UINT_PTR)hMenuNppExec, L"NppExec Script(s)");
    }
    else {
        /* version not supported */
        ::DestroyMenu(hMenuNppExec);
    }
    ::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_CMD, L"Open Command Window Here");
    ::AppendMenu(hMainMenu, MF_STRING, CTX_SET_AS_ROOT_FOLDER, L"Set as Root Folder");
    if (!exProp.rootFolder.empty()) {
        ::AppendMenu(hMainMenu, MF_STRING, CTX_GO_TO_ROOT_FOLDER, L"Go to Root Folder");
        ::AppendMenu(hMainMenu, MF_STRING, CTX_CLEAR_ROOT_FOLDER, L"Clear Root Folder");
    }

    ::AppendMenu(hMainMenu, MF_STRING, CTX_QUICK_OPEN, L"Quick Open...");
    ::InsertMenu(hMainMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
    ::AppendMenu(hMainMenu, MF_STRING, CTX_ADD_TO_FAVES, L"Add to 'Favorites'...");
    std::wstring currentDirectory = NppInterface::getCurrentDirectory();
    if (!currentDirectory.empty()) {
        ::AppendMenu(hMainMenu, MF_STRING, CTX_RELATIVE_PATH, L"Relative File Path(s) to Clipboard");
    }
    ::AppendMenu(hMainMenu, MF_STRING, CTX_FULL_PATH, L"Full File Path(s) to Clipboard");
    ::AppendMenu(hMainMenu, MF_STRING, CTX_FULL_FILES, L"File Name(s) to Clipboard");

    if (nullptr != _pidlArray) {
        WCHAR           szText[MAX_PATH] = {0};
        int             copyAt  = -1;
        int             items   = ::GetMenuItemCount(hShellMenu);
        MENUITEMINFO    info {
            .cbSize = sizeof(MENUITEMINFO),
            .fMask  = MIIM_TYPE | MIIM_ID | MIIM_SUBMENU,
        };

        ::AppendMenu(hMainMenu, MF_SEPARATOR, 0, 0);

        if (normal) {
            /* store all items in an separate sub menu until "cut" (25) or "copy" (26) */
            for (int i = 0; i < items; i++) {
                info.cch        = _countof(szText);
                info.dwTypeData = szText;
                if (copyAt == -1) {
                    ::GetMenuItemInfo(hShellMenu, i, TRUE, &info);
                    if ((info.wID == CTX_CUT) || (info.wID == CTX_COPY) || (info.wID == CTX_PASTE)) {
                        copyAt  = i - 1;
                        ::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
                        ::DeleteMenu(hShellMenu, i  , MF_BYPOSITION);
                        ::DeleteMenu(hShellMenu, i-1, MF_BYPOSITION);
                    }
                }
                else {
                    ::GetMenuItemInfo(hShellMenu, copyAt, TRUE, &info);
                    if ((MFT_STRING == info.fType) || (MFT_SEPARATOR == info.fType)) {
                        ::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
                    }
                    ::DeleteMenu(hShellMenu, copyAt, MF_BYPOSITION);
                }
            }
            WCHAR szMenuName[MAX_PATH];
            wcscpy(szMenuName, L"Standard Menu");
            ::InsertMenu(hMainMenu, 4, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)hShellMenu, szMenuName);
            ::InsertMenu(hMainMenu, (dwExecVer >= 0x02F5 ? 7 : 6), MF_BYPOSITION | MF_SEPARATOR, 0, 0);
        }
        else {
            /* ignore all items until "cut" (25) or "copy" (26) */
            for (int i = 0; i < items; i++) {
                info.cch        = _countof(szText);
                info.dwTypeData = szText;
                ::GetMenuItemInfo(hShellMenu, i, TRUE, &info);
                if ((copyAt == -1) && ((info.wID == CTX_CUT) || (info.wID == CTX_COPY) || (info.wID == CTX_PASTE))) {
                    copyAt = 0;
                }
                else if ((info.wID == 20) || (info.wID == 27)) {
                    ::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
                    ::AppendMenu(hMainMenu, MF_SEPARATOR, 0, 0);
                }
            }
            ::DeleteMenu(hMainMenu, ::GetMenuItemCount(hMainMenu) - 1, MF_BYPOSITION);
        }
    }
    /*****************************************************************************************************/

    /* change language */
    UINT idCommand = ::TrackPopupMenu(hMainMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hWndParent, nullptr);

    if (bWindowSubclassed) {
        ::RemoveWindowSubclass(hWndParent, defaultHookWndProc, CONTEXT_MENU_SUBCLASS_ID);
    }

    // see if returned idCommand belongs to shell menu entries but not for renaming (19)
    if ((idCommand >= CTX_MIN) && (idCommand < CTX_MAX) && (idCommand != CTX_RENAME)) {
        InvokeCommand(pContextMenu, idCommand - CTX_MIN); // execute related command
    }
    else {
        HandleCustomCommand(idCommand);
    }

    ::DestroyMenu(hShellMenu);
    ::DestroyMenu(hMenuNppExec);
    ::DestroyMenu(hMainMenu);

    if (pContextMenu != nullptr) {
        pContextMenu->Release();
    }
    _contextMenu2 = nullptr;
    _contextMenu3 = nullptr;

    return (idCommand);
}

void ContextMenu::InvokeCommand(LPCONTEXTMENU pContextMenu, UINT idCommand)
{
    CMINVOKECOMMANDINFOEX cmi = { 
        .cbSize     = sizeof(CMINVOKECOMMANDINFO),
        .fMask      = CMIC_MASK_UNICODE,
        .hwnd       = _hWndNpp,
        .lpVerb     = MAKEINTRESOURCEA(idCommand),
        .nShow      = SW_SHOWNORMAL,
        .lpVerbW    = MAKEINTRESOURCEW(idCommand),
    };

    pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&cmi);
}

void ContextMenu::HandleCustomCommand(UINT idCommand)
{
    switch (idCommand) {
    case CTX_QUICK_OPEN:
        quickOpen();
        break;
    case CTX_RENAME:
        Rename();
        break;
    case CTX_NEW_FILE:
        newFile();
        break;
    case CTX_NEW_FOLDER:
        newFolder();
        break;
    case CTX_FIND_IN_FILES:
        findInFiles();
        break;
    case CTX_OPEN:
        openFile();
        break;
    case CTX_OPEN_DIFF_VIEW:
        openFileInOtherView();
        break;
    case CTX_OPEN_NEW_INST:
        openFileInNewInstance();
        break;
    case CTX_OPEN_CMD:
        openPrompt();
        break;
    case CTX_SET_AS_ROOT_FOLDER:
        setRootFolder();
        break;
    case CTX_GO_TO_ROOT_FOLDER:
        gotoRootFolder();
        break;
    case CTX_CLEAR_ROOT_FOLDER:
        clearRootFolder();
        break;
    case CTX_ADD_TO_FAVES:
        addToFaves();
        break;
    case CTX_RELATIVE_PATH:
        addRelativePathsCB();
        break;
    case CTX_FULL_PATH:
        addFullPathsCB();
        break;
    case CTX_FULL_FILES:
        addFileNamesCB();
        break;
    case CTX_GOTO_SCRIPT_PATH:
        openScriptPath(_hInst);
        break;
    default: /* and greater */
        if ((idCommand >= CTX_START_SCRIPT) && (idCommand <= (CTX_START_SCRIPT + _strNppScripts.size()))) {
            startNppExec(_hInst, idCommand - CTX_START_SCRIPT);
        }
        break;
    }
}

void ContextMenu::SetObjects(const std::wstring &strObject)
{
    // only one object is passed
    std::vector<std::wstring> strArray;
    strArray.push_back(strObject);  // create a CStringArray with one element

    SetObjects (strArray);              // and pass it to SetObjects (vector<string> strArray)
                                        // for further processing
}


void ContextMenu::SetObjects(const std::vector<std::wstring> &strArray)
{
    // store also the string for later menu use
    _strFirstElement    = strArray[0];
    _strArray           = strArray;

    // free all allocated datas
    if (_psfFolder && _bDelete) {
        _psfFolder->Release ();
    }
    _psfFolder = nullptr;
    FreePIDLArray (_pidlArray);
    _pidlArray = nullptr;

    // get IShellFolder interface of Desktop (root of shell namespace)
    IShellFolder * psfDesktop = nullptr;
    SHGetDesktopFolder (&psfDesktop);   // needed to obtain full qualified pidl

    // ParseDisplayName creates a PIDL from a file system path relative to the IShellFolder interface
    // but since we use the Desktop as our interface and the Desktop is the namespace root
    // that means that it's a fully qualified PIDL, which is what we need
    LPITEMIDLIST pidl = nullptr;

    psfDesktop->ParseDisplayName (nullptr, 0, (LPOLESTR)strArray[0].c_str(), nullptr, &pidl, nullptr);

    if (pidl != nullptr) {
        // now we need the parent IShellFolder interface of pidl, and the relative PIDL to that interface
        LPITEMIDLIST pidlItem = nullptr; // relative pidl
        SHBindToParentEx (pidl, IID_IShellFolder, (void **) &_psfFolder, nullptr);
        free (pidlItem);
        // get interface to IMalloc (need to free the PIDLs allocated by the shell functions)
        LPMALLOC lpMalloc = nullptr;
        SHGetMalloc (&lpMalloc);
        if (lpMalloc != nullptr) {
            lpMalloc->Free (pidl);
        }

        // now we have the IShellFolder interface to the parent folder specified in the first element in strArray
        // since we assume that all objects are in the same folder (as it's stated in the MSDN)
        // we now have the IShellFolder interface to every objects parent folder

        IShellFolder * psfFolder = nullptr;
        _nItems = strArray.size();
        for (SIZE_T i = 0; i < _nItems; i++) {
            psfDesktop->ParseDisplayName (nullptr, 0, (LPOLESTR)strArray[i].c_str(), nullptr, &pidl, nullptr);
            _pidlArray = (LPITEMIDLIST *) realloc (_pidlArray, (i + 1) * sizeof (LPITEMIDLIST));
            // get relative pidl via SHBindToParent
            SHBindToParentEx (pidl, IID_IShellFolder, (void **) &psfFolder, (LPCITEMIDLIST *) &pidlItem);
            _pidlArray[i] = CopyPIDL (pidlItem); // copy relative pidl to pidlArray
            free (pidlItem);
            // free pidl allocated by ParseDisplayName
            if (lpMalloc != nullptr) {
                lpMalloc->Free (pidl);
            }
            if (psfFolder != nullptr) {
                psfFolder->Release ();
            }
        }

        if (lpMalloc != nullptr) {
            lpMalloc->Release ();
        }
    }
    if (psfDesktop != nullptr) {
        psfDesktop->Release ();
    }

    _bDelete = TRUE; // indicates that _psfFolder should be deleted by ContextMenu
}


void ContextMenu::FreePIDLArray(LPITEMIDLIST *pidlArray)
{
    if (!pidlArray) {
        return;
    }

    SIZE_T iSize = _msize (pidlArray) / sizeof (LPITEMIDLIST);

    for (SIZE_T i = 0; i < iSize; i++) {
        free(pidlArray[i]);
    }
    free (pidlArray);
}


LPITEMIDLIST ContextMenu::CopyPIDL (LPCITEMIDLIST pidl, int cb)
{
    if (cb == -1) {
        cb = GetPIDLSize (pidl); // Calculate size of list.
    }

    LPITEMIDLIST pidlRet = (LPITEMIDLIST) calloc (cb + sizeof (USHORT), sizeof (BYTE));
    if (pidlRet) {
        CopyMemory(pidlRet, pidl, cb);
    }

    return (pidlRet);
}


UINT ContextMenu::GetPIDLSize (LPCITEMIDLIST pidl)
{
    if (!pidl) {
        return 0;
    }
    int nSize = 0;
    LPITEMIDLIST pidlTemp = (LPITEMIDLIST) pidl;
    while (pidlTemp->mkid.cb) {
        nSize += pidlTemp->mkid.cb;
        pidlTemp = (LPITEMIDLIST) (((LPBYTE) pidlTemp) + pidlTemp->mkid.cb);
    }
    return nSize;
}

// this is workaround function for the Shell API Function SHBindToParent
// SHBindToParent is not available under Win95/98
HRESULT ContextMenu::SHBindToParentEx (LPCITEMIDLIST pidl, REFIID riid, VOID **ppv, LPCITEMIDLIST *ppidlLast)
{
    HRESULT hr = 0;
    if (!pidl || !ppv) {
        return E_POINTER;
    }

    int nCount = GetPIDLCount (pidl);
    if (nCount == 0) { // desktop pidl of invalid pidl
        return E_POINTER;
    }

    IShellFolder * psfDesktop = nullptr;
    SHGetDesktopFolder (&psfDesktop);
    if (nCount == 1) { // desktop pidl
        if ((hr = psfDesktop->QueryInterface(riid, ppv)) == S_OK) {
            if (ppidlLast) {
                *ppidlLast = CopyPIDL (pidl);
            }
        }
        psfDesktop->Release ();
        return hr;
    }

    LPBYTE pRel = GetPIDLPos (pidl, nCount - 1);
    LPITEMIDLIST pidlParent = nullptr;
    pidlParent = CopyPIDL (pidl, (int)(pRel - (LPBYTE) pidl));
    IShellFolder * psfFolder = nullptr;

    if ((hr = psfDesktop->BindToObject (pidlParent, nullptr, __uuidof (psfFolder), (void **) &psfFolder)) != S_OK) {
        free (pidlParent);
        psfDesktop->Release ();
        return hr;
    }
    if ((hr = psfFolder->QueryInterface (riid, ppv)) == S_OK) {
        if (ppidlLast) {
            *ppidlLast = CopyPIDL ((LPCITEMIDLIST) pRel);
        }
    }
    free (pidlParent);
    psfFolder->Release ();
    psfDesktop->Release ();
    return hr;
}


LPBYTE ContextMenu::GetPIDLPos(LPCITEMIDLIST pidl, int nPos)
{
    if (!pidl) {
        return 0;
    }
    int nCount = 0;

    BYTE * pCur = (BYTE *) pidl;
    while (((LPCITEMIDLIST) pCur)->mkid.cb) {
        if (nCount == nPos) {
            return pCur;
        }
        nCount++;
        pCur += ((LPCITEMIDLIST) pCur)->mkid.cb; // + sizeof(pidl->mkid.cb);
    }
    if (nCount == nPos) {
        return pCur;
    }
    return nullptr;
}


int ContextMenu::GetPIDLCount (LPCITEMIDLIST pidl)
{
    if (!pidl) {
        return 0;
    }

    int nCount = 0;
    BYTE*  pCur = (BYTE *) pidl;
    while (((LPCITEMIDLIST) pCur)->mkid.cb) {
        nCount++;
        pCur += ((LPCITEMIDLIST) pCur)->mkid.cb;
    }
    return nCount;
}


/*********************************************************************************************
 * Notepad specific functions
 */
void ContextMenu::Rename()
{
    NewDlg  dlg;
    WCHAR   newFirstElement[MAX_PATH];
    WCHAR   szNewName[MAX_PATH];
    WCHAR   szComment[] = L"Rename";

    /* copy current element information */
    wcscpy(newFirstElement, _strFirstElement.c_str());

    /* when it is folder, remove the last backslash */
    if (newFirstElement[wcslen(newFirstElement) - 1] == '\\') {
        newFirstElement[wcslen(newFirstElement) - 1] = 0;
    }

    /* init field to current selected item */
    wcscpy(szNewName, &_tcsrchr(newFirstElement, '\\')[1]);

    (_tcsrchr(newFirstElement, '\\')[1]) = 0;

    dlg.init(_hInst, _hWndNpp);
    if (dlg.doDialog(szNewName, szComment) == TRUE) {
        wcscat(newFirstElement, szNewName);
        ::MoveFile(_strFirstElement.c_str(), newFirstElement);
    }
}

void ContextMenu::quickOpen()
{
    auto path = _strArray[0];

    // remove file name
    if (path.at(path.size() - 1) != '\\') {
        SIZE_T pos = path.rfind(L"\\", path.size() - 1);
        if (std::wstring::npos != pos) {
            path.erase(pos, path.size());
        }
    }

    extern QuickOpenDlg quickOpenDlg;
    quickOpenDlg.setRootPath(path);
    quickOpenDlg.show();
}

void ContextMenu::newFile()
{
    NewDlg  dlg;
    BOOL    bLeave = FALSE;
    WCHAR   szFileName[MAX_PATH];
    WCHAR   szComment[] = L"New file";

    szFileName[0] = '\0';

    dlg.init(_hInst, _hWndNpp);
    while (bLeave == FALSE) {
        if (dlg.doDialog(szFileName, szComment) == TRUE) {
            /* test if is correct */
            if (IsValidFileName(szFileName)) {
                std::filesystem::path newFilePath = _strFirstElement;
                if (std::filesystem::is_regular_file(newFilePath)) {
                    newFilePath = newFilePath.parent_path();
                }
                newFilePath /= szFileName;

                ::CloseHandle(::CreateFile(newFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                ::SendMessage(_hWndNpp, NPPM_DOOPEN, 0, (LPARAM)newFilePath.c_str());
                bLeave = TRUE;
            }
        }
        else {
            bLeave = TRUE;
        }
    }
}

void ContextMenu::newFolder()
{
    NewDlg  dlg;
    BOOL    bLeave = FALSE;
    WCHAR   szFolderName[MAX_PATH];
    WCHAR   szComment[MAX_PATH] = L"New folder";

    szFolderName[0] = '\0';

    dlg.init(_hInst, _hWndNpp);
    while (bLeave == FALSE) {
        if (dlg.doDialog(szFolderName, szComment) == TRUE) {
            /* test if is correct */
            if (IsValidFileName(szFolderName)) {
                std::filesystem::path newFolderPath = _strFirstElement;
                if (std::filesystem::is_regular_file(newFolderPath)) {
                    newFolderPath = newFolderPath.parent_path();
                }
                newFolderPath /= szFolderName;

                if (::CreateDirectory(newFolderPath.c_str(), nullptr) == FALSE) {
                    ::MessageBox(_hWndNpp, L"Folder couldn't be created.", L"Error", MB_OK);
                }
                bLeave = TRUE;
            }
        }
        else {
            bLeave = TRUE;
        }
    }
}

void ContextMenu::findInFiles()
{
    ::SendMessage(_hWndNpp, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)_strFirstElement.c_str(), NULL);
}

void ContextMenu::openFile()
{
    for (const auto &path : _strArray) {
        ::SendMessage(_hWndNpp, NPPM_DOOPEN, 0, (LPARAM)path.c_str());
    }
}

void ContextMenu::openFileInOtherView()
{
    BOOL isFirstItem = TRUE;
    for (const auto &path : _strArray) {
        ::SendMessage(_hWndNpp, NPPM_DOOPEN, 0, (LPARAM)path.c_str());
        if (isFirstItem) {
            ::SendMessage(_hWndNpp, WM_COMMAND, IDM_VIEW_GOTO_ANOTHER_VIEW, 0);
            isFirstItem = FALSE;
        }
    }
}

void ContextMenu::openFileInNewInstance()
{
    std::wstring    args2Exec;
    WCHAR           szNpp[MAX_PATH];

    // get notepad++.exe path
    ::GetModuleFileName(nullptr, szNpp, _countof(szNpp));

    for (UINT i = 0; i < _strArray.size(); i++) {
        if (i == 0) {
            args2Exec = L"-multiInst \"" + _strArray[i] + L"\"";
        } else {
            args2Exec += L" \"" + _strArray[i] + L"\"";
        }
    }
    ::ShellExecute(_hWndNpp, L"open", szNpp, args2Exec.c_str(), L".", SW_SHOW);
}

void ContextMenu::openPrompt()
{
    for (auto &path : _strArray) {
        /* is file */
        if (path.at(path.size() - 1) != '\\') {
            SIZE_T pos = path.rfind(L'\\', path.size() - 1);
            if (std::wstring::npos != pos) {
                path.erase(pos, path.size());
            }
        }
        ::ShellExecute(_hWndNpp, L"open", exProp.cphProgram.szAppName, nullptr, path.c_str(), SW_SHOW);
    }
}

void ContextMenu::setRootFolder()
{
    auto path = _strArray[0];

    // remove file name
    if (path.at(path.size() - 1) != '\\') {
        SIZE_T pos = path.rfind(L"\\", path.size() - 1);
        if (std::wstring::npos != pos) {
            path.erase(pos, path.size());
        }
    }

    exProp.rootFolder = path;
}

void ContextMenu::gotoRootFolder()
{
    extern ExplorerDialog explorerDlg;
    explorerDlg.gotoFileLocation(exProp.rootFolder);
}

void ContextMenu::clearRootFolder()
{
    exProp.rootFolder.clear();
}


void ContextMenu::addToFaves()
{
    extern FavesDialog favesDlg;

    /* test if only one file is selected */
    if (_strArray.size() > 1) {
        const BOOL isFolder = ('\\' == _strArray[0].back());
        for (auto&& path : _strArray) {
            if (isFolder != ('\\' == path.back())) {
                ::MessageBox(_hWndNpp, L"Files and folders cannot be added at the same time!", L"Error", MB_OK);
                return;
            }
        }
        favesDlg.AddToFavorties(isFolder, std::move(_strArray));
    }
    else {
        BOOL isFolder = ('\\' == _strArray[0].back());
        favesDlg.AddToFavorties(isFolder, _strArray[0].data());
    }
}

void ContextMenu::addRelativePathsCB()
{
    const std::wstring currentDirectory = NppInterface::getCurrentDirectory();
    if (currentDirectory.empty()) {
        return;
    }

    WCHAR           relativePath[MAX_PATH];
    std::wstring    relativePaths;
    BOOL            isFirstItem = TRUE;
    for (auto &&path : _strArray) {
        if (isFirstItem) {
            isFirstItem = FALSE;
        }
        else {
            relativePaths += L"\n";
        }
        ::PathRelativePathTo(relativePath, currentDirectory.c_str(), FILE_ATTRIBUTE_DIRECTORY, path.c_str(), FILE_ATTRIBUTE_NORMAL);
        relativePaths += relativePath;
    }
    Str2CB(relativePaths.c_str());
}

void ContextMenu::addFullPathsCB()
{
    std::wstring temp;
    BOOL isFirstItem = TRUE;
    for (auto &&path : _strArray) {
        if (isFirstItem) {
            isFirstItem = FALSE;
        }
        else {
            temp += L"\n";
        }
        temp += path;
    }
    Str2CB(temp.c_str());
}

void ContextMenu::addFileNamesCB()
{
    std::wstring temp;
    BOOL isFirstItem = TRUE;
    for (auto &&path : _strArray) {
        SIZE_T pos = path.rfind(L'\\', path.size() - 1);
        if (std::wstring::npos != pos) {
            if (isFirstItem) {
                isFirstItem = FALSE;
            }
            else {
                temp += L"\n";
            }

            /* is folder */
            if (path.at(path.size() - 1) == '\\') {
                pos = path.rfind(L'\\', pos - 1);
                if (std::wstring::npos != pos) {
                    path.erase(0, pos);
                    path.erase(path.size() - 1);
                }
            }
            else {
                path.erase(0, pos + 1);
            }
            temp += path;
        }
    }
    Str2CB(temp.c_str());
}

void ContextMenu::openScriptPath(HMODULE hInst)
{
    WCHAR TEMP[MAX_PATH];

    if (exProp.nppExecProp.szScriptPath[0] == '.') {
        /* module path of notepad */
        GetModuleFileName(hInst, TEMP, _countof(TEMP));
        PathRemoveFileSpec(TEMP);
        PathAppend(TEMP, exProp.nppExecProp.szScriptPath);
    }
    else {
        wcscpy(TEMP, exProp.nppExecProp.szScriptPath);
    }
    ::SendMessage(_hWndParent, EXM_OPENDIR, 0, (LPARAM)TEMP);
}

void ContextMenu::startNppExec(HMODULE hInst, UINT cmdID)
{
    WCHAR szScriptPath[MAX_PATH];

    /* concatinate execute command */
    if (exProp.nppExecProp.szScriptPath[0] == '.') {
        /* module path of notepad */
        GetModuleFileName(hInst, szScriptPath, _countof(szScriptPath));
        PathRemoveFileSpec(szScriptPath);
        PathAppend(szScriptPath, exProp.nppExecProp.szScriptPath);
    }
    else {
        wcscpy(szScriptPath, exProp.nppExecProp.szScriptPath);
    }
    if (szScriptPath[wcslen(szScriptPath) - 1] != '\\') {
        wcscat(szScriptPath, L"\\");
    }
    wcscat(szScriptPath, _strNppScripts[cmdID].c_str());

    /* get arguments and convert */
    HANDLE hFile = ::CreateFile(szScriptPath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwSize = ::GetFileSize(hFile, nullptr);

        if (dwSize != -1) {
            WCHAR   szAppName[MAX_PATH];
            DWORD   hasRead = 0;
            LPTSTR  pszPtr  = nullptr;
            LPTSTR  pszArg  = nullptr;
            LPTSTR  pszData = (LPTSTR)new WCHAR[dwSize+1];

            if (pszData != nullptr) {
                /* read data from file */
                ::ReadFile(hFile, pszData, dwSize, &hasRead, nullptr);

                WCHAR   szBOM       = 0xFEFF;
                LPTSTR  pszData2    = nullptr;

                if (pszData[0] == szBOM) {
                    pszPtr = _tcstok(&pszData[1], L"\n");
                } else if (pszData[0] == '/') {
                    pszPtr = _tcstok(pszData, L"\n");
                } else if (((LPSTR)pszData)[0] == '/') {
                    pszData2 = new WCHAR[dwSize * 2];
                    ::MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszData, -1, pszData2, dwSize * 2);
                    pszPtr = _tcstok(pszData2, L"\n");
                } else {
                    ::MessageBox(_hWndNpp, L"Wrong file format", L"Error", MB_OK | MB_ICONERROR);
                    delete [] pszData;
                    return; /* ============= Leave Function ================== */
                }

                if (ConvertCall(pszPtr, szAppName, &pszArg, _strArray) == TRUE) {
                    WCHAR szPath[MAX_PATH];
                    ::GetModuleFileName((HMODULE)_hInst, szPath, _countof(szPath));

                    NpeNppExecParam npep {
                        .szScriptName       = szScriptPath,
                        .szScriptArguments  = pszArg,
                        .dwResult           = 1,
                    };
                    /* get version information */
                    CommunicationInfo ci {
                        .internalMsg    = NPEM_NPPEXEC,
                        .srcModuleName  = PathFindFileName(szPath),
                        .info           = &npep,
                    };
                    ::SendMessage(_hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)szAppName, (LPARAM)&ci);

                    if (npep.dwResult != NPE_NPPEXEC_OK) {
                        ::MessageBox(_hWndNpp, L"NppExec currently in use!", L"Error", MB_OK);
                    }

                    delete [] pszArg;
                }
                delete [] pszData;
                delete [] pszData2;
            }
        }

        ::CloseHandle(hFile);
    }
}

/******************************************************************************************
 * Sets a string to clipboard
 */
bool ContextMenu::Str2CB(LPCTSTR str2cpy)
{
    if (!str2cpy) {
        return false;
    }

    if (!::OpenClipboard(_hWndNpp)) {
        return false;
    }

    ::EmptyClipboard();

    HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, wcslen(str2cpy) * 2 + 2);

    if (hglbCopy == nullptr) {
        ::CloseClipboard();
        return false;
    }

    // Lock the handle and copy the text to the buffer.
    LPTSTR pStr = (LPTSTR)::GlobalLock(hglbCopy);
    if (pStr) {
    wcscpy(pStr, str2cpy);
    ::GlobalUnlock(hglbCopy);
    }

    // Place the handle on the clipboard.
    ::SetClipboardData(CF_UNICODETEXT, hglbCopy);
    ::CloseClipboard();
    return true;
}

