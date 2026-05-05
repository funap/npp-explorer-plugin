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

#include "Settings.h"

#include <shlwapi.h>
#include <ranges>
#include <format>

#include "Editor.h"

/* ini file sections */
namespace {

constexpr WCHAR SECTION_EXPLORER[]      = L"Explorer";
constexpr WCHAR SECTION_FILTER_HISTORY[] = L"FilterHistory";

/* section Explorer keys */
constexpr WCHAR LastPath[]          = L"LastPath";
constexpr WCHAR RootFolder[]        = L"RootFolder";
constexpr WCHAR SplitterPos[]       = L"SplitterPos";
constexpr WCHAR SplitterPosHor[]    = L"SplitterPosHor";
constexpr WCHAR SortAsc[]           = L"SortAsc";
constexpr WCHAR SortPos[]           = L"SortPos";
constexpr WCHAR ColPosName[]        = L"ColPosName";
constexpr WCHAR ColPosExt[]         = L"ColPosExt";
constexpr WCHAR ColPosSize[]        = L"ColPosSize";
constexpr WCHAR ColPosDate[]        = L"ColPosDate";
constexpr WCHAR ShowHiddenData[]    = L"ShowHiddenData";
constexpr WCHAR ShowBraces[]        = L"ShowBraces";
constexpr WCHAR ShowLongInfo[]      = L"ShowLongInfo";
constexpr WCHAR AddExtToName[]      = L"AddExtToName";
constexpr WCHAR AutoUpdate[]        = L"AutoUpdate";
constexpr WCHAR AutoNavigate[]      = L"AutoNavigate";
constexpr WCHAR UseFullTree[]       = L"UseFullTree";
constexpr WCHAR SizeFormat[]        = L"SizeFormat";
constexpr WCHAR DateFormat[]        = L"DateFormat";
constexpr WCHAR LastFilter[]        = L"LastFilter";
constexpr WCHAR TimeOut[]           = L"TimeOut";
constexpr WCHAR UseSystemIcons[]    = L"UseSystemIcons";
constexpr WCHAR NppExecAppName[]    = L"NppExecAppName";
constexpr WCHAR NppExecScriptPath[] = L"NppExecScriptPath";
constexpr WCHAR CphProgramName[]    = L"CphProgramName";
constexpr WCHAR MaxHistorySize[]    = L"MaxHistorySize";
constexpr WCHAR FontHeight[]        = L"FontHeight";
constexpr WCHAR FontWeight[]        = L"FontWeight";
constexpr WCHAR FontItalic[]        = L"FontItalic";
constexpr WCHAR FontFaceName[]      = L"FontFaceName";
constexpr WCHAR HideFolders[]       = L"HideFolders";

constexpr WCHAR EXPLORER_INI[]      = L"Explorer.ini";

// Helper functions for INI operations
int ReadInt(const WCHAR* key, int defaultValue, const std::filesystem::path& iniPath) {
    return ::GetPrivateProfileInt(SECTION_EXPLORER, key, defaultValue, iniPath.c_str());
}

void WriteInt(const WCHAR* key, int value, const std::filesystem::path& iniPath) {
    ::WritePrivateProfileString(SECTION_EXPLORER, key, std::to_wstring(value).c_str(), iniPath.c_str());
}

bool ReadBool(const WCHAR* key, bool defaultValue, const std::filesystem::path& iniPath) {
    return ::GetPrivateProfileInt(SECTION_EXPLORER, key, defaultValue ? TRUE : FALSE, iniPath.c_str()) != FALSE;
}

void WriteBool(const WCHAR* key, bool value, const std::filesystem::path& iniPath) {
    ::WritePrivateProfileString(SECTION_EXPLORER, key, value ? L"1" : L"0", iniPath.c_str());
}

std::wstring ReadString(const WCHAR* key, const WCHAR* defaultValue, const std::filesystem::path& iniPath) {
    WCHAR temp[MAX_PATH];
    ::GetPrivateProfileString(SECTION_EXPLORER, key, defaultValue, temp, MAX_PATH, iniPath.c_str());
    return temp;
}

void WriteString(const WCHAR* key, const std::wstring& value, const std::filesystem::path& iniPath) {
    ::WritePrivateProfileString(SECTION_EXPLORER, key, value.c_str(), iniPath.c_str());
}

template<typename T>
T ReadEnum(const WCHAR* key, T defaultValue, const std::filesystem::path& iniPath) {
    return static_cast<T>(::GetPrivateProfileInt(SECTION_EXPLORER, key, static_cast<int>(defaultValue), iniPath.c_str()));
}

template<typename T>
void WriteEnum(const WCHAR* key, T value, const std::filesystem::path& iniPath) {
    WriteInt(key, static_cast<int>(value), iniPath);
}

} // namespace


