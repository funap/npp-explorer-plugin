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

#pragma once

#include <windows.h>
#include <commdlg.h>
#include <WCHAR.H>

#include <vector>
#include <string>

const int nbExtMax = 10;
const int extLenMax = 10;

typedef std::vector<std::wstring> stringVector;
//const bool styleOpen = true;
//const bool styleSave = false;

class FileDlg
{
public:
    FileDlg(HINSTANCE hInst, HWND hwnd);
    void setExtFilter(LPCTSTR, LPCTSTR, ...);
    void setDefFileName(LPCTSTR fn) { wcscpy(_fileName, fn); }

    LPTSTR doSaveDlg();
    stringVector * doOpenMultiFilesDlg();
    LPTSTR doOpenSingleFileDlg();
    bool isReadOnly() const { return _ofn.Flags & OFN_READONLY; }

private:
    WCHAR _fileName[MAX_PATH*8];

    WCHAR _fileExt[MAX_PATH*2];
    int _nbCharFileExt;
    //bool _isMultiSel;

    stringVector _fileNames;
    OPENFILENAME _ofn;

    WCHAR _extArray[nbExtMax][extLenMax];
    int _nbExt;

    static FileDlg *staticThis;
};
