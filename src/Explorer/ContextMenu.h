/***********************************************************\
*	Original in MFC by Roman Engels		Copyright 2003		*
*															*
*	http://www.codeproject.com/shell/shellcontextmenu.asp	*
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

#pragma once

#include "Explorer.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "window.h"

#include <shlobj.h>
#include <commctrl.h>

#include <malloc.h>
#include <vector>
#include <string>

struct __declspec(uuid("000214e6-0000-0000-c000-000000000046")) IShellFolder;

class ContextMenu  
{
public:
	ContextMenu();
	~ContextMenu();

	void SetObjects(const std::wstring &strObject);
	void SetObjects(const std::vector<std::wstring> &strArray);
	UINT ShowContextMenu(HINSTANCE hInst, HWND hWndNpp, HWND hWndParent, POINT pt, bool normal = true);

private:
	static LRESULT CALLBACK defaultHookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK HookWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	HRESULT SHBindToParentEx (LPCITEMIDLIST pidl, REFIID riid, VOID **ppv, LPCITEMIDLIST *ppidlLast);

	void	InvokeCommand(LPCONTEXTMENU pContextMenu, UINT idCommand);
	void	HandleCustomCommand(UINT idCommand);
	LPCONTEXTMENU	GetContextMenu();
	void	FreePIDLArray(LPITEMIDLIST * pidlArray);
	UINT	GetPIDLSize(LPCITEMIDLIST pidl);
	LPBYTE	GetPIDLPos(LPCITEMIDLIST pidl, int nPos);
	int		GetPIDLCount(LPCITEMIDLIST pidl);
	LPITEMIDLIST CopyPIDL(LPCITEMIDLIST pidl, int cb = -1);

	/* notepad functions */
	void	Rename(void);
	void	newFile(void);
	void	newFolder(void);
	void	findInFiles(void);
	void	openFile(void);
	void	openFileInOtherView(void);
	void	openFileInNewInstance(void);
	void	openPrompt(void);
	void	addToFaves(void);
	void	addRelativePathsCB(void);
	void	addFullPathsCB(void);
	void	addFileNamesCB(void);
	bool	Str2CB(LPCTSTR str2cpy);
	void	openScriptPath(HMODULE hInst);
	void	startNppExec(HMODULE hInst, UINT cmdID);

private:
	HINSTANCE				_hInst;
	HWND					_hWndNpp;
	HWND					_hWndParent;

	SIZE_T					_nItems;
	BOOL					_bDelete;
	IShellFolder*			_psfFolder;
	LPITEMIDLIST*			_pidlArray;	

	IContextMenu2*			_contextMenu2;
	IContextMenu3*			_contextMenu3;

	std::wstring				_strFirstElement;
	std::vector<std::wstring>	_strArray;
	std::vector<std::wstring>	_strNppScripts;
};
