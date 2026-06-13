/*
  The MIT License (MIT)

  Copyright (c) 2026 funap

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/
#include "NppContext.h"

#include "../NppPlugin/Scintilla.h"
#include "../NppPlugin/menuCmdID.h"

void NppContext::SetNppData(NppData nppData)
{
    _nppData = nppData;
}

HWND NppContext::GetWindow() const
{
    return _nppData._nppHandle;
}

bool NppContext::DoOpen(const std::filesystem::path& path)
{
    return static_cast<bool>(::SendMessage(_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)path.c_str()));
}

std::wstring NppContext::GetSelectedText()
{
    std::wstring selectedTextW;

    HWND currentSciHandle = GetCurrentScintilla();

    INT charLength = (INT)::SendMessage(currentSciHandle, SCI_GETSELTEXT, 0, 0);
    if (0 < charLength) {
        std::string selectedTextA;
        selectedTextA.resize(charLength);
        ::SendMessage(currentSciHandle, SCI_GETSELTEXT, 0, (LPARAM)&selectedTextA[0]);
        INT wideCharLength = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, selectedTextA.data(), charLength, nullptr, 0);
        selectedTextW.resize(wideCharLength);
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, selectedTextA.data(), charLength, selectedTextW.data(), wideCharLength);
    }

    return selectedTextW;
}

COLORREF NppContext::GetEditorDefaultForegroundColor()
{
    return static_cast<COLORREF>(::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR, 0, 0));
}

COLORREF NppContext::GetEditorDefaultBackgroundColor()
{
    return static_cast<COLORREF>(::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR, 0, 0));
}

COLORREF NppContext::GetEditorCurrentLineBackgroundColor()
{
    return static_cast<COLORREF>(::SendMessage(_nppData._scintillaMainHandle, SCI_GETCARETLINEBACK, 0, 0));
}

bool NppContext::IsDarkMode()
{
    return static_cast<bool>(::SendMessage(_nppData._nppHandle, NPPM_ISDARKMODEENABLED, 0, 0));
}

EditorColors NppContext::GetColors()
{
    if (IsDarkMode()) {
        EditorColors colors;
        auto success = static_cast<bool>(::SendMessage(_nppData._nppHandle, NPPM_GETDARKMODECOLORS, sizeof(colors), reinterpret_cast<LPARAM>(&colors)));
        if (!success) {
            // default dark colors;
            return {
                .background         = 0x202020,
                .softerBackground   = 0x404040,
                .hotBackground      = 0x404040,
                .pureBackground     = 0x202020,
                .errorBackground    = 0x0000B0,
                .text               = 0xE0E0E0,
                .darkerText         = 0xC0C0C0,
                .disabledText       = 0x808080,
                .linkText           = 0x00FFFF,
                .edge               = 0x646464,
                .hotEdge            = 0x9B9B9B,
                .disabledEdge       = 0x484848,
            };
        }
        return colors;
    }
    // default colors;
    return {
        .background         = ::GetSysColor(COLOR_3DFACE),
        .softerBackground   = ::GetSysColor(COLOR_WINDOW),
        .hotBackground      = RGB(204, 232, 255),
        .pureBackground     = ::GetSysColor(COLOR_3DFACE),
        .errorBackground    = ::GetSysColor(COLOR_WINDOW),
        .text               = ::GetSysColor(COLOR_WINDOWTEXT),
        .darkerText         = ::GetSysColor(COLOR_WINDOWTEXT),
        .disabledText       = ::GetSysColor(COLOR_GRAYTEXT),
        .linkText           = ::GetSysColor(COLOR_HOTLIGHT),
        .edge               = ::GetSysColor(COLOR_INACTIVEBORDER),
        .hotEdge            = ::GetSysColor(COLOR_ACTIVEBORDER),
        .disabledEdge       = ::GetSysColor(COLOR_INACTIVEBORDER),
    };
}

void NppContext::SetFocusToCurrentEdit()
{
    ::SetFocus(GetCurrentScintilla());
}

std::vector<std::wstring> NppContext::GetSessionFiles(const std::filesystem::path& sessionFilePath)
{
    std::vector<std::wstring> sessionFiles;

    /* get document count and create resources */
    int fileCount = (int)::SendMessage(_nppData._nppHandle, NPPM_GETNBSESSIONFILES, 0, (LPARAM)sessionFilePath.c_str());

    std::vector<WCHAR*> fileNames(fileCount);
    for (auto &fileName : fileNames) {
        fileName = new WCHAR[MAX_PATH];
    }

    /* get file names */
    if (::SendMessage(_nppData._nppHandle, NPPM_GETSESSIONFILES, (WPARAM)fileNames.data(), (LPARAM)sessionFilePath.c_str())) {
        for (auto &&fileName : fileNames) {
            sessionFiles.push_back(std::wstring(fileName));
        }
    }

    for (auto &fileName : fileNames) {
        delete []fileName;
        fileName = nullptr;
    }

    return sessionFiles;
}

