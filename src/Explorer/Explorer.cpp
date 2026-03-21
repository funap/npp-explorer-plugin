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
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <dbt.h>
#include <ranges>
#include <format>
#include <wrl/client.h>

#include "Editor.h"
#include "ExplorerDialog.h"
#include "FavesDialog.h"
#include "QuickOpenDialog.h"
#include "OptionDialog.h"
#include "HelpDialog.h"
#include "ThemeRenderer.h"
#include "../NppPlugin/PluginInterface.h"
#include "../NppPlugin/menuCmdID.h"


/* information for notepad */
constexpr WCHAR  PLUGIN_NAME[]      = L"&Explorer";




/* global values */
NppData             g_nppData{};
HINSTANCE           g_hInst = nullptr;
HWND                g_HSource = nullptr;
INT                 g_docCount = 0;
WCHAR               g_currentFile[MAX_PATH]{};
FuncItem            funcItem[] = {
//      {  _itemName,                         _pFunc,                _cmdID, _init2Check,   _pShKey}
/*  0 */{ L"&Explorer...",                    toggleExplorerDialog,       0,       false,   nullptr},
/*  1 */{ L"&Favorites...",                   toggleFavesDialog,          0,       false,   nullptr},
/*  2 */{ L"&Quick Open...",                  openQuickOpenDlg,           0,       false,   nullptr},
/*  3 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/*  4 */{ L"Go to &Path...",                  gotoPath,                   0,       false,   nullptr},
/*  5 */{ L"Go to &User Folder",              gotoUserFolder,             0,       false,   nullptr},
/*  6 */{ L"Go to &Current Folder",           gotoCurrentFolder,          0,       false,   nullptr},
/*  7 */{ L"Go to &Root Folder",              gotoRootFolder,             0,       false,   nullptr},
/*  8 */{ L"&Go to Current File",             gotoCurrentFile,            0,       false,   nullptr},
/*  9 */{ L"&Show Explorer (Focus on folder)",showExplorerDialogOnFolder, 0,       false,   nullptr},
/* 10 */{ L"Show E&xplorer (Focus on file)",  showExplorerDialogOnFile,   0,       false,   nullptr},
/* 11 */{ L"Show Fa&vorites",                 showFavesDialog,            0,       false,   nullptr},
/* 12 */{ L"C&lear Filter",                   clearFilter,                0,       false,   nullptr},
/* 13 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/* 14 */{ L"Explorer &Options...",            openOptionDlg,              0,       false,   nullptr},
/* 15 */{ L"-",                               nullptr,                    0,       false,   nullptr},
/* 16 */{ L"&Help...",                        openHelpDlg,                0,       false,   nullptr},
};

/* see in notepad sources */
LPCWSTR cVarExNppExec[] = {
    L"EXP_FULL_PATH",
    L"EXP_ROOT_PATH",
    L"EXP_PARENT_FULL_DIR",
    L"EXP_PARENT_DIR",
    L"EXP_FULL_FILE",
    L"EXP_FILE_NAME",
    L"EXP_FILE_EXT",
};



toolbarIconsWithDarkMode    g_explorerIcons{};
toolbarIconsWithDarkMode    g_favesIcons{};

/* create classes */
ExplorerDialog      explorerDlg;
FavesDialog         favesDlg;
QuickOpenDlg        quickOpenDlg;
OptionDlg           optionDlg;
HelpDlg             helpDlg;

/* global settings */
Settings            settings;

/* for subclassing */
WNDPROC             wndProcNotepad      = nullptr;

/* own image list variables */
std::vector<DrvMap> gvDrvMap;
HIMAGELIST          ghImgList           = nullptr;

/* current open docs */
std::vector<std::wstring>   g_openedFilePaths;

void UpdateThemeColor();

BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD  reasonForCall, LPVOID /* lpReserved */)
{
    g_hInst = hInst;

    switch (reasonForCall) {
    case DLL_PROCESS_ATTACH:

        /* Set shortcuts */
        funcItem[0]._pShKey = new ShortcutKey{ true, true, true, 'E' };
        funcItem[1]._pShKey = new ShortcutKey{ true, true, true, 'V' };
        funcItem[2]._pShKey = new ShortcutKey{ true, false, false, 'P' };

        /* set image list and icon */
        ghImgList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 6, 30);
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_FOLDER)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_FILE)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_WEB)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_SESSION)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_GROUP)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_PARENTFOLDER)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_WARN_SESSION)));
        ImageList_AddIcon(ghImgList, ::LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MISSING_FILE)));

        ThemeRenderer::Create();
        break;
    case DLL_PROCESS_DETACH:
        /* save settings */
        settings.Save();

        /* destroy image list */
        ImageList_Destroy(ghImgList);

        /* Remove subclaasing */
        if (wndProcNotepad != nullptr) {
            SetWindowLongPtr(g_nppData._nppHandle, GWLP_WNDPROC, (LONG_PTR)wndProcNotepad);
        }


        delete funcItem[0]._pShKey;
        funcItem[0]._pShKey = nullptr;
        delete funcItem[1]._pShKey;
        funcItem[1]._pShKey = nullptr;
        delete funcItem[2]._pShKey;
        funcItem[2]._pShKey = nullptr;

        ::DeleteObject(g_explorerIcons.hToolbarBmp);
        ::DestroyIcon(g_explorerIcons.hToolbarIcon);
        ::DestroyIcon(g_explorerIcons.hToolbarIconDarkMode);
        ::DeleteObject(g_favesIcons.hToolbarBmp);
        ::DestroyIcon(g_favesIcons.hToolbarIcon);
        ::DestroyIcon(g_favesIcons.hToolbarIconDarkMode);

        ThemeRenderer::Destroy();
        break;

    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    default:
        break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notpadPlusData)
{
    Editor::Instance().SetNppData(notpadPlusData);

    /* stores notepad data */
    g_nppData   = notpadPlusData;

    /* load data */
    settings.Load(Editor::Instance().GetConfigDir());
    settings.InitializeFonts();

    UpdateThemeColor();

    /* initial dialogs */
    explorerDlg .init(g_hInst, g_nppData._nppHandle, &settings);
    favesDlg    .init(g_hInst, g_nppData._nppHandle, &settings);
    quickOpenDlg.init(g_hInst, g_nppData._nppHandle, &settings);
    optionDlg   .init(g_hInst, g_nppData._nppHandle);
    helpDlg     .init(g_hInst, g_nppData._nppHandle);

    explorerDlg.VisibleChanged([](bool visible) {
        Editor::Instance().SetMenuItemCheck(funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, visible);
    });
    favesDlg.VisibleChanged([](bool visible) {
        Editor::Instance().SetMenuItemCheck(funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, visible);
    });

    /* Subclassing for Notepad */
    wndProcNotepad = (WNDPROC)::SetWindowLongPtr(g_nppData._nppHandle, GWLP_WNDPROC, (LONG_PTR)SubWndProcNotepad);
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

// This function is called, if a notification in Scintilla/Notepad++ occurs
extern "C" __declspec(dllexport) void beNotified(SCNotification *notifyCode)
{
    switch (notifyCode->nmhdr.code) {
    case NPPN_BUFFERACTIVATED:
        if (settings.IsAutoNavigate()) {
            ::KillTimer(explorerDlg.getHSelf(), EXT_AUTOGOTOFILE);
            ::SetTimer(explorerDlg.getHSelf(), EXT_AUTOGOTOFILE, 200, nullptr);
        }
        UpdateDocs();
        break;
    case NPPN_FILECLOSED:
    case NPPN_FILEOPENED:
        UpdateDocs();
        break;
    case NPPN_TBMODIFICATION: {
        SIZE smallIconSize = {
            .cx = GetSystemMetrics(SM_CXSMICON),
            .cy = GetSystemMetrics(SM_CYSMICON),
        };
        g_explorerIcons.hToolbarBmp            = (HBITMAP)  ::LoadImage(g_hInst, MAKEINTRESOURCE(IDB_TB_EXPLORER),                  IMAGE_BITMAP,   smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);
        g_explorerIcons.hToolbarIcon           = (HICON)    ::LoadImage(g_hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_EXPLORER),           IMAGE_ICON,     smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);
        g_explorerIcons.hToolbarIconDarkMode   = (HICON)    ::LoadImage(g_hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_EXPLORER_DARKMODE),  IMAGE_ICON,     smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);
        g_favesIcons.hToolbarBmp               = (HBITMAP)  ::LoadImage(g_hInst, MAKEINTRESOURCE(IDB_TB_FAVES),                     IMAGE_BITMAP,   smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);
        g_favesIcons.hToolbarIcon              = (HICON)    ::LoadImage(g_hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES),              IMAGE_ICON,     smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);
        g_favesIcons.hToolbarIconDarkMode      = (HICON)    ::LoadImage(g_hInst, MAKEINTRESOURCE(IDI_TB_FLUENT_FAVES_DARKMODE),     IMAGE_ICON,     smallIconSize.cx, smallIconSize.cy, LR_LOADMAP3DCOLORS);

        /* change menu language */
        if (Editor::Instance().IsSupportFluentUI()) {
            Editor::Instance().AddToolbarIcon(funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, &g_explorerIcons, true);
            Editor::Instance().AddToolbarIcon(funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, &g_favesIcons, true);
        }
        else {
            Editor::Instance().AddToolbarIcon(funcItem[DOCKABLE_EXPLORER_INDEX]._cmdID, &g_explorerIcons, false);
            Editor::Instance().AddToolbarIcon(funcItem[DOCKABLE_FAVORTIES_INDEX]._cmdID, &g_favesIcons, false);
        }
        break;
    }
    case NPPN_READY:
        UpdateThemeColor();
        explorerDlg.initFinish();
        favesDlg.initFinish();
        break;
    case NPPN_WORDSTYLESUPDATED:
        UpdateThemeColor();
        break;
    default:
        break;
    }
}

