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


/* include files */
#include "stdafx.h"
#include "Explorer.h"

#include <stdlib.h>
#include <iostream>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <dbt.h>
#include <atlbase.h>

#include "NppInterface.h"
#include "ExplorerDialog.h"
#include "FavesDialog.h"
#include "QuickOpenDialog.h"
#include "OptionDialog.h"
#include "HelpDialog.h"
#include "ToolTip.h"
#include "SysMsg.h"


#define SHGFI_OVERLAYINDEX 0x000000040


/* information for notepad */
CONST TCHAR  PLUGIN_NAME[] = _T("&Explorer");

TCHAR		configPath[MAX_PATH];
TCHAR		iniFilePath[MAX_PATH];

/* ini file sections */
CONST TCHAR WindowData[]		= _T("WindowData");
CONST TCHAR Explorer[]			= _T("Explorer");
CONST TCHAR Faves[]				= _T("Faves");



/* section Explorer */
CONST TCHAR LastPath[]			= _T("LastPath");
CONST TCHAR SplitterPos[]		= _T("SplitterPos");
CONST TCHAR SplitterPosHor[]	= _T("SplitterPosHor");
CONST TCHAR SortAsc[]			= _T("SortAsc");
CONST TCHAR SortPos[]			= _T("SortPos");
CONST TCHAR ColPosName[]		= _T("ColPosName");
CONST TCHAR ColPosExt[]			= _T("ColPosExt");
CONST TCHAR ColPosSize[]		= _T("ColPosSize");
CONST TCHAR ColPosDate[]		= _T("ColPosDate");
CONST TCHAR ShowHiddenData[]	= _T("ShowHiddenData");
CONST TCHAR ShowBraces[]		= _T("ShowBraces");
CONST TCHAR ShowLongInfo[]		= _T("ShowLongInfo");
CONST TCHAR AddExtToName[]		= _T("AddExtToName");
CONST TCHAR AutoUpdate[]		= _T("AutoUpdate");
CONST TCHAR SizeFormat[]		= _T("SizeFormat");
CONST TCHAR DateFormat[]		= _T("DateFormat");
CONST TCHAR FilterHistory[]		= _T("FilterHistory");
CONST TCHAR LastFilter[]		= _T("LastFilter");
CONST TCHAR TimeOut[]			= _T("TimeOut");
CONST TCHAR UseSystemIcons[]	= _T("UseSystemIcons");
CONST TCHAR NppExecAppName[]	= _T("NppExecAppName");
CONST TCHAR NppExecScriptPath[]	= _T("NppExecScriptPath");


/* global values */
HMODULE				hShell32;
NppData				nppData;
HANDLE				g_hModule;
HWND				g_HSource;
INT					g_docCnt = 0;
TCHAR				g_currentFile[MAX_PATH];
FuncItem			funcItem[] = {
//      {  _itemName,                         _pFunc,                _cmdID, _init2Check,   _pShKey}
/*  0 */{ L"&Explorer...",                    toggleExplorerDialog,       0,       false,   nullptr},
/*  1 */{ L"&Favorites...",                   toggleFavesDialog,          0,       false,   nullptr},
/*  2 */{ L"&Quick Open...",                  openQuickOpenDlg,           0,       false,   nullptr},
/*  3 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/*  4 */{ L"&Go to Path...",                  gotoPath,                   0,       false,   nullptr},
/*  5 */{ L"&Go to User Folder",              gotoUserFolder,             0,       false,   nullptr},
/*  6 */{ L"&Go to Current Folder",           gotoCurrentFolder,          0,       false,   nullptr},
/*  7 */{ L"&Go to Current File",             gotoCurrentFile,            0,       false,   nullptr},
/*  8 */{ L"Show Explorer (Focus on folder)", showExplorerDialogOnFolder, 0,       false,   nullptr},
/*  9 */{ L"Show Explorer (Focus on file)",   showExplorerDialogOnFile,   0,       false,   nullptr},
/* 10 */{ L"Show Favorites",                  showFavesDialog,            0,       false,   nullptr},
/* 11 */{ L"&Clear Filter",                   clearFilter,                0,       false,   nullptr},
/* 12 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/* 13 */{ L"Explorer &Options...",            openOptionDlg,              0,       false,   nullptr},
/* 14 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/* 15 */{ L"&Help...",                        openHelpDlg,                0,       false,   nullptr},
};

toolbarIcons		g_TBExplorer;
toolbarIcons		g_TBFaves;

/* create classes */
ExplorerDialog		explorerDlg;
FavesDialog			favesDlg;
QuickOpenDlg		quickOpenDlg;
OptionDlg			optionDlg;
HelpDlg				helpDlg;

/* global explorer params */
ExProp				exProp;

/* global favorite params */
TCHAR				szLastElement[MAX_PATH];

/* get system information */
BOOL				isNotepadCreated	= FALSE;

/* section Faves */
CONST TCHAR			LastElement[]		= _T("LastElement");

/* for subclassing */
WNDPROC				wndProcNotepad		= NULL;

/* win version */
winVer				gWinVersion			= WV_UNKNOWN;

/* own image list variables */
std::vector<DrvMap>	gvDrvMap;
HIMAGELIST			ghImgList			= NULL;

/* current open docs */
std::vector<std::wstring>		g_vStrCurrentFiles;



BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  reasonForCall, 
                       LPVOID lpReserved )
{
	g_hModule = hModule;

    switch (reasonForCall)
    {
		case DLL_PROCESS_ATTACH:
		{
			/* Set shortcuts */
			funcItem[0]._pShKey = new ShortcutKey;
			funcItem[0]._pShKey->_isAlt		= true;
			funcItem[0]._pShKey->_isCtrl	= true;
			funcItem[0]._pShKey->_isShift	= true;
			funcItem[0]._pShKey->_key		= 'E';
			funcItem[1]._pShKey = new ShortcutKey;
			funcItem[1]._pShKey->_isAlt		= true;
			funcItem[1]._pShKey->_isCtrl	= true;
			funcItem[1]._pShKey->_isShift	= true;
			funcItem[1]._pShKey->_key		= 'V';
			funcItem[2]._pShKey = new ShortcutKey;
			funcItem[2]._pShKey->_isAlt		= false;
			funcItem[2]._pShKey->_isCtrl	= true;
			funcItem[2]._pShKey->_isShift	= false;
			funcItem[2]._pShKey->_key		= 'P';

			/* set image list and icon */
			ghImgList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 6, 30);
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_FOLDER)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_FILE)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_WEB)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_SESSION)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_GROUP)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_PARENTFOLDER)));

			break;
		}	
		case DLL_PROCESS_DETACH:
		{
			/* save settings */
			saveSettings();

			/* destroy image list */
			ImageList_Destroy(ghImgList);

			/* Remove subclaasing */
			if (wndProcNotepad != NULL)
				SetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC, (LONG_PTR)wndProcNotepad);

			FreeLibrary(hShell32);
	
			delete funcItem[0]._pShKey;
			funcItem[0]._pShKey = nullptr;
			delete funcItem[1]._pShKey;
			funcItem[1]._pShKey = nullptr;
			delete funcItem[2]._pShKey;
			funcItem[2]._pShKey = nullptr;

			if (g_TBExplorer.hToolbarBmp) {
				::DeleteObject(g_TBExplorer.hToolbarBmp);
				g_TBExplorer.hToolbarBmp = nullptr;
			}
			if (g_TBExplorer.hToolbarIcon) {
				::DestroyIcon(g_TBExplorer.hToolbarIcon);
				g_TBExplorer.hToolbarIcon = nullptr;
			}

			if (g_TBFaves.hToolbarBmp) {
				::DeleteObject(g_TBFaves.hToolbarBmp);
				g_TBFaves.hToolbarBmp = nullptr;
			}
			if (g_TBFaves.hToolbarIcon) {
				::DestroyIcon(g_TBFaves.hToolbarIcon);
				g_TBFaves.hToolbarIcon = nullptr;
			}

			break;
		}
		case DLL_THREAD_ATTACH:
			break;
			
		case DLL_THREAD_DETACH:
			break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
	NppInterface::setNppData(notpadPlusData);

	/* stores notepad data */
	nppData = notpadPlusData;

	/* get windows version */
	gWinVersion  = (winVer)::SendMessage(nppData._nppHandle, NPPM_GETWINDOWSVERSION, 0, 0);

	/* load data */
	loadSettings();

	/* initial dialogs */
	explorerDlg.init((HINSTANCE)g_hModule, nppData, &exProp);
	favesDlg.init((HINSTANCE)g_hModule, nppData, szLastElement, &exProp);
	quickOpenDlg.init((HINSTANCE)g_hModule, NppInterface::getWindow(), & exProp);
	optionDlg.init((HINSTANCE)g_hModule, nppData);
	helpDlg.init((HINSTANCE)g_hModule, nppData);

	/* Subclassing for Notepad */
	wndProcNotepad = (WNDPROC)::SetWindowLongPtr(nppData._nppHandle, GWLP_WNDPROC, (LONG_PTR)SubWndProcNotepad);
}

extern "C" __declspec(dllexport) LPCTSTR getName()
{
	return PLUGIN_NAME;
}

extern "C" __declspec(dllexport) FuncItem * getFuncsArray(INT *nbF)
{
	*nbF = _countof(funcItem);
	return funcItem;
}

/***
 *	beNotification()
 *
 *	This function is called, if a notification in Scantilla/Notepad++ occurs
 */
extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
	if ((notifyCode->nmhdr.hwndFrom == nppData._scintillaMainHandle) ||
		(notifyCode->nmhdr.hwndFrom == nppData._scintillaSecondHandle) ||
		(notifyCode->nmhdr.code == TCN_TABDELETE) ||
		(notifyCode->nmhdr.code == TCN_SELCHANGE) ||
		(notifyCode->nmhdr.code == NPPN_FILECLOSED) ||
		(notifyCode->nmhdr.code == NPPN_FILEOPENED)) {
		UpdateDocs();
	}
	if (notifyCode->nmhdr.hwndFrom == nppData._nppHandle) {
		if (notifyCode->nmhdr.code == NPPN_TBMODIFICATION) {
			/* change menu language */
			NLChangeNppMenu((HINSTANCE)g_hModule, nppData._nppHandle, PLUGIN_NAME, funcItem, _countof(funcItem));

			g_TBExplorer.hToolbarBmp = (HBITMAP)::LoadImage((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDB_TB_EXPLORER), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
			g_TBFaves.hToolbarBmp = (HBITMAP)::LoadImage((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDB_TB_FAVES), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
			::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON, (WPARAM)funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, (LPARAM)&g_TBExplorer);
			::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON, (WPARAM)funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, (LPARAM)&g_TBFaves);
		}
		if (notifyCode->nmhdr.code == NPPN_READY) {
			explorerDlg.initFinish();
			favesDlg.initFinish();
			isNotepadCreated = TRUE;
		}
		if (notifyCode->nmhdr.code == NPPN_WORDSTYLESUPDATED) {
			explorerDlg.UpdateColors();
			favesDlg.UpdateColors();
		}
	}
}

/***
 *	messageProc()
 *
 *	This function is called, if a notification from Notepad occurs
 */
extern "C" __declspec(dllexport) LRESULT messageProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
   return TRUE;
}