void Settings::Load(const std::filesystem::path& configDir)
{
    _configPath = configDir;
    
    if (!std::filesystem::exists(_configPath)) {
        std::filesystem::create_directories(_configPath);
    }

    _iniFilePath = _configPath / EXPLORER_INI;
    if (!std::filesystem::exists(_iniFilePath)) {
        HANDLE hFile = ::CreateFile(_iniFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            BYTE bom[] = {0xFF, 0xFE};
            DWORD dwByteWritten = 0;
            ::WriteFile(hFile, bom, sizeof(bom), &dwByteWritten, nullptr);
            ::CloseHandle(hFile);
        }
    }

    _currentDir = ReadString(LastPath, L"C:\\", _iniFilePath);
    _rootFolder = ReadString(RootFolder, L"", _iniFilePath);
    _iSplitterPos = ReadInt(SplitterPos, 120, _iniFilePath);
    _iSplitterPosHorizontal = ReadInt(SplitterPosHor, 200, _iniFilePath);
    _bAscending = ReadBool(SortAsc, true, _iniFilePath);
    _iSortPos = ReadInt(SortPos, 0, _iniFilePath);
    _iColumnPosName = ReadInt(ColPosName, 150, _iniFilePath);
    _iColumnPosExt = ReadInt(ColPosExt, 50, _iniFilePath);
    _iColumnPosSize = ReadInt(ColPosSize, 70, _iniFilePath);
    _iColumnPosDate = ReadInt(ColPosDate, 100, _iniFilePath);
    _bShowHidden = ReadBool(ShowHiddenData, false, _iniFilePath);
    _bViewBraces = ReadBool(ShowBraces, true, _iniFilePath);
    _bViewLong = ReadBool(ShowLongInfo, false, _iniFilePath);
    _bAddExtToName = ReadBool(AddExtToName, false, _iniFilePath);
    _bAutoUpdate = ReadBool(AutoUpdate, true, _iniFilePath);
    _bAutoNavigate = ReadBool(AutoNavigate, false, _iniFilePath);
    _bUseFullTree = ReadBool(UseFullTree, false, _iniFilePath);
    _bHideFoldersInFileList = ReadBool(HideFolders, false, _iniFilePath);
    _fmtSize = ReadEnum(SizeFormat, SizeFmt::SFMT_KBYTE, _iniFilePath);
    _fmtDate = ReadEnum(DateFormat, DateFmt::DFMT_ENG, _iniFilePath);
    _uTimeout = static_cast<UINT>(ReadInt(TimeOut, 1000, _iniFilePath));
    _bUseSystemIcons = ReadBool(UseSystemIcons, true, _iniFilePath);
    _maxHistorySize = static_cast<size_t>(ReadInt(MaxHistorySize, 50, _iniFilePath));
    
    _nppExecProp.szAppName = ReadString(NppExecAppName, L"NppExec.dll", _iniFilePath);
    _nppExecProp.szScriptPath = ReadString(NppExecScriptPath, _configPath.c_str(), _iniFilePath);
    _cphProgram.szAppName = ReadString(CphProgramName, L"cmd.exe", _iniFilePath);

    _vStrFilterHistory.clear();
    for (int i = 0; i < 20; i++) {
        std::wstring key = std::to_wstring(i);
        WCHAR pszTemp[MAX_PATH];
        if (::GetPrivateProfileString(SECTION_FILTER_HISTORY, key.c_str(), L"", pszTemp, MAX_PATH, _iniFilePath.c_str()) != 0) {
            _vStrFilterHistory.emplace_back(pszTemp);
        }
    }
    _fileFilter.setFilter(ReadString(LastFilter, L"*.*", _iniFilePath).c_str());

    if (!std::filesystem::exists(_currentDir)) {
        _currentDir = L"C:\\";
    }

    // get default font
    ::SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &_logFont, 0);

    int fontHeight = ReadInt(FontHeight, 0, _iniFilePath);
    if (fontHeight != 0) _logFont.lfHeight = fontHeight;

    int fontWeight = ReadInt(FontWeight, 0, _iniFilePath);
    if (fontWeight != 0) _logFont.lfWeight = fontWeight;

    int fontItalic = ReadInt(FontItalic, 0, _iniFilePath);
    if (fontItalic != 0) _logFont.lfItalic = TRUE;

    std::wstring faceName = ReadString(FontFaceName, L"", _iniFilePath);
    if (!faceName.empty()) {
        wcsncpy(_logFont.lfFaceName, faceName.c_str(), LF_FACESIZE);
    }
}

