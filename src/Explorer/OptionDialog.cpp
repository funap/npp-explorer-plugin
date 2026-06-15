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


#include "OptionDialog.h"

#include <commctrl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <Shlwapi.h>
#include <array>

#include "Explorer.h"

namespace {

constexpr std::wstring_view EXAMPLE_SCRIPT = 
    L"//Explorer: NppExec.dll EXP_FULL_PATH[0]\r\n"
    L"// ------------------------------------------------------------------\r\n"
    L"// NOTE: The first line is in every script necessary\r\n"
    L"// Format of the first line:\r\n"
    L"//   //Explorer:          = Identification for Explorer support\r\n"
    L"//   NppExec.dll          = NppExec DLL identification\r\n"
    L"//   EXP_FULL_PATH[0] ... = Exec arguments - [0]=First selected file\r\n"
    L"// ------------------------------------------------------------------\r\n"
    L"// Example for selected files in file list of Explorer:\r\n"
    L"// - C:\\Folder1\\Folder2\\Filename1.Ext\r\n"
    L"// - C:\\Folder1\\Folder2\\Filename2.Ext\r\n"
    L"// ------------------------------------------------------------------\r\n"
    L"// EXP_FULL_PATH[1]       = C:\\Folder1\\Folder2\\Filename2.Ext\r\n"
    L"// EXP_ROOT_PATH[0]       = C:\r\n"
    L"// EXP_PARENT_FULL_DIR[0] = C:\\Folder1\\Folder2\r\n"
    L"// EXP_PARENT_DIR[0]      = Folder2\r\n"
    L"// EXP_FULL_FILE[1]       = Filename2.Ext\r\n"
    L"// EXP_FILE_NAME[0]       = Filename1\r\n"
    L"// EXP_FILE_EXT[0]        = Ext\r\n"
    L"\r\n"
    L"// NppExec script body:\r\n"
    L"cd $(ARGV[1])";


constexpr std::array BYTE_UNIT_STRINGS = {
    L"Bytes",
    L"kBytes",
    L"Dynamic x b/k/M",
    L"Dynamic x,x b/k/M",
};

constexpr std::array DATE_FORMAT_STRINGS = {
    L"Y/M/D HH:MM",
    L"D.M.Y HH:MM",
};


// Set a call back with the handle after init to set the path.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/callbackfunctions/browsecallbackproc.asp
int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*unused*/, LPARAM pData)
{
    if (uMsg == BFFM_INITIALIZED) {
        ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
    }
    return 0;
};
} // namespace

OptionDlg::OptionDlg()
    : StaticDialog()
    , _logfont()
    , _pProp(nullptr)
{
}

OptionDlg::~OptionDlg()
{
}

INT_PTR OptionDlg::doDialog(Settings *prop)
{
    _pProp = prop;
    return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_OPTION_DLG), _hParent,  (DLGPROC)dlgProc, (LPARAM)this);
}