// This function is called, if a notification from Notepad occurs
extern "C" __declspec(dllexport) LRESULT messageProc(UINT /* Message */, WPARAM /* wParam */, LPARAM /* lParam */)
{
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
    return TRUE;
}

void UpdateThemeColor()
{
    auto IsDarkColor = [](COLORREF rgb) -> bool {
        uint8_t r = GetRValue(rgb);
        uint8_t g = GetGValue(rgb);
        uint8_t b = GetBValue(rgb);
        float brightness = (0.2126F * r + 0.7152F * g + 0.0722F * b) / 255.0F;
        return brightness < 0.5F;
    };

    auto editorColors = Editor::Instance().GetColors();

    ThemeColors colors{
        .body               = editorColors.darkerText,
        .body_bg            = editorColors.pureBackground,
        .secondary          = Editor::Instance().GetEditorDefaultForegroundColor(),
        .secondary_bg       = Editor::Instance().GetEditorDefaultBackgroundColor(),
        .border             = editorColors.edge,
        .primary            = editorColors.text,
        .primary_bg         = editorColors.hotBackground,
        .primary_border     = editorColors.hotEdge,
    };
    auto isDarkMode = IsDarkColor(colors.body_bg);
    ThemeRenderer::Instance().SetTheme(isDarkMode, colors);
}



void toggleExplorerDialog()
{
    if (explorerDlg.isVisible()) {
        explorerDlg.doDialog(false);
    } else {
        UpdateDocs();
        explorerDlg.doDialog();
    }
}

void toggleFavesDialog()
{
    if (favesDlg.isVisible()) {
        favesDlg.doDialog(false);
    } else {
        UpdateDocs();
        favesDlg.doDialog();
    }
}

void gotoPath()
{
    if (explorerDlg.gotoPath()) {
        explorerDlg.doDialog();
        explorerDlg.setFocusOnFile();
    }
}

void gotoUserFolder()
{
    explorerDlg.doDialog();
    explorerDlg.gotoUserFolder();
}

