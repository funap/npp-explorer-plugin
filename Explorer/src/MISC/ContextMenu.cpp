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


#include "stdafx.h"
#include "FavesDialog.h"
#include "ContextMenu.h"
#include "nppexec_msgs.h"



IContextMenu2 * g_IContext2		= NULL;
IContextMenu3 * g_IContext3		= NULL;

WNDPROC			g_OldWndProc	= NULL;

/* global explorer params */
extern ExProp	exProp;


ContextMenu::ContextMenu() :
	_hInst(nullptr),
	_hWndNpp(nullptr),
	_hWndParent(nullptr),
	_nItems(0),
	_bDelete(FALSE),
	_hMenu(nullptr),
	_psfFolder(nullptr),
	_pidlArray(nullptr)
{
}

ContextMenu::~ContextMenu()
{
	/* free all allocated datas */
	if (_psfFolder && _bDelete)
		_psfFolder->Release ();
	_psfFolder = NULL;
	FreePIDLArray(_pidlArray);
	_pidlArray = NULL;

	::DestroyMenu(_hMenu);
}



// this functions determines which version of IContextMenu is avaibale for those objects (always the highest one)
// and returns that interface
BOOL ContextMenu::GetContextMenu (void ** ppContextMenu, int & iMenuType)
{
	*ppContextMenu = NULL;
	LPCONTEXTMENU icm1 = NULL;
	
	// first we retrieve the normal IContextMenu interface (every object should have it)
	_psfFolder->GetUIObjectOf (NULL, (UINT)_nItems, (LPCITEMIDLIST *) _pidlArray, IID_IContextMenu, NULL, (void**) &icm1);

	if (icm1)
	{	// since we got an IContextMenu interface we can now obtain the higher version interfaces via that
		if (icm1->QueryInterface (IID_IContextMenu3, ppContextMenu) == NOERROR)
			iMenuType = 3;
		else if (icm1->QueryInterface (IID_IContextMenu2, ppContextMenu) == NOERROR)
			iMenuType = 2;

		if (*ppContextMenu) 
			icm1->Release(); // we can now release version 1 interface, cause we got a higher one
		else 
		{	
			iMenuType = 1;
			*ppContextMenu = icm1;	// since no higher versions were found
		}							// redirect ppContextMenu to version 1 interface
	}
	else
		return (FALSE);	// something went wrong
	
	return (TRUE); // success
}


LRESULT CALLBACK ContextMenu::HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{ 
		case WM_MENUCHAR:	// only supported by IContextMenu3
		{
			if (g_IContext3)
			{
				LRESULT lResult = 0;
				g_IContext3->HandleMenuMsg2 (message, wParam, lParam, &lResult);
				return (lResult);
			}
			break;
		}
		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		{
			if (wParam) 
				break; // if wParam != 0 then the message is not menu-related
		}
		case WM_INITMENUPOPUP:
		{
			if (g_IContext2)
				g_IContext2->HandleMenuMsg (message, wParam, lParam);
			else	// version 3
				g_IContext3->HandleMenuMsg (message, wParam, lParam);
			return (message == WM_INITMENUPOPUP ? 0 : TRUE); // inform caller that we handled WM_INITPOPUPMENU by ourself
			break;
		}
		default:
			break;
	}

	// call original WndProc of window to prevent undefined bevhaviour of window
	return ::CallWindowProc (g_OldWndProc, hWnd, message, wParam, lParam);
}