INT_PTR CALLBACK OptionDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_INITDIALOG:
    {
        goToCenter();

        // Initialize Tab Control
        HWND hTab = ::GetDlgItem(_hSelf, IDC_TAB_OPTION);
        TCITEM tie{};
        tie.mask = TCIF_TEXT;
        
        std::wstring tabGeneral = L"General";
        tie.pszText = tabGeneral.data();
        ::SendMessage(hTab, TCM_INSERTITEM, 0, (LPARAM)&tie);

        std::wstring tabWorkspace = L"Workspace Folders";
        tie.pszText = tabWorkspace.data();
        ::SendMessage(hTab, TCM_INSERTITEM, 1, (LPARAM)&tie);

        std::wstring tabTools = L"Tools";
        tie.pszText = tabTools.data();
        ::SendMessage(hTab, TCM_INSERTITEM, 2, (LPARAM)&tie);

        for (const auto& byteUnit : BYTE_UNIT_STRINGS) {
            ::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_ADDSTRING, 0, (LPARAM)byteUnit);
        }
        for (const auto& dateFormat : DATE_FORMAT_STRINGS) {
            ::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_ADDSTRING, 0, (LPARAM)dateFormat);
        }

        SetParams();
        LongUpdate();
        ShowTab(0);

        break;
    }
    case WM_NOTIFY:
    {
        LPNMHDR pnmhdr = (LPNMHDR)lParam;
        if (pnmhdr->idFrom == IDC_TAB_OPTION && pnmhdr->code == TCN_SELCHANGE) {
            int activeTab = (int)::SendMessage(pnmhdr->hwndFrom, TCM_GETCURSEL, 0, 0);
            ShowTab(activeTab);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND :
        switch (LOWORD(wParam)) {
            case IDC_CHECK_LONG:
                LongUpdate();
                return TRUE;
            case IDC_BTN_ADD_WORKSPACE: {
                LPMALLOC pShellMalloc = 0;
                if (::SHGetMalloc(&pShellMalloc) == NO_ERROR) {
                    BROWSEINFO info {
                        .hwndOwner      = _hSelf,
                        .pidlRoot       = nullptr,
                        .pszDisplayName = (LPTSTR)new WCHAR[MAX_PATH],
                        .lpszTitle      = L"Select a root folder or enter a network path:",
                        .ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX,
                        .lpfn           = BrowseCallbackProc,
                        .lParam         = (LPARAM)L"",
                    };
                    PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolder(&info);
                    if (pidl) {
                        WCHAR szPath[MAX_PATH];
                        if (::SHGetPathFromIDList(pidl, szPath)) {
                            bool exists = false;
                            for (const auto& path : _tempWorkspaceFolders) {
                                if (_wcsicmp(path.c_str(), szPath) == 0) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) {
                                _tempWorkspaceFolders.push_back(szPath);
                                ::SendDlgItemMessage(_hSelf, IDC_LIST_WORKSPACE_DIRS, LB_ADDSTRING, 0, (LPARAM)szPath);
                            }
                        }
                        pShellMalloc->Free(pidl);
                    }
                    pShellMalloc->Release();
                    delete [] info.pszDisplayName;
                }
                break;
            }
            case IDC_BTN_DEL_WORKSPACE: {
                HWND hList = ::GetDlgItem(_hSelf, IDC_LIST_WORKSPACE_DIRS);
                int sel = (int)::SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    ::SendMessage(hList, LB_DELETESTRING, (WPARAM)sel, 0);
                    if (sel < (int)_tempWorkspaceFolders.size()) {
                        _tempWorkspaceFolders.erase(_tempWorkspaceFolders.begin() + sel);
                    }
                }
                break;
            }
            case IDC_BTN_UP_WORKSPACE: {
                HWND hList = ::GetDlgItem(_hSelf, IDC_LIST_WORKSPACE_DIRS);
                int sel = (int)::SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR && sel > 0) {
                    std::swap(_tempWorkspaceFolders[sel], _tempWorkspaceFolders[sel - 1]);
                    
                    WCHAR szText[MAX_PATH];
                    ::SendMessage(hList, LB_GETTEXT, sel, (LPARAM)szText);
                    ::SendMessage(hList, LB_DELETESTRING, sel, 0);
                    ::SendMessage(hList, LB_INSERTSTRING, sel - 1, (LPARAM)szText);
                    ::SendMessage(hList, LB_SETCURSEL, sel - 1, 0);
                }
                break;
            }
            case IDC_BTN_DOWN_WORKSPACE: {
                HWND hList = ::GetDlgItem(_hSelf, IDC_LIST_WORKSPACE_DIRS);
                int sel = (int)::SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR && sel < (int)_tempWorkspaceFolders.size() - 1) {
                    std::swap(_tempWorkspaceFolders[sel], _tempWorkspaceFolders[sel + 1]);
                    
                    WCHAR szText[MAX_PATH];
                    ::SendMessage(hList, LB_GETTEXT, sel, (LPARAM)szText);
                    ::SendMessage(hList, LB_DELETESTRING, sel, 0);
                    ::SendMessage(hList, LB_INSERTSTRING, sel + 1, (LPARAM)szText);
                    ::SendMessage(hList, LB_SETCURSEL, sel + 1, 0);
                }
                break;
            }
            case IDC_BTN_OPENDLG: {
                // This code was copied and slightly modifed from:
                // http://www.bcbdev.com/faqs/faq62.htm

                // SHBrowseForFolder returns a PIDL. The memory for the PIDL is
                // allocated by the shell. Eventually, we will need to free this
                // memory, so we need to get a pointer to the shell malloc COM
                // object that will free the PIDL later on.
                LPMALLOC pShellMalloc = 0;
                if (::SHGetMalloc(&pShellMalloc) == NO_ERROR) {
                    // If we were able to get the shell malloc object,
                    // then proceed by initializing the BROWSEINFO stuct
                    BROWSEINFO info {
                        .hwndOwner      = _hParent,
                        .pidlRoot       = nullptr,
                        .pszDisplayName = (LPTSTR)new WCHAR[MAX_PATH],
                        .lpszTitle      = L"Select a folder:",
                        .ulFlags        = BIF_RETURNONLYFSDIRS,
                        .lpfn           = BrowseCallbackProc,
                        .lParam         = (LPARAM)_pProp->GetNppExecProp().szScriptPath.c_str(),
                    };
                    // Execute the browsing dialog.
                    PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolder(&info);

                    // pidl will be null if they cancel the browse dialog.
                    // pidl will be not null when they select a folder.
                    if (pidl) {
                        // Try to convert the pidl to a display string.
                        // Return is true if success.
                        WCHAR szPath[MAX_PATH];
                        if (::SHGetPathFromIDList(pidl, szPath)) {
                            // Set edit control to the directory path.
                            _pProp->GetNppExecProp().szScriptPath = szPath;
                            ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_SCRIPTPATH), szPath);
                        }
                        pShellMalloc->Free(pidl);
                    }
                    pShellMalloc->Release();
                    delete [] info.pszDisplayName;
                }
                break;
            }
            case IDC_BTN_EXAMPLE_FILE: {
                BYTE    szBOM[]         = {0xFF, 0xFE};
                DWORD   dwByteWritten   = 0;
                WCHAR   szExampleScriptPath[MAX_PATH];

                if (_pProp->GetNppExecProp().szScriptPath[0] == '.') {
                    /* module path of notepad */
                    ::GetModuleFileName(_hInst, szExampleScriptPath, _countof(szExampleScriptPath));
                    PathRemoveFileSpec(szExampleScriptPath);
                    PathAppend(szExampleScriptPath, _pProp->GetNppExecProp().szScriptPath.c_str());
                } else {
                    wcscpy(szExampleScriptPath, _pProp->GetNppExecProp().szScriptPath.c_str());
                }
                ::PathAppend(szExampleScriptPath, L"Goto path.exec");

                HANDLE hFile = ::CreateFile(szExampleScriptPath,
                    GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

                ::WriteFile(hFile, szBOM, sizeof(szBOM), &dwByteWritten, nullptr);
                ::WriteFile(hFile, EXAMPLE_SCRIPT.data(), static_cast<DWORD>(EXAMPLE_SCRIPT.size() * sizeof(wchar_t)), &dwByteWritten, nullptr);

                ::CloseHandle(hFile);
                break;
            }
            case IDC_BTN_CHOOSEFONT: {
                CHOOSEFONT cf {
                    .lStructSize    = sizeof(CHOOSEFONT),
                    .hwndOwner      = _hSelf,
                    .lpLogFont      = &_logfont,
                    .Flags          = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_NOVERTFONTS | CF_NOSCRIPTSEL,
                };

                ChooseFont(&cf);
                ::SetDlgItemText(_hSelf, IDC_BTN_CHOOSEFONT, _logfont.lfFaceName);
                break;
            }
            case IDCANCEL:
                ::EndDialog(_hSelf, IDCANCEL);
                return TRUE;
            case IDOK:
                if (GetParams() == FALSE) {
                    return FALSE;
                }
                ::EndDialog(_hSelf, IDOK);
                return TRUE;
            default:
                return FALSE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}


