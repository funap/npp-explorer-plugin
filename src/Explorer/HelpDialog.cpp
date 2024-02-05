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

#include "HelpDialog.h"

#include <string>

#include "Explorer.h"
#include "ExplorerResource.h"
#include "version.h"

void HelpDlg::doDialog()
{
    if (!isCreated()) {
        create(IDD_HELP_DLG);
    }

    goToCenter();
}


INT_PTR CALLBACK HelpDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message) {
    case WM_INITDIALOG:
        setVersionString();

        _emailLink.init(_hInst, _hSelf);
        _emailLink.create(::GetDlgItem(_hSelf, IDC_EMAIL_LINK), L"mailto:");

        _urlNppPlugins.init(_hInst, _hSelf);
        _urlNppPlugins.create(::GetDlgItem(_hSelf, IDC_NPP_PLUGINS_URL), L"https://github.com/funap/npp-explorer-plugin");

        return TRUE;
    case WM_COMMAND:
        switch (wParam) {
        case IDOK :
        case IDCANCEL :
            display(FALSE);
            return TRUE;
        default :
            break;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

void HelpDlg::setVersionString()
{
    HWND target = ::GetDlgItem(_hSelf, IDC_STATIC_VERSION);
    const int bufferLength = ::GetWindowTextLength(target) + 1;
    // Allocate string of proper size
    std::wstring text;
    text.resize(bufferLength);
    // Get the text of the specified control
    // Note that the address of the internal string buffer
    // can be obtained with the &text[0] syntax
    ::GetWindowText(target, &text[0], bufferLength);
    // Resize down the string to avoid bogus double-NUL-terminated strings
    text.resize(bufferLength - 1);

    text.append(L" v");
    text.append(std::to_wstring(VERSION_MAJOR));
    text.append(L".");
    text.append(std::to_wstring(VERSION_MINOR));
    text.append(L".");
    text.append(std::to_wstring(VERSION_REVISION));
    text.append(L".");
    text.append(std::to_wstring(VERSION_BUILD));

    ::SetWindowText(target, text.c_str());
}
