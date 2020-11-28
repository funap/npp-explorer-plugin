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


#ifndef EXPLORER_H
#define EXPLORER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <tchar.h>

#include "PluginInterface.h"
#include "Notepad_plus_rc.h"
#include "NativeLang_def.h"
#include "FileFilter.h"

#include <TCHAR.H>
#include <vector>
#include <string>


#define DOCKABLE_EXPLORER_INDEX		0
#define DOCKABLE_FAVORTIES_INDEX	1


extern enum winVer gWinVersion;

/************* some global defines ********************/

#define DND_SCR_TIMEOUT		200

constexpr CHAR SHORTCUT_ALL		= 0x01;
constexpr CHAR SHORTCUT_DELETE	= 0x04;
constexpr CHAR SHORTCUT_COPY	= 0x03;
constexpr CHAR SHORTCUT_PASTE	= 0x16;
constexpr CHAR SHORTCUT_CUT		= 0x18;
constexpr CHAR SHORTCUT_REFRESH	= 0x12;

/******************** faves ***************************/

constexpr INT ICON_FOLDER		= 0;
constexpr INT ICON_FILE			= 1;
constexpr INT ICON_WEB			= 2;
constexpr INT ICON_SESSION		= 3;
constexpr INT ICON_GROUP		= 4;
constexpr INT ICON_PARENT		= 5;
constexpr INT ICON_WARN_SESSION	= 6;
constexpr INT ICON_MISSING_FILE	= 7;

enum FavesElements {
	FAVES_FOLDERS = 0,
	FAVES_FILES,
	FAVES_WEB,
	FAVES_SESSIONS,
	FAVES_ITEM_MAX
};

static LPCTSTR cFavesItemNames[4] = {
	_T("[Folders]"),
	_T("[Files]"),
	_T("[Web]"),
	_T("[Sessions]"),
};


constexpr UINT FAVES_PARAM					= 0x0000000F;
constexpr UINT FAVES_PARAM_MAIN				= 0x00000010;
constexpr UINT FAVES_PARAM_GROUP			= 0x00000020;
constexpr UINT FAVES_PARAM_LINK				= 0x00000040;
constexpr UINT FAVES_PARAM_SESSION_CHILD	= 0x00000080;
constexpr UINT FAVES_PARAM_EXPAND			= 0x00000100;
constexpr UINT FAVES_PARAM_USERIMAGE		= 0x00000200;

struct ItemElement {
	UINT						uParam		= 0;
	std::wstring				name		= std::wstring();
	std::wstring				link		= std::wstring();
	std::vector<ItemElement>	vElements	= std::vector<ItemElement>();
};
typedef ItemElement* PELEM;

typedef std::vector<ItemElement>::iterator		ELEM_ITR;


/******************** explorer ***************************/

static LPCTSTR cColumns[5] = {
	_T("Name"),
	_T("Ext."),
	_T("Size"),
	_T("Date")
};

static TCHAR FAVES_DATA[]		= _T("\\Favorites.dat");
static TCHAR EXPLORER_INI[]		= _T("\\Explorer.ini");
static TCHAR CONFIG_PATH[]		= _T("\\plugins\\Config");

/********************************************************/

/* see in notepad sources */
#define TCN_TABDROPPED (TCN_FIRST - 10)
#define TCN_TABDROPPEDOUTSIDE (TCN_FIRST - 11)
#define TCN_TABDELETE (TCN_FIRST - 12)


/********************************************************/

/* see in notepad sources */
static LPCTSTR cVarExNppExec[] = {
	_T("EXP_FULL_PATH"),
	_T("EXP_ROOT_PATH"),
	_T("EXP_PARENT_FULL_DIR"),
	_T("EXP_PARENT_DIR"),
	_T("EXP_FULL_FILE"),
	_T("EXP_FILE_NAME"),
	_T("EXP_FILE_EXT"),
};


enum VarExNppExec {
	VAR_FULL_PATH,
	VAR_ROOT_PATH,
	VAR_PARENT_FULL_DIR,
	VAR_PARENT_DIR,
	VAR_FULL_FILE,
	VAR_FILE_NAME,
	VAR_FILE_EXT,
	VAR_UNKNOWN
};


/********************************************************/

enum DevType {
	DEVT_DRIVE,
	DEVT_DIRECTORY,
	DEVT_FILE
} ;


enum SizeFmt {
	SFMT_BYTES,
	SFMT_KBYTE,
	SFMT_DYNAMIC,
	SFMT_DYNAMIC_EX,
	SFMT_MAX
} ;


const LPCTSTR pszSizeFmt[18] = {
	_T("Bytes"),
	_T("kBytes"),
	_T("Dynamic x b/k/M"),
	_T("Dynamic x,x b/k/M")
};

