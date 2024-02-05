/*
  The MIT License (MIT)

  Copyright (c) 2019 funap

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

#include "FileFilter.h"

namespace {
std::vector<std::wstring> split(const std::wstring_view &string, WCHAR delim)
{
    std::vector<std::wstring> result;
    std::wstring item;
    for (WCHAR ch : string) {
        if (ch == delim) {
            if (!item.empty()) {
                result.push_back(item);
            }
            item.clear();
        }
        else {
            item += ch;
        }
    }
    if (!item.empty()) {
        result.push_back(item);
    }
    return result;
}

BOOL wildcmp(LPCTSTR wild, LPCTSTR string)
{
    // Written by Jack Handy - jakkhandy@hotmail.com
    // See: http://www.codeproject.com/string/wildcmp.asp
    LPCTSTR cp = nullptr;
    LPCTSTR mp = nullptr;

    while ((*string) && (*wild != '*')) {
        if ((tolower(*wild) != tolower(*string)) && (*wild != '?')) {
            return FALSE;
        }
        wild++;
        string++;
    }

    while (*string) {
        if (*wild == '*') {
            if (!*++wild) {
                return TRUE;
            }
            mp = wild;
            cp = string + 1;
        }
        else if ((tolower(*wild) == tolower(*string)) || (*wild == '?')) {
            wild++;
            string++;
        }
        else {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*') {
        wild++;
    }
    return !*wild;
}

} // namespace


FileFilter::FileFilter()
{
}

FileFilter::~FileFilter()
{
}

void FileFilter::setFilter(std::wstring_view newFilter)
{
    _filterString = std::wstring(L"*.*");
    if (0 < newFilter.length()) {
        _filterString = newFilter;
    }

    const WCHAR             SEPARATOR  = ';';
    const std::wstring_view DENY_BEGIN = L"[^";
    const std::wstring_view DENY_END   = L"]";

    SIZE_T denyBeginPos = _filterString.find_first_of(DENY_BEGIN);
    if (std::wstring::npos == denyBeginPos) {
        _allowList = split(_filterString, SEPARATOR);
    }
    else {
        SIZE_T denyEndPos = _filterString.find_first_of(DENY_END, denyBeginPos);
        if (std::wstring::npos != denyEndPos && denyEndPos > denyBeginPos) {
            _allowList = split(_filterString.substr(0, denyBeginPos), SEPARATOR);
            denyBeginPos += DENY_BEGIN.length();
            _denyList  = split(_filterString.substr(denyBeginPos, denyEndPos - denyBeginPos), SEPARATOR);
        }
        else {
            _allowList = split(_filterString.substr(0, denyBeginPos), SEPARATOR);
            denyBeginPos += DENY_BEGIN.length();
            _denyList  = split(_filterString.substr(denyBeginPos), SEPARATOR);
        }
    }

    // When only the deny list was inputted
    if (_allowList.empty()) {
        _allowList.emplace_back(L"*.*");
    }
}

LPCWSTR FileFilter::getFilterString()
{
    return _filterString.c_str();
}


BOOL FileFilter::match(const std::wstring &fileName)
{
    if (fileName.empty()) {
        return FALSE;
    }

    if (L"*.*" == _filterString) {
        return TRUE;
    }

    for (const auto &deny : _denyList) {
        if (0 != wildcmp(deny.c_str(), fileName.c_str())) {
            return FALSE;
        }
    }

    for (const auto& allow : _allowList) {
        if (0 != wildcmp(allow.c_str(), fileName.c_str())) {
            return TRUE;
        }
    }

    return FALSE;
}