#ifdef UNICODE
extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}
#endif


/***
 *	ScintillaMsg()
 *
 *	API-Wrapper
 */
LRESULT ScintillaMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	return ::SendMessage(g_HSource, message, wParam, lParam);
}

/***
 *	loadSettings()
 *
 *	Load the parameters for plugin
 */
void loadSettings(void)
{
	/* initialize the config directory */
	::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configPath);

	/* Test if config path exist, if not create */
	if (::PathFileExists(configPath) == FALSE) {
		std::vector<std::wstring> vPaths;
		do {
			vPaths.push_back(configPath);
			::PathRemoveFileSpec(configPath);
		} while (::PathFileExists(configPath) == FALSE);

		for (auto itr = vPaths.crbegin(), end = vPaths.crend(); itr != end; ++itr) {
			_tcscpy(configPath, itr->c_str());
			::CreateDirectory(configPath, NULL);
		}
		vPaths.clear();
	}

	_tcscpy(iniFilePath, configPath);
	_tcscat(iniFilePath, EXPLORER_INI);
	if (::PathFileExists(iniFilePath) == FALSE)	{
		HANDLE	hFile			= NULL;
#ifdef UNICODE
		BYTE	szBOM[]			= {0xFF, 0xFE};
		DWORD	dwByteWritten	= 0;
#endif
			
		if (hFile != INVALID_HANDLE_VALUE) {
			hFile = ::CreateFile(iniFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#ifdef UNICODE
			::WriteFile(hFile, szBOM, sizeof(szBOM), &dwByteWritten, NULL);
#endif
			::CloseHandle(hFile);
		}
	}

	::GetPrivateProfileString(Explorer, LastPath, _T("C:\\"), exProp.szCurrentPath, MAX_PATH, iniFilePath);
	exProp.iSplitterPos				= ::GetPrivateProfileInt(Explorer, SplitterPos, 120, iniFilePath);
	exProp.iSplitterPosHorizontal	= ::GetPrivateProfileInt(Explorer, SplitterPosHor, 200, iniFilePath);
	exProp.bAscending				= ::GetPrivateProfileInt(Explorer, SortAsc, TRUE, iniFilePath);
	exProp.iSortPos					= ::GetPrivateProfileInt(Explorer, SortPos, 0, iniFilePath);
	exProp.iColumnPosName			= ::GetPrivateProfileInt(Explorer, ColPosName, 150, iniFilePath);
	exProp.iColumnPosExt			= ::GetPrivateProfileInt(Explorer, ColPosExt, 50, iniFilePath);
	exProp.iColumnPosSize			= ::GetPrivateProfileInt(Explorer, ColPosSize, 70, iniFilePath);
	exProp.iColumnPosDate			= ::GetPrivateProfileInt(Explorer, ColPosDate, 100, iniFilePath);
	exProp.bShowHidden				= ::GetPrivateProfileInt(Explorer, ShowHiddenData, FALSE, iniFilePath);
	exProp.bViewBraces				= ::GetPrivateProfileInt(Explorer, ShowBraces, TRUE, iniFilePath);
	exProp.bViewLong				= ::GetPrivateProfileInt(Explorer, ShowLongInfo, FALSE, iniFilePath);
	exProp.bAddExtToName			= ::GetPrivateProfileInt(Explorer, AddExtToName, FALSE, iniFilePath);
	exProp.bAutoUpdate				= ::GetPrivateProfileInt(Explorer, AutoUpdate, TRUE, iniFilePath);
	exProp.fmtSize					= (SizeFmt)::GetPrivateProfileInt(Explorer, SizeFormat, SFMT_KBYTE, iniFilePath);
	exProp.fmtDate					= (DateFmt)::GetPrivateProfileInt(Explorer, DateFormat, DFMT_ENG, iniFilePath);
	exProp.uTimeout					= ::GetPrivateProfileInt(Explorer, TimeOut, 1000, iniFilePath);
	exProp.bUseSystemIcons			= ::GetPrivateProfileInt(Explorer, UseSystemIcons, TRUE, iniFilePath);
	::GetPrivateProfileString(Explorer, NppExecAppName, _T("NppExec.dll"), exProp.nppExecProp.szAppName, MAX_PATH, iniFilePath);
	::GetPrivateProfileString(Explorer, NppExecScriptPath, configPath, exProp.nppExecProp.szScriptPath, MAX_PATH, iniFilePath);

	TCHAR	number[3];
	TCHAR	pszTemp[MAX_PATH];
	for (INT i = 0; i < 20; i++) {
		_stprintf(number, _T("%d"), i);
		if (::GetPrivateProfileString(FilterHistory, number, _T(""), pszTemp, MAX_PATH, iniFilePath) != 0) {
			exProp.vStrFilterHistory.push_back(pszTemp);
		}
	}
	::GetPrivateProfileString(Explorer, LastFilter, _T("*.*"), pszTemp, MAX_PATH, iniFilePath);
	exProp.fileFilter.setFilter(pszTemp);

	if (::PathFileExists(exProp.szCurrentPath) == FALSE) {
		_tcscpy(exProp.szCurrentPath, _T("C:\\"));
	}
}

/***
 *	saveSettings()
 *
 *	Saves the parameters for plugin
 */
void saveSettings(void)
{
	TCHAR	temp[256];

	::WritePrivateProfileString(Explorer, LastPath, exProp.szCurrentPath, iniFilePath);
	::WritePrivateProfileString(Explorer, SplitterPos, _itot(exProp.iSplitterPos, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, SplitterPosHor, _itot(exProp.iSplitterPosHorizontal, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, SortAsc, _itot(exProp.bAscending, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, SortPos, _itot(exProp.iSortPos, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ColPosName, _itot(exProp.iColumnPosName, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ColPosExt, _itot(exProp.iColumnPosExt, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ColPosSize, _itot(exProp.iColumnPosSize, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ColPosDate, _itot(exProp.iColumnPosDate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ShowHiddenData, _itot(exProp.bShowHidden, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ShowBraces, _itot(exProp.bViewBraces, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, ShowLongInfo, _itot(exProp.bViewLong, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, AddExtToName, _itot(exProp.bAddExtToName, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, AutoUpdate, _itot(exProp.bAutoUpdate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, SizeFormat, _itot((INT)exProp.fmtSize, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, DateFormat, _itot((INT)exProp.fmtDate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, DateFormat, _itot((INT)exProp.fmtDate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, TimeOut, _itot((INT)exProp.uTimeout, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, UseSystemIcons, _itot(exProp.bUseSystemIcons, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, NppExecAppName, exProp.nppExecProp.szAppName, iniFilePath);
	::WritePrivateProfileString(Explorer, NppExecScriptPath, exProp.nppExecProp.szScriptPath, iniFilePath);


	for (INT i = (INT)exProp.vStrFilterHistory.size() - 1; i >= 0 ; i--)	{
		::WritePrivateProfileString(FilterHistory, _itot(i, temp, 10), exProp.vStrFilterHistory[i].c_str(), iniFilePath);
	}
	::WritePrivateProfileString(Explorer, LastFilter, exProp.fileFilter.getFilterString(), iniFilePath);
}


/***
 *	initMenu()
 *
 *	Initialize the menu
 */
void initMenu(void)
{
}


/***
 *	getCurrentHScintilla()
 *
 *	Get the handle of the current scintilla
 */
HWND getCurrentHScintilla(INT which)
{
	return (which == 0)?nppData._scintillaMainHandle:nppData._scintillaSecondHandle;
}	


/**************************************************************************
 *	Interface functions
 */

void toggleExplorerDialog(void)
{
	UINT state = ::GetMenuState(::GetMenu(nppData._nppHandle), funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, MF_BYCOMMAND);
	if (state & MF_CHECKED) {
		explorerDlg.doDialog(false);
	} else {
		UpdateDocs();
		explorerDlg.doDialog();
	}
}

void toggleFavesDialog(void)
{
	UINT state = ::GetMenuState(::GetMenu(nppData._nppHandle), funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, MF_BYCOMMAND);
	if (state & MF_CHECKED) {
		favesDlg.doDialog(false);
	} else {
		UpdateDocs();
		favesDlg.doDialog();
	}
}

void gotoPath(void)
{
	if (explorerDlg.gotoPath()) {
		explorerDlg.doDialog();
		explorerDlg.setFocusOnFile();
	}
}

void gotoUserFolder(void)
{
	explorerDlg.doDialog();
	explorerDlg.gotoUserFolder();
}

void gotoCurrentFolder(void)
{
	explorerDlg.doDialog();
	explorerDlg.gotoCurrentFolder();
}

void gotoCurrentFile(void)
{
	explorerDlg.doDialog();
	explorerDlg.gotoCurrentFile();
}

void showExplorerDialogOnFolder(void)
{
	explorerDlg.doDialog();
	explorerDlg.setFocusOnFolder();
}

void showExplorerDialogOnFile(void)
{
	explorerDlg.doDialog();
	explorerDlg.setFocusOnFile();
}

void showFavesDialog(void)
{
	favesDlg.doDialog();
}

void clearFilter(void)
{
	explorerDlg.clearFilter();
}

void openOptionDlg(void)
{
	if (optionDlg.doDialog(&exProp) == IDOK) {
		explorerDlg.redraw();
		favesDlg.redraw();
	}
}

void openHelpDlg(void)
{
	helpDlg.doDialog();
}

void openQuickOpenDlg(void)
{
	quickOpenDlg.setCurrentPath(exProp.szCurrentPath);
	quickOpenDlg.show();
}

void openTerminal(void)
{
	std::filesystem::path path(exProp.szCurrentPath);
	::ShellExecute(nppData._nppHandle, _T("open"), _T("cmd.exe"), nullptr, path.c_str(), SW_SHOW);
}

/**************************************************************************
 *	Subclass of Notepad
 */
LRESULT CALLBACK SubWndProcNotepad(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT			ret		= 0;

	switch (message)
	{
		case WM_ACTIVATE:
		{
			if (((LOWORD(wParam) == WA_ACTIVE) || (LOWORD(wParam) == WA_CLICKACTIVE)) && 
				(explorerDlg.isVisible()) && ((HWND)lParam != hWnd))
			{
				if (exProp.bAutoUpdate == TRUE) {
					::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATE);
					::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATE, 200, NULL);
				} else {
					::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATEPATH);
					::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATEPATH, 200, NULL);
				}
			}
			ret = ::CallWindowProc(wndProcNotepad, hWnd, message, wParam, lParam);
			break;
		}
		case WM_DEVICECHANGE:
		{
			if ( (explorerDlg.isVisible()) &&
				((wParam == DBT_DEVICEARRIVAL) || (wParam == DBT_DEVICEREMOVECOMPLETE)))
			{
				::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEDEVICE);
				::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEDEVICE, 1000, NULL);
			}
			ret = ::CallWindowProc(wndProcNotepad, hWnd, message, wParam, lParam);
			break;
		}
		case WM_COMMAND:
		{
			if (wParam == IDM_FILE_SAVESESSION)
			{
				favesDlg.SaveSession();
				return TRUE;
			}
		}
		default:
			ret = ::CallWindowProc(wndProcNotepad, hWnd, message, wParam, lParam);
			break;
	}

	return ret;
}

/**************************************************************************
 *	Functions for file system
 */
BOOL VolumeNameExists(LPTSTR rootDrive, LPTSTR volumeName)
{
	BOOL	bRet = FALSE;

	if ((volumeName[0] != '\0') && (GetVolumeInformation(rootDrive, volumeName, MAX_PATH, NULL, NULL, NULL, NULL, 0) == TRUE))
	{
		bRet = TRUE;
	}
	return bRet;
}

bool IsValidFileName(LPTSTR pszFileName)
{
	if (_tcspbrk(pszFileName, _T("\\/:*?\"<>")) == NULL)
		return true;

	TCHAR	TEMP[128];
	TCHAR	msgBoxTxt[128];

	if (NLGetText((HINSTANCE)g_hModule, nppData._nppHandle, _T("PossibleChars"), TEMP, 128)) {
		_stprintf(msgBoxTxt, TEMP, _T("\n       \\ / : * ? \" < >"));
		::MessageBox(NULL, msgBoxTxt, _T("Error"), MB_OK);
	} else {
		::MessageBox(NULL, _T("Filename does not contain any of this characters:\n       \\ / : * ? \" < >"), _T("Error"), MB_OK);
	}
	return false;
}

bool IsValidFolder(const WIN32_FIND_DATA & Find)
{
	if ((Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
		(!(Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || exProp.bShowHidden) &&
		 (_tcscmp(Find.cFileName, _T(".")) != 0) && 
		 (_tcscmp(Find.cFileName, _T("..")) != 0) &&
		 (Find.cFileName[0] != '?'))
		return true;

	return false;
}

bool IsValidParentFolder(const WIN32_FIND_DATA & Find)
{
	if ((Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
		(!(Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || exProp.bShowHidden) &&
		 (_tcscmp(Find.cFileName, _T(".")) != 0) &&
		 (Find.cFileName[0] != '?'))
		return true;

	return false;
}

bool IsValidFile(const WIN32_FIND_DATA & Find)
{
	if (!(Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
		(!(Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || exProp.bShowHidden))
		return true;

	return false;
}

BOOL HaveChildren(LPTSTR parentFolderPathName)
{
	WIN32_FIND_DATA		Find		= {0};
	HANDLE				hFind		= NULL;
	BOOL				bFound		= TRUE;
	BOOL				bRet		= FALSE;

	if (parentFolderPathName[_tcslen(parentFolderPathName) - 1] != '\\')
		_tcscat(parentFolderPathName, _T("\\"));

	/* add wildcard */
	_tcscat(parentFolderPathName, _T("*"));

	if ((hFind = ::FindFirstFile(parentFolderPathName, &Find)) == INVALID_HANDLE_VALUE)
		return FALSE;

	do
	{
		if (IsValidFolder(Find) == TRUE)
		{
			bFound = FALSE;
			bRet = TRUE;
		}
	} while ((FindNextFile(hFind, &Find)) && (bFound == TRUE));

	::FindClose(hFind);

	return bRet;
}

BOOL ConvertNetPathName(LPCTSTR pPathName, LPTSTR pRemotePath, UINT length)
{
	DWORD			dwRemoteLength	= 0;
	DWORD			driveList		= ::GetLogicalDrives();
	TCHAR			drivePathName[]	= _T(" :");	// it is longer for function 'HaveChildren()'
	TCHAR			volumeName[MAX_PATH];
	TCHAR			remoteName[MAX_PATH];

	for (int i = 1; i < 32; i++)
	{
		drivePathName[0] = 'A' + i;

		if (0x01 & (driveList >> i))
		{
			_stprintf(volumeName, _T("%c:"), 'A' + i);

			/* call get connection twice to get the real size */
			dwRemoteLength = 1;
			if (ERROR_MORE_DATA == WNetGetConnection(volumeName, remoteName, &dwRemoteLength))
			{
				if ((dwRemoteLength < MAX_PATH) && (NO_ERROR == WNetGetConnection(volumeName, remoteName, &dwRemoteLength)))
				{
					if (_tcsstr(pPathName, remoteName) != NULL)
					{
						_tcscpy(pRemotePath, volumeName);
						_tcsncat(pRemotePath, &pPathName[dwRemoteLength - 1], length - 2);
						return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}

/**************************************************************************
 *	Get system images
 */
HIMAGELIST GetSmallImageList(BOOL bSystem)
{
	static
	HIMAGELIST		himlSys	= NULL;
	HIMAGELIST		himl	= NULL;
	SHFILEINFO		sfi		= {0};

	if (bSystem) {
		if (himlSys == NULL) {
			himlSys = (HIMAGELIST)SHGetFileInfo(_T("C:\\"), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
		}
		himl = himlSys;
	} else {
		himl = ghImgList;
	}

	return himl;
}

void ExtractIcons(LPCTSTR currentPath, LPCTSTR volumeName, DevType type, 
	LPINT piIconNormal, LPINT piIconSelected, LPINT piIconOverlayed)
{
	SHFILEINFO		sfi	= {0};
	SIZE_T			length = _tcslen(currentPath) - 1;
	TCHAR			TEMP[MAX_PATH];
	UINT			stOverlay = (piIconOverlayed ? SHGFI_OVERLAYINDEX : 0);

	_tcscpy(TEMP, currentPath);
	if (TEMP[length] == '*') {
		TEMP[length] = '\0';
	} else if (TEMP[length] != '\\') {
		_tcscat(TEMP, _T("\\"));
	}
	if (volumeName != NULL) {
		_tcscat(TEMP, volumeName);
	}

	if (_tcsstr(TEMP, _T("C:\\Users\\Public\\"))) {
#ifdef _DEBUG
		OutputDebugString(TEMP);
		OutputDebugString(_T("\n"));
		OutputDebugString(currentPath);
		OutputDebugString(_T(" "));
		OutputDebugString(volumeName);
		OutputDebugString(_T("\n"));
#endif
	}

	if (exProp.bUseSystemIcons == FALSE)
	{
		/* get drive icon in any case correct */
		if (type == DEVT_DRIVE)
		{
			::ZeroMemory(&sfi, sizeof(SHFILEINFO));
			SHGetFileInfo(TEMP, 
				-1,
				&sfi, 
				sizeof(SHFILEINFO), 
				SHGFI_ICON | SHGFI_SMALLICON | stOverlay);
			::DestroyIcon(sfi.hIcon);

			::ZeroMemory(&sfi, sizeof(SHFILEINFO));
			SHGetFileInfo(TEMP, 
				FILE_ATTRIBUTE_NORMAL, 
				&sfi, 
				sizeof(SHFILEINFO), 
				SHGFI_ICON | SHGFI_SMALLICON | stOverlay | SHGFI_USEFILEATTRIBUTES);

			/* find drive icon in own image list */
			UINT	onPos = 0;
			for (UINT i = 0; i < gvDrvMap.size(); i++) {
				if (gvDrvMap[i].cDrv == TEMP[0]) {
					onPos = gvDrvMap[i].pos;
					break;
				}
			}
			/* if not found add to list otherwise replace new drive icon */
			if (onPos == 0) {
				onPos = ImageList_AddIcon(ghImgList, sfi.hIcon);
				DrvMap drvMap = {TEMP[0], onPos};
				gvDrvMap.push_back(drvMap);
			} else {
				ImageList_ReplaceIcon(ghImgList, onPos, sfi.hIcon);
			}
			::DestroyIcon(sfi.hIcon);

			*piIconNormal	= onPos;
			*piIconSelected	= onPos;
		}
		else if (type == DEVT_DIRECTORY)
		{
			*piIconNormal	= ICON_FOLDER;
			*piIconSelected	= ICON_FOLDER;
		}
		else
		{
			*piIconNormal	= ICON_FILE;
			*piIconSelected	= ICON_FILE;
		}
		if (piIconOverlayed != NULL)
			*piIconOverlayed = 0;
	}
	else
	{
		/* get normal and overlayed icon */
		if ((type == DEVT_DIRECTORY) || (type == DEVT_DRIVE))
		{
			::ZeroMemory(&sfi, sizeof(SHFILEINFO));
			SHGetFileInfo(TEMP, 
				-1,
				&sfi, 
				sizeof(SHFILEINFO), 
				SHGFI_ICON | SHGFI_SMALLICON | stOverlay);
			::DestroyIcon(sfi.hIcon);

			if (type == DEVT_DRIVE)
			{	
				::ZeroMemory(&sfi, sizeof(SHFILEINFO));
				SHGetFileInfo(TEMP, 
					FILE_ATTRIBUTE_NORMAL, 
					&sfi, 
					sizeof(SHFILEINFO), 
					SHGFI_ICON | SHGFI_SMALLICON | stOverlay | SHGFI_USEFILEATTRIBUTES);
				::DestroyIcon(sfi.hIcon);
			}
		}
		else
		{
			::ZeroMemory(&sfi, sizeof(SHFILEINFO));
			SHGetFileInfo(TEMP, 
				FILE_ATTRIBUTE_NORMAL, 
				&sfi, 
				sizeof(SHFILEINFO), 
				SHGFI_ICON | SHGFI_SMALLICON | stOverlay | SHGFI_USEFILEATTRIBUTES);
			::DestroyIcon(sfi.hIcon);
		}

		*piIconNormal	= sfi.iIcon & 0x00ffffff;
		if (piIconOverlayed != NULL)
			*piIconOverlayed = sfi.iIcon >> 24;

		/* get selected (open) icon */
		if (type == DEVT_DIRECTORY)
		{
			::ZeroMemory(&sfi, sizeof(SHFILEINFO));
			SHGetFileInfo(TEMP, 
				0,
				&sfi, 
				sizeof(SHFILEINFO), 
				SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON);
			::DestroyIcon(sfi.hIcon);

			*piIconSelected = sfi.iIcon;
		}
		else
		{
			*piIconSelected = *piIconNormal;
		}
	}

	if (_tcsstr(TEMP, _T("C:\\Users\\Public\\"))) {
		_stprintf(TEMP, _T("  TYPE   %d\n  Normal %d\n  Selected %d\n"), type, *piIconNormal, *piIconSelected);
		OutputDebugString(TEMP);
	}
}

/**************************************************************************
 *	Resolve files if they are shortcuts
 */
HRESULT ResolveShortCut(LPCTSTR lpszShortcutPath, LPTSTR lpszFilePath, int maxBuf)
{
    HRESULT hRes = S_FALSE;
    CComPtr<IShellLink> ipShellLink;
    lpszFilePath[0] = '\0';

    // Get a pointer to the IShellLink interface
    hRes = CoCreateInstance(CLSID_ShellLink,
                            NULL, 
                            CLSCTX_INPROC_SERVER,
                            IID_IShellLink,
                            (void**)&ipShellLink); 

    if (hRes == S_OK) 
    { 
        // Get a pointer to the IPersistFile interface
        CComQIPtr<IPersistFile> ipPersistFile(ipShellLink);

        // IPersistFile is using LPCOLESTR, so make sure that the string is Unicode
		WCHAR wszTemp[MAX_PATH];
#if !defined UNICODE
        MultiByteToWideChar(CP_ACP, 0, lpszShortcutPath, -1, wszTemp, MAX_PATH);
#else
        wcsncpy(wszTemp, lpszShortcutPath, MAX_PATH);
#endif

        // Open the shortcut file and initialize it from its contents
        hRes = ipPersistFile->Load(wszTemp, STGM_READ); 
        if (hRes == S_OK) 
        {
            // Try to find the target of a shortcut, even if it has been moved or renamed
            hRes = ipShellLink->Resolve(NULL, SLR_UPDATE); 
            if (hRes == S_OK) 
            {
                // Get the path to the shortcut target
				TCHAR szPath[MAX_PATH];
                hRes = ipShellLink->GetPath(szPath, MAX_PATH, nullptr, SLGP_RAWPATH); 
				if (hRes == S_OK) 
				{
	                _tcsncpy(lpszFilePath, szPath, maxBuf);

					if (::PathIsDirectory(lpszFilePath) != FALSE) {
						if (lpszFilePath[wcslen(lpszFilePath) - 1] != '\\') {
							wcsncat(lpszFilePath, L"\\", MAX_PATH);
						}
					}
				}
            } 
        } 
    } 

    return hRes;
}


/**************************************************************************
 *	Current docs
 */
void UpdateDocs(void)
{
	static
	UINT	currentDoc;
	UINT	currentEdit;

	::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
	g_HSource = (currentEdit == 0)?nppData._scintillaMainHandle:nppData._scintillaSecondHandle;

	INT			newDocCnt = 0;
	TCHAR		newPath[MAX_PATH];

	/* update open files */
	::SendMessage(nppData._nppHandle, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)newPath);
	newDocCnt = (INT)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);

	if ((_tcscmp(newPath, g_currentFile) != 0) || (newDocCnt != g_docCnt))
	{
		/* update current path in explorer and favorites */
		_tcscpy(g_currentFile, newPath);
		g_docCnt = newDocCnt;
		explorerDlg.NotifyNewFile();
		favesDlg.NotifyNewFile();

		/* update documents list */
		INT			i = 0;
		LPTSTR		*fileNames;

		INT docCnt = (INT)::SendMessage(nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);

		/* update doc information for file list () */
		fileNames	= (LPTSTR*)new LPTSTR[docCnt];

		for (i = 0; i < docCnt; i++)
			fileNames[i] = (LPTSTR)new TCHAR[MAX_PATH];

		if (::SendMessage(nppData._nppHandle, NPPM_GETOPENFILENAMES, (WPARAM)fileNames, (LPARAM)docCnt)) {
			if (explorerDlg.isVisible() || favesDlg.isVisible()) {
				UpdateCurrUsedDocs(fileNames, docCnt);
			}
			if (explorerDlg.isVisible()) {
				RedrawWindow(explorerDlg.getHSelf(), NULL, NULL, TRUE);
			}
			if (favesDlg.isVisible()) {
				RedrawWindow(favesDlg.getHSelf(), NULL, NULL, TRUE);
			}
		}

		for (i = 0; i < docCnt; i++)
			delete [] fileNames[i];
		delete [] fileNames;

		currentDoc = docCnt;
	}
}

void UpdateCurrUsedDocs(LPTSTR* pFiles, UINT numFiles)
{
	TCHAR	pszLongName[MAX_PATH];

	/* clear old elements */
	g_vStrCurrentFiles.clear();

	for (UINT i = 0; i < numFiles; i++)
	{
		/* store only long pathes */
		if (GetLongPathName(pFiles[i], pszLongName, MAX_PATH) != 0)
		{
			g_vStrCurrentFiles.push_back(pszLongName);
		}
	}
}

BOOL IsFileOpen(LPCTSTR pCurrFile)
{
	for (UINT iCurrFiles = 0; iCurrFiles < g_vStrCurrentFiles.size(); iCurrFiles++)
	{
		if (_tcsicmp(g_vStrCurrentFiles[iCurrFiles].c_str(), pCurrFile) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**************************************************************************
 *	compare arguments and convert
 */
BOOL ConvertCall(LPTSTR pszExplArg, LPTSTR pszName, LPTSTR *p_pszNppArg, std::vector<std::wstring> vFileList)
{
	TCHAR			szElement[MAX_PATH];
	LPTSTR			pszPtr		= NULL;
	LPTSTR			pszEnd		= NULL;
	UINT			iCount		= 0;
	VarExNppExec	varElement	= VAR_UNKNOWN;

	/* get name of NppExec plugin */
	pszPtr = _tcstok(pszExplArg, _T(" "));

	if (_tcscmp(pszPtr, _T("//Explorer:")) != 0) {
		TCHAR	szTemp[MAX_PATH];
		_stprintf(szTemp, _T("Format of first line needs to be:\n//Explorer: NPPEXEC_DLL_NAME PARAM_X[0] PARAM_X[1]"));
		::MessageBox(nppData._nppHandle, szTemp, _T("Error"), MB_OK | MB_ICONERROR);
		return FALSE;
	}

	pszPtr = _tcstok(NULL, _T(" "));
	_tcscpy(pszName, pszPtr);

	pszPtr = _tcstok(NULL, _T(" "));

	while (pszPtr != NULL)
	{
		/* find string element in explorer argument list */
		if (_tcsstr(pszPtr, cVarExNppExec[VAR_FULL_PATH]) != NULL) {
			varElement = VAR_FULL_PATH;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_ROOT_PATH]) != NULL) {
			varElement = VAR_ROOT_PATH;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_PARENT_FULL_DIR]) != NULL) {
			varElement = VAR_PARENT_FULL_DIR;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_PARENT_DIR]) != NULL) {
			varElement = VAR_PARENT_DIR;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FULL_FILE]) != NULL) {
			varElement = VAR_FULL_FILE;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FILE_NAME]) != NULL) {
			varElement = VAR_FILE_NAME;
		} else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FILE_EXT]) != NULL) {
			varElement = VAR_FILE_EXT;
		} else {
			TCHAR	szTemp[MAX_PATH];
			_stprintf(szTemp, _T("Argument \"%s\" unknown."), pszPtr);
			::MessageBox(nppData._nppHandle, szTemp, _T("Error"), MB_OK | MB_ICONERROR);

			return FALSE;
		}

		/* get array list position of given files. Note: of by 1 because of '[' */
		pszPtr += _tcslen(cVarExNppExec[varElement]) + 1;

		pszEnd = _tcsstr(pszPtr, _T("]"));

		if (pszEnd == NULL) {
			return FALSE;
		} else{
			*pszEnd = NULL;
		}

		/* control if file element exist */
		iCount = _ttoi(pszPtr);
		if (iCount > vFileList.size())
		{
			TCHAR	szTemp[MAX_PATH];
			_stprintf(szTemp, _T("Element \"%d\" of argument \"%s\" not selected."), iCount, pszPtr);
			::MessageBox(nppData._nppHandle, szTemp, _T("Error"), MB_OK | MB_ICONERROR);
			return FALSE;
		}

		BOOL	cpyDone	= TRUE;

		/* copy full file in any case to element array */
		_tcscpy(szElement, vFileList[iCount].c_str());
		::PathRemoveBackslash(szElement);

		/* copy file element to argument list as requested */
		switch (varElement)
		{
			case VAR_FULL_PATH:
				/* nothing to do, because copy still done */
				break;
			case VAR_ROOT_PATH:
				cpyDone = ::PathStripToRoot(szElement);
				break;
			case VAR_PARENT_FULL_DIR:
				if (::PathRemoveFileSpec(szElement) == 0) {
					cpyDone = FALSE;
				}
				break;
			case VAR_PARENT_DIR:
				if (::PathRemoveFileSpec(szElement) != 0) {
					::PathStripPath(szElement);
				} else {
					cpyDone = FALSE;
				}
				break;
			case VAR_FULL_FILE:
				::PathStripPath(szElement);
				break;
			case VAR_FILE_NAME:
				::PathStripPath(szElement);
				if (*(::PathFindExtension(szElement)) != 0) {
					*(::PathFindExtension(szElement)) = 0;
				} else {
					cpyDone = FALSE;
				}
				break;
			case VAR_FILE_EXT:
				if (*(::PathFindExtension(szElement)) != 0) {
					_tcscpy(szElement, ::PathFindExtension(vFileList[iCount].c_str()) + 1);
				} else {
					cpyDone = FALSE;
				}
				break;
		}

		if (cpyDone == TRUE)
		{
			/* concatinate arguments finally */
			LPTSTR	pszTemp	= *p_pszNppArg;

			if (*p_pszNppArg != NULL) {
				*p_pszNppArg = (LPTSTR)new TCHAR[_tcslen(pszTemp) + _tcslen(szElement) + 4];
			} else {
				*p_pszNppArg = (LPTSTR)new TCHAR[_tcslen(szElement) + 3];
			}

			if (*p_pszNppArg != NULL)
			{
				if (pszTemp != NULL) {
					_stprintf(*p_pszNppArg, _T("%s \"%s\""), pszTemp, szElement);
					delete [] pszTemp;
				} else {
					_stprintf(*p_pszNppArg, _T("\"%s\""), szElement);
				}
			}
			else
			{
				if (pszTemp != NULL) {
					delete [] pszTemp;
				}
				return FALSE;
			}
		}

		pszPtr = _tcstok(NULL, _T(" "));
	}

	return TRUE;
}


/**************************************************************************
 *	Scroll up/down test function
 */
ScDir GetScrollDirection(HWND hWnd, UINT offTop, UINT offBottom)
{
	RECT	rcUp	= {0};
	RECT	rcDown	= {0};

	::GetClientRect(hWnd, &rcUp);
	::ClientToScreen(hWnd, &rcUp);
	rcDown = rcUp;

	rcUp.top += offTop;
	rcUp.bottom = rcUp.top + 20;
	rcDown.bottom += offBottom;
	rcDown.top = rcDown.bottom - 20;

	POINT	pt		= {0};
	::GetCursorPos(&pt);
	if (::PtInRect(&rcUp, pt) == TRUE)
		return SCR_UP;
	else if (::PtInRect(&rcDown, pt) == TRUE)
		return SCR_DOWN;
	return SCR_OUTSIDE;
}

/**************************************************************************
 *	Windows helper functions
 */
void ClientToScreen(HWND hWnd, RECT* rect)
{
	POINT		pt;

	pt.x		 = rect->left;
	pt.y		 = rect->top;
	::ClientToScreen( hWnd, &pt );
	rect->left   = pt.x;
	rect->top    = pt.y;

	pt.x		 = rect->right;
	pt.y		 = rect->bottom;
	::ClientToScreen( hWnd, &pt );
	rect->right  = pt.x;
	rect->bottom = pt.y;
}

void ScreenToClient(HWND hWnd, RECT* rect)
{
	POINT		pt;

	pt.x		 = rect->left;
	pt.y		 = rect->top;
	::ScreenToClient( hWnd, &pt );
	rect->left   = pt.x;
	rect->top    = pt.y;

	pt.x		 = rect->right;
	pt.y		 = rect->bottom;
	::ScreenToClient( hWnd, &pt );
	rect->right  = pt.x;
	rect->bottom = pt.y;
}

void ErrorMessage(DWORD err)
{
	LPVOID	lpMsgBuf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) & lpMsgBuf, 0, NULL);	// Process any inserts in lpMsgBuf.
	::MessageBox(NULL, (LPCTSTR) lpMsgBuf, _T("Error"), MB_OK | MB_ICONINFORMATION);

	LocalFree(lpMsgBuf);
}
