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

#include <map>
#include <future>
#include <thread>
#include <cwctype>

#include <windowsx.h>

#include "FuzzyMatcher.h"
#include "NppInterface.h"

namespace {
	constexpr UINT WM_BUILD_INDEX_COMPLETE = WM_USER + 1;
	constexpr UINT_PTR UPDATE_TIMER = 1;

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
	
	void rebuildFileIndex(std::vector<std::filesystem::path> *fileIndex , const std::filesystem::path& path)
	{
		namespace fs = std::filesystem;
		fs::directory_iterator it(path, fs::directory_options::skip_permission_denied);
		fs::directory_iterator end;
		while (it != end) {
			try {
				if (it->is_directory()) {
					const std::wstring name = it->path().filename().wstring();
					if ('.' == name[0]) {
						// skip hidden name directory
					}
					else {
						rebuildFileIndex(fileIndex, it->path());
					}
				}
				else if (it->is_regular_file()) {
					fileIndex->emplace_back(it->path());
				}
			}
			catch (fs::filesystem_error& err) {
				OutputDebugStringA(err.what());
			}
			++it;
		}
	};

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

	auto scoredMatches(std::vector<std::filesystem::path>& fileIndex, const std::wstring& pattern)
	{
		FuzzyMatcher matcher(pattern);
		std::vector<std::pair<int, const std::filesystem::path*>> matches;

		for (const auto& entry : fileIndex) {
			int score = matcher.ScoreMatch(entry.filename().wstring());
			if (0 < score) {
				matches.emplace_back(std::make_pair(score, &entry));
			}
		}
		std::sort(matches.begin(), matches.end(), [](auto&& a, auto&& b) { return a.first > b.first; });
		return matches;
	};
}

void QuickOpenDlg::init(HINSTANCE hInst, HWND parent, ExProp* prop)
{
	_pExProp = prop;

	Window::init(hInst, parent);
	create(IDD_QUICK_OPEN_DLG, FALSE, TRUE);
}

void QuickOpenDlg::setCurrentPath(const std::filesystem::path& currentPath)
{
	if (_currentPath.compare(currentPath)) {
		_currentPath = currentPath;
		_pattern = L"*";

		::SendMessage(_hWndResult, LB_SETCOUNT, 0, 0);
		::SendMessage(_hWndResult, LB_SETCURSEL, 0, 0);

		if (DialogState::BUILDING_INDEX != _dialogState) {
			_dialogState = DialogState::BUILDING_INDEX;
			std::thread t([](QuickOpenDlg *arg) { arg->rebuildIndex(); }, this);
			t.detach();
		}
	}
}

void QuickOpenDlg::show()
{
	std::wstring selectedText = NppInterface::getSelectedText();
	if (!selectedText.empty()) {
		::Edit_SetText(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), selectedText.c_str());
	}
	populateResultList();

	setDefaultPosition();
	display(true);
	::PostMessage(_hSelf, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(_hSelf, IDC_EDIT_SEARCH), TRUE);
}

void QuickOpenDlg::close()
{
	::KillTimer(_hSelf, 1);
	display(false);
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
		text = _results[itemID].second->parent_path().wstring();
		::DrawText(drawItem->hDC, text.c_str(), static_cast<INT>(text.length()), &drawPosition, DT_SINGLELINE);
	}
	return TRUE;
}

BOOL CALLBACK QuickOpenDlg::run_dlgProc(HWND /* hWnd */, UINT Message, WPARAM wParam, LPARAM lParam)
{
	BOOL ret = FALSE;
	switch (Message) {
	case WM_BUILD_INDEX_COMPLETE:
		populateResultList();
		break;
	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_LIST_RESULTS) {
			ret = onDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
		}
		break;
	case WM_TIMER:
		switch (wParam) {
		case UPDATE_TIMER:
			populateResultList();
			::KillTimer(_hSelf, UPDATE_TIMER);
			ret = TRUE;
			break;
		default:
			break;
		}
		break;
	case WM_COMMAND : 
		switch (LOWORD(wParam)) {
		case IDC_EDIT_SEARCH:
			if (EN_CHANGE == HIWORD(wParam)) {
				::KillTimer(_hSelf, UPDATE_TIMER);
				::SetTimer(_hSelf, UPDATE_TIMER, 100, NULL);
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
		calcMetrics();
		const int height = _itemTextHeight * 2 + _itemTextExternalLeading;
		::SendMessage(_hWndResult, LB_SETITEMHEIGHT, 0, height);
		::SetWindowLongPtr(::GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		_defaultEditProc = (WNDPROC)::SetWindowLongPtr(::GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndDefaultEditProc));
		break;
	}
	case WM_DESTROY:
		if (_defaultEditProc) {
			::SetWindowLongPtr(GetDlgItem(_hSelf, IDC_EDIT_SEARCH), GWLP_WNDPROC, (LONG_PTR)_defaultEditProc);
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

void QuickOpenDlg::rebuildIndex()
{
	_fileIndex.clear();
	rebuildFileIndex(&_fileIndex, _currentPath);
	_dialogState = DialogState::READY;
	::PostMessage(_hSelf, WM_BUILD_INDEX_COMPLETE, 0, 0);
}

void QuickOpenDlg::populateResultList()
{
	if (DialogState::BUILDING_INDEX == _dialogState) {
		return;
	}

	const HWND hEdit = ::GetDlgItem(_hSelf, IDC_EDIT_SEARCH);
	const int bufferLength = ::Edit_GetTextLength(hEdit) + 1;	// text length + null terminated
	std::wstring pattern;
	if (1 < bufferLength) {
		pattern.resize(bufferLength);
		::Edit_GetText(hEdit, &pattern[0], bufferLength);
		removeWhitespaces(pattern);
	}

	if (_pattern != pattern) {
		if (0 < pattern.length()) {
			_results = scoredMatches(_fileIndex, pattern);
		}
		else {
			_results.clear();
			for (const auto& entry : _fileIndex) {
				_results.emplace_back(0, &entry);
			}
		}
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

