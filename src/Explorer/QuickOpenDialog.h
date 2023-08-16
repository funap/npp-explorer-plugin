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

#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "Explorer.h"
#include "../NppPlugin/DockingFeature/StaticDialog.h"
#include "ExplorerResource.h"
#include "DirectoryReader.h"
#include "FileSystemWatcher.h"

class QuickOpenModel;
class QuickOpenEntry;

class QuickOpenDlg : public StaticDialog
{
public:
    QuickOpenDlg();
    ~QuickOpenDlg();

    void init(HINSTANCE hInst, HWND parent, ExProp* prop);
    void show();
    void setRootPath(const std::filesystem::path& currentPath, BOOL forceRefresh = FALSE);
    void close();
    void SetFont(HFONT font);
private:
    BOOL onDrawItem(LPDRAWITEMSTRUCT drawItem);
    INT_PTR CALLBACK run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam) override;
    static LRESULT APIENTRY wndDefaultEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT APIENTRY runEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void openSelectedItem() const;
    void setDefaultPosition();
    void updateQuery();
    void updateResultList();

    struct Layout {
        int             itemMarginLeft;
        int             itemTextHeight;
        int             itemMargin;
    };

    std::unique_ptr<QuickOpenModel>                 _model;
    DirectoryReader                                 _directoryReader;
    FileSystemWatcher                               _filesystemWatcher;
    std::wstring                                    _query;
    std::vector<std::shared_ptr<QuickOpenEntry>>    _results;

    Layout              _layout;
    WNDPROC             _defaultEditProc;
    HWND                _hWndResult;
    HWND                _hWndEdit;
    ExProp*             _pExProp;
    RECT                _progressBarRect;
    bool                _shouldAutoClose;
    bool                _needsRefresh;

};
