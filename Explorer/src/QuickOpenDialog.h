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

#ifndef QUICKOPENDIALOG_H
#define QUICKOPENDIALOG_H

#include <string>
#include <vector>
#include <filesystem>

#include "Explorer.h"
#include "StaticDialog.h"
#include "ExplorerResource.h"

class HilightListbox;

class QuickOpenDlg : public StaticDialog
{
public:
	QuickOpenDlg() : 
		StaticDialog(),
		_defaultEditProc(nullptr),
		_hWndResult(nullptr),
		_pExProp(nullptr),
		_dialogState(DialogState::READY)
	{};
	
	void init(HINSTANCE hInst, HWND parent, ExProp* prop);
	void show();
	void setCurrentPath(const std::filesystem::path& currentPath);
	void close();


protected :
	BOOL OnDrawItem(LPDRAWITEMSTRUCT drawItem);


	BOOL CALLBACK run_dlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
	
	static LRESULT APIENTRY wndDefaultEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT APIENTRY runEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:

	WNDPROC														_defaultEditProc;
	HWND														_hWndResult;
	ExProp*														_pExProp;
	std::filesystem::path										_currentPath;
	std::wstring												_pattern;
	std::vector<std::filesystem::path>							_fileIndex;
	std::vector<std::pair<int, const std::filesystem::path*>>	_results;

	enum class DialogState {
		BUILDING_INDEX = 0,
		READY
	};
	DialogState													_dialogState;

	void setDefaultPosition();
	void rebuildIndex();
	void populateResultList();
};


#endif // QUICK_OPEN_DIALOG