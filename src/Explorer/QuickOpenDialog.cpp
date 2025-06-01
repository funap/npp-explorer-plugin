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

#include <algorithm>
#include <cwctype>
#include <execution>
#include <list>
#include <string_view>
#include <memory>
#include <optional>
#include <condition_variable>

#include <shlwapi.h>
#include <windowsx.h>

#include "ExplorerResource.h"
#include "FuzzyMatcher.h"
#include "NppInterface.h"
#include "ThemeRenderer.h"

namespace {
    constexpr UINT WM_UPDATE_RESULT_LIST = WM_USER + 1;
    constexpr UINT_PTR SCAN_QUERY    = 1;
    constexpr UINT_PTR UPDATE_PROGRESSBAR   = 2;
    constexpr UINT_PTR EDIT_SUBCLASS_ID = 1;
    constexpr UINT_PTR LISTVIEW_SUBCLASS_ID = 2;

    UINT getDpiForWindow(HWND hWnd) {
        UINT dpi = 96;
        HMODULE user32dll = ::LoadLibraryW(L"User32.dll");
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
            if ((std::iswcntrl(*it) != 0) || (std::iswblank(*it) != 0)) {
                it = str.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    bool IsFile(const std::wstring& path) {
        DWORD attributes = GetFileAttributesW(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U);
    }
}

class QuickOpenEntry {
public:
    QuickOpenEntry() = delete;
    explicit QuickOpenEntry(const std::wstring& path)
        : _relativePath(path.substr(s_rootPath.size()))
        , _score(0)
        , _matchType(MATCH_TYPE::INIT)
    {
    }

    ~QuickOpenEntry()
    {
    }

    std::wstring_view FileName() const
    {
        std::wstring_view result(_relativePath);
        size_t lastSlashPos = result.find_last_of(L"/\\");
        if (lastSlashPos != std::wstring_view::npos) {
            return result.substr(lastSlashPos + 1);
        }
        return _relativePath;
    }

    const std::wstring& RelativePath() const
    {
        return _relativePath;
    }

    void Rename(const std::wstring& oldName, const std::wstring& newName)
    {
        std::wstring path = FullPath();
        std::string::size_type pos = path.find(oldName);
        if (pos == 0) {
            path.replace(pos, oldName.length(), newName);
            _relativePath = path.substr(s_rootPath.size());
        }
    }

    std::wstring FullPath() const
    {
        std::wstring fullPath = s_rootPath;
        fullPath.append(_relativePath);
        return fullPath;
    }

    enum class MATCH_TYPE {
        INIT = 0,
        FILE,
        PATH,
        NO_MATCH,
    };

    void SetScore(MATCH_TYPE type, int score, std::vector<size_t>&& matches)
    {
        _matchType = type;
        _score = score;
        _matches = std::move(matches);
    }

    void ResetScore()
    {
        _matchType = MATCH_TYPE::INIT;
        _score = 0;
    }

    MATCH_TYPE MatchType() const
    {
        return _matchType;
    }

    int Score() const {
        return _score;
    }

    std::vector<size_t> Matches() const
    {
        return _matches;
    }

    static void SetRootPath(const std::wstring& rootPath)
    {
        s_rootPath = rootPath;
    }

private:
    static std::wstring s_rootPath;
    std::wstring        _relativePath;
    int                 _score;
    std::vector<size_t> _matches;
    MATCH_TYPE          _matchType;
};
std::wstring QuickOpenEntry::s_rootPath;


class QuickOpenModel {
public:
    QuickOpenModel()
        : _condition{}
    {
    }

    ~QuickOpenModel()
    {
        StopSearchThread();
    }

    void RootPath(const std::wstring& rootPath)
    {
        StopSearchThread();
        ClearEntries();
        {
            std::unique_lock<std::mutex> lock(_conditionMtx);
            _condition.query.reset();
            _condition.revision++;
        }
        QuickOpenEntry::SetRootPath(rootPath);
    }

    void AddEntry(const std::wstring& path)
    {
        {
            std::lock_guard<std::mutex> lock(_entriesMtx);
            _entries.emplace_back(std::make_shared<QuickOpenEntry>(path));
        }

        {
            std::unique_lock<std::mutex> lock(_conditionMtx);
            _condition.revision++;
        }
        _searchCond.notify_one();
    }

    void RemoveEntry(const std::wstring& path)
    {
        {
            std::lock_guard<std::mutex> lock(_entriesMtx);
            auto it = std::find_if(_entries.begin(), _entries.end(), [&path](const auto entry) {
                return entry->FullPath() == path;
            });
            // file was removed
            if (it != _entries.end()) {
                (*it)->ResetScore();
                _entries.erase(it);
            }
            // directory was removed
            else {
                std::wstring dirName = path + L"\\";
                for (it = _entries.begin(); it != _entries.end(); ) {
                    if ((*it)->FullPath().starts_with(dirName)) {
                        (*it)->ResetScore();
                        it = _entries.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(_conditionMtx);
            _condition.revision++;
        }
        _searchCond.notify_one();
    }

    void RenameEntry(const std::wstring& oldPath, const std::wstring& newPath)
    {
        {
            std::lock_guard<std::mutex> lock(_entriesMtx);
            auto it = std::find_if(_entries.begin(), _entries.end(), [&oldPath](const auto entry) {
                return entry->FullPath() == oldPath;
            });
            // file name was changed
            if (it != _entries.end()) {
                (*it)->Rename(oldPath, newPath);
                (*it)->ResetScore();
            }
            // directory name was changed
            else {
                std::wstring oldDirName = oldPath + L"\\";
                std::wstring newDirName = newPath + L"\\";
                for (it = _entries.begin(); it != _entries.end(); ) {
                    if ((*it)->FullPath().starts_with(oldDirName)) {
                        (*it)->Rename(oldDirName, newDirName);
                        (*it)->ResetScore();
                    }
                    else {
                        ++it;
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(_conditionMtx);
            _condition.revision++;
        }
        _searchCond.notify_one();
    }

    void Search(const std::wstring& query)
    {
        {
            std::lock_guard<std::mutex> lock(_conditionMtx);
            if (_condition.query.has_value() && (query == _condition.query.value())) {
                return;
            }
            _condition.query = query;
            _condition.revision++;
        }
        _searchCond.notify_one();
    }

    std::vector<std::shared_ptr<QuickOpenEntry>> GetResults()
    {
        std::lock_guard<std::mutex> lock(_weakResultsMtx);
        std::vector<std::shared_ptr<QuickOpenEntry>> results;
        for (auto& result : _weakResults) {
            auto entry = result.lock();
            if (entry) {
                results.push_back(entry);
            }
        }
        return results;
    }

    using SearchCallback = std::function<void()>;
    void StartSearchThread(SearchCallback callback)
    {
        StopSearchThread();
        {
            std::lock_guard<std::mutex> lock(_conditionMtx);
            _condition.revision = 0;
            _condition.stop = false;
        }
        _searchThread = std::thread(&QuickOpenModel::Run, this, callback);
    }

    void StopSearchThread()
    {
        {
            std::lock_guard<std::mutex> lock(_conditionMtx);
            _condition.stop = true;
            _condition.revision++;
        }
        _searchCond.notify_one();

        if (_searchThread.joinable()) {
            _searchThread.join();
        }
    }
private:
    void ClearEntries()
    {
        {
            std::lock_guard<std::mutex> lock(_weakResultsMtx);
            _weakResults.clear();
        }
        {
            std::lock_guard<std::mutex> lock(_entriesMtx);
            _entries.clear();
        }
    }

    std::vector<std::shared_ptr<QuickOpenEntry>> GetUnscoredEntries()
    {
        std::lock_guard<std::mutex> lock(_entriesMtx);
        std::vector<std::shared_ptr<QuickOpenEntry>> entries;
        entries.reserve(_entries.size());
        for (auto& entry : _entries) {
            if (entry->MatchType() == QuickOpenEntry::MATCH_TYPE::INIT) {
                entries.push_back(entry);
            }
        }
        return entries;
    }

    std::vector<std::shared_ptr<QuickOpenEntry>> GetScoredEntries()
    {
        std::lock_guard<std::mutex> lock(_entriesMtx);
        std::vector<std::shared_ptr<QuickOpenEntry>> entries;
        entries.reserve(_entries.size());
        for (auto& entry : _entries) {
            if (entry->Score() > 0) {
                entries.push_back(entry);
            }
        }
        return entries;
    }

    void Run(SearchCallback callback)
    {
        int revision = 0;
        std::wstring query = _condition.query.value_or(L"");
        std::vector<std::shared_ptr<QuickOpenEntry>> results;
        try {
            while (true) {
                {
                    std::unique_lock<std::mutex> conditionLock(_conditionMtx);
                    _searchCond.wait(conditionLock, [&] { return revision != _condition.revision; });
                    revision = _condition.revision;
                    if (_condition.stop) {
                        break;
                    }

                    // If the query is the same, do not recalculate the scores.
                    if (_condition.query.value_or(L"") == query) {
                        ;
                    }
                    // When characters are added to the query, inherit the narrowed-down results.
                    else if (_condition.query.value_or(L"").starts_with(query)) {
                        std::lock_guard<std::mutex> lock(_entriesMtx);
                        for (auto& entry : results) {
                            entry->ResetScore();
                        }
                    }
                    // Otherwise, recalculate all scores.
                    else {
                        std::lock_guard<std::mutex> lock(_entriesMtx);
                        for (auto& entry : _entries) {
                            entry->ResetScore();
                        }
                        results.clear();
                    }
                    query = _condition.query.value_or(L"");
                }

                auto entries = GetUnscoredEntries();
                if (query.empty()) {
                    std::lock_guard<std::mutex> lock(_entriesMtx);
                    for (auto& entry : entries) {
                        entry->SetScore(QuickOpenEntry::MATCH_TYPE::NO_MATCH, 1, {});
                    }
                }
                else {
                    FuzzyMatcher matcher(query);
                    for (auto& entry : entries) {
                        {
                            std::lock_guard<std::mutex> lock(_conditionMtx);
                            if (_condition.stop) {
                                break;
                            }
                        }

                        std::vector<size_t> matches;
                        int score = matcher.ScoreMatch(entry->FileName(), &matches);
                        if (0 < score) {
                            constexpr int FILE_MATCH_BONUS = 1 << 30;
                            entry->SetScore(QuickOpenEntry::MATCH_TYPE::FILE, score + FILE_MATCH_BONUS, std::move(matches));
                            continue;
                        }

                        score = matcher.ScoreMatch(entry->RelativePath(), &matches);
                        if (0 < score) {
                            entry->SetScore(QuickOpenEntry::MATCH_TYPE::PATH, score, std::move(matches));
                            continue;
                        }

                        entry->SetScore(QuickOpenEntry::MATCH_TYPE::NO_MATCH, 0, std::move(matches));
                   }
                }

                results = GetScoredEntries();
                {
                    std::lock_guard<std::mutex> lock(_conditionMtx);
                    if (_condition.stop) {
                        break;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(_entriesMtx);
                    std::sort(std::execution::par, results.begin(), results.end(), [](const auto& lhs, const auto& rhs) {
                        if (lhs->Score() == rhs->Score()) {
                            return ::StrCmpLogicalW(lhs->RelativePath().c_str(), rhs->RelativePath().c_str()) < 0;
                        }
                        return lhs->Score() > rhs->Score();
                    });
                }
                {
                    std::lock_guard<std::mutex> lock(_weakResultsMtx);
                    _weakResults.clear();
                    for (auto& result : results) {
                        _weakResults.push_back(result);
                    }
                }
                callback();
            }
        }
        catch (...) {
            // do nothing
        }
    }
    struct Condition {
        int                         revision{0};
        bool                        stop{false};
        std::optional<std::wstring> query;
    };
    std::list<std::shared_ptr<QuickOpenEntry>>  _entries;
    std::mutex                                  _entriesMtx;
    std::vector<std::weak_ptr<QuickOpenEntry>>  _weakResults;
    std::mutex                                  _weakResultsMtx;
    std::thread                                 _searchThread;
    std::mutex                                  _conditionMtx;
    Condition                                   _condition;
    std::condition_variable                     _searchCond;
};

QuickOpenDlg::QuickOpenDlg()
    : StaticDialog()
    , _model(std::make_unique<QuickOpenModel>())
    , _layout{}
    , _hWndResult(nullptr)
    , _hWndEdit(nullptr)
    , _pExProp(nullptr)
    , _progressBarRect()
    , _shouldAutoClose(true)
    , _needsRefresh(true)
{
}

QuickOpenDlg::~QuickOpenDlg()
{
}

void QuickOpenDlg::init(HINSTANCE hInst, HWND parent, ExProp* prop)
{
    _pExProp = prop;

    Window::init(hInst, parent);
    create(IDD_QUICK_OPEN_DLG, FALSE);
    ThemeRenderer::Instance().Register(_hSelf);

    _filesystemWatcher.Created([this](const std::wstring& path) {
        if (IsFile(path)) {
            _model->AddEntry(path);
        }
        else {
            _needsRefresh = true;
        }
    });
    _filesystemWatcher.Deleted([this](const std::wstring& path) {
        _model->RemoveEntry(path);
     });
    _filesystemWatcher.Renamed([this](const std::wstring& oldPath, const std::wstring& newPath) {
        _model->RenameEntry(oldPath, newPath);
    });
}



void QuickOpenDlg::setRootPath(const std::filesystem::path& rootPath)
{
    if (_needsRefresh || (_directoryReader.GetRootPath() != rootPath)) {
        _needsRefresh = false;
        _directoryReader.Cancel();

        _model->RootPath(rootPath);
        updateResultList();

        _filesystemWatcher.Reset(rootPath);
        _directoryReader.ReadDir(rootPath,
            [this](const std::filesystem::path& path) {
                _model->AddEntry(path);
            },
            [this]() {
                PostMessage(_hSelf, WM_UPDATE_RESULT_LIST, 0, 0);
            }
        );
    }
}

void QuickOpenDlg::show()
{
    std::wstring selectedText = NppInterface::getSelectedText();
    if (!selectedText.empty()) {
        ::Edit_SetText(_hWndEdit, selectedText.c_str());
    }
    _model->StartSearchThread([this]() {
        PostMessage(_hSelf, WM_UPDATE_RESULT_LIST, 0, 0);
    });

    updateQuery();
    updateResultList();

    setDefaultPosition();
    display(true);
    ::PostMessage(_hSelf, WM_NEXTDLGCTL, (WPARAM)_hWndEdit, TRUE);

    if (_directoryReader.IsReading()) {
        ::SetTimer(_hSelf, SCAN_QUERY,  100, nullptr);
        ::SetTimer(_hSelf, UPDATE_PROGRESSBAR,  33, nullptr);
    }
}

void QuickOpenDlg::close()
{
    _model->StopSearchThread();
    ::KillTimer(_hSelf, SCAN_QUERY);
    ::KillTimer(_hSelf, UPDATE_PROGRESSBAR);
    display(false);
}

void QuickOpenDlg::SetFont(HFONT font)
{
    ::SendMessage(_hWndResult, WM_SETFONT, (WPARAM)font, TRUE);

    UINT dpi = getDpiForWindow(_hSelf);
    if (0 == dpi) {
        dpi = USER_DEFAULT_SCREEN_DPI;
    }

    TEXTMETRIC textMetric;
    HDC hdc = ::GetDC(_hWndResult);
    HGDIOBJ hOldFont = ::SelectObject(hdc, font);
    ::GetTextMetrics(hdc, &textMetric);
    ::SelectObject(hdc, hOldFont);
    ::ReleaseDC(_hWndResult, hdc);

    _layout.itemMargin      = ::MulDiv(3, dpi, USER_DEFAULT_SCREEN_DPI);
    _layout.itemMarginLeft  = ::MulDiv(7, dpi, USER_DEFAULT_SCREEN_DPI);
    _layout.itemTextHeight  = textMetric.tmHeight;

    // Deliberately reconfigure for WM_MEASUREITEM.
    ListView_SetView(_hWndResult, LV_VIEW_TILE);
    ListView_SetView(_hWndResult, LVS_REPORT);
}

void QuickOpenDlg::setDefaultPosition()
{
    RECT selfRect;
    ::GetWindowRect(_hSelf, &selfRect);

    RECT parentRect;
    ::GetClientRect(_hParent, &parentRect);
    POINT center{
        .x = parentRect.left + (parentRect.right - parentRect.left) / 2,
        .y = 0,
    };
    ::ClientToScreen(_hParent, &center);

    int x = center.x - (selfRect.right - selfRect.left) / 2;
    int y = center.y;

    ::SetWindowPos(_hSelf, HWND_TOP, x, y, selfRect.right - selfRect.left, selfRect.bottom - selfRect.top, SWP_SHOWWINDOW);
}

BOOL QuickOpenDlg::onDrawItem(LPDRAWITEMSTRUCT drawItem)
{
    UINT& itemID = drawItem->itemID;

    if (-1 == itemID) {
        return TRUE;
    }

    if (ODA_FOCUS == (drawItem->itemAction & ODA_FOCUS)) {
        return TRUE;
    }

    const HDC& hdc = drawItem->hDC;
    COLORREF backgroundMatchColor   = RGB(254, 230, 177);
    COLORREF matchColor             = RGB(0, 0, 0);

    COLORREF backgroundColor        = ThemeRenderer::Instance().GetColors().secondary_bg;
    COLORREF textColor1             = ThemeRenderer::Instance().GetColors().secondary;
    COLORREF textColor2             = RGB(128, 128, 128);

    if (ODS_SELECTED == ((drawItem->itemState) & (ODS_SELECTED))) {
        backgroundColor             = ThemeRenderer::Instance().GetColors().primary_bg;
        textColor1                  = ThemeRenderer::Instance().GetColors().primary;
        textColor2                  = RGB(128, 128, 128);
    }

    // Fill background
    const HBRUSH hBrush = ::CreateSolidBrush(backgroundColor);
    ::FillRect(hdc, &drawItem->rcItem, hBrush);
    ::DeleteObject(hBrush);

    RECT drawPosition = drawItem->rcItem;
    drawPosition.top += _layout.itemMargin;
    drawPosition.left = drawItem->rcItem.left + _layout.itemMarginLeft;
    ::SetBkMode(hdc, OPAQUE);

    RECT calcRect = {};
    auto type           = _results[itemID]->MatchType();
    const auto filename = _results[itemID]->FileName();
    const auto path     = _results[itemID]->RelativePath();
    auto matches        = _results[itemID]->Matches();
    auto itr            = matches.cbegin();
    auto last           = matches.cend();

    if (type == QuickOpenEntry::MATCH_TYPE::FILE) {
        for (size_t i = 0; i < filename.length(); ++i) {
            if ((itr != last) && (i == *itr)) {
                ::SetTextColor(hdc, matchColor);
                ::SetBkColor(hdc, backgroundMatchColor);
                ++itr;
            }
            else {
                ::SetTextColor(hdc, textColor1);
                ::SetBkColor(hdc, backgroundColor);
            }
            ::DrawText(hdc, &filename[i], 1, &drawPosition, DT_SINGLELINE);
            ::DrawText(hdc, &filename[i], 1, &calcRect, DT_SINGLELINE | DT_CALCRECT);
            drawPosition.left += calcRect.right;
        }
    }
    else {
        ::SetTextColor(hdc, textColor1);
        ::SetBkColor(hdc, backgroundColor);
        ::DrawText(hdc, filename.data(), static_cast<INT>(filename.length()), &drawPosition, DT_SINGLELINE);
        ::DrawText(hdc, filename.data(), static_cast<INT>(filename.length()), &calcRect, DT_SINGLELINE | DT_CALCRECT);
    }

    drawPosition.top += calcRect.bottom;
    drawPosition.left = drawItem->rcItem.left + _layout.itemMarginLeft;

    if (type == QuickOpenEntry::MATCH_TYPE::PATH) {
        for (size_t i = 0; i < path.length(); ++i) {
            if ((itr != last) && (i == *itr)) {
                ::SetTextColor(hdc, matchColor);
                ::SetBkColor(hdc, backgroundMatchColor);
                ++itr;
            }
            else {
                ::SetTextColor(hdc, textColor2);
                ::SetBkColor(hdc, backgroundColor);
            }
            ::DrawText(hdc, &path[i], 1, &drawPosition, DT_SINGLELINE);
            ::DrawText(hdc, &path[i], 1, &calcRect, DT_SINGLELINE | DT_CALCRECT);
            drawPosition.left += calcRect.right;
        }
    }
    else {
        ::SetTextColor(hdc, textColor2);
        ::SetBkColor(hdc, backgroundColor);
        ::DrawText(hdc, path.data(), static_cast<INT>(path.length()), &drawPosition, DT_SINGLELINE);
    }
    return TRUE;
}

INT_PTR CALLBACK QuickOpenDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    BOOL ret = FALSE;
    switch (Message) {
    case WM_UPDATE_RESULT_LIST:
        updateResultList();
        ret = TRUE;
        break;
    case WM_MEASUREITEM:
        if ((UINT)wParam == IDC_LIST_RESULTS) {
            LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
            lpmis->itemHeight = (_layout.itemTextHeight + _layout.itemMargin) * 2;
            return TRUE;
        }
        break;
    case WM_NOTIFY:
        if ((UINT)wParam == IDC_LIST_RESULTS) {
            if (((LPNMHDR)lParam)->code == NM_DBLCLK) {
                openSelectedItem();
                close();
                return TRUE;
            }
        }
        break;
    case WM_DRAWITEM:
        if ((UINT)wParam == IDC_LIST_RESULTS) {
            return onDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
        }
        break;
    case WM_TIMER:
        switch (wParam) {
        case SCAN_QUERY:
            ::KillTimer(_hSelf, SCAN_QUERY);
            updateQuery();
            ret = TRUE;
            break;
        case UPDATE_PROGRESSBAR:
            if (!_directoryReader.IsReading()) {
                ::KillTimer(_hSelf, UPDATE_PROGRESSBAR);
            }
            ::InvalidateRect(_hSelf, &_progressBarRect, TRUE);
            ::UpdateWindow(_hSelf);
            ret = TRUE;
            break;
        default:
            break;
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = ::BeginPaint(_hSelf, &ps);

        // draw progress bar
        if (_directoryReader.IsReading()) {
            static LONG s_progressPos = 0;
            constexpr LONG PROGRESSBAR_WIDTH = 16U;
            RECT barRect {
                .left   = std::max(_progressBarRect.left, s_progressPos - PROGRESSBAR_WIDTH),
                .top    = _progressBarRect.top,
                .right  = std::min(s_progressPos + PROGRESSBAR_WIDTH, _progressBarRect.right),
                .bottom = _progressBarRect.bottom,
            };
            s_progressPos += 2;
            s_progressPos %= _progressBarRect.right;

            const COLORREF progressBarColor = RGB(14, 112, 192);
            const HBRUSH hPbBrush = ::CreateSolidBrush(progressBarColor);
            ::FillRect(hDC, &barRect, hPbBrush);
            ::DeleteObject(hPbBrush);
        }
        EndPaint(_hSelf, &ps);
        ret = TRUE;
        break;
    }
    case WM_COMMAND :
        switch (LOWORD(wParam)) {
        case IDC_EDIT_SEARCH:
            if (EN_CHANGE == HIWORD(wParam)) {
                ::KillTimer(_hSelf, SCAN_QUERY);
                ::SetTimer(_hSelf, SCAN_QUERY, 100, nullptr);
                ret = TRUE;
            }
            break;
        case IDCANCEL:
            close();
            ret = TRUE;
            break;
        case IDOK:
        {
            openSelectedItem();
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
        _hWndEdit = ::GetDlgItem(_hSelf, IDC_EDIT_SEARCH);
        _hWndResult = ::GetDlgItem(_hSelf, IDC_LIST_RESULTS);

        SetFont(_pExProp->defaultFont);

        ListView_SetExtendedListViewStyle(_hWndResult, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
        LVCOLUMN column{};
        ListView_InsertColumn(_hWndResult, 0, &column);

        ::SetWindowSubclass(_hWndEdit, DefaultSubclassProc, EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        ::SetWindowSubclass(_hWndResult, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(this));
        GetClientRect(_hSelf, &_progressBarRect);
        _progressBarRect.top = 2;
        _progressBarRect.bottom = _progressBarRect.top + 2;
        break;
    }
    case WM_DESTROY:
        _directoryReader.Cancel();
        _filesystemWatcher.Stop();
        break;
    case WM_ACTIVATE:
        if (_shouldAutoClose && (WA_INACTIVE == LOWORD(wParam))) {
            close();
        }
        break;
    default:
        break;
    }
    return ret;
}

void QuickOpenDlg::updateQuery()
{
    const int bufferLength = ::Edit_GetTextLength(_hWndEdit) + 1;   // text length + null terminated string
    std::wstring query;
    if (1 < bufferLength) {
        query.resize(bufferLength);
        ::Edit_GetText(_hWndEdit, query.data(), bufferLength);
        removeWhitespaces(query);
    }

    _model->Search(query);
}

void QuickOpenDlg::updateResultList()
{
    _results = _model->GetResults();
    ::SendMessage(_hWndResult, WM_SETREDRAW, FALSE, 0);
    ListView_SetItemCountEx(_hWndResult, _results.size(), LVSICF_NOSCROLL);
    ListView_SetColumnWidth(_hWndResult, 0, LVSCW_AUTOSIZE_USEHEADER);
    if (0 == ListView_GetSelectedCount(_hWndResult)) {
        ListView_SetItemState(_hWndResult, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    }
    ::SendMessage(_hWndResult, WM_SETREDRAW, TRUE, 0);
}

void QuickOpenDlg::openSelectedItem() const
{
    const int index = ListView_GetSelectionMark(_hWndResult);
    if (0 <= index) {
        if (static_cast<SIZE_T>(index) < _results.size()) {
            NppInterface::doOpen(_results[index]->FullPath());
        }
    }
}

LRESULT CALLBACK QuickOpenDlg::DefaultSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uIdSubclass) {
    case EDIT_SUBCLASS_ID:
        return reinterpret_cast<QuickOpenDlg*>(dwRefData)->EditProc(hWnd, uMsg, wParam, lParam);
    case LISTVIEW_SUBCLASS_ID:
        return reinterpret_cast<QuickOpenDlg*>(dwRefData)->ListViewProc(hWnd, uMsg, wParam, lParam);
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
LRESULT APIENTRY QuickOpenDlg::EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_RETURN:
            openSelectedItem();
            return TRUE;
        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:  // Page Up
        case VK_NEXT:   // Page Down
            // transfer to listview
            return ::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, WM_KEYDOWN, wParam, lParam);
        case VK_RIGHT:{
            int editSel     = LOWORD(Edit_GetSel(_hWndEdit));
            int editLength  = Edit_GetTextLength(_hWndEdit);
            if (editSel == editLength) {
                _shouldAutoClose = false;
                openSelectedItem();
                _shouldAutoClose = true;
                ::SetFocus(_hWndEdit);
                return TRUE;
            }
            break;
        }
        case VK_ESCAPE:
            close();
            return TRUE;
        default:
            break;
        }
        break;
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, EDIT_SUBCLASS_ID);
        break;

    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT QuickOpenDlg::ListViewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_SETFOCUS:
        SetFocus(_hWndEdit);
        break;
    case WM_NCDESTROY:
        ::RemoveWindowSubclass(hWnd, DefaultSubclassProc, LISTVIEW_SUBCLASS_ID);
        break;
    default:
        break;
    }
    return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
