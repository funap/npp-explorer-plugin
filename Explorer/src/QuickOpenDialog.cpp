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

#include "QuickOpenDialog.h"

#include <cwctype>

#include <windowsx.h>

#include "FuzzyMatcher.h"
#include "NppInterface.h"

namespace {
	constexpr UINT WM_INDEX_BUILD_COMPLETED	= WM_USER + 1;
	constexpr UINT WM_INDEX_BUILD_CANCELED	= WM_USER + 2;
	constexpr UINT_PTR UPDATE_RESULTLIST	= 1;
	constexpr UINT_PTR UPDATE_PROGRESSBAR	= 2;

	UINT getDpiForWindow(HWND hWnd) {
		UINT dpi = 96;
		HMODULE user32dll = ::LoadLibrary(L"User32.dll");
		if (nullptr != user32dll) {
			typedef UINT(WINAPI * PGETDPIFORWINDOW)(HWND);
			PGETDPIFORWINDOW pGetDpiForWindow = (PGETDPIFORWINDOW)::GetProcAddress(user32dll, "GetDpiForWindow");
			if (nullptr != pGetDpiForWindow) {
				dpi = pGetDpiForWindow(hWnd);
			}
			else {
				HDC hdc = GetDC(hWnd);
				dpi = GetDeviceCaps(hdc, LOGPIXELSX);
				ReleaseDC(hWnd, hdc);
			}
			::FreeLibrary(user32dll);
		}
		return dpi;
	}
	
	void removeWhitespaces(std::wstring& str)
	{
		auto it = str.begin();
		while (it != str.end()) {
			if (std::iswcntrl(*it) || std::iswblank(*it)) {
				it = str.erase(it);
			}
			else {
				++it;
			}
		}
	};
}

void QuickOpenDlg::init(HINSTANCE hInst, HWND parent, ExProp* prop)
{
	_pExProp = prop;
	_direcotryIndex.setListener(this);

	Window::init(hInst, parent);
	create(IDD_QUICK_OPEN_DLG, FALSE, TRUE);
}

void QuickOpenDlg::setCurrentPath(const std::filesystem::path& currentPath)
{
	if (_direcotryIndex.GetCurrentDir().compare(currentPath)) {
		_pattern = L"*";

		::SendMessage(_hWndResult, LB_SETCOUNT, 0, 0);
		::SendMessage(_hWndResult, LB_SETCURSEL, 0, 0);

		_direcotryIndex.cancel();
		_direcotryIndex.init(currentPath);
		_direcotryIndex.build();
	}
}

void QuickOpenDlg::show()
{
	std::wstring selectedText = NppInterface::getSelectedText();
	if (!selectedText.empty()) {
		::Edit_SetText(_hWndEdit, selectedText.c_str());
	}
	populateResultList();

	setDefaultPosition();
	display(true);
	::PostMessage(_hSelf, WM_NEXTDLGCTL, (WPARAM)_hWndEdit, TRUE);

	if (_direcotryIndex.isIndexing()) {
		::SetTimer(_hSelf, UPDATE_PROGRESSBAR, 33, nullptr);
	}
}

void QuickOpenDlg::close()
{
	::KillTimer(_hSelf, UPDATE_RESULTLIST);
	::KillTimer(_hSelf, UPDATE_PROGRESSBAR);
	display(false);
}

void QuickOpenDlg::onIndexBuildCompleted() const
{
	::PostMessage(_hSelf, WM_INDEX_BUILD_COMPLETED, 0, 0);
}

void QuickOpenDlg::onIndexBuildCanceled() const
{
	::PostMessage(_hSelf, WM_INDEX_BUILD_CANCELED, 0, 0);	
}

void QuickOpenDlg::setDefaultPosition()
{
	RECT rc;
	::GetClientRect(_hParent, &rc);
	POINT center;
	center.x = rc.left + (rc.right - rc.left) / 2;
	center.y = 0;
	::ClientToScreen(_hParent, &center);

	int x = center.x - (_rc.right - _rc.left) / 2;
	int y = center.y;

	::SetWindowPos(_hSelf, HWND_TOP, x, y, _rc.right - _rc.left, _rc.bottom - _rc.top, SWP_SHOWWINDOW);
}

VOID QuickOpenDlg::calcMetrics()
{
	UINT dpi = getDpiForWindow(_hSelf);
	if (0 == dpi) {
		dpi = 96;
	}

	TEXTMETRIC textMetric;
	HDC hdc = ::GetDC(_hWndResult);
	::GetTextMetrics(hdc, &textMetric);
	::ReleaseDC(_hWndResult, hdc);

	_itemMarginLeft          = ::MulDiv(7, dpi, 96);
	_itemTextHeight          = ::MulDiv(textMetric.tmHeight, dpi, 96);
	_itemTextExternalLeading = ::MulDiv(textMetric.tmExternalLeading, dpi, 96);
}

BOOL QuickOpenDlg::onDrawItem(LPDRAWITEMSTRUCT drawItem)
{
	UINT& itemID = drawItem->itemID;

	if (-1 != itemID) {
		COLORREF backgroundColor		= RGB(255, 255, 255);
		COLORREF backgroundMatchColor	= RGB(252, 234, 128);
		COLORREF textColor1				= RGB( 33,  33,  33);
		COLORREF textColor2				= RGB(128, 128, 128);

		if ((drawItem->itemState) & (ODS_SELECTED)) {
			backgroundColor				= RGB(230, 231, 239);
			backgroundMatchColor		= RGB(252, 234, 128);
			textColor1					= RGB( 33,  33,  33);
			textColor2					= RGB(128, 128, 128);
		}

		// Fill background
		const HBRUSH hBrush = ::CreateSolidBrush(backgroundColor);
		::FillRect(drawItem->hDC, &drawItem->rcItem, hBrush);
		::DeleteObject(hBrush);

		// first line
		RECT drawPosition = drawItem->rcItem;
		drawPosition.top += _itemTextExternalLeading;
		drawPosition.left = drawItem->rcItem.left + _itemMarginLeft;
		::SetTextColor(drawItem->hDC, textColor1);
		::SetBkMode(drawItem->hDC, OPAQUE);
		std::wstring text = _results[itemID].second->filename().wstring();
		

		std::vector<size_t> match;
		FuzzyMatcher matcher(_pattern);
		if (matcher.ScoreMatch(text, &match)) {
			match.emplace_back(std::wstring::npos);		// Add terminal for iterator
			size_t matchIndex = 0;
			RECT calcRect = {};
			for (size_t i = 0; i < text.length(); ++i) {
				if (i == match[matchIndex]) {
					::SetBkColor(drawItem->hDC, backgroundMatchColor);
					++matchIndex;
				}
				else {
					::SetBkColor(drawItem->hDC, backgroundColor);
				}
				::DrawText(drawItem->hDC, &text[i], 1, &drawPosition, DT_SINGLELINE);
				::DrawText(drawItem->hDC, &text[i], 1, &calcRect, DT_SINGLELINE | DT_CALCRECT);
				drawPosition.left += calcRect.right;
			}
		}
		else {
			::SetBkColor(drawItem->hDC, backgroundColor);
			::DrawText(drawItem->hDC, text.c_str(), static_cast<INT>(text.length()), &drawPosition, DT_SINGLELINE);
		}

		// second line
		drawPosition.top += _itemTextHeight;
		drawPosition.left = drawItem->rcItem.left + _itemMarginLeft;
		::SetTextColor(drawItem->hDC, textColor2);
		::SetBkMode(drawItem->hDC, TRANSPARENT);
		text = std::filesystem::relative(_results[itemID].second->wstring(), _direcotryIndex.GetCurrentDir()).wstring();
		::DrawText(drawItem->hDC, text.c_str(), static_cast<INT>(text.length()), &drawPosition, DT_SINGLELINE);
	}
	return TRUE;
}

