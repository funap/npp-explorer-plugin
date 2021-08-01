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
CONST WCHAR  PLUGIN_NAME[]		= L"&Explorer";

TCHAR		configPath[MAX_PATH];
TCHAR		iniFilePath[MAX_PATH];

/* ini file sections */
CONST WCHAR WindowData[]		= L"WindowData";
CONST WCHAR Explorer[]			= L"Explorer";
CONST WCHAR Faves[]				= L"Faves";



/* section Explorer */
CONST WCHAR LastPath[]			= L"LastPath";
CONST WCHAR SplitterPos[]		= L"SplitterPos";
CONST WCHAR SplitterPosHor[]	= L"SplitterPosHor";
CONST WCHAR SortAsc[]			= L"SortAsc";
CONST WCHAR SortPos[]			= L"SortPos";
CONST WCHAR ColPosName[]		= L"ColPosName";
CONST WCHAR ColPosExt[]			= L"ColPosExt";
CONST WCHAR ColPosSize[]		= L"ColPosSize";
CONST WCHAR ColPosDate[]		= L"ColPosDate";
CONST WCHAR ShowHiddenData[]	= L"ShowHiddenData";
CONST WCHAR ShowBraces[]		= L"ShowBraces";
CONST WCHAR ShowLongInfo[]		= L"ShowLongInfo";
CONST WCHAR AddExtToName[]		= L"AddExtToName";
CONST WCHAR AutoUpdate[]		= L"AutoUpdate";
CONST WCHAR AutoNavigate[]		= L"AutoNavigate";
CONST WCHAR SizeFormat[]		= L"SizeFormat";
CONST WCHAR DateFormat[]		= L"DateFormat";
CONST WCHAR FilterHistory[]		= L"FilterHistory";
CONST WCHAR LastFilter[]		= L"LastFilter";
CONST WCHAR TimeOut[]			= L"TimeOut";
CONST WCHAR UseSystemIcons[]	= L"UseSystemIcons";
CONST WCHAR NppExecAppName[]	= L"NppExecAppName";
CONST WCHAR NppExecScriptPath[]	= L"NppExecScriptPath";
CONST WCHAR CphProgramName[]	= L"CphProgramName";
CONST WCHAR MaxHistorySize[]	= L"MaxHistorySize";
CONST WCHAR FontHeight[]		= L"FontHeight";
CONST WCHAR FontWeight[]		= L"FontWeight";
CONST WCHAR FontItalic[]		= L"FontItalic";
CONST WCHAR FontFaceName[]		= L"FontFaceName";


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

toolbarIcons				g_TBExplorer;
toolbarIcons				g_TBFaves;
toolbarIconsWithDarkMode	g_TBExplorerWithDarkMode;
toolbarIconsWithDarkMode	g_TBFavesWithDarkMode;

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
CONST WCHAR			LastElement[]		= L"LastElement";

/* for subclassing */
WNDPROC				wndProcNotepad		= NULL;

/* win version */
winVer				gWinVersion			= winVer::WV_UNKNOWN;

/* own image list variables */
std::vector<DrvMap>	gvDrvMap;
HIMAGELIST			ghImgList			= NULL;

/* current open docs */
std::vector<std::wstring>		g_vStrCurrentFiles;

