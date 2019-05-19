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

#include "NppInterface.h"

#include "Notepad_plus_rc.h"

NppData NppInterface::_nppData;

void NppInterface::setNppData(NppData nppData)
{
	_nppData = nppData;
}

HWND NppInterface::getWindow()
{
	return _nppData._nppHandle;
}

BOOL NppInterface::doOpen(std::wstring_view path)
{
	return static_cast<BOOL>(::SendMessage(_nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)path.data()));
}

std::wstring NppInterface::getSelectedText()
{
	UINT currentEdit;
	HWND currentSciHandle;
	std::wstring selectedTextW;

	::SendMessage(_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
	currentSciHandle = (0 == currentEdit) ? _nppData._scintillaMainHandle : _nppData._scintillaSecondHandle;

	INT charLength = (INT)::SendMessage(currentSciHandle, SCI_GETSELTEXT, 0, 0) - 1;
	if (0 < charLength) {
		std::string selectedTextA;
		selectedTextA.resize(charLength);
		::SendMessage(currentSciHandle, SCI_GETSELTEXT, 0, (LPARAM)&selectedTextA[0]);
		INT wideCharLength = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, selectedTextA.data(), charLength, nullptr, 0);
		selectedTextW.resize(wideCharLength);
		::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, selectedTextA.data(), charLength, &selectedTextW[0], wideCharLength);
	}

	return selectedTextW;
}

COLORREF NppInterface::getEditorDefaultForegroundColor()
{
	return static_cast<COLORREF>(::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR, 0, 0));
}

COLORREF NppInterface::getEditorDefaultBackgroundColor()
{
	return static_cast<COLORREF>(::SendMessage(_nppData._nppHandle, NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR, 0, 0));
}

