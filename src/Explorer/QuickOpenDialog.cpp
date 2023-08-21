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
#include <execution>

#include <shlwapi.h>
#include <windowsx.h>

#include "FuzzyMatcher.h"
#include "NppInterface.h"

namespace {
    constexpr UINT WM_UPDATE_RUSULT_LIST = WM_USER + 1;
    constexpr UINT_PTR SCAN_QUERY    = 1;
    constexpr UINT_PTR UPDATE_PROGRESSBAR   = 2;

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
    }

    bool IsFile(const std::wstring& path) {
        DWORD attributes = GetFileAttributesW(path.c_str());
        return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    }
}

class QuickOpenEntry {
public:
    QuickOpenEntry() = delete;
    explicit QuickOpenEntry(const std::wstring& path)
        : _relativePath(path.substr(s_rootPath.size()))
        , _score(0)
        , _matches()
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

    void InitScore()
    {
        _matchType = MATCH_TYPE::INIT;
        _score = 0;
        _matches.clear();
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
                (*it)->InitScore();
                _entries.erase(it);
            }
            // directory was removed
            else {
                std::wstring dirName = path + L"\\";
                for (auto it = _entries.begin(); it != _entries.end(); ) {
                    if ((*it)->FullPath().starts_with(dirName)) {
                        (*it)->InitScore();
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
                (*it)->InitScore();
            }
            // directory name was changed
            else {
                std::wstring oldDirName = oldPath + L"\\";
                std::wstring newDirName = newPath + L"\\";
                for (auto it = _entries.begin(); it != _entries.end(); ) {
                    if ((*it)->FullPath().starts_with(oldDirName)) {
                        (*it)->Rename(oldDirName, newDirName);
                        (*it)->InitScore();
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

    using SearchCallback = std::function<void()>;
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

    std::vector<std::shared_ptr<QuickOpenEntry>> GetInitEntries()
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
        std::wstring query;
        std::vector<std::shared_ptr<QuickOpenEntry>> results;
        try {
            while (true) {
                {
                    std::unique_lock<std::mutex> lock(_conditionMtx);
                    _searchCond.wait(lock, [&] { return revision != _condition.revision; });
                    revision = _condition.revision;
                    if (_condition.stop) {
                        break;
                    }
                    if (query.empty()) {
                        std::lock_guard<std::mutex> lock(_entriesMtx);
                        for (auto& entry : _entries) {
                            entry->InitScore();
                        }
                    }
                    else if (!_condition.query.value_or(std::wstring()).starts_with(query)) {
                        std::lock_guard<std::mutex> lock(_entriesMtx);
                        for (auto& entry : _entries) {
                            entry->InitScore();
                        }
                        results.clear();
                    }
                    else {
                        for (auto& entry : results) {
                            entry->InitScore();
                        }
                    }
                    query = _condition.query.value_or(std::wstring());
                }

                auto newEntries = GetInitEntries();
                if (query.empty()) {
                    std::lock_guard<std::mutex> lock(_entriesMtx);
                    for (auto& entry : newEntries) {
                        entry->SetScore(QuickOpenEntry::MATCH_TYPE::NO_MATCH, 1, {});
                    }
                }
                else {
                    FuzzyMatcher matcher(query);
                    for (auto& entry : newEntries) {
                        {
                            std::lock_guard<std::mutex> lock(_conditionMtx);
                            if (_condition.stop) {
                                break;
                            }
                        }

                        std::vector<size_t> matches;
                        int score = matcher.ScoreMatch(entry->FileName(), &matches);
                        if (0 < score) {
                            entry->SetScore(QuickOpenEntry::MATCH_TYPE::FILE, score | 1 << 30, std::move(matches));
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
        return;
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
    , _directoryReader()
    , _query()
    , _results()
    , _layout{}
    , _defaultEditProc(nullptr)
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



void QuickOpenDlg::setRootPath(const std::filesystem::path& rootPath, BOOL forceRefresh)
{
    if (_needsRefresh || _directoryReader.GetRootPath().compare(rootPath)) {
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
                PostMessage(_hSelf, WM_UPDATE_RUSULT_LIST, 0, 0);
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
        PostMessage(_hSelf, WM_UPDATE_RUSULT_LIST, 0, 0);
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
    const int height = (_layout.itemTextHeight + _layout.itemMargin) * 2;
    ::SendMessage(_hWndResult, LB_SETITEMHEIGHT, 0, height);
}

void QuickOpenDlg::setDefaultPosition()
{
    RECT rc;
    ::GetClientRect(_hParent, &rc);
    POINT center{
        .x = rc.left + (rc.right - rc.left) / 2,
        .y = 0,
    };
    ::ClientToScreen(_hParent, &center);

    int x = center.x - (_rc.right - _rc.left) / 2;
    int y = center.y;

    ::SetWindowPos(_hSelf, HWND_TOP, x, y, _rc.right - _rc.left, _rc.bottom - _rc.top, SWP_SHOWWINDOW);
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
    COLORREF backgroundColor        = RGB(255, 255, 255);
    COLORREF backgroundMatchColor   = RGB(252, 234, 128);
    COLORREF textColor1             = RGB(  0,   0,   0);
    COLORREF textColor2             = RGB(128, 128, 128);

    if ((drawItem->itemState) & (ODS_SELECTED)) {
        backgroundColor             = RGB(230, 231, 239);
        backgroundMatchColor        = RGB(252, 234, 128);
        textColor1                  = RGB(  0,   0,   0);
        textColor2                  = RGB(128, 128, 128);
    }

    // Fill background
    const HBRUSH hBrush = ::CreateSolidBrush(backgroundColor);
    ::FillRect(hdc, &drawItem->rcItem, hBrush);
    ::DeleteObject(hBrush);

    RECT drawPosition = drawItem->rcItem;
    drawPosition.top += _layout.itemMargin;
    drawPosition.left = drawItem->rcItem.left + _layout.itemMarginLeft;
    ::SetTextColor(hdc, textColor1);
    ::SetBkMode(hdc, OPAQUE);

    RECT calcRect = {};
    auto type           = _results[itemID]->MatchType();
    const auto score    = _results[itemID]->Score();
    const auto filename = _results[itemID]->FileName();
    const auto path     = _results[itemID]->RelativePath();
    auto matches        = _results[itemID]->Matches();
    auto itr            = matches.cbegin();
    auto last           = matches.cend();

    if (type == QuickOpenEntry::MATCH_TYPE::FILE) {
        size_t matchIndex = 0;
        for (size_t i = 0; i < filename.length(); ++i) {
            if ((itr != last) && (i == *itr)) {
                ::SetBkColor(hdc, backgroundMatchColor);
                ++itr;
            }
            else {
                ::SetBkColor(hdc, backgroundColor);
            }
            ::DrawText(hdc, &filename[i], 1, &drawPosition, DT_SINGLELINE);
            ::DrawText(hdc, &filename[i], 1, &calcRect, DT_SINGLELINE | DT_CALCRECT);
            drawPosition.left += calcRect.right;
        }
    }
    else {
        ::SetBkColor(hdc, backgroundColor);
        ::DrawText(hdc, filename.data(), static_cast<INT>(filename.length()), &drawPosition, DT_SINGLELINE);
        ::DrawText(hdc, filename.data(), static_cast<INT>(filename.length()), &calcRect, DT_SINGLELINE | DT_CALCRECT);
    }

    drawPosition.top += calcRect.bottom;
    drawPosition.left = drawItem->rcItem.left + _layout.itemMarginLeft;
    ::SetTextColor(hdc, textColor2);
    if (type == QuickOpenEntry::MATCH_TYPE::PATH) {
        size_t matchIndex = 0;
        for (size_t i = 0; i < path.length(); ++i) {
            if ((itr != last) && (i == *itr)) {
                ::SetBkColor(hdc, backgroundMatchColor);
                ++itr;
            }
            else {
                ::SetBkColor(hdc, backgroundColor);
            }
            ::DrawText(hdc, &path[i], 1, &drawPosition, DT_SINGLELINE);
            ::DrawText(hdc, &path[i], 1, &calcRect, DT_SINGLELINE | DT_CALCRECT);
            drawPosition.left += calcRect.right;
        }
    }
    else {
        ::SetBkColor(hdc, backgroundColor);
        ::DrawText(hdc, path.data(), static_cast<INT>(path.length()), &drawPosition, DT_SINGLELINE);
    }
    return TRUE;
}

INT_PTR CALLBACK QuickOpenDlg::run_dlgProc(UINT Message, WPARAM wParam, LPARAM lParam)
{
    static int progressPos = 0;

    BOOL ret = FALSE;
    switch (Message) {
    case WM_UPDATE_RUSULT_LIST:
        updateResultList();
        ret = TRUE;
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
            if (_directoryReader.IsReading()) {
                ::SetTimer(_hSelf, SCAN_QUERY, 1000, nullptr);
            }
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
            : GetDCEx(_hSelf, nullptr, DCX_INTERSECTUPDATE | DCX_CACHE | DCX_CLIPCHILDREN | DCX_CLIPSIBLINGS);

        // Erase progressbar background
        const COLORREF backgroundColor = ::GetSysColor(COLOR_3DFACE);
        const HBRUSH hBkBrush = ::CreateSolidBrush(backgroundColor);
        ::FillRect(hDC, &_progressBarRect, hBkBrush);
        ::DeleteObject(hBkBrush);

        // draw progress bar
        if (_directoryReader.IsReading()) {
            RECT barRect    = _progressBarRect;
            barRect.left    = max(_progressBarRect.left, progressPos - 16);
            barRect.right   = min(progressPos + 16,     _progressBarRect.right);
            progressPos += 2;
            progressPos %= _progressBarRect.right;

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
                ::KillTimer(_hSelf, SCAN_QUERY);
                ::SetTimer(_hSelf, SCAN_QUERY, 100, nullptr);
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
            [[fallthrough]];
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
        _hWndResult = ::GetDlgItem(_hSelf, IDC_LIST_RESULTS);
        _hWndEdit = ::GetDlgItem(_hSelf, IDC_EDIT_SEARCH);

        SetFont(_pExProp->defaultFont);
        ::SetWindowLongPtr(_hWndEdit, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        _defaultEditProc = (WNDPROC)::SetWindowLongPtr(_hWndEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndDefaultEditProc));

        GetClientRect(_hSelf, &_progressBarRect);
        _progressBarRect.top = 2;
        _progressBarRect.bottom = _progressBarRect.top + 2;
        break;
    }
    case WM_DESTROY:
        _directoryReader.Cancel();
        _filesystemWatcher.Stop();

        if (_defaultEditProc) {
            ::SetWindowLongPtr(_hWndEdit, GWLP_WNDPROC, (LONG_PTR)_defaultEditProc);
            _defaultEditProc = nullptr;
        }
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
        ::Edit_GetText(_hWndEdit, &query[0], bufferLength);
        removeWhitespaces(query);
    }

    _model->Search(query);
}

void QuickOpenDlg::updateResultList()
{
    _results = _model->GetResults();
    const auto cursel = max(::SendMessage(_hWndResult, LB_GETCURSEL, 0, 0), 0);
    ::SendMessage(_hWndResult, LB_SETCOUNT, _results.size(), 0);
    ::SendMessage(_hWndResult, LB_SETCURSEL, cursel, 0);
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
    default:
        break;
    }
    return ::CallWindowProc(_defaultEditProc, hWnd, uMsg, wParam, lParam);
}

void QuickOpenDlg::openSelectedItem() const
{
    const int index = (int)::SendDlgItemMessage(_hSelf, IDC_LIST_RESULTS, LB_GETCURSEL, 0, 0);
    if (0 <= index) {
        if (static_cast<SIZE_T>(index) < _results.size()) {
            NppInterface::doOpen(_results[index]->FullPath());
        }
    }
}