UINT ContextMenu::ShowContextMenu(HINSTANCE hInst, HWND hWndNpp, HWND hWndParent, POINT pt, bool normal)
{
	TCHAR	szText[64] = {0};

	/* store notepad handle */
	_hInst		= hInst;
	_hWndNpp	= hWndNpp;
	_hWndParent = hWndParent;

	// to know which version of IContextMenu is supported
	int iMenuType = 0;

	// common pointer to IContextMenu and higher version interface
	LPCONTEXTMENU pContextMenu = NULL;

	if (_pidlArray != NULL)
	{
		if (!_hMenu)
		{
			_hMenu = NULL;
			_hMenu = ::CreateMenu();
		}

		if (!GetContextMenu((void**) &pContextMenu, iMenuType))	
			return (0);	// something went wrong

		// lets fill out our popupmenu 
		pContextMenu->QueryContextMenu( _hMenu,
										::GetMenuItemCount(_hMenu),
										CTX_MIN,
										CTX_MAX,
										CMF_EXPLORE | ((_strFirstElement.size() > 4)?CMF_CANRENAME:0));
 
		// subclass window to handle menurelated messages in ContextMenu 
		g_OldWndProc	= NULL;
		if (iMenuType > 1)	// only subclass if its version 2 or 3
		{
			g_OldWndProc = (WNDPROC)::SetWindowLongPtr (hWndParent, GWLP_WNDPROC, (LONG_PTR) HookWndProc);
			if (iMenuType == 2)
				g_IContext2 = (LPCONTEXTMENU2) pContextMenu;
			else	// version 3
				g_IContext3 = (LPCONTEXTMENU3) pContextMenu;
		}
	}

	/************************************* modification for notepad ***********************************/
	HMENU		hMainMenu		= ::CreatePopupMenu();
	HMENU		hMenuNppExec	= ::CreatePopupMenu();
	bool		isFolder		= (_strFirstElement[_strFirstElement.size()-1] == '\\');
	DWORD		dwExecVer		= 0;
	DWORD		dwExecState		= 0;

	TCHAR		szPath[MAX_PATH];
	::GetModuleFileName((HMODULE)hInst, szPath, MAX_PATH);

	/* get version information */
	CommunicationInfo	ci;
	ci.srcModuleName	= PathFindFileName(szPath);
	ci.internalMsg		= NPEM_GETVERDWORD;
	ci.info				= &dwExecVer;
	::SendMessage(hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)exProp.nppExecProp.szAppName, (LPARAM)&ci);
	
	/* get acivity state of NppExec */
	ci.srcModuleName	= PathFindFileName(szPath);
	ci.internalMsg		= NPEM_GETSTATE;
	ci.info				= &dwExecState;
	::SendMessage(hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)exProp.nppExecProp.szAppName, (LPARAM)&ci);

	/* Add notepad menu items */
	if (isFolder)
	{
		::AppendMenu(hMainMenu, MF_STRING, CTX_NEW_FILE, _T("New File..."));
		::AppendMenu(hMainMenu, MF_STRING, CTX_NEW_FOLDER, _T("New Folder..."));
		::AppendMenu(hMainMenu, MF_STRING, CTX_FIND_IN_FILES, _T("Find in Files..."));
	}
	else
	{
		::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN, _T("Open"));
		::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_DIFF_VIEW, _T("Open in Other View"));
		::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_NEW_INST, _T("Open in New Instance"));
	}

	if (dwExecVer >= 0x02F5)
	{
		TCHAR					TEMP[MAX_PATH];
		WIN32_FIND_DATA			Find			= {0};
		HANDLE					hFind			= NULL;

		/* initialize scripts */
		_strNppScripts.clear();

		/* add backslash if necessary */
		if ((exProp.nppExecProp.szScriptPath[0] == '.') &&
			(exProp.nppExecProp.szScriptPath[1] == '.'))
		{
			/* module path of notepad */
			GetModuleFileName(hInst, TEMP, sizeof(TEMP));
			PathRemoveFileSpec(TEMP);
			PathAppend(TEMP, exProp.nppExecProp.szScriptPath);
		} else {
			_tcsncpy(TEMP, exProp.nppExecProp.szScriptPath, MAX_PATH-1);
		}
		if (TEMP[_tcslen(TEMP) - 1] != '\\')
			_tcscat(TEMP, _T("\\"));

		/* find every element in folder */
		_tcscat(TEMP, _T("*.exec"));
		hFind = ::FindFirstFile(TEMP, &Find);

		if (hFind != INVALID_HANDLE_VALUE)
		{
			do 
			{
				::AppendMenu(hMenuNppExec, MF_STRING | (dwExecState == NPE_STATEREADY ? 0 : MF_DISABLED), CTX_START_SCRIPT + _strNppScripts.size(), Find.cFileName);
				_strNppScripts.push_back(Find.cFileName);
			} while (FindNextFile(hFind, &Find));

			/* close file search */
			::FindClose(hFind);
		}
		if (_strNppScripts.size() != 0)
			::AppendMenu(hMenuNppExec, MF_SEPARATOR, 0, NULL);
		::AppendMenu(hMenuNppExec, MF_STRING, CTX_GOTO_SCRIPT_PATH, _T("Go to script folder"));
		::AppendMenu(hMainMenu, MF_STRING | MF_POPUP, (UINT_PTR)hMenuNppExec, _T("NppExec Script(s)"));
	}
	else
	{
		/* version not supported */
		::DestroyMenu(hMenuNppExec);
	}
	::AppendMenu(hMainMenu, MF_STRING, CTX_OPEN_CMD, _T("Open Command Window Here"));

	::InsertMenu(hMainMenu, 3, MF_BYPOSITION | MF_SEPARATOR, 0, 0);
	::AppendMenu(hMainMenu, MF_STRING, CTX_ADD_TO_FAVES, _T("Add to 'Favorites'..."));
	::AppendMenu(hMainMenu, MF_STRING, CTX_FULL_PATH, _T("Full File Path(s) to Clipboard"));
	::AppendMenu(hMainMenu, MF_STRING, CTX_FULL_FILES, _T("File Name(s) to Clipboard"));

	if (_pidlArray != NULL)
	{
		int				copyAt		= -1;
		int				items		= ::GetMenuItemCount(_hMenu);
		MENUITEMINFO	info		= {0};

		info.cbSize		= sizeof(MENUITEMINFO);
		info.fMask		= MIIM_TYPE | MIIM_ID | MIIM_SUBMENU;

		::AppendMenu(hMainMenu, MF_SEPARATOR, 0, 0);

		if (normal)
		{
			/* store all items in an seperate sub menu until "cut" (25) or "copy" (26) */
			for (int i = 0; i < items; i++)
			{
				info.cch		= 256;
				info.dwTypeData	= szText;
				if (copyAt == -1)
				{
					::GetMenuItemInfo(_hMenu, i, TRUE, &info);
					if ((info.wID == CTX_CUT) || (info.wID == CTX_COPY) || (info.wID == CTX_PASTE))
					{
						copyAt	= i - 1;
						::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
						::DeleteMenu(_hMenu, i  , MF_BYPOSITION);
						::DeleteMenu(_hMenu, i-1, MF_BYPOSITION);
					}
				}
				else
				{
					::GetMenuItemInfo(_hMenu, copyAt, TRUE, &info);
					if ((MFT_STRING == info.fType) || (MFT_SEPARATOR == info.fType)) {
						::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
					}
					::DeleteMenu(_hMenu, copyAt, MF_BYPOSITION);
				}
			}

			TCHAR	szMenuName[MAX_PATH];
			if (!NLGetText(_hInst, _hWndNpp, _T("Standard Menu"), szMenuName, MAX_PATH)) {
				_tcscpy(szMenuName, _T("Standard Menu"));
			}
			::InsertMenu(hMainMenu, 4, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR)_hMenu, szMenuName);
			::InsertMenu(hMainMenu, (dwExecVer >= 0x02F5 ? 7 : 6), MF_BYPOSITION | MF_SEPARATOR, 0, 0);
		}
		else
		{
			/* ignore all items until "cut" (25) or "copy" (26) */
			for (int i = 0; i < items; i++)
			{
				info.cch		= 256;
				info.dwTypeData	= szText;
				::GetMenuItemInfo(_hMenu, i, TRUE, &info);
				if ((copyAt == -1) && ((info.wID == CTX_CUT) || (info.wID == CTX_COPY) || (info.wID == CTX_PASTE)))
				{
					copyAt	= 0;
				}
				else if ((info.wID == 20) || (info.wID == 27))
				{
					::AppendMenu(hMainMenu, info.fType, info.wID, info.dwTypeData);
					::AppendMenu(hMainMenu, MF_SEPARATOR, 0, 0);
				}
			}
			::DeleteMenu(hMainMenu, ::GetMenuItemCount(hMainMenu) - 1, MF_BYPOSITION);
		}
	}

	/*****************************************************************************************************/

	/* change language */
	NLChangeMenu(_hInst, _hWndNpp, hMainMenu, _T("ContextMenu"), MF_BYCOMMAND);

	UINT idCommand = ::TrackPopupMenu(hMainMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hWndParent, NULL);

	/* free resources */
	::DestroyMenu(hMainMenu);
	::DestroyMenu(hMenuNppExec);

	if ((_pidlArray != NULL) && (g_OldWndProc != NULL)) // unsubclass
	{
		::SetWindowLongPtr(hWndParent, GWLP_WNDPROC, (LONG_PTR) g_OldWndProc);
	}

	// see if returned idCommand belongs to shell menu entries but not for renaming (19)
	if ((idCommand >= CTX_MIN) && (idCommand < CTX_MAX) && (idCommand != CTX_RENAME))	
	{
		InvokeCommand (pContextMenu, idCommand - CTX_MIN);	// execute related command
	}
	else
	{

	/************************************* modification for notepad ***********************************/

		switch (idCommand)
		{
			case CTX_RENAME:
			{
				Rename();
				break;
			}
			case CTX_NEW_FILE:
			{
				newFile();
				break;
			}
			case CTX_NEW_FOLDER:
			{
				newFolder();
				break;
			}
			case CTX_FIND_IN_FILES:
			{
				findInFiles();
				break;
			}
			case CTX_OPEN:
			{
				openFile();
				break;
			}
			case CTX_OPEN_DIFF_VIEW:
			{
				openFileInOtherView();
				break;
			}
			case CTX_OPEN_NEW_INST:
			{
				openFileInNewInstance();
				break;
			}
			case CTX_OPEN_CMD:
			{
				openPrompt();
				break;
			}
			case CTX_ADD_TO_FAVES:
			{
				addToFaves(isFolder);
				break;
			}
			case CTX_FULL_PATH:
			{
				addFullPathsCB();
				break;
			}
			case CTX_FULL_FILES:
			{
				addFileNamesCB();
				break;
			}
			case CTX_GOTO_SCRIPT_PATH:
			{
				openScriptPath(hInst);
				break;
			}
			default: /* and greater */
			{
				if ((idCommand >= CTX_START_SCRIPT) && (idCommand <= (CTX_START_SCRIPT + _strNppScripts.size())))
				{
					startNppExec(hInst, idCommand - CTX_START_SCRIPT);
				}
				break;
			}
		}

	/*****************************************************************************************************/

	}
	
	if (pContextMenu != NULL)
		pContextMenu->Release();
	g_IContext2 = NULL;
	g_IContext3 = NULL;

	return (idCommand);
}


