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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "FileFilter.h"

#include <string>
#include <vector>


constexpr INT DOCKABLE_EXPLORER_INDEX   = 0;
constexpr INT DOCKABLE_FAVORTIES_INDEX  = 1;


constexpr CHAR SHORTCUT_ALL     = 0x01;
constexpr CHAR SHORTCUT_DELETE  = 0x04;
constexpr CHAR SHORTCUT_COPY    = 0x03;
constexpr CHAR SHORTCUT_PASTE   = 0x16;
constexpr CHAR SHORTCUT_CUT     = 0x18;
constexpr CHAR SHORTCUT_REFRESH = 0x12;

constexpr INT ICON_FOLDER       = 0;
constexpr INT ICON_FILE         = 1;
constexpr INT ICON_WEB          = 2;
constexpr INT ICON_SESSION      = 3;
constexpr INT ICON_GROUP        = 4;
constexpr INT ICON_PARENT       = 5;
constexpr INT ICON_WARN_SESSION = 6;
constexpr INT ICON_MISSING_FILE = 7;

enum VarExNppExec {
    VAR_FULL_PATH,
    VAR_ROOT_PATH,
    VAR_PARENT_FULL_DIR,
    VAR_PARENT_DIR,
    VAR_FULL_FILE,
    VAR_FILE_NAME,
    VAR_FILE_EXT,
    VAR_UNKNOWN,
};

enum DevType {
    DEVT_DRIVE,
    DEVT_DIRECTORY,
    DEVT_FILE,
};

enum SizeFmt {
    SFMT_BYTES,
    SFMT_KBYTE,
    SFMT_DYNAMIC,
    SFMT_DYNAMIC_EX,
    SFMT_MAX,
};

enum DateFmt {
    DFMT_ENG,
    DFMT_GER,
    DFMT_MAX,
};

struct DrvMap {
    WCHAR   cDrv;
    UINT    pos;
};

enum ScDir {
    SCR_OUTSIDE,
    SCR_UP,
    SCR_DOWN,
};

struct NppExecScripts {
    WCHAR   szScriptName[MAX_PATH] {};
    WCHAR   szArguments[MAX_PATH]  {};
};

struct NppExecProp {
    WCHAR   szAppName[MAX_PATH]    {};
    WCHAR   szScriptPath[MAX_PATH] {};
    std::vector<NppExecScripts> vNppExecScripts;
};

struct CphProgram {
    WCHAR   szAppName[MAX_PATH]    {};
};
struct ExProp{
    /* pointer to global current path */
    std::wstring                currentDir;
    std::wstring                rootFolder;
    LOGFONT                     logfont                 {};
    HFONT                       defaultFont             = nullptr;
    HFONT                       underlineFont           = nullptr;
    INT                         iSplitterPos            = 0;
    INT                         iSplitterPosHorizontal  = 0;
    BOOL                        bAscending              = false;
    INT                         iSortPos                = 0;
    INT                         iColumnPosName          = 0;
    INT                         iColumnPosExt           = 0;
    INT                         iColumnPosSize          = 0;
    INT                         iColumnPosDate          = 0;
    BOOL                        bShowHidden             = false;
    BOOL                        bViewBraces             = false;
    BOOL                        bViewLong               = false;
    BOOL                        bAddExtToName           = false;
    BOOL                        bAutoUpdate             = false;
    BOOL                        bAutoNavigate           = false;
    bool                        bHideFoldersInFileList{ false };
    SizeFmt                     fmtSize                 = SizeFmt::SFMT_BYTES;
    DateFmt                     fmtDate                 = DateFmt::DFMT_ENG;
    std::vector<std::wstring>   vStrFilterHistory       {};
    FileFilter                  fileFilter              {};
    UINT                        uTimeout                = 0;
    BOOL                        bUseSystemIcons         = false;
    NppExecProp                 nppExecProp             {};
    CphProgram                  cphProgram              {};
    SIZE_T                      maxHistorySize          = 0;
    BOOL                        useFullTree             = false;
};



void toggleExplorerDialog();
void toggleFavesDialog();
void openQuickOpenDlg();

void gotoPath();
void gotoUserFolder();
void gotoCurrentFolder();
void gotoRootFolder();
void gotoCurrentFile();
void showExplorerDialogOnFolder();
void showExplorerDialogOnFile();
void showFavesDialog();
void clearFilter();

void openOptionDlg();
void openHelpDlg();
void openTerminal();

LRESULT CALLBACK SubWndProcNotepad(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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
HRESULT ResolveShortCut(const std::wstring& shortcutPath, LPWSTR lpszFilePath, int maxBuf);

/* current open files */
void UpdateDocs();
void UpdateCurrUsedDocs(LPTSTR *ppFiles, UINT numFiles);
BOOL IsFileOpen(const std::wstring& filePath);

/* scroll up/down test function */
ScDir GetScrollDirection(HWND hWnd, UINT offTop = 0, UINT offBottom = 0);

/* Extended Window Functions */
void ClientToScreen(HWND hWnd, RECT* rect);
void ScreenToClient(HWND hWnd, RECT* rect);
void ErrorMessage(DWORD err);

/* Helper functions for NppExec */
BOOL ConvertCall(LPTSTR pszExplArg, LPTSTR pszName, LPTSTR *p_pszNppArg, const std::vector<std::wstring> &vFileList);