void gotoRootFolder()
{
    explorerDlg.doDialog();
    if (!settings.GetRootFolder().empty()) {
        explorerDlg.gotoFileLocation(settings.GetRootFolder());
    }
}

void gotoCurrentFolder()
{
    explorerDlg.doDialog();
    explorerDlg.gotoCurrentFolder();
}

void gotoCurrentFile()
{
    explorerDlg.doDialog();
    explorerDlg.gotoCurrentFile();
}

void showExplorerDialogOnFolder()
{
    explorerDlg.doDialog();
    explorerDlg.setFocusOnFolder();
}

void showExplorerDialogOnFile()
{
    explorerDlg.doDialog();
    explorerDlg.setFocusOnFile();
}

void showFavesDialog()
{
    favesDlg.doDialog();
}

void clearFilter()
{
    explorerDlg.clearFilter();
}

void openOptionDlg()
{
    if (optionDlg.doDialog(&settings) == IDOK) {
        settings.Save();
        settings.InitializeFonts();
        explorerDlg.SetFont(settings.GetDefaultFont());
        favesDlg.SetFont(settings.GetDefaultFont());
        quickOpenDlg.SetFont(settings.GetDefaultFont());

        explorerDlg.redraw();
        favesDlg.redraw();
    }
}

void openHelpDlg()
{
    helpDlg.doDialog();
}

void openQuickOpenDlg()
{
    if (!settings.GetRootFolder().empty()) {
        quickOpenDlg.setRootPath(settings.GetRootFolder());
    }
    else {
        quickOpenDlg.setRootPath(settings.GetCurrentDir());
    }
    quickOpenDlg.show();
}

void openTerminal()
{
    std::filesystem::path path(settings.GetCurrentDir());
    ::ShellExecute(g_nppData._nppHandle, L"open", settings.GetCphProgram().szAppName.c_str(), nullptr, path.c_str(), SW_SHOW);
}

// Subclass of Notepad
LRESULT CALLBACK SubWndProcNotepad(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_ACTIVATE:
        if (explorerDlg.isVisible()
        && ((LOWORD(wParam) == WA_ACTIVE) || (LOWORD(wParam) == WA_CLICKACTIVE))
        && ((HWND)lParam != hWnd)) {
            if (settings.IsAutoUpdate()) {
                ::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATE);
                ::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATE, 200, nullptr);
            } else {
                ::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATEPATH);
                ::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEACTIVATEPATH, 200, nullptr);
            }
        }
        break;
    case WM_DEVICECHANGE:
        if (explorerDlg.isVisible()
        && ((wParam == DBT_DEVICEARRIVAL) || (wParam == DBT_DEVICEREMOVECOMPLETE))) {
            ::KillTimer(explorerDlg.getHSelf(), EXT_UPDATEDEVICE);
            ::SetTimer(explorerDlg.getHSelf(), EXT_UPDATEDEVICE, 1000, nullptr);
        }
        break;
    case WM_COMMAND:
        if (wParam == IDM_FILE_SAVESESSION) {
            favesDlg.SaveSession();
            return TRUE;
        }
        break;
    default:
        break;
    }

    return ::CallWindowProc(wndProcNotepad, hWnd, message, wParam, lParam);
}

BOOL VolumeNameExists(LPTSTR rootDrive, LPTSTR volumeName)
{
    BOOL bRet = FALSE;

    if ((volumeName[0] != '\0') && (GetVolumeInformation(rootDrive, volumeName, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) == TRUE)) {
        bRet = TRUE;
    }
    return bRet;
}

bool IsValidFileName(LPTSTR pszFileName)
{
    if (_tcspbrk(pszFileName, L"\\/:*?\"<>") == nullptr) {
        return true;
    }

    ::MessageBox(nullptr, L"Filename does not contain any of this characters:\n       \\ / : * ? \" < >", L"Error", MB_OK);
    return false;
}