void ContextMenu::InvokeCommand (LPCONTEXTMENU pContextMenu, UINT idCommand)
{
	CMINVOKECOMMANDINFO cmi = {0};
	cmi.cbSize = sizeof (CMINVOKECOMMANDINFO);
	cmi.lpVerb = (LPSTR) MAKEINTRESOURCE (idCommand);
	cmi.nShow = SW_SHOWNORMAL;
	
	pContextMenu->InvokeCommand (&cmi);
}



void ContextMenu::SetObjects(std::wstring strObject)
{
	// only one object is passed
	std::vector<std::wstring>	strArray;
	strArray.push_back(strObject);	// create a CStringArray with one element
	
	SetObjects (strArray);			// and pass it to SetObjects (vector<string> strArray)
									// for further processing
}


void ContextMenu::SetObjects(std::vector<std::wstring> strArray)
{
	// store also the string for later menu use
	_strFirstElement = strArray[0];
	_strArray		 = strArray;

	// free all allocated datas
	if (_psfFolder && _bDelete)
		_psfFolder->Release ();
	_psfFolder = NULL;
	FreePIDLArray (_pidlArray);
	_pidlArray = NULL;
	
	// get IShellFolder interface of Desktop (root of shell namespace)
	IShellFolder * psfDesktop = NULL;
	SHGetDesktopFolder (&psfDesktop);	// needed to obtain full qualified pidl

	// ParseDisplayName creates a PIDL from a file system path relative to the IShellFolder interface
	// but since we use the Desktop as our interface and the Desktop is the namespace root
	// that means that it's a fully qualified PIDL, which is what we need
	LPITEMIDLIST pidl = NULL;

#ifndef _UNICODE
	OLECHAR * olePath = NULL;
	olePath = (OLECHAR *) calloc (strArray[0].size() + 1, sizeof (OLECHAR));
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, strArray[0].c_str(), -1, olePath, strArray[0].size() + 1);	
	psfDesktop->ParseDisplayName (NULL, 0, olePath, NULL, &pidl, NULL);
	free (olePath);
