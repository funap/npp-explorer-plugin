//this file is part of notepad++
//Copyright (C)2003 Don HO ( donho@altern.org )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "FileDlg.h"

#include <cstdarg>

//FileDlg *FileDlg::staticThis = NULL;

FileDlg::FileDlg(HINSTANCE hInst, HWND hwnd) 
    : _nbCharFileExt(0), _nbExt(0)
{//staticThis = this;
    for (int i = 0 ; i < nbExtMax ; i++) {
        _extArray[i][0] = '\0';
    }

    ::ZeroMemory(_fileExt, sizeof(_fileExt));
    _fileName[0] = '\0';
 
    _ofn.lStructSize = sizeof(_ofn);
    _ofn.hwndOwner = hwnd; 
    _ofn.hInstance = hInst;
    _ofn.lpstrFilter = _fileExt;
    _ofn.lpstrCustomFilter = nullptr;
    _ofn.nMaxCustFilter = 0L;
    _ofn.nFilterIndex = 1L;
    _ofn.lpstrFile = _fileName;
    _ofn.nMaxFile = sizeof(_fileName);
    _ofn.lpstrFileTitle = nullptr;
    _ofn.nMaxFileTitle = 0;
    _ofn.lpstrInitialDir = nullptr;
    _ofn.lpstrTitle = nullptr;
    _ofn.nFileOffset  = 0;
    _ofn.nFileExtension = 0;
    _ofn.lpstrDefExt = nullptr;  // No default extension
    _ofn.lCustData = 0;
    _ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_LONGNAMES | DS_CENTER | OFN_HIDEREADONLY;
}

// This function set and concatenate the filter into the list box of FileDlg.
// The 1st parameter is the description of the file type, the 2nd .. Nth parameter(s) is (are)
// the file extension which should be ".WHATEVER", otherwise it (they) will be considered as
// a file name to filter. Since the nb of arguments is variable, you have to add NULL at the end.
// example : 
// FileDlg.setExtFilter("c/c++ src file", ".c", ".cpp", ".cxx", ".h", NULL);
// FileDlg.setExtFilter("Makefile", "makefile", "GNUmakefile", NULL);
void FileDlg::setExtFilter(LPCTSTR extText, LPCTSTR ext, ...)
{
    // fill out the ext array for save as file dialog
    if (_nbExt < nbExtMax) {
        wcscpy(_extArray[_nbExt++], ext);
    }
    // 
    std::wstring extFilter = extText;
    std::wstring exts;

    va_list pArg;
    va_start(pArg, ext);

    const WCHAR* ext2Concat;
    ext2Concat = ext;
    do {
        if (ext2Concat[0] == '.') {
            exts += L"*";
        }
        exts += ext2Concat;
        exts += L";";
    } while ((ext2Concat = va_arg(pArg, const WCHAR*)) != nullptr);

    va_end(pArg);

    // remove the last ';'
    exts = exts.substr(0, exts.length()-1);

    extFilter += L" (";
    extFilter += exts + L")";
    
    LPTSTR pFileExt = _fileExt + _nbCharFileExt;
    memcpy(pFileExt, extFilter.c_str(), extFilter.length() + 1);
    _nbCharFileExt += (int)extFilter.length() + 1;
    
    pFileExt = _fileExt + _nbCharFileExt;
    memcpy(pFileExt, exts.c_str(), exts.length() + 1);
    _nbCharFileExt += (int)exts.length() + 1;
}

LPTSTR FileDlg::doOpenSingleFileDlg() 
{
    WCHAR dir[MAX_PATH];
    ::GetCurrentDirectory(_countof(dir), dir);
    _ofn.lpstrInitialDir = dir;

    _ofn.Flags |= OFN_FILEMUSTEXIST;

    LPTSTR fn = nullptr;
    try {
        fn = ::GetOpenFileName(&_ofn) ? _fileName : nullptr;
    }
    catch(...) {
        ::MessageBox(nullptr, TEXT("GetSaveFileName crashes!!!"), TEXT(""), MB_OK);
    }
    return (fn);
}

stringVector * FileDlg::doOpenMultiFilesDlg()
{
    WCHAR dir[MAX_PATH];
    ::GetCurrentDirectory(_countof(dir), dir);
    _ofn.lpstrInitialDir = dir;

    _ofn.Flags |= OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

    if (::GetOpenFileName(&_ofn)) {
        WCHAR fn[MAX_PATH] = {};
        LPTSTR pFn = _fileName + wcslen(_fileName) + 1;
        if (!(*pFn)) {
            _fileNames.emplace_back(_fileName);
        }
        else {
            wcscpy(fn, _fileName);
            if (fn[wcslen(fn)-1] != '\\') {
                wcscat(fn, TEXT("\\"));
            }
        }
        int term = int(wcslen(fn));

        while (*pFn) {
            fn[term] = '\0';
            wcscat(fn, pFn);
            _fileNames.emplace_back(fn);
            pFn += wcslen(pFn) + 1;
        }

        return &_fileNames;
    }
    return nullptr;
}

LPTSTR FileDlg::doSaveDlg() 
{
    WCHAR dir[MAX_PATH];
    ::GetCurrentDirectory(_countof(dir), dir);
    _ofn.lpstrInitialDir = dir;
    _ofn.Flags |= OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;

    LPTSTR fn = nullptr;
    try {
        fn = ::GetSaveFileName(&_ofn) ? _fileName : nullptr;
    }
    catch(...) {
        ::MessageBox(nullptr, TEXT("GetSaveFileName crashes!!!"), TEXT(""), MB_OK);
    }
    return (fn);
}


