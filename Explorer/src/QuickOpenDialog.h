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
#include "DirectoryIndex.h"


class QuickOpenDlg : public StaticDialog, public DirectoryIndexListener
{
public:
	QuickOpenDlg();
	~QuickOpenDlg();

	void init(HINSTANCE hInst, HWND parent, ExProp* prop);
	void show();
	void setCurrentPath(const std::filesystem::path& currentPath);
	void close();

	void onIndexBuildCompleted() const override;
	void onIndexBuildCanceled() const override;
protected :
	VOID calcMetrics();
	BOOL onDrawItem(LPDRAWITEMSTRUCT drawItem);
	INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
	static LRESULT APIENTRY wndDefaultEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT APIENTRY runEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	INT _itemMarginLeft;
	INT _itemTextHeight;
	INT _itemTextExternalLeading;
	WNDPROC														_defaultEditProc;
	HWND														_hWndResult;
	HWND														_hWndEdit;
	ExProp*														_pExProp;
	DirectoryIndex												_direcotryIndex;
	std::wstring												_pattern;
	std::vector<std::pair<int, const std::filesystem::path*>>	_results;
	RECT														_progressBarRect;

	void setDefaultPosition();
	void populateResultList();
};


#endif // QUICK_OPEN_DIALOG