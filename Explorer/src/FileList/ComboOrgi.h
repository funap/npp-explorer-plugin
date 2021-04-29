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


#ifndef COMBOORGI_DEFINE_H
#define COMBOORGI_DEFINE_H

#include "Explorer.h"

#include <string>
#include <vector>
#include <functional>

#ifndef CB_GETCOMBOBOXINFO
#define	CB_GETCOMBOBOXINFO	0x0164
#endif

#if(WINVER <= 0x0400)
struct COMBOBOXINFO 
{
    int cbSize;
    RECT rcItem;
    RECT rcButton;
    DWORD stateButton;
    HWND hwndCombo;
    HWND hwndItem;
    HWND hwndList; 
};
#endif 

class ComboOrgi
{
public :
	ComboOrgi();
    ~ComboOrgi ();
	virtual void init(HWND hCombo, HWND parent);
	virtual void destroy() {
	};

	void addText(LPCTSTR pszText);
	void setText(LPCTSTR pszText, UINT size = MAX_PATH);
	void getText(LPTSTR pszText, UINT size = MAX_PATH);
	bool getSelText(LPTSTR pszText);

	void setComboList(const std::vector<std::wstring> &vStrList);
	std::vector<std::wstring> getComboList();

	void clearComboList(void)
	{
		_comboItems.clear();
	};

	void setDefaultOnCharHandler(std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)> onCharHandler);

private:
	void selectComboText(LPCTSTR pszText);

private :
	HWND					_hCombo;
    WNDPROC					_hDefaultComboProc;
	HWND					_hParent;

	std::wstring				_currData;
	std::vector<std::wstring>	_comboItems;
	std::function<BOOL(UINT /* nChar */, UINT /* nRepCnt */, UINT /* nFlags */)>		_onCharHandler;

	LRESULT runProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK wndDefaultProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
		return (((ComboOrgi *)(::GetWindowLongPtr(hwnd, GWLP_USERDATA)))->runProc(hwnd, Message, wParam, lParam));
	};
};

#endif // COMBOORGI_DEFINE_H