void initializeFonts();

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
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_WARN_SESSION)));
			ImageList_AddIcon(ghImgList, ::LoadIcon((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDI_MISSING_FILE)));

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

			::DeleteObject(exProp.defaultFont);
			::DeleteObject(exProp.underlineFont);

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

			if (g_TBExplorerWithDarkMode.hToolbarBmp) {
				::DeleteObject(g_TBExplorerWithDarkMode.hToolbarBmp);
				g_TBExplorerWithDarkMode.hToolbarBmp = nullptr;
			}

			if (g_TBExplorerWithDarkMode.hToolbarIcon) {
				::DeleteObject(g_TBExplorerWithDarkMode.hToolbarIcon);
				g_TBExplorerWithDarkMode.hToolbarIcon = nullptr;
			}

			if (g_TBExplorerWithDarkMode.hToolbarIconDarkMode) {
				::DeleteObject(g_TBExplorerWithDarkMode.hToolbarIconDarkMode);
				g_TBExplorerWithDarkMode.hToolbarIconDarkMode = nullptr;
			}

			if (g_TBFavesWithDarkMode.hToolbarBmp) {
				::DeleteObject(g_TBFavesWithDarkMode.hToolbarBmp);
				g_TBFavesWithDarkMode.hToolbarBmp = nullptr;
			}

			if (g_TBFavesWithDarkMode.hToolbarIcon) {
				::DeleteObject(g_TBFavesWithDarkMode.hToolbarIcon);
				g_TBFavesWithDarkMode.hToolbarIcon = nullptr;
			}

			if (g_TBFavesWithDarkMode.hToolbarIconDarkMode) {
				::DeleteObject(g_TBFavesWithDarkMode.hToolbarIconDarkMode);
				g_TBFavesWithDarkMode.hToolbarIconDarkMode = nullptr;
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
	initializeFonts();

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
	switch (notifyCode->nmhdr.code) {
	case TCN_TABDELETE:
	case TCN_SELCHANGE:
	case NPPN_FILECLOSED:
	case NPPN_FILEOPENED:
		UpdateDocs();
		break;
	default:
		break;
	}

	if (notifyCode->nmhdr.hwndFrom == nppData._nppHandle) {
		if (notifyCode->nmhdr.code == NPPN_TBMODIFICATION) {
			/* change menu language */
			const HINSTANCE& hInst = reinterpret_cast<const HINSTANCE>(g_hModule);
			NLChangeNppMenu(hInst, nppData._nppHandle, PLUGIN_NAME, funcItem, _countof(funcItem));

			if (NppInterface::isSupportFluentUI()) {
				g_TBExplorerWithDarkMode.hToolbarBmp			= (HBITMAP)	::LoadImage(hInst, MAKEINTRESOURCE(IDB_TB_EXPLORER), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				g_TBExplorerWithDarkMode.hToolbarIcon			= (HICON)	::LoadImage(hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_EXPLORER), IMAGE_ICON, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				g_TBExplorerWithDarkMode.hToolbarIconDarkMode	= (HICON)	::LoadImage(hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_EXPLORER_DARKMODE), IMAGE_ICON, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE, (WPARAM)funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, (LPARAM)&g_TBExplorerWithDarkMode);

				g_TBFavesWithDarkMode.hToolbarBmp			= (HBITMAP)	::LoadImage(hInst, MAKEINTRESOURCE(IDB_TB_FAVES), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				g_TBFavesWithDarkMode.hToolbarIcon			= (HICON)	::LoadImage(hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES), IMAGE_ICON, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				g_TBFavesWithDarkMode.hToolbarIconDarkMode	= (HICON)	::LoadImage(hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES_DARKMODE), IMAGE_ICON, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE, (WPARAM)funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, (LPARAM)&g_TBFavesWithDarkMode);
			}
			else {
				g_TBExplorer.hToolbarBmp	= (HBITMAP)::LoadImage((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDB_TB_EXPLORER), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_DEPRECATED, (WPARAM)funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, (LPARAM)&g_TBExplorer);

				g_TBFaves.hToolbarBmp		= (HBITMAP)::LoadImage((HINSTANCE)g_hModule, MAKEINTRESOURCE(IDB_TB_FAVES), IMAGE_BITMAP, 0, 0, (LR_DEFAULTSIZE | LR_LOADMAP3DCOLORS));
				::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_DEPRECATED, (WPARAM)funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, (LPARAM)&g_TBFaves);
			}
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
        if (notifyCode->nmhdr.code == NPPN_BUFFERACTIVATED) {
            if (exProp.bAutoNavigate == TRUE) {
                ::KillTimer(explorerDlg.getHSelf(), EXT_AUTOGOTOFILE);
    		    ::SetTimer(explorerDlg.getHSelf(), EXT_AUTOGOTOFILE, 200, NULL);
            }
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

extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}


/***
 *	ScintillaMsg()
 *
 *	API-Wrapper
 */
LRESULT ScintillaMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	return ::SendMessage(g_HSource, message, wParam, lParam);
}


void initializeFonts()
{
	if (exProp.defaultFont) {
		::DeleteObject(exProp.defaultFont);
	}
	if (exProp.underlineFont) {
		::DeleteObject(exProp.underlineFont);
	}
	exProp.defaultFont = ::CreateFontIndirect(&exProp.logfont);
	LOGFONT	logfontUnder = exProp.logfont;
	logfontUnder.lfUnderline = TRUE;
	exProp.underlineFont = ::CreateFontIndirect(&logfontUnder);
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
		BYTE	szBOM[]			= {0xFF, 0xFE};
		DWORD	dwByteWritten	= 0;
			
		if (hFile != INVALID_HANDLE_VALUE) {
			hFile = ::CreateFile(iniFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			::WriteFile(hFile, szBOM, sizeof(szBOM), &dwByteWritten, NULL);
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
	exProp.bAutoNavigate			= ::GetPrivateProfileInt(Explorer, AutoNavigate, FALSE, iniFilePath);
	exProp.fmtSize					= (SizeFmt)::GetPrivateProfileInt(Explorer, SizeFormat, SizeFmt::SFMT_KBYTE, iniFilePath);
	exProp.fmtDate					= (DateFmt)::GetPrivateProfileInt(Explorer, DateFormat, DFMT_ENG, iniFilePath);
	exProp.uTimeout					= ::GetPrivateProfileInt(Explorer, TimeOut, 1000, iniFilePath);
	exProp.bUseSystemIcons			= ::GetPrivateProfileInt(Explorer, UseSystemIcons, TRUE, iniFilePath);
	exProp.maxHistorySize			= ::GetPrivateProfileInt(Explorer, MaxHistorySize, 50, iniFilePath);
	::GetPrivateProfileString(Explorer, NppExecAppName, _T("NppExec.dll"), exProp.nppExecProp.szAppName, MAX_PATH, iniFilePath);
	::GetPrivateProfileString(Explorer, NppExecScriptPath, configPath, exProp.nppExecProp.szScriptPath, MAX_PATH, iniFilePath);
	::GetPrivateProfileString(Explorer, CphProgramName, _T("cmd.exe"), exProp.cphProgram.szAppName, MAX_PATH, iniFilePath);

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

	// get default font
	SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &exProp.logfont, 0);

	INT fontHeight = ::GetPrivateProfileInt(Explorer, FontHeight, 0, iniFilePath);
	if (fontHeight != 0) {
		exProp.logfont.lfHeight = fontHeight;
	}
	
	INT fontWeight = ::GetPrivateProfileInt(Explorer, FontWeight, 0, iniFilePath);
	if (fontWeight != 0) {
		exProp.logfont.lfWeight = fontWeight;
	}

	INT fontItalic = ::GetPrivateProfileInt(Explorer, FontItalic, 0, iniFilePath);
	if (fontItalic != 0) {
		exProp.logfont.lfItalic = TRUE;
	}

	WCHAR fontFaceName[LF_FACESIZE] = {};
	::GetPrivateProfileString(Explorer, FontFaceName, _T(""), fontFaceName, LF_FACESIZE, iniFilePath);
	if (wcslen(fontFaceName) > 0) {
		wcsncpy(exProp.logfont.lfFaceName, fontFaceName, LF_FACESIZE);
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
	::WritePrivateProfileString(Explorer, AutoNavigate, _itot(exProp.bAutoNavigate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, SizeFormat, _itot((INT)exProp.fmtSize, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, DateFormat, _itot((INT)exProp.fmtDate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, DateFormat, _itot((INT)exProp.fmtDate, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, TimeOut, _itot((INT)exProp.uTimeout, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, UseSystemIcons, _itot(exProp.bUseSystemIcons, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, NppExecAppName, exProp.nppExecProp.szAppName, iniFilePath);
	::WritePrivateProfileString(Explorer, NppExecScriptPath, exProp.nppExecProp.szScriptPath, iniFilePath);
	::WritePrivateProfileString(Explorer, CphProgramName, exProp.cphProgram.szAppName, iniFilePath);
	::WritePrivateProfileString(Explorer, MaxHistorySize, std::to_wstring(exProp.maxHistorySize).c_str(), iniFilePath);

	::WritePrivateProfileString(Explorer, FontHeight, _itot(exProp.logfont.lfHeight, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, FontWeight, _itot(exProp.logfont.lfWeight, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, FontItalic, _itot(exProp.logfont.lfItalic, temp, 10), iniFilePath);
	::WritePrivateProfileString(Explorer, FontFaceName, exProp.logfont.lfFaceName, iniFilePath);

	::WritePrivateProfileString(FilterHistory, nullptr, nullptr, iniFilePath);
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
	if (explorerDlg.isVisible()) {
		explorerDlg.doDialog(false);
	} else {
		UpdateDocs();
		explorerDlg.doDialog();
	}
}

void toggleFavesDialog(void)
{
	if (favesDlg.isVisible()) {
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
		initializeFonts();
		explorerDlg.SetFont(exProp.defaultFont);
		favesDlg.SetFont(exProp.defaultFont);

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
	::ShellExecute(nppData._nppHandle, _T("open"), exProp.cphProgram.szAppName, nullptr, path.c_str(), SW_SHOW);
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

BOOL HaveChildren(const std::wstring &folderPath)
{
	WIN32_FIND_DATA		Find = { 0 };
	HANDLE				hFind = NULL;
	BOOL				bFound = TRUE;
	BOOL				bRet = FALSE;

	std::wstring searchPath = folderPath;
	if (searchPath.back() != '\\') {
		searchPath.append(L"\\");
	}
	/* add wildcard */
	searchPath.append(L"*");

	if ((hFind = ::FindFirstFile(searchPath.c_str(), &Find)) == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	do {
		if (IsValidFolder(Find) == TRUE) {
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
	TCHAR			volumeName[MAX_PATH];
	TCHAR			remoteName[MAX_PATH];

	for (INT i = 0; i < 26; ++i) {
		if (0x01 & (driveList >> i)) {
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
}

/**************************************************************************
 *	Resolve files if they are shortcuts
 */
HRESULT ResolveShortCut(const std::wstring &shortcutPath, LPTSTR lpszFilePath, int maxBuf)
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

        // Open the shortcut file and initialize it from its contents
        hRes = ipPersistFile->Load(shortcutPath.c_str(), STGM_READ);
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

BOOL IsFileOpen(const std::wstring &filePath)
{
	for (const auto &openedFilePath : g_vStrCurrentFiles) {
		if (_tcsicmp(openedFilePath.c_str(), filePath.c_str()) == 0) {
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