void OptionDlg::LongUpdate()
{
    BOOL bViewLong = FALSE;

    if (::SendDlgItemMessage(_hSelf, IDC_CHECK_LONG, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        bViewLong = TRUE;
    }

    ::EnableWindow(::GetDlgItem(_hSelf, IDC_COMBO_SIZE_FORMAT), bViewLong);
    ::EnableWindow(::GetDlgItem(_hSelf, IDC_COMBO_DATE_FORMAT), bViewLong);
}


void OptionDlg::SetParams()
{
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_LONG,        BM_SETCHECK, _pProp->IsViewLong()      ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_SETCURSEL, (WPARAM)_pProp->GetFmtSize(), 0);
    ::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_SETCURSEL, (WPARAM)_pProp->GetFmtDate(), 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_SEPEXT,      BM_SETCHECK, _pProp->IsAddExtToName()  ? BST_UNCHECKED : BST_CHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_BRACES,      BM_SETCHECK, _pProp->IsViewBraces()    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTO,        BM_SETCHECK, _pProp->IsAutoUpdate()    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDDEN,      BM_SETCHECK, _pProp->IsShowHidden()    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_USEICON,     BM_SETCHECK, _pProp->IsUseSystemIcons()? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFLUENTICONS, BM_SETCHECK, _pProp->IsUseFluentIcons()? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTONAV,     BM_SETCHECK, _pProp->IsAutoNavigate()  ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFULLTREE, BM_SETCHECK, _pProp->IsUseFullTree()    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDE_FOLDERS, BM_SETCHECK, _pProp->IsHideFoldersInFileList() ? BST_CHECKED : BST_UNCHECKED, 0);


    ::SetDlgItemText(_hSelf, IDC_EDIT_EXECNAME,     _pProp->GetNppExecProp().szAppName.c_str());
    ::SetDlgItemText(_hSelf, IDC_EDIT_SCRIPTPATH,   _pProp->GetNppExecProp().szScriptPath.c_str());
    ::SetDlgItemText(_hSelf, IDC_EDIT_HISTORYSIZE,  std::to_wstring(_pProp->GetMaxHistorySize()).c_str());
    ::SetDlgItemText(_hSelf, IDC_EDIT_CPH,          _pProp->GetCphProgram().szAppName.c_str());

    _tempWorkspaceFolders = _pProp->GetWorkspaceFolders();
    HWND hList = ::GetDlgItem(_hSelf, IDC_LIST_WORKSPACE_DIRS);
    ::SendMessage(hList, LB_RESETCONTENT, 0, 0);
    for (const auto& path : _tempWorkspaceFolders) {
        ::SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)path.c_str());
    }

    _logfont = _pProp->GetLogFont();
    ::SetDlgItemText(_hSelf, IDC_BTN_CHOOSEFONT,    _logfont.lfFaceName);
}