std::filesystem::path NppContext::GetCurrentDirectory()
{
    WCHAR directoryPath[MAX_PATH];
    if (::SendMessage(_nppData._nppHandle, NPPM_GETCURRENTDIRECTORY, std::size(directoryPath), (LPARAM)&directoryPath[0])) {
        return {directoryPath};
    }
    return {};
}

int NppContext::GetVersion()
{
    return static_cast<int>(::SendMessage(_nppData._nppHandle, NPPM_GETNPPVERSION, 0, 0));
}

bool NppContext::IsSupportFluentUI()
{
    return (HIWORD(GetVersion()) >= 8);
}

void NppContext::SetMenuItemCheck(int cmdID, bool visible)
{
    ::SendMessage(_nppData._nppHandle, NPPM_SETMENUITEMCHECK, cmdID, (LPARAM)visible);
}

void NppContext::AddToolbarIcon(int cmdID, void* iconInfo, bool useDarkMode)
{
    if (useDarkMode) {
        ::SendMessage(_nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE, (WPARAM)cmdID, (LPARAM)iconInfo);
    }
    else {
        ::SendMessage(_nppData._nppHandle, NPPM_ADDTOOLBARICON_DEPRECATED, (WPARAM)cmdID, (LPARAM)iconInfo);
    }
}

std::filesystem::path NppContext::GetConfigDir()
{
    WCHAR configPath[MAX_PATH];
    ::SendMessage(_nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configPath);
    return {configPath};
}

HWND NppContext::GetCurrentScintilla()
{
    UINT currentEdit;
    ::SendMessage(_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    return (0 == currentEdit) ? _nppData._scintillaMainHandle : _nppData._scintillaSecondHandle;
}

HWND NppContext::GetMainScintilla()
{
    return _nppData._scintillaMainHandle;
}

HWND NppContext::GetSecondScintilla()
{
    return _nppData._scintillaSecondHandle;
}

std::filesystem::path NppContext::GetFullCurrentPath()
{
    WCHAR newPath[MAX_PATH];
    ::SendMessage(_nppData._nppHandle, NPPM_GETFULLCURRENTPATH, 0, (LPARAM)newPath);
    return {newPath};
}

int NppContext::GetNbOpenFiles()
{
    return (INT)::SendMessage(_nppData._nppHandle, NPPM_GETNBOPENFILES, 0, ALL_OPEN_FILES);
}

bool NppContext::GetOpenFileNames(std::vector<std::wstring>& fileNames)
{
    int docCnt = GetNbOpenFiles();
    if (docCnt <= 0) return false;

    std::vector<LPTSTR> fileNamesPtr(docCnt);
    for (int i = 0; i < docCnt; i++) {
        fileNamesPtr[i] = new WCHAR[MAX_PATH];
    }

    bool success = false;
    if (::SendMessage(_nppData._nppHandle, NPPM_GETOPENFILENAMES, (WPARAM)fileNamesPtr.data(), (LPARAM)docCnt)) {
        fileNames.clear();
        for (int i = 0; i < docCnt; i++) {
            fileNames.push_back(fileNamesPtr[i]);
        }
        success = true;
    }

    for (int i = 0; i < docCnt; i++) {
        delete[] fileNamesPtr[i];
    }

    return success;
}

void NppContext::LaunchFindFileDialog(const std::filesystem::path& directory)
{
    ::SendMessage(_nppData._nppHandle, NPPM_LAUNCHFINDINFILESDLG, (WPARAM)directory.c_str(), NULL);
}

void NppContext::RunMenuCommand(int cmdID)
{
    ::SendMessage(_nppData._nppHandle, WM_COMMAND, cmdID, 0);
}

intptr_t NppContext::SendMsgToPlugin(const std::wstring& destinationPluginName, void* communicationInfo)
{
    return ::SendMessage(_nppData._nppHandle, NPPM_MSGTOPLUGIN, (WPARAM)destinationPluginName.c_str(), (LPARAM)communicationInfo);
}
