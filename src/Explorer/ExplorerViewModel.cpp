// The MIT License (MIT)
//
// Copyright (c) 2026 funap
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "ExplorerViewModel.h"
#include "ExplorerTasks.h"
#include "ExplorerResource.h"
#include <algorithm>
#include <shellapi.h>

ExplorerViewModel::ExplorerViewModel(std::shared_ptr<ExplorerModel> model, Settings* settings, WorkerThread* workerThread)
    : _model(model), _settings(settings), _workerThread(workerThread)
{
    _historyItr = _history.end();
}

ExplorerViewModel::~ExplorerViewModel()
{
    if (_cancelToken) {
        _cancelToken->store(true);
    }
}

void ExplorerViewModel::SetNotificationWindow(HWND hWnd)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _hNotifyWnd = hWnd;
}

void ExplorerViewModel::AddObserver(IExplorerViewModelObserver* observer)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (std::find(_observers.begin(), _observers.end(), observer) == _observers.end()) {
        _observers.push_back(observer);
    }
}

void ExplorerViewModel::RemoveObserver(IExplorerViewModelObserver* observer)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.erase(
        std::remove(_observers.begin(), _observers.end(), observer),
        _observers.end()
    );
}

void ExplorerViewModel::NavigateTo(const std::wstring& path, bool recordHistory)
{
    if (path.empty()) {
        return;
    }

    std::wstring targetPath = path;
    
    // Convert relative ".." or join with current dir
    if (path == L"..") {
        std::filesystem::path current(_currentDir);
        if (current.has_parent_path() && current.parent_path() != current.root_path()) {
            targetPath = current.parent_path().wstring();
        } else {
            targetPath = current.root_path().wstring();
        }
    } else if (!std::filesystem::path(path).is_absolute()) {
        targetPath = FileSystemService::CombinePath(_currentDir, path);
    }

    if (_settings->IsShowWorkspaceMode() && !_settings->GetWorkspaceFolders().empty()) {
        if (!_settings->IsPathInWorkspace(targetPath)) {
            targetPath = _settings->GetWorkspaceFolders()[0];
        }
    }

    if (_currentDir == targetPath) {
        return;
    }

    if (recordHistory) {
        if (!_history.empty() && _historyItr != _history.end() - 1) {
            // Overwrite forward history if we navigated to a new path from the middle of the history stack
            _history.erase(_historyItr + 1, _history.end());
        }

        NavigationState state;
        state.path = targetPath;
        _history.push_back(state);
        _historyItr = _history.end() - 1;
    }

    _currentDir = targetPath;
    _settings->SetCurrentDir(targetPath);

    NotifyCurrentDirectoryChanged();
    NotifyNavigationStateChanged();

    // Trigger async loading of directories
    if (_cancelToken) {
        _cancelToken->store(true);
    }
    _cancelToken = std::make_shared<std::atomic<bool>>(false);
    _currentGeneration++;

    ClearPendingTasks(TaskCategory::FileList);

    EnqueueAsyncTask(std::make_unique<TaskLoadFileList>(targetPath, _settings, this));
}

void ExplorerViewModel::NavigateBack()
{
    if (CanNavigateBack()) {
        _historyItr--;
        NavigateTo(_historyItr->path, false);
    }
}

void ExplorerViewModel::NavigateForward()
{
    if (CanNavigateForward()) {
        _historyItr++;
        NavigateTo(_historyItr->path, false);
    }
}

bool ExplorerViewModel::CanNavigateBack() const
{
    return !_history.empty() && _historyItr != _history.begin();
}

bool ExplorerViewModel::CanNavigateForward() const
{
    return !_history.empty() && _historyItr != _history.end() - 1;
}

INT ExplorerViewModel::GetBackHistory(LPTSTR* pszPathes) const
{
    INT i = 0;
    if (_history.size() > 1 && _historyItr != _history.begin()) {
        auto itr = _historyItr;
        while (itr != _history.begin()) {
            itr--;
            if (pszPathes) {
                wcscpy(pszPathes[i], itr->path.c_str());
            }
            i++;
        }
    }
    return i;
}

INT ExplorerViewModel::GetForwardHistory(LPTSTR* pszPathes) const
{
    INT i = 0;
    if (_history.size() > 1 && _historyItr != _history.end() - 1) {
        auto itr = _historyItr;
        while (itr != _history.end() - 1) {
            itr++;
            if (pszPathes) {
                wcscpy(pszPathes[i], itr->path.c_str());
            }
            i++;
        }
    }
    return i;
}