bool IsValidFolder(const WIN32_FIND_DATA & Find)
{
    return (Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        && (!(Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || settings.IsShowHidden())
        && (_tcscmp(Find.cFileName, L".") != 0)
        && (_tcscmp(Find.cFileName, L"..") != 0) 
        && (Find.cFileName[0] != '?');
}

bool IsValidParentFolder(const WIN32_FIND_DATA & Find)
{
    return (Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        && (_tcscmp(Find.cFileName, L"..") == 0);
}

bool IsValidFile(const WIN32_FIND_DATA & Find)
{
    return !(Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        && (!(Find.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) || settings.IsShowHidden());
}

BOOL HaveChildren(const std::wstring &folderPath)
{
    std::wstring searchPath = folderPath;
    if (searchPath.back() != '\\') {
        searchPath.append(L"\\");
    }
    /* add wildcard */
    searchPath.append(L"*");

    WIN32_FIND_DATA findData{};
    HANDLE hFind = ::FindFirstFile(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    BOOL bFound = TRUE;
    BOOL bRet = FALSE;
    do {
        if (IsValidFolder(findData) == TRUE) {
            bFound = FALSE;
            bRet = TRUE;
            break;
        }
        if (settings.IsUseFullTree() && IsValidFile(findData) == TRUE) {
            bFound = FALSE;
            bRet = TRUE;
            break;
        }

    } while ((FindNextFile(hFind, &findData)) && (bFound == TRUE));

    ::FindClose(hFind);

    return bRet;
}

BOOL ConvertNetPathName(LPCTSTR pPathName, LPTSTR pRemotePath, UINT length)
{
    DWORD driveList      = ::GetLogicalDrives();
    WCHAR volumeName[MAX_PATH];
    WCHAR remoteName[MAX_PATH];

    for (INT i = 0; i < 26; ++i) {
        if (0x01 & (driveList >> i)) {
            _stprintf(volumeName, L"%c:", 'A' + i);

            /* call get connection twice to get the real size */
            DWORD dwRemoteLength = 1;
            if (ERROR_MORE_DATA == WNetGetConnection(volumeName, remoteName, &dwRemoteLength)) {
                if ((dwRemoteLength < MAX_PATH) && (NO_ERROR == WNetGetConnection(volumeName, remoteName, &dwRemoteLength))) {
                    if (_tcsstr(pPathName, remoteName) != nullptr) {
                        wcscpy(pRemotePath, volumeName);
                        _tcsncat(pRemotePath, &pPathName[dwRemoteLength - 1], length - 2);
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

// Get system images
HIMAGELIST GetSmallImageList(BOOL bSystem)
{
    static HIMAGELIST   s_himlSys   = nullptr;
    if (bSystem) {
        if (s_himlSys == nullptr) {
            SHFILEINFO sfi{};
            s_himlSys = (HIMAGELIST)SHGetFileInfo(L"C:\\", 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
        }
        return s_himlSys;
    }
    return ghImgList;
}

void ExtractIcons(LPCTSTR currentPath, LPCTSTR fileName, DevType type, LPINT piIconNormal, LPINT piIconSelected, LPINT piIconOverlayed)
{
    SHFILEINFO  sfi{};
    SIZE_T      length = wcslen(currentPath) - 1;
    WCHAR       TEMP[MAX_PATH];
    UINT        stOverlay = (piIconOverlayed ? SHGFI_OVERLAYINDEX : 0);

    wcscpy(TEMP, currentPath);
    if (TEMP[length] == '*') {
        TEMP[length] = '\0';
    }
    else if (TEMP[length] != '\\') {
        wcscat(TEMP, L"\\");
    }

    if (fileName != nullptr) {
        wcscat(TEMP, fileName);
    }

    if (!settings.IsUseSystemIcons()) {
        /* get drive icon in any case correct */
        if (type == DEVT_DRIVE) {
            ::ZeroMemory(&sfi, sizeof(SHFILEINFO));
            SHGetFileInfo(TEMP,
                0,
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
            UINT onPos = 0;
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

            *piIconNormal   = onPos;
            *piIconSelected = onPos;
        }
        else if (type == DEVT_DIRECTORY) {
            *piIconNormal   = ICON_FOLDER;
            *piIconSelected = ICON_FOLDER;
        }
        else{
            *piIconNormal   = ICON_FILE;
            *piIconSelected = ICON_FILE;
        }
        if (piIconOverlayed != nullptr) {
            *piIconOverlayed = 0;
        }
    }
    else {
        /* get normal and overlayed icon */
        if ((type == DEVT_DIRECTORY) || (type == DEVT_DRIVE)) {
            ::ZeroMemory(&sfi, sizeof(SHFILEINFO));
            SHGetFileInfo(TEMP,
                0,
                &sfi,
                sizeof(SHFILEINFO),
                SHGFI_ICON | SHGFI_SMALLICON | stOverlay);
            ::DestroyIcon(sfi.hIcon);

            if (type == DEVT_DRIVE) {
                ::ZeroMemory(&sfi, sizeof(SHFILEINFO));
                SHGetFileInfo(TEMP,
                    FILE_ATTRIBUTE_NORMAL,
                    &sfi,
                    sizeof(SHFILEINFO),
                    SHGFI_ICON | SHGFI_SMALLICON | stOverlay | SHGFI_USEFILEATTRIBUTES);
                ::DestroyIcon(sfi.hIcon);
            }
        }
        else {
            ::ZeroMemory(&sfi, sizeof(SHFILEINFO));
            SHGetFileInfo(TEMP,
                FILE_ATTRIBUTE_NORMAL,
                &sfi,
                sizeof(SHFILEINFO),
                SHGFI_ICON | SHGFI_SMALLICON | stOverlay | SHGFI_USEFILEATTRIBUTES);
            ::DestroyIcon(sfi.hIcon);
        }

        *piIconNormal = sfi.iIcon & 0x00ffffff;
        if (piIconOverlayed != nullptr) {
            *piIconOverlayed = sfi.iIcon >> 24;
        }

        /* get selected (open) icon */
        if (type == DEVT_DIRECTORY) {
            ::ZeroMemory(&sfi, sizeof(SHFILEINFO));
            SHGetFileInfo(TEMP,
                0,
                &sfi,
                sizeof(SHFILEINFO),
                SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON);
            ::DestroyIcon(sfi.hIcon);

            *piIconSelected = sfi.iIcon;
        }
        else {
            *piIconSelected = *piIconNormal;
        }
    }
}

// Resolve files if they are shortcuts
HRESULT ResolveShortCut(const std::wstring &shortcutPath, LPWSTR lpszFilePath, int maxBuf)
{
    Microsoft::WRL::ComPtr<IShellLink> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    hr = shellLink.As(&persistFile);
    if (FAILED(hr)) {
        return hr;
    }

    hr = persistFile->Load(shortcutPath.c_str(), STGM_READ);
    if (FAILED(hr)) {
        return hr;
    }

    hr = shellLink->Resolve(nullptr, SLR_UPDATE);
    if (FAILED(hr)) {
        return hr;
    }

    hr = shellLink->GetPath(lpszFilePath, maxBuf, nullptr, SLGP_RAWPATH);
    if (FAILED(hr)) {
        return hr;
    }

    if (::PathIsDirectory(lpszFilePath) && lpszFilePath[wcslen(lpszFilePath) - 1] != L'\\') {
        wcsncat_s(lpszFilePath, maxBuf, L"\\", 1);
    }

    return S_OK;
}


// Current docs
void UpdateDocs()
{
    g_HSource = Editor::Instance().GetCurrentScintilla();

    /* update open files */
    int     newDocCount = 0;
    std::filesystem::path newPath = Editor::Instance().GetFullCurrentPath();
    newDocCount = Editor::Instance().GetNbOpenFiles();

    if ((wcscmp(newPath.c_str(), g_currentFile) != 0) || (newDocCount != g_docCount)) {
        /* update current path in explorer and favorites */
        wcscpy(g_currentFile, newPath.c_str());
        g_docCount = newDocCount;
        explorerDlg.NotifyNewFile();
        favesDlg.NotifyNewFile();

        std::vector<std::wstring> fileNames;
        if (Editor::Instance().GetOpenFileNames(fileNames)) {
            if (explorerDlg.isVisible() || favesDlg.isVisible()) {
                /* update documents list */
                g_openedFilePaths.clear();
                for (const auto& fileName : fileNames) {
                    WCHAR pszLongName[MAX_PATH];
                    if (GetLongPathName(fileName.c_str(), pszLongName, MAX_PATH) != 0) {
                        g_openedFilePaths.push_back(pszLongName);
                    }
                }
            }
            if (explorerDlg.isVisible()) {
                RedrawWindow(explorerDlg.getHSelf(), nullptr, nullptr, TRUE);
            }
            if (favesDlg.isVisible()) {
                RedrawWindow(favesDlg.getHSelf(), nullptr, nullptr, TRUE);
            }
        }
    }
}

void UpdateCurrUsedDocs(LPTSTR* pFiles, UINT numFiles)
{
    WCHAR pszLongName[MAX_PATH];

    /* clear old elements */
    g_openedFilePaths.clear();

    for (UINT i = 0; i < numFiles; i++) {
        /* store only long pathes */
        if (GetLongPathName(pFiles[i], pszLongName, MAX_PATH) != 0) {
            g_openedFilePaths.push_back(pszLongName);
        }
    }
}

BOOL IsFileOpen(const std::wstring &filePath)
{
    for (const auto &openedFilePath : g_openedFilePaths) {
        if (openedFilePath == filePath) {
            return TRUE;
        }
    }
    return FALSE;
}

// compare arguments and convert
BOOL ConvertCall(LPTSTR pszExplArg, LPTSTR pszName, LPTSTR *p_pszNppArg, const std::vector<std::wstring> &vFileList)
{
    WCHAR           szElement[MAX_PATH];
    LPTSTR          pszPtr      = nullptr;
    LPTSTR          pszEnd      = nullptr;
    UINT            iCount      = 0;
    VarExNppExec    varElement  = VAR_UNKNOWN;

    /* get name of NppExec plugin */
    pszPtr = _tcstok(pszExplArg, L" ");

    if (_tcscmp(pszPtr, L"//Explorer:") != 0) {
        WCHAR   szTemp[MAX_PATH];
        _stprintf(szTemp, L"Format of first line needs to be:\n//Explorer: NPPEXEC_DLL_NAME PARAM_X[0] PARAM_X[1]");
        ::MessageBox(g_nppData._nppHandle, szTemp, L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    pszPtr = _tcstok(nullptr, L" ");
    wcscpy(pszName, pszPtr);

    pszPtr = _tcstok(nullptr, L" ");

    while (pszPtr != nullptr)
    {
        /* find string element in explorer argument list */
        if (_tcsstr(pszPtr, cVarExNppExec[VAR_FULL_PATH]) != nullptr) {
            varElement = VAR_FULL_PATH;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_ROOT_PATH]) != nullptr) {
            varElement = VAR_ROOT_PATH;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_PARENT_FULL_DIR]) != nullptr) {
            varElement = VAR_PARENT_FULL_DIR;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_PARENT_DIR]) != nullptr) {
            varElement = VAR_PARENT_DIR;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FULL_FILE]) != nullptr) {
            varElement = VAR_FULL_FILE;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FILE_NAME]) != nullptr) {
            varElement = VAR_FILE_NAME;
        } else if (_tcsstr(pszPtr, cVarExNppExec[VAR_FILE_EXT]) != nullptr) {
            varElement = VAR_FILE_EXT;
        } else {
            WCHAR szTemp[MAX_PATH];
            _stprintf(szTemp, L"Argument \"%ls\" unknown.", pszPtr);
            ::MessageBox(g_nppData._nppHandle, szTemp, L"Error", MB_OK | MB_ICONERROR);

            return FALSE;
        }

        /* get array list position of given files. Note: of by 1 because of '[' */
        pszPtr += wcslen(cVarExNppExec[varElement]) + 1;

        pszEnd = _tcsstr(pszPtr, L"]");

        if (pszEnd == nullptr) {
            return FALSE;
        }

        *pszEnd = '\0';

        /* control if file element exist */
        iCount = _ttoi(pszPtr);
        if (iCount > vFileList.size()) {
            auto message = std::format(L"Element \"{}\" of argument \"{}\" not selected.", iCount, pszPtr);
            ::MessageBox(g_nppData._nppHandle, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
            return FALSE;
        }

        BOOL cpyDone = TRUE;

        /* copy full file in any case to element array */
        wcscpy(szElement, vFileList[iCount].c_str());
        ::PathRemoveBackslash(szElement);

        /* copy file element to argument list as requested */
        switch (varElement) {
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
            }
            else {
                cpyDone = FALSE;
            }
            break;
        case VAR_FILE_EXT:
            if (*(::PathFindExtension(szElement)) != 0) {
                wcscpy(szElement, ::PathFindExtension(vFileList[iCount].c_str()) + 1);
            }
            else {
                cpyDone = FALSE;
            }
            break;
        default:
            break;
        }

        if (cpyDone == TRUE) {
            /* concatinate arguments finally */
            LPTSTR pszTemp = *p_pszNppArg;

            if (*p_pszNppArg != nullptr) {
                *p_pszNppArg = (LPTSTR)new WCHAR[wcslen(pszTemp) + wcslen(szElement) + 4];
            }
            else {
                *p_pszNppArg = (LPTSTR)new WCHAR[wcslen(szElement) + 3];
            }

            if (*p_pszNppArg != nullptr) {
                if (pszTemp != nullptr) {
                    _stprintf(*p_pszNppArg, L"%ls \"%ls\"", pszTemp, szElement);
                    delete [] pszTemp;
                }
                else {
                    _stprintf(*p_pszNppArg, L"\"%ls\"", szElement);
                }
            }
            else {
                delete [] pszTemp;
                return FALSE;
            }
        }

        pszPtr = _tcstok(nullptr, L" ");
    }

    return TRUE;
}


// Scroll up/down test function
ScDir GetScrollDirection(HWND hWnd, UINT offTop, UINT offBottom)
{
    RECT rcUp{};
    RECT rcDown{};

    ::GetClientRect(hWnd, &rcUp);
    ::ClientToScreen(hWnd, &rcUp);
    rcDown = rcUp;

    rcUp.top += offTop;
    rcUp.bottom = rcUp.top + 20;
    rcDown.bottom += offBottom;
    rcDown.top = rcDown.bottom - 20;

    POINT pt{};
    ::GetCursorPos(&pt);
    if (::PtInRect(&rcUp, pt) == TRUE) {
        return SCR_UP;
    }
    if (::PtInRect(&rcDown, pt) == TRUE) {
        return SCR_DOWN;
    }
    return SCR_OUTSIDE;
}

// Windows helper functions
void ClientToScreen(HWND hWnd, RECT* rect)
{
    POINT pt;

    pt.x            = rect->left;
    pt.y            = rect->top;
    ::ClientToScreen( hWnd, &pt );
    rect->left      = pt.x;
    rect->top       = pt.y;

    pt.x            = rect->right;
    pt.y            = rect->bottom;
    ::ClientToScreen( hWnd, &pt );
    rect->right     = pt.x;
    rect->bottom    = pt.y;
}

void ScreenToClient(HWND hWnd, RECT* rect)
{
    POINT pt;

    pt.x            = rect->left;
    pt.y            = rect->top;
    ::ScreenToClient( hWnd, &pt );
    rect->left      = pt.x;
    rect->top       = pt.y;

    pt.x            = rect->right;
    pt.y            = rect->bottom;
    ::ScreenToClient( hWnd, &pt );
    rect->right     = pt.x;
    rect->bottom    = pt.y;
}

void ErrorMessage(DWORD err)
{
    LPVOID lpMsgBuf;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) & lpMsgBuf, 0, nullptr); // Process any inserts in lpMsgBuf.
    ::MessageBox(nullptr, (LPCTSTR) lpMsgBuf, L"Error", MB_OK | MB_ICONINFORMATION);

    LocalFree(lpMsgBuf);
}
