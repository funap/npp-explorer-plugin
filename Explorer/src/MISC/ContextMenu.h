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



#ifndef	CONTEXTMENU_DEFINE_H
#define CONTEXTMENU_DEFINE_H

#include "Explorer.h"
#include "ExplorerResource.h"
#include "NewDlg.h"
#include "window.h"
#include <malloc.h>
#include <vector>
#include <string>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>

using namespace std;


struct __declspec(uuid("000214e6-0000-0000-c000-000000000046")) IShellFolder;

typedef struct
{
	TCHAR * pszFullPath;
	TCHAR	szFileName[MAX_PATH];
	TCHAR	szTypeName[MAX_PATH];
	UINT64	u64FileSize;
	DWORD	dwFileAttributes;
	int		iIcon;
	FILETIME ftLastModified;
} OBJECT_DATA;

#define CTX_MIN 1
#define CTX_MAX 10000

typedef enum
{
	CTX_DELETE			= 18,
	CTX_RENAME			= 19,
	CTX_CUT				= 25,
	CTX_COPY			= 26,
	CTX_PASTE			= 27,
	CTX_NEW_FILE		= CTX_MAX,
	CTX_NEW_FOLDER,
	CTX_FIND_IN_FILES,
	CTX_OPEN,
	CTX_OPEN_DIFF_VIEW,
	CTX_OPEN_NEW_INST,
	CTX_OPEN_CMD,
	CTX_ADD_TO_FAVES,
	CTX_FULL_PATH,
	CTX_FULL_FILES,
	CTX_GOTO_SCRIPT_PATH,
	CTX_START_SCRIPT
} eContextMenuID;

class ContextMenu  
{
public:
	ContextMenu();
	~ContextMenu();

	/* get menu */
	HMENU	GetMenu (void);

	void SetObjects(string strObject);
	void SetObjects(vector<string> strArray);
	UINT ShowContextMenu(HINSTANCE hInst, HWND hWndNpp, HWND hWndParent, POINT pt, bool normal = true);

private:
	static LRESULT CALLBACK HookWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	HRESULT SHBindToParentEx (LPCITEMIDLIST pidl, REFIID riid, VOID **ppv, LPCITEMIDLIST *ppidlLast);

	void	InvokeCommand(LPCONTEXTMENU pContextMenu, UINT idCommand);
	BOOL	GetContextMenu(void ** ppContextMenu, int & iMenuType);
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
	void	addToFaves(bool isFolder);
	void	addFullPathsCB(void);
	void	addFileNamesCB(void);
	bool	Str2CB(LPCTSTR str2cpy);
	void	openScriptPath(HMODULE hInst);
	void	startNppExec(HMODULE hInst, UINT cmdID);

private:
	HINSTANCE				_hInst;
	HWND					_hWndNpp;
	HWND					_hWndParent;

	int						_nItems;
	BOOL					_bDelete;
	HMENU					_hMenu;
	IShellFolder*			_psfFolder;
	LPITEMIDLIST*			_pidlArray;	

	string					_strFirstElement;
	vector<string>			_strArray;
	vector<string>			_strNppScripts;
};

#endif // CONTEXTMENU_DEFINE_H