void Settings::Save()
{
    WriteString(LastPath, _currentDir, _iniFilePath);
    WriteString(RootFolder, _rootFolder, _iniFilePath);
    WriteInt(SplitterPos, _iSplitterPos, _iniFilePath);
    WriteInt(SplitterPosHor, _iSplitterPosHorizontal, _iniFilePath);
    WriteBool(SortAsc, _bAscending, _iniFilePath);
    WriteInt(SortPos, _iSortPos, _iniFilePath);
    WriteInt(ColPosName, _iColumnPosName, _iniFilePath);
    WriteInt(ColPosExt, _iColumnPosExt, _iniFilePath);
    WriteInt(ColPosSize, _iColumnPosSize, _iniFilePath);
    WriteInt(ColPosDate, _iColumnPosDate, _iniFilePath);
    WriteBool(ShowHiddenData, _bShowHidden, _iniFilePath);
    WriteBool(ShowBraces, _bViewBraces, _iniFilePath);
    WriteBool(ShowLongInfo, _bViewLong, _iniFilePath);
    WriteBool(AddExtToName, _bAddExtToName, _iniFilePath);
    WriteBool(AutoUpdate, _bAutoUpdate, _iniFilePath);
    WriteBool(AutoNavigate, _bAutoNavigate, _iniFilePath);
    WriteBool(UseFullTree, _bUseFullTree, _iniFilePath);
    WriteBool(HideFolders, _bHideFoldersInFileList, _iniFilePath);    
    WriteEnum(SizeFormat, _fmtSize, _iniFilePath);
    WriteEnum(DateFormat, _fmtDate, _iniFilePath);
    WriteInt(TimeOut, static_cast<int>(_uTimeout), _iniFilePath);
    WriteBool(UseSystemIcons, _bUseSystemIcons, _iniFilePath);
    WriteString(NppExecAppName, _nppExecProp.szAppName, _iniFilePath);
    WriteString(NppExecScriptPath, _nppExecProp.szScriptPath, _iniFilePath);
    WriteString(CphProgramName, _cphProgram.szAppName, _iniFilePath);
    WriteInt(MaxHistorySize, static_cast<int>(_maxHistorySize), _iniFilePath);

    WriteInt(FontHeight, _logFont.lfHeight, _iniFilePath);
    WriteInt(FontWeight, _logFont.lfWeight, _iniFilePath);
    WriteInt(FontItalic, _logFont.lfItalic, _iniFilePath);
    WriteString(FontFaceName, _logFont.lfFaceName, _iniFilePath);

    ::WritePrivateProfileString(SECTION_FILTER_HISTORY, nullptr, nullptr, _iniFilePath.c_str());
    for (size_t i = 0; i < _vStrFilterHistory.size() && i < 20; ++i) {
        ::WritePrivateProfileString(SECTION_FILTER_HISTORY, std::to_wstring(i).c_str(), _vStrFilterHistory[i].c_str(), _iniFilePath.c_str());
    }
    WriteString(LastFilter, _fileFilter.getFilterString(), _iniFilePath);
}

void Settings::InitializeFonts()
{
    _defaultFont.reset(::CreateFontIndirect(&_logFont));

    LOGFONT logfontUnder = _logFont;
    logfontUnder.lfUnderline = TRUE;
    _underlineFont.reset(::CreateFontIndirect(&logfontUnder));
}