enum DateFmt {
	DFMT_ENG,
	DFMT_GER,
	DFMT_MAX
};

const LPCTSTR pszDateFmt[12] = {
	_T("Y/M/D HH:MM"),
	_T("D.M.Y HH:MM")
};

struct DrvMap {
	TCHAR	cDrv;
	UINT	pos;
};

enum ScDir {
	SCR_OUTSIDE,
	SCR_UP,
	SCR_DOWN
};

struct NppExecScripts {
	TCHAR			szScriptName[MAX_PATH]	= {};
	TCHAR			szArguments[MAX_PATH]	= {};
};

struct NppExecProp {
	TCHAR			szAppName[MAX_PATH]		= {};
	TCHAR			szScriptPath[MAX_PATH]	= {};
	std::vector<NppExecScripts>	vNppExecScripts;
};

struct CphProgram {
	TCHAR			szAppName[MAX_PATH]		= {};
};

struct ExProp{
	/* pointer to global current path */
	TCHAR						szCurrentPath[MAX_PATH]	= {};
	LOGFONT						logfont					= {};
	HFONT						defaultFont				= nullptr;
	HFONT						underlineFont			= nullptr;
	INT							iSplitterPos			= 0;
	INT							iSplitterPosHorizontal	= 0;
	BOOL						bAscending				= false;
	INT							iSortPos				= 0;
	INT							iColumnPosName			= 0;
	INT							iColumnPosExt			= 0;
	INT							iColumnPosSize			= 0;
	INT							iColumnPosDate			= 0;
	BOOL						bShowHidden				= false;
	BOOL						bViewBraces				= false;
	BOOL						bViewLong				= false;
	BOOL						bAddExtToName			= false;
	BOOL						bAutoUpdate				= false;
	BOOL						bAutoNavigate			= false;
	SizeFmt						fmtSize					= SizeFmt::SFMT_BYTES;
	DateFmt						fmtDate					= DateFmt::DFMT_ENG;
	std::vector<std::wstring>	vStrFilterHistory;
	FileFilter					fileFilter;
	UINT						uTimeout				= 0;
	BOOL						bUseSystemIcons			= false;
	NppExecProp					nppExecProp;
	CphProgram					cphProgram;
	SIZE_T						maxHistorySize			= 0;
};



LRESULT ScintillaMsg(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);

void loadSettings(void);
void saveSettings(void);
void initMenu(void);

void toggleExplorerDialog(void);
void toggleFavesDialog(void);
void openQuickOpenDlg(void);

void gotoPath(void);
void gotoUserFolder(void);
void gotoCurrentFolder(void);
void gotoCurrentFile(void);
void showExplorerDialogOnFolder(void);
void showExplorerDialogOnFile(void);
void showFavesDialog(void);
void clearFilter(void);

void openOptionDlg(void);
void openHelpDlg(void);
void openTerminal(void);

LRESULT CALLBACK SubWndProcNotepad(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


#define	ALLOW_PARENT_SEL	1

BOOL VolumeNameExists(LPTSTR rootDrive, LPTSTR volumeName);
bool IsValidFileName(LPTSTR pszFileName);
bool IsValidFolder(const WIN32_FIND_DATA & Find);
bool IsValidParentFolder(const WIN32_FIND_DATA & Find);
bool IsValidFile(const WIN32_FIND_DATA & Find);
BOOL HaveChildren(const std::wstring &folderPath);
BOOL ConvertNetPathName(LPCTSTR pPathName, LPTSTR pRemotePath, UINT length);

/* Get Image Lists */
HIMAGELIST GetSmallImageList(BOOL bSystem);
void ExtractIcons(LPCTSTR currentPath, LPCTSTR fileName, DevType type, LPINT iIconNormal, LPINT iIconSelected, LPINT iIconOverlayed);

/* Resolve Links */
HRESULT ResolveShortCut(const std::wstring& shortcutPath, LPTSTR lpszFilePath, int maxBuf);

/* current open files */
void UpdateDocs(void);
void UpdateCurrUsedDocs(LPTSTR *ppFiles, UINT numFiles);
BOOL IsFileOpen(const std::wstring& filePath);

/* scroll up/down test function */
ScDir GetScrollDirection(HWND hWnd, UINT offTop = 0, UINT offBottom = 0);

/* Extended Window Functions */
void ClientToScreen(HWND hWnd, RECT* rect);
void ScreenToClient(HWND hWnd, RECT* rect);
void ErrorMessage(DWORD err);

/* Helper functions for NppExec */
BOOL ConvertCall(LPTSTR pszExplArg, LPTSTR pszName, LPTSTR *p_pszNppArg, std::vector<std::wstring> vFileList);


#endif //EXPLORER_H
