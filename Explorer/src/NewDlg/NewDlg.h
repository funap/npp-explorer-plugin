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


#ifndef NEW_DLG_DEFINE_H
#define NEW_DLG_DEFINE_H

#include "StaticDialog.h"
#include "Explorer.h"
#include "ExplorerResource.h"



class NewDlg : public StaticDialog
{

public:
	NewDlg() : StaticDialog() {};
    
    void init(HINSTANCE hInst, HWND hWnd, LPTSTR pszWndName = NULL) {
		Window::init(hInst, hWnd);
		_pszWndName = pszWndName;
	};

	INT_PTR doDialog(LPTSTR pFileName, LPTSTR pDesc);

    virtual void destroy() {};


protected :
	BOOL CALLBACK run_dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);

private:
	LPTSTR			_pszWndName;
	LPTSTR			_pFileName;
	LPTSTR			_pDesc;

};



#endif // NEW_DLG_DEFINE_H
