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

#include "SysMsg.h"
#include "ExplorerResource.h"
#include "ComboOrgi.h"



ComboOrgi::ComboOrgi()
{
	_comboItems.clear();
}

ComboOrgi::~ComboOrgi()
{
	_comboItems.clear();
}

void ComboOrgi::init(HWND hCombo)
{
	_hCombo	= hCombo;

	/* subclass combo to get edit messages */
	COMBOBOXINFO	comboBoxInfo;
	comboBoxInfo.cbSize = sizeof(COMBOBOXINFO);

	::SendMessage(_hCombo, CB_GETCOMBOBOXINFO, 0, (LPARAM)&comboBoxInfo);
	::SetWindowLongPtr(comboBoxInfo.hwndItem, GWLP_USERDATA, (LONG_PTR)this);
	_hDefaultComboProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(comboBoxInfo.hwndItem, GWLP_WNDPROC, (LONG_PTR)wndDefaultProc));
}


LRESULT ComboOrgi::runProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_KEYUP:
		{
			if (wParam == 13)
			{
				TCHAR	pszText[MAX_PATH];

				getText(pszText);
				addText(pszText);
				return TRUE;
			}
			break;
		}
		default :
			break;
	}
	return ::CallWindowProc(_hDefaultComboProc, hwnd, Message, wParam, lParam);
}


void ComboOrgi::addText(LPTSTR pszText)
{
	/* find item */
	INT		count		= _comboItems.size();
	INT		i			= 0;
	INT		hasFoundOn	= -1;

	for (; i < count; i++)
	{
		if (_tcscmp(pszText, _comboItems[i].c_str()) == 0)
		{
			hasFoundOn = count - i - 1;
		}
	}

	/* item missed create new and select it correct */
	if (hasFoundOn == -1)
	{
		_comboItems.push_back(pszText);

		::SendMessage(_hCombo, CB_RESETCONTENT, 0, 0);
		for (i = count; i >= 0 ; --i)
		{
			::SendMessage(_hCombo, CB_ADDSTRING, MAX_PATH, (LPARAM)_comboItems[i].c_str());
		}
	}
	selectComboText(pszText);
}


void ComboOrgi::setText(LPTSTR pszText, UINT size)
{
	::SendMessage(_hCombo, WM_SETTEXT, size, (LPARAM)pszText);
}

void ComboOrgi::getText(LPTSTR pszText, UINT size)
{
	::SendMessage(_hCombo, WM_GETTEXT, size, (LPARAM)pszText);
}

bool ComboOrgi::getSelText(LPTSTR pszText)
{
	INT curSel = ::SendMessage(_hCombo, CB_GETCURSEL, 0, 0);

	if (curSel != CB_ERR)
	{
		if (MAX_PATH > ::SendMessage(_hCombo, CB_GETLBTEXTLEN, curSel, 0))
		{
			::SendMessage(_hCombo, CB_GETLBTEXT, curSel, (LPARAM)pszText);
			return true;
		}
	}
	return false;
}

void ComboOrgi::selectComboText(LPTSTR pszText)
{
	LRESULT lResult	= -1;

	lResult = ::SendMessage(_hCombo, CB_FINDSTRINGEXACT, -1, (LPARAM)pszText);
	::SendMessage(_hCombo, CB_SETCURSEL, lResult, 0);
}

void ComboOrgi::setComboList(vector<string> vStrList)
{
	size_t	iCnt	= vStrList.size();

	::SendMessage(_hCombo, CB_RESETCONTENT, 0, 0);
	for (size_t i = 0; i < iCnt; i++)
	{
		addText((LPTSTR)vStrList[i].c_str());
	}
}

void ComboOrgi::getComboList(vector<string> & vStrList)
{
	TCHAR	szTemp[MAX_PATH];
	size_t	iCnt	= ::SendMessage(_hCombo, CB_GETCOUNT, 0, 0);

	vStrList.clear();

	for (size_t i = 0; i < iCnt; i++)
	{
		if (MAX_PATH > ::SendMessage(_hCombo, CB_GETLBTEXTLEN, i, 0))
		{
			::SendMessage(_hCombo, CB_GETLBTEXT, i, (LPARAM)szTemp);
			vStrList.push_back(szTemp);
		}
	}
}