BOOL CALLBACK QuickOpenDlg::run_dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static int progressPos = 0;

	BOOL ret = FALSE;
	switch (Message) {
	case WM_INDEX_BUILD_COMPLETED:
		progressPos = 0;
		::KillTimer(_hSelf, UPDATE_PROGRESSBAR);
		::InvalidateRect(_hSelf, &_progressBarRect, false);
		::UpdateWindow(_hSelf);
		populateResultList();
		ret = TRUE;
		break;
	case WM_INDEX_BUILD_CANCELED:
		progressPos = 0;
		ret = TRUE;
		break;
	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_LIST_RESULTS) {
			ret = onDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
		}
		break;
	case WM_TIMER:
		switch (wParam) {
		case UPDATE_RESULTLIST:
			populateResultList();
			::KillTimer(_hSelf, UPDATE_RESULTLIST);
			ret = TRUE;
			break;
		case UPDATE_PROGRESSBAR:
			::InvalidateRect(_hSelf, &_progressBarRect, false);
			::UpdateWindow(_hSelf);
			ret = TRUE;
			break;
		default:
			break;
		}
		break;
	case WM_PRINTCLIENT:
	case WM_PAINT:
	{
		HDC hDC = (Message == WM_PRINTCLIENT)
			? reinterpret_cast<HDC>(wParam)
			: GetDCEx(hWnd, nullptr, DCX_INTERSECTUPDATE | DCX_CACHE | DCX_CLIPCHILDREN | DCX_CLIPSIBLINGS);

		// Erase progressbar background
		const COLORREF backgroundColor = ::GetSysColor(COLOR_3DFACE);
		const HBRUSH hBkBrush = ::CreateSolidBrush(backgroundColor);
		::FillRect(hDC, &_progressBarRect, hBkBrush);
		::DeleteObject(hBkBrush);

		// draw progress bar
		if (_direcotryIndex.isIndexing()) {
			RECT barRect	= _progressBarRect;
			barRect.left	= max(_progressBarRect.left, progressPos - 16);
			barRect.right	= min(progressPos + 16,		_progressBarRect.right);
			progressPos	+= 2;
			progressPos	%= _progressBarRect.right;

			const COLORREF progressBarColor = RGB(14, 112, 192);
			const HBRUSH hPbBrush = ::CreateSolidBrush(progressBarColor);
			::FillRect(hDC, &barRect, hPbBrush);
			::DeleteObject(hPbBrush);
		}

		ret = FALSE; // continue default proc
		break;
	}
	case WM_COMMAND : 
		switch (LOWORD(wParam)) {
		case IDC_EDIT_SEARCH:
			if (EN_CHANGE == HIWORD(wParam)) {
				::KillTimer(_hSelf, UPDATE_RESULTLIST);
				::SetTimer(_hSelf, UPDATE_RESULTLIST, 100, nullptr);
				ret = TRUE;
			}
			break;
		case IDCANCEL:
			close();
			ret = TRUE;
			break;
		case IDC_LIST_RESULTS:
			if (LBN_DBLCLK != HIWORD(wParam)) {
				break;
			}
			// FALLTHROUGH
		case IDOK:	
		{
			const int selection = (INT)::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETCURSEL, 0, 0);
			if (0 <= selection) {
				if (static_cast<SIZE_T>(selection) < _results.size()) {
					NppInterface::doOpen(_results[selection].second->wstring());
				}
			}
			close();
			ret = TRUE;
			break;
		}
		default:
			break;
		}
		break;
	case WM_INITDIALOG:
	{
		_hWndResult = ::GetDlgItem(_hSelf, IDC_LIST_RESULTS);
		_hWndEdit = ::GetDlgItem(_hSelf, IDC_EDIT_SEARCH);
		calcMetrics();
		const int height = _itemTextHeight * 2 + _itemTextExternalLeading;
		::SendMessage(_hWndResult, LB_SETITEMHEIGHT, 0, height);

		::SetWindowLongPtr(_hWndEdit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		_defaultEditProc = (WNDPROC)::SetWindowLongPtr(_hWndEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndDefaultEditProc));

		GetClientRect(hWnd, &_progressBarRect);
		_progressBarRect.top = 2;
		_progressBarRect.bottom = _progressBarRect.top + 2;
		break;
	}
	case WM_DESTROY:
		if (_defaultEditProc) {
			::SetWindowLongPtr(_hWndEdit, GWLP_WNDPROC, (LONG_PTR)_defaultEditProc);
			_defaultEditProc = nullptr;
		}
		break;
	case WM_ACTIVATE:
		if (WA_INACTIVE == LOWORD(wParam)) {
			close();
		}
		break;
	default:
		break;
	}
	return ret;
}

void QuickOpenDlg::populateResultList()
{
	if (_direcotryIndex.isIndexing()) {
		return;
	}

	const int bufferLength = ::Edit_GetTextLength(_hWndEdit) + 1;	// text length + null terminated string
	std::wstring pattern;
	if (1 < bufferLength) {
		pattern.resize(bufferLength);
		::Edit_GetText(_hWndEdit, &pattern[0], bufferLength);
		removeWhitespaces(pattern);
	}

	if (_pattern != pattern) {
		_results.clear();

		FuzzyMatcher matcher(pattern);

		for (const auto& entry : _direcotryIndex.GetFileIndex()) {
			if (_pExProp->fileFilter.match(entry.filename())) {
				if (0 < pattern.length()) {
					int score = matcher.ScoreMatch(entry.filename().wstring());
					if (0 < score) {
						_results.emplace_back(std::make_pair(score, &entry));
					}
				}
				else {
					_results.emplace_back(std::make_pair(0, &entry));
				}
			}
		}
		std::sort(_results.begin(), _results.end(), [](auto&& a, auto&& b) { return a.first > b.first; });

		::SendMessage(_hWndResult, LB_SETCOUNT, _results.size(), 0);
		::SendMessage(_hWndResult, LB_SETCURSEL, 0, 0);
		_pattern = pattern;
	}
}

LRESULT APIENTRY QuickOpenDlg::wndDefaultEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	QuickOpenDlg *dlg = (QuickOpenDlg *)(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	return dlg->runEditProc(hWnd, uMsg, wParam, lParam);
}

LRESULT APIENTRY QuickOpenDlg::runEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_UP:
		case VK_DOWN:
		case VK_PRIOR:	// Page Up
		case VK_NEXT:	// Page Down
			// transfer to listview
			return ::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, WM_KEYDOWN, wParam, lParam);
		default:
			break;
		}
		break;
	default:
		break;
	}
	return ::CallWindowProc(_defaultEditProc, hWnd, uMsg, wParam, lParam);
}