BOOL OptionDlg::GetParams()
{
    BOOL bRet = TRUE;

    _pProp->SetViewBraces((::SendDlgItemMessage(_hSelf, IDC_CHECK_BRACES, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetAddExtToName((::SendDlgItemMessage(_hSelf, IDC_CHECK_SEPEXT, BM_GETCHECK, 0, 0) == BST_CHECKED) ? false : true);
    _pProp->SetViewLong((::SendDlgItemMessage(_hSelf, IDC_CHECK_LONG, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetFmtSize((SizeFmt)::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_GETCURSEL, 0, 0));
    _pProp->SetFmtDate((DateFmt)::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_GETCURSEL, 0, 0));
    _pProp->SetAutoUpdate((::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTO, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetShowHidden((::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDDEN, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetAutoNavigate((::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTONAV, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetUseSystemIcons((::SendDlgItemMessage(_hSelf, IDC_CHECK_USEICON, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetUseFluentIcons((::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFLUENTICONS, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetUseFullTree((::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFULLTREE, BM_GETCHECK, 0, 0) == BST_CHECKED));
    _pProp->SetHideFoldersInFileList((::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDE_FOLDERS, BM_GETCHECK, 0, 0) == BST_CHECKED));

    WCHAR TEMP[MAX_PATH];
    _pProp->SetTimeout((UINT)_wtoi(TEMP));

    ::GetDlgItemText(_hSelf, IDC_EDIT_HISTORYSIZE, TEMP, 6);
    _pProp->SetMaxHistorySize((UINT)_wtoi(TEMP));

    ::GetDlgItemText(_hSelf, IDC_EDIT_EXECNAME, TEMP, MAX_PATH);
    _pProp->GetNppExecProp().szAppName = TEMP;

    ::GetDlgItemText(_hSelf, IDC_EDIT_SCRIPTPATH, TEMP, MAX_PATH);
    _pProp->GetNppExecProp().szScriptPath = TEMP;

    ::GetDlgItemText(_hSelf, IDC_EDIT_CPH, TEMP, MAX_PATH);
    _pProp->GetCphProgram().szAppName = TEMP;

    _pProp->SetLogFont(_logfont);

    _pProp->SetWorkspaceFolders(_tempWorkspaceFolders);

    return bRet;
}

void OptionDlg::ShowTab(int activeTab)
{
    std::vector<int> tabGeneralCtrls = {
        IDC_STATIC_FILELIST, IDC_CHECK_BRACES, IDC_CHECK_SEPEXT, IDC_CHECK_HIDE_FOLDERS,
        IDC_STATIC_LONG, IDC_CHECK_LONG, IDC_STATIC_SIZE, IDC_STATIC_DATE,
        IDC_COMBO_SIZE_FORMAT, IDC_COMBO_DATE_FORMAT,
        IDC_STATIC_GENOPT, IDC_CHECK_AUTO, IDC_CHECK_HIDDEN, IDC_CHECK_USEICON,
        IDC_CHECK_USEFLUENTICONS,
        IDC_CHECK_AUTONAV, IDC_CHECK_USEFULLTREE, IDC_STATIC_HISTORY,
        IDC_EDIT_HISTORYSIZE, IDC_BTN_CHOOSEFONT
    };
    
    std::vector<int> tabWorkspaceCtrls = {
        IDC_STATIC_WORKSPACE_DIRS, IDC_LIST_WORKSPACE_DIRS, IDC_BTN_ADD_WORKSPACE, IDC_BTN_DEL_WORKSPACE,
        IDC_BTN_UP_WORKSPACE, IDC_BTN_DOWN_WORKSPACE
    };
    
    std::vector<int> tabToolsCtrls = {
        IDC_STATIC_NPPEXEC, IDC_EDIT_EXECNAME, IDC_EDIT_SCRIPTPATH, IDC_STATIC_EXECNAME,
        IDC_STATIC_SCRIPTPATH, IDC_BTN_OPENDLG, IDC_BTN_EXAMPLE_FILE,
        IDC_STATIC_COMMANDPROMPT, IDC_EDIT_CPH, IDC_STATIC_CPHNAME
    };
    
    for (int id : tabGeneralCtrls) {
        ::ShowWindow(::GetDlgItem(_hSelf, id), (activeTab == 0) ? SW_SHOW : SW_HIDE);
    }
    for (int id : tabWorkspaceCtrls) {
        ::ShowWindow(::GetDlgItem(_hSelf, id), (activeTab == 1) ? SW_SHOW : SW_HIDE);
    }
    for (int id : tabToolsCtrls) {
        ::ShowWindow(::GetDlgItem(_hSelf, id), (activeTab == 2) ? SW_SHOW : SW_HIDE);
    }
}
