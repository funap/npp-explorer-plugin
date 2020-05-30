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
	NewDlg() : 
		StaticDialog(),
		_pszWndName(nullptr),
		_pFileName(nullptr),
		_pDesc(nullptr)
	{
	};
    
    void init(HINSTANCE hInst, HWND hWnd, LPCTSTR pszWndName = NULL) {
		Window::init(hInst, hWnd);
		_pszWndName = pszWndName;
	};

	INT_PTR doDialog(LPCTSTR pFileName, LPCTSTR pDesc);

    virtual void destroy() {};


protected :
	INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;

private:
	LPCTSTR			_pszWndName;
	LPCTSTR			_pFileName;
	LPCTSTR			_pDesc;

};



#endif // NEW_DLG_DEFINE_H