#else
	psfDesktop->ParseDisplayName (NULL, 0, (LPOLESTR)strArray[0].c_str(), NULL, &pidl, NULL);
#endif

	if (pidl != NULL) {
		// now we need the parent IShellFolder interface of pidl, and the relative PIDL to that interface
		LPITEMIDLIST pidlItem = NULL;	// relative pidl
		SHBindToParentEx (pidl, IID_IShellFolder, (void **) &_psfFolder, NULL);
		free (pidlItem);
		// get interface to IMalloc (need to free the PIDLs allocated by the shell functions)
		LPMALLOC lpMalloc = NULL;
		SHGetMalloc (&lpMalloc);
		if (lpMalloc != NULL) lpMalloc->Free (pidl);

		// now we have the IShellFolder interface to the parent folder specified in the first element in strArray
		// since we assume that all objects are in the same folder (as it's stated in the MSDN)
		// we now have the IShellFolder interface to every objects parent folder
		
		IShellFolder * psfFolder = NULL;
		_nItems = strArray.size();
		for (SIZE_T i = 0; i < _nItems; i++) {
#ifndef _UNICODE
			olePath = (OLECHAR *) calloc (strArray[i].size() + 1, sizeof (OLECHAR));
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, strArray[i].c_str(), -1, olePath, strArray[i].size() + 1);	
			psfDesktop->ParseDisplayName (NULL, 0, olePath, NULL, &pidl, NULL);
			free (olePath);
#else
			psfDesktop->ParseDisplayName (NULL, 0, (LPOLESTR)strArray[i].c_str(), NULL, &pidl, NULL);
#endif
			_pidlArray = (LPITEMIDLIST *) realloc (_pidlArray, (i + 1) * sizeof (LPITEMIDLIST));
			// get relative pidl via SHBindToParent
			SHBindToParentEx (pidl, IID_IShellFolder, (void **) &psfFolder, (LPCITEMIDLIST *) &pidlItem);
			_pidlArray[i] = CopyPIDL (pidlItem);	// copy relative pidl to pidlArray
			free (pidlItem);
			// free pidl allocated by ParseDisplayName
			if (lpMalloc != NULL) lpMalloc->Free (pidl);
			if (psfFolder != NULL) psfFolder->Release ();
		}

		if (lpMalloc != NULL) lpMalloc->Release ();
	}
	if (psfDesktop != NULL) psfDesktop->Release ();

	_bDelete = TRUE;	// indicates that _psfFolder should be deleted by ContextMenu
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

