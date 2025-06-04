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

INT_PTR OptionDlg::doDialog(ExProp *prop)
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

        for (const auto& byteUnit : BYTE_UNIT_STRINGS) {
            ::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_ADDSTRING, 0, (LPARAM)byteUnit);
        }
        for (const auto& dateFormat : DATE_FORMAT_STRINGS) {
            ::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_ADDSTRING, 0, (LPARAM)dateFormat);
        }
        ::SendDlgItemMessage(_hSelf, IDC_EDIT_TIMEOUT, EM_LIMITTEXT, 5, 0);

        SetParams();
        LongUpdate();

        break;
    }
    case WM_COMMAND :
        switch (LOWORD(wParam)) {
            case IDC_CHECK_LONG:
                LongUpdate();
                return TRUE;
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
                        .lParam         = (LPARAM)_pProp->nppExecProp.szScriptPath,
                    };
                    // Execute the browsing dialog.
                    PIDLIST_ABSOLUTE pidl = ::SHBrowseForFolder(&info);

                    // pidl will be null if they cancel the browse dialog.
                    // pidl will be not null when they select a folder.
                    if (pidl) {
                        // Try to convert the pidl to a display string.
                        // Return is true if success.
                        if (::SHGetPathFromIDList(pidl, _pProp->nppExecProp.szScriptPath)) {
                            // Set edit control to the directory path.
                            ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_SCRIPTPATH), _pProp->nppExecProp.szScriptPath);
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

                if (_pProp->nppExecProp.szScriptPath[0] == '.') {
                    /* module path of notepad */
                    ::GetModuleFileName(_hInst, szExampleScriptPath, _countof(szExampleScriptPath));
                    PathRemoveFileSpec(szExampleScriptPath);
                    PathAppend(szExampleScriptPath, _pProp->nppExecProp.szScriptPath);
                } else {
                    wcscpy(szExampleScriptPath, _pProp->nppExecProp.szScriptPath);
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
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_LONG,        BM_SETCHECK, _pProp->bViewLong      ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_SETCURSEL, (WPARAM)_pProp->fmtSize, 0);
    ::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_SETCURSEL, (WPARAM)_pProp->fmtDate, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_SEPEXT,      BM_SETCHECK, _pProp->bAddExtToName  ? BST_UNCHECKED : BST_CHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_BRACES,      BM_SETCHECK, _pProp->bViewBraces    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTO,        BM_SETCHECK, _pProp->bAutoUpdate    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDDEN,      BM_SETCHECK, _pProp->bShowHidden    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_USEICON,     BM_SETCHECK, _pProp->bUseSystemIcons? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTONAV,     BM_SETCHECK, _pProp->bAutoNavigate  ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFULLTREE, BM_SETCHECK, _pProp->useFullTree    ? BST_CHECKED : BST_UNCHECKED, 0);
    ::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDE_FOLDERS, BM_SETCHECK, _pProp->bHideFoldersInFileList ? BST_CHECKED : BST_UNCHECKED, 0);


    ::SetDlgItemText(_hSelf, IDC_EDIT_EXECNAME,     _pProp->nppExecProp.szAppName);
    ::SetDlgItemText(_hSelf, IDC_EDIT_SCRIPTPATH,   _pProp->nppExecProp.szScriptPath);
    ::SetDlgItemText(_hSelf, IDC_EDIT_TIMEOUT,      std::to_wstring(_pProp->uTimeout).c_str());
    ::SetDlgItemText(_hSelf, IDC_EDIT_HISTORYSIZE,  std::to_wstring(_pProp->maxHistorySize).c_str());
    ::SetDlgItemText(_hSelf, IDC_EDIT_CPH,          _pProp->cphProgram.szAppName);

    _logfont = _pProp->logfont;
    ::SetDlgItemText(_hSelf, IDC_BTN_CHOOSEFONT,    _logfont.lfFaceName);
}


BOOL OptionDlg::GetParams()
{
    BOOL bRet = TRUE;

    _pProp->bViewBraces     = (::SendDlgItemMessage(_hSelf, IDC_CHECK_BRACES, BM_GETCHECK, 0, 0) == BST_CHECKED)        ? TRUE : FALSE;
    _pProp->bAddExtToName   = (::SendDlgItemMessage(_hSelf, IDC_CHECK_SEPEXT, BM_GETCHECK, 0, 0) == BST_CHECKED)        ? FALSE : TRUE;
    _pProp->bViewLong       = (::SendDlgItemMessage(_hSelf, IDC_CHECK_LONG, BM_GETCHECK, 0, 0) == BST_CHECKED)          ? TRUE : FALSE;
    _pProp->fmtSize         = (SizeFmt)::SendDlgItemMessage(_hSelf, IDC_COMBO_SIZE_FORMAT, CB_GETCURSEL, 0, 0);
    _pProp->fmtDate         = (DateFmt)::SendDlgItemMessage(_hSelf, IDC_COMBO_DATE_FORMAT, CB_GETCURSEL, 0, 0);
    _pProp->bAutoUpdate     = (::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTO, BM_GETCHECK, 0, 0) == BST_CHECKED)          ? TRUE : FALSE;
    _pProp->bShowHidden     = (::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDDEN, BM_GETCHECK, 0, 0) == BST_CHECKED)        ? TRUE : FALSE;
    _pProp->bAutoNavigate   = (::SendDlgItemMessage(_hSelf, IDC_CHECK_AUTONAV, BM_GETCHECK, 0, 0) == BST_CHECKED)       ? TRUE : FALSE;
    _pProp->bUseSystemIcons = (::SendDlgItemMessage(_hSelf, IDC_CHECK_USEICON, BM_GETCHECK, 0, 0) == BST_CHECKED)       ? TRUE : FALSE;
    _pProp->useFullTree     = (::SendDlgItemMessage(_hSelf, IDC_CHECK_USEFULLTREE, BM_GETCHECK, 0, 0) == BST_CHECKED)   ? TRUE : FALSE;
    _pProp->bHideFoldersInFileList = (::SendDlgItemMessage(_hSelf, IDC_CHECK_HIDE_FOLDERS, BM_GETCHECK, 0, 0) == BST_CHECKED) ? TRUE : FALSE;

    WCHAR TEMP[MAX_PATH];
    ::GetDlgItemText(_hSelf, IDC_EDIT_TIMEOUT, TEMP, 6);
    _pProp->uTimeout = (UINT)_wtoi(TEMP);

    ::GetDlgItemText(_hSelf, IDC_EDIT_HISTORYSIZE, TEMP, 6);
    _pProp->maxHistorySize = (UINT)_wtoi(TEMP);

    ::GetDlgItemText(_hSelf, IDC_EDIT_EXECNAME, _pProp->nppExecProp.szAppName, MAX_PATH);
    ::GetDlgItemText(_hSelf, IDC_EDIT_SCRIPTPATH, _pProp->nppExecProp.szScriptPath, MAX_PATH);
    ::GetDlgItemText(_hSelf, IDC_EDIT_CPH, _pProp->cphProgram.szAppName, MAX_PATH);

    _pProp->logfont = _logfont;

    return bRet;
}