void ExplorerViewModel::NavigateToHistoryOffset(int offset)
{
    if (!_history.empty()) {
        _historyItr += offset;
        NavigateTo(_historyItr->path, false);
    }
}

void ExplorerViewModel::OnParentDirectoryRenamed(const std::wstring& oldPath, const std::wstring& newPath)
{
    // Update current directory if it or its parent was renamed
    if (_currentDir.compare(0, oldPath.length(), oldPath) == 0) {
        std::wstring relative = _currentDir.substr(oldPath.length());
        _currentDir = newPath + relative;
        _settings->SetCurrentDir(_currentDir);
        NotifyCurrentDirectoryChanged();
    }

    // Update history entries that are affected by this rename
    for (auto& state : _history) {
        if (state.path.compare(0, oldPath.length(), oldPath) == 0) {
            std::wstring relative = state.path.substr(oldPath.length());
            state.path = newPath + relative;
        }
    }
}

void ExplorerViewModel::UpdateSelection(const std::vector<std::wstring>& selectedItems)
{
    if (!_history.empty() && _historyItr != _history.end()) {
        _historyItr->selectedItems = selectedItems;
    }
}

std::vector<std::wstring> ExplorerViewModel::GetCurrentSelection() const
{
    if (!_history.empty() && _historyItr != _history.end()) {
        return _historyItr->selectedItems;
    }
    return {};
}

std::wstring ExplorerViewModel::GetCurrentDir() const
{
    return _currentDir;
}

const std::vector<FileSystemEntry>& ExplorerViewModel::GetCurrentDirEntries() const
{
    return _currentDirEntries;
}

void ExplorerViewModel::OnEntriesLoaded(const std::wstring& currentDir, std::vector<FileSystemEntry> entries)
{
    auto normalizePath = [](std::wstring p) {
        if (!p.empty() && p.back() == '\\') {
            p.pop_back();
        }
        return p;
    };

    if (_wcsicmp(normalizePath(currentDir).c_str(), normalizePath(_currentDir).c_str()) != 0) {
        return;
    }

    _currentDirEntries = std::move(entries);
    NotifyEntriesLoaded();
}

void ExplorerViewModel::Refresh()
{
    // Trigger async loading of directories directly, bypassing targetPath == currentDir check in NavigateTo
    if (_cancelToken) {
        _cancelToken->store(true);
    }
    _cancelToken = std::make_shared<std::atomic<bool>>(false);
    _currentGeneration++;

    ClearPendingTasks(TaskCategory::FileList);

    EnqueueAsyncTask(std::make_unique<TaskLoadFileList>(_currentDir, _settings, this));
}

void ExplorerViewModel::SetFilter(const std::wstring& filter)
{
    _filter = filter;
    _settings->GetFileFilter().setFilter(filter.c_str());
    Refresh();
}

std::wstring ExplorerViewModel::GetFilter() const
{
    return _filter;
}

void ExplorerViewModel::EnqueueAsyncTask(std::unique_ptr<IAsyncTask> task)
{
    _workerThread->Enqueue(std::move(task));
}

void ExplorerViewModel::ClearPendingTasks(std::optional<TaskCategory> category)
{
    _workerThread->ClearPendingTasks(category);
}

void ExplorerViewModel::OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task)
{
    HWND targetWnd = nullptr;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        targetWnd = _hNotifyWnd;
    }

    if (targetWnd) {
        IAsyncTask* rawTask = task.release();
        if (!::PostMessage(targetWnd, EXM_ASYNCTASK_COMPLETED, reinterpret_cast<WPARAM>(rawTask), 0)) {
            delete rawTask;
        }
    }
}

void ExplorerViewModel::ProcessTaskCompleted(IAsyncTask* rawTask)
{
    std::unique_ptr<IAsyncTask> task(rawTask);
    task->OnCompleted();
}

void ExplorerViewModel::NotifyCurrentDirectoryChanged()
{
    std::vector<IExplorerViewModelObserver*> observersCopy;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        observersCopy = _observers;
    }
    for (auto* observer : observersCopy) {
        observer->OnCurrentDirectoryChanged(_currentDir);
    }
}