HMENU ContextMenu::GetMenu()
{
	if (!_hMenu) {
		_hMenu = ::CreatePopupMenu();	// create the popupmenu (its empty)
	}
	return (_hMenu);
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
	if (nCount == 0) {	// desktop pidl of invalid pidl
		return E_POINTER;
	}

	IShellFolder * psfDesktop = NULL;
	SHGetDesktopFolder (&psfDesktop);
	if (nCount == 1) {	// desktop pidl
		if ((hr = psfDesktop->QueryInterface(riid, ppv)) == S_OK) {
			if (ppidlLast) {
				*ppidlLast = CopyPIDL (pidl);
			}
		}
		psfDesktop->Release ();
		return hr;
	}

	LPBYTE pRel = GetPIDLPos (pidl, nCount - 1);
	LPITEMIDLIST pidlParent = NULL;
	pidlParent = CopyPIDL (pidl, (int)(pRel - (LPBYTE) pidl));
	IShellFolder * psfFolder = NULL;
	
	if ((hr = psfDesktop->BindToObject (pidlParent, NULL, __uuidof (psfFolder), (void **) &psfFolder)) != S_OK) {
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


LPBYTE ContextMenu::GetPIDLPos (LPCITEMIDLIST pidl, int nPos)
{
	if (!pidl)
		return 0;
	int nCount = 0;
	
	BYTE * pCur = (BYTE *) pidl;
	while (((LPCITEMIDLIST) pCur)->mkid.cb) {
		if (nCount == nPos) {
			return pCur;
		}
		nCount++;
		pCur += ((LPCITEMIDLIST) pCur)->mkid.cb;	// + sizeof(pidl->mkid.cb);
	}
	if (nCount == nPos) {
		return pCur;
	}
	return NULL;
}


int ContextMenu::GetPIDLCount (LPCITEMIDLIST pidl)
{
	if (!pidl)
		return 0;

	int nCount = 0;
	BYTE*  pCur = (BYTE *) pidl;
	while (((LPCITEMIDLIST) pCur)->mkid.cb)
	{
		nCount++;
		pCur += ((LPCITEMIDLIST) pCur)->mkid.cb;
	}
	return nCount;
}


/*********************************************************************************************
 *	Notepad specific functions
 */
void ContextMenu::Rename(void)
{
	NewDlg				dlg;
	extern	HANDLE		g_hModule;
	TCHAR				newFirstElement[MAX_PATH];
	TCHAR				szNewName[MAX_PATH];
	TCHAR				szComment[MAX_PATH];

	/* copy current element information */
	_tcscpy(newFirstElement, _strFirstElement.c_str());

	/* when it is folder, remove the last backslash */
	if (newFirstElement[_tcslen(newFirstElement) - 1] == '\\')
	{
		newFirstElement[_tcslen(newFirstElement) - 1] = 0;
	}

	/* init field to current selected item */
	_tcscpy(szNewName, &_tcsrchr(newFirstElement, '\\')[1]);

	(_tcsrchr(newFirstElement, '\\')[1]) = 0;

	/* rename comment */
	if (NLGetText(_hInst, _hWndNpp, _T("Rename"), szComment, MAX_PATH) == 0) {
		_tcscpy(szComment, _T("Rename"));
	}

	dlg.init((HINSTANCE)g_hModule, _hWndNpp);
	if (dlg.doDialog(szNewName, szComment) == TRUE)
	{
		_tcscat(newFirstElement, szNewName);
		::MoveFile(_strFirstElement.c_str(), newFirstElement);
	}
}

void ContextMenu::newFile(void)
{
	NewDlg		dlg;
	extern		HANDLE		g_hModule;
	BOOL		bLeave		= FALSE;
	TCHAR		szFileName[MAX_PATH];
	TCHAR		szComment[MAX_PATH];

	szFileName[0] = '\0';

	/* rename comment */
	if (NLGetText(_hInst, _hWndNpp, _T("New file"), szComment, MAX_PATH) == 0) {
		_tcscpy(szComment, _T("New file"));
	}

	dlg.init((HINSTANCE)g_hModule, _hWndNpp);
	while (bLeave == FALSE)
	{
		if (dlg.doDialog(szFileName, szComment) == TRUE)
		{
			/* test if is correct */
			if (IsValidFileName(szFileName))
			{
				std::wstring newFile = _strFirstElement + szFileName;
				
				::CloseHandle(::CreateFile(newFile.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
				::SendMessage(_hWndNpp, NPPM_DOOPEN, 0, (LPARAM)newFile.c_str());
				bLeave = TRUE;
			}
		}
		else
			bLeave = TRUE;
	}
}

void ContextMenu::newFolder(void)
{
	NewDlg		dlg;
	extern		HANDLE		g_hModule;
	BOOL		bLeave		= FALSE;
	TCHAR		szFolderName[MAX_PATH];
	TCHAR		szComment[MAX_PATH];

	szFolderName[0] = '\0';

	/* rename comment */
	if (NLGetText(_hInst, _hWndNpp, _T("New folder"), szComment, MAX_PATH) == 0) {
		_tcscpy(szComment, _T("New folder"));
	}

	dlg.init((HINSTANCE)g_hModule, _hWndNpp);
	while (bLeave == FALSE)
	{
		if (dlg.doDialog(szFolderName, szComment) == TRUE)
		{
			/* test if is correct */
			if (IsValidFileName(szFolderName))
			{
				std::wstring newFolder = _strFirstElement + szFolderName;
				if (::CreateDirectory(newFolder.c_str(), NULL) == FALSE) {
					if (NLMessageBox(_hInst, _hWndNpp, _T("MsgBox FolderCreateError"), MB_OK) == FALSE)
						::MessageBox(_hWndNpp, _T("Folder couldn't be created."), _T("Error"), MB_OK);
				}
				bLeave = TRUE;
			}
		}
		else
			bLeave = TRUE;
	}
}

void ContextMenu::findInFiles(void)
{
	::SendMessage(_hWndNpp, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)_strFirstElement.c_str(), NULL);
}

void ContextMenu::openFile(void)
{
	for (const auto &path : _strArray) {
		::SendMessage(_hWndNpp, NPPM_DOOPEN, 0, (LPARAM)path.c_str());
	}
}

void ContextMenu::openFileInOtherView(void)
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

void ContextMenu::openFileInNewInstance(void)
{
	std::wstring		args2Exec;
	extern	HANDLE		g_hModule;
	TCHAR				szNpp[MAX_PATH];

    // get path name
	GetModuleFileName((HMODULE)g_hModule, szNpp, _countof(szNpp));

    // remove the module name : get plugins directory path
	PathRemoveFileSpec(szNpp);
	PathRemoveFileSpec(szNpp);

	/* add notepad as default program */
	_tcscat(szNpp, _T("\\"));
	_tcscat(szNpp, _T("notepad++.exe"));

	for (UINT i = 0; i < _strArray.size(); i++)
	{
		if (i == 0) {
			args2Exec = _T("-multiInst \"") + _strArray[i] + _T("\"");
		} else {
			args2Exec += _T(" \"") + _strArray[i] + _T("\"");
		}
	}
	::ShellExecute(_hWndNpp, _T("open"), szNpp, args2Exec.c_str(), _T("."), SW_SHOW);
}

void ContextMenu::openPrompt(void)
{
	for (auto &path : _strArray) {
		/* is file */
		if (path.at(path.size() - 1) != '\\') {
			SIZE_T pos = path.rfind(_T("\\"), path.size() - 1);
			if (std::wstring::npos != pos) {
				path.erase(pos, path.size());
			}
		}
		::ShellExecute(_hWndNpp, _T("open"), _T("cmd.exe"), NULL, path.c_str(), SW_SHOW);
	}
}

void ContextMenu::addToFaves(bool isFolder)
{
	/* test if only one file is selected */
	if (_strArray.size() > 1)
	{
		if (NLMessageBox(_hInst, _hWndNpp, _T("MsgBox OneFileToFaves"), MB_OK) == FALSE)
			::MessageBox(_hWndNpp, _T("Only one file could be added!"), _T("Error"), MB_OK);
	}
	else
	{
		extern FavesDialog	favesDlg;
		favesDlg.AddToFavorties((WPARAM)isFolder, (LPTSTR)_strArray[0].c_str());
	}
}

void ContextMenu::addFullPathsCB(void)
{
	std::wstring temp;
	BOOL isFirstItem = TRUE;
	for (auto &path : _strArray) {
		if (isFirstItem) {
			isFirstItem = FALSE;
		}
		else {
			temp += _T("\n");
		}
		temp += path;
	}
	Str2CB(temp.c_str());
}

void ContextMenu::addFileNamesCB(void)
{
	std::wstring	temp;
	BOOL isFirstItem = TRUE;
	for (auto &path : _strArray) {
		SIZE_T	pos = path.rfind(_T("\\"), path.size() - 1);
		if (std::wstring::npos != pos) {
			if (isFirstItem) {
				isFirstItem = FALSE;
			}
			else {
				temp += _T("\n");
			}

			/* is folder */
			if (path.at(path.size() - 1) == '\\') {
				pos = path.rfind(_T("\\"), pos - 1);
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
	TCHAR	TEMP[MAX_PATH];

	if (exProp.nppExecProp.szScriptPath[0] == '.')
	{
		/* module path of notepad */
		GetModuleFileName(hInst, TEMP, _countof(TEMP));
		PathRemoveFileSpec(TEMP);
		PathAppend(TEMP, exProp.nppExecProp.szScriptPath);
	} else {
		_tcscpy(TEMP, exProp.nppExecProp.szScriptPath);
	}
	::SendMessage(_hWndParent, EXM_OPENDIR, 0, (LPARAM)TEMP);
}

void ContextMenu::startNppExec(HMODULE hInst, UINT cmdID)
{
	TCHAR	szScriptPath[MAX_PATH];

	/* concatinate execute command */
	if (exProp.nppExecProp.szScriptPath[0] == '.')
	{
		/* module path of notepad */
		GetModuleFileName(hInst, szScriptPath, _countof(szScriptPath));
		PathRemoveFileSpec(szScriptPath);
		PathAppend(szScriptPath, exProp.nppExecProp.szScriptPath);
	} else {
		_tcscpy(szScriptPath, exProp.nppExecProp.szScriptPath);
	}
	if (szScriptPath[_tcslen(szScriptPath) - 1] != '\\')
		_tcscat(szScriptPath, _T("\\"));
	_tcscat(szScriptPath, _strNppScripts[cmdID].c_str());

	/* get arguments and convert */
	HANDLE	hFile = ::CreateFile(szScriptPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD	dwSize = ::GetFileSize(hFile, NULL);

		if (dwSize != -1)
		{
			TCHAR		szAppName[MAX_PATH];
			DWORD		hasRead		= 0;
			LPTSTR		pszPtr		= NULL;
			LPTSTR		pszArg		= NULL;
			LPTSTR		pszData		= (LPTSTR)new TCHAR[dwSize+1];

			if (pszData != NULL)
			{
				/* read data from file */
				::ReadFile(hFile, pszData, dwSize, &hasRead, NULL);

#ifdef UNICODE
				TCHAR		szBOM		= 0xFEFF;
				LPTSTR		pszData2	= NULL;

				if (pszData[0] == szBOM) {
					pszPtr = _tcstok(&pszData[1], _T("\n"));
				} else if ((pszData[0] == _T('/'))) {
					pszPtr = _tcstok(pszData, _T("\n"));
				} else if (((LPSTR)pszData)[0] == '/') {
					pszData2 = new TCHAR[dwSize * 2];
					::MultiByteToWideChar(CP_ACP, 0, (LPSTR)pszData, -1, pszData2, dwSize * 2);
					pszPtr = _tcstok(pszData2, _T("\n"));
				} else {
					::MessageBox(_hWndNpp, _T("Wrong file format"), _T("Error"), MB_OK | MB_ICONERROR);
					delete [] pszData;
					return; /* ============= Leave Function ================== */
				}
#else
				/* get argument string to convert */
				pszPtr = _tcstok(pszPtr, _T("\n"));
#endif

				if (ConvertCall(pszPtr, szAppName, &pszArg, _strArray) == TRUE)
				{
					TCHAR	szPath[MAX_PATH];
					::GetModuleFileName((HMODULE)_hInst, szPath, _countof(szPath));

					NpeNppExecParam			npep;
					npep.szScriptName		= szScriptPath;
					npep.szScriptArguments	= pszArg;
					npep.dwResult			= 1;

					/* get version information */
					CommunicationInfo		ci;
					ci.srcModuleName		= PathFindFileName(szPath);
					ci.internalMsg			= NPEM_NPPEXEC;
					ci.info					= &npep;

					::SendMessage(_hWndNpp, NPPM_MSGTOPLUGIN, (WPARAM)szAppName, (LPARAM)&ci);

					if (npep.dwResult != NPE_NPPEXEC_OK)
					{
						if (NLMessageBox(_hInst, _hWndNpp, _T("MsgBox NppExecBusy"), MB_OK) == FALSE)
							::MessageBox(_hWndNpp, _T("NppExec currently in use!"), _T("Error"), MB_OK);
					}
					
					delete [] pszArg;
				}
				delete [] pszData;
#ifdef UNICODE
				delete [] pszData2;
#endif
			}
		}

		::CloseHandle(hFile);
	}
}

/******************************************************************************************
 *	Sets a string to clipboard
 */
bool ContextMenu::Str2CB(LPCTSTR str2cpy)
{
	if (!str2cpy)
		return false;
		
	if (!::OpenClipboard(_hWndNpp)) 
		return false; 
		
	::EmptyClipboard();
	
#ifdef _UNICODE
	HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, _tcslen(str2cpy) * 2 + 2);
#else
	HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_MOVEABLE, _tcslen(str2cpy) + 1);
#endif
	
	if (hglbCopy == NULL) 
	{ 
		::CloseClipboard(); 
		return false; 
	} 

	// Lock the handle and copy the text to the buffer. 
	LPTSTR pStr = (LPTSTR)::GlobalLock(hglbCopy);
	if (pStr) {
	_tcscpy(pStr, str2cpy);
	::GlobalUnlock(hglbCopy); 
	}

	// Place the handle on the clipboard. 
#ifdef _UNICODE
	::SetClipboardData(CF_UNICODETEXT, hglbCopy);
#else
	::SetClipboardData(CF_TEXT, hglbCopy);
#endif
	::CloseClipboard();
	return true;
}

