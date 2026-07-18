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
#include "../NppPlugin/DockingFeature/StaticDialog.h"
#include "Explorer.h"
#include "ExplorerResource.h"

class NewDlg : public StaticDialog
{

public:
    NewDlg() : StaticDialog() {}

    void init(HINSTANCE hInst, HWND hWnd, const std::wstring& wndName = L"") {
        Window::init(hInst, hWnd);
        _wndName = wndName;
    };

    INT_PTR doDialog(std::wstring* pFileName, const std::wstring& desc);

    void destroy() override {};


protected :
    INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

private:
    std::wstring  _wndName;
    std::wstring* _pFileName = nullptr;
    std::wstring  _desc;

};