void ExplorerViewModel::NotifyEntriesLoaded()
{
    std::vector<IExplorerViewModelObserver*> observersCopy;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        observersCopy = _observers;
    }
    for (auto* observer : observersCopy) {
        observer->OnDirectoryEntriesLoaded(_currentDir, _currentDirEntries);
    }
}

void ExplorerViewModel::NotifyNavigationStateChanged()
{
    std::vector<IExplorerViewModelObserver*> observersCopy;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        observersCopy = _observers;
    }
    for (auto* observer : observersCopy) {
        observer->OnNavigationStateChanged();
    }
}

void ExplorerViewModel::OpenFile(const std::wstring& filePath)
{
    if (filePath.empty()) return;

    std::wstring resolvedPath;
    if (FileSystemService::ResolveShortCut(filePath, resolvedPath)) {
        std::error_code ec;
        if (std::filesystem::is_directory(resolvedPath, ec)) {
            NavigateTo(resolvedPath);
            return;
        }
    } else {
        resolvedPath = filePath;
    }

    std::error_code ec;
    if (std::filesystem::is_directory(resolvedPath, ec)) {
        NavigateTo(resolvedPath);
    } else {
        std::vector<IExplorerViewModelObserver*> observersCopy;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            observersCopy = _observers;
        }
        for (auto* observer : observersCopy) {
            observer->OnOpenFileRequested(resolvedPath);
        }
    }
}

void ExplorerViewModel::NavigateOrExecute(const std::wstring& input)
{
    std::wstring trimmed = input;
    size_t first = trimmed.find_first_not_of(L" \t\r\n\"'");
    if (first != std::wstring::npos) {
        size_t last = trimmed.find_last_not_of(L" \t\r\n\"'");
        trimmed = trimmed.substr(first, (last - first + 1));
    }
    else {
        trimmed.clear();
    }

    if (trimmed.empty()) {
        return;
    }

    bool isPathValid = false;
    std::wstring checkPath;
    std::wstring resolvedPathStr;
    try {
        std::wstring currentPath = _settings->GetCurrentDir();
        std::filesystem::path inputPath(trimmed);
        std::filesystem::path resolvedPath = inputPath;
        if (!inputPath.is_absolute()) {
            resolvedPath = std::filesystem::path(currentPath) / inputPath;
        }

        std::error_code ec;
        std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(resolvedPath, ec);
        if (!ec) {
            resolvedPath = canonicalPath;
        }

        if (std::filesystem::exists(resolvedPath, ec)) {
            isPathValid = true;
            checkPath = resolvedPath.wstring();
            if (!std::filesystem::is_directory(resolvedPath, ec)) {
                checkPath = resolvedPath.parent_path().wstring();
            }
            resolvedPathStr = resolvedPath.wstring();
        }
    }
    catch (const std::exception&) {
        isPathValid = false;
    }

    if (isPathValid) {
        if (_settings->IsShowWorkspaceMode() && !_settings->IsPathInWorkspace(checkPath)) {
            std::vector<IExplorerViewModelObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                observersCopy = _observers;
            }
            for (auto* observer : observersCopy) {
                observer->OnToggleWorkspaceModeRequested();
            }
        }

        std::error_code ec;
        if (std::filesystem::is_directory(resolvedPathStr, ec)) {
            NavigateTo(resolvedPathStr);
        }
        else {
            std::vector<IExplorerViewModelObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                observersCopy = _observers;
            }
            for (auto* observer : observersCopy) {
                observer->OnOpenFileRequested(resolvedPathStr);
            }
        }
    }
    else {
        std::wstring exe;
        std::wstring args;
        size_t space = trimmed.find(L' ');
        if (space != std::wstring::npos) {
            exe = trimmed.substr(0, space);
            args = trimmed.substr(space + 1);
        }
        else {
            exe = trimmed;
        }

        std::wstring currentPath = _settings->GetCurrentDir();
        HINSTANCE hInst = ::ShellExecute(nullptr, L"open", exe.c_str(), args.empty() ? nullptr : args.c_str(), currentPath.c_str(), SW_SHOWNORMAL);
        if ((INT_PTR)hInst <= 32) {
            std::vector<IExplorerViewModelObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                observersCopy = _observers;
            }
            for (auto* observer : observersCopy) {
                observer->OnCommandExecutionFailed(trimmed);
            }
        }
    }
}
