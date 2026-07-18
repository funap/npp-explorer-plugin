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

#include "NewDlg.h"

#include "ExplorerResource.h"
#include "../NppPlugin/Notepad_plus_msgs.h"

INT_PTR NewDlg::doDialog(std::wstring* pFileName, const std::wstring& desc)
{
    _pFileName = pFileName;
    _desc = desc;
    return ::DialogBoxParam(_hInst, MAKEINTRESOURCE(IDD_NEW_DLG), _hParent,  (DLGPROC)dlgProc, (LPARAM)this);
}


INT_PTR CALLBACK NewDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_INITDIALOG: {
        if (!_wndName.empty()) {
            ::SetWindowText(_hSelf, _wndName.c_str());
        }

        std::wstring szDesc = _desc + L":";
        ::SetWindowText(::GetDlgItem(_hSelf, IDC_STATIC_NEW_DESC), szDesc.c_str());

        if (_pFileName) {
            ::SetWindowText(::GetDlgItem(_hSelf, IDC_EDIT_NEW), _pFileName->c_str());
        }

        goToCenter();
        SetFocus(::GetDlgItem(_hSelf, IDC_EDIT_NEW));
        break;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
            case IDCANCEL:
                ::EndDialog(_hSelf, FALSE);
                return TRUE;
            case IDOK: {
                UINT length = (UINT)::SendDlgItemMessage(_hSelf, IDC_EDIT_NEW, WM_GETTEXTLENGTH, 0, 0) + 1;
                std::wstring textBuf(length, L'\0');
                SendDlgItemMessage(_hSelf, IDC_EDIT_NEW, WM_GETTEXT, length, (LPARAM)textBuf.data());
                textBuf.resize(std::wcslen(textBuf.c_str()));
                if (_pFileName) {
                    *_pFileName = textBuf;
                }
                ::EndDialog(_hSelf, TRUE);
                return TRUE;
            }
            default:
                break;
        }
        break;
    }
    case WM_DESTROY :
        /* deregister this dialog */
        ::SendMessage(_hParent, NPPM_MODELESSDIALOG, MODELESSDIALOGREMOVE, (LPARAM)_hSelf);
        break;
    default:
        break;
    }
    return FALSE;
}



