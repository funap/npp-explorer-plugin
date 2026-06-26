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

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <optional>
#include <windows.h>

#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "IDispatcher.h"
#include "Settings.h"
#include "FileSystemService.h"
#include "Explorer.h"

struct PromptForNameEvent {
    using ReturnType = std::optional<std::wstring>;
    std::wstring defaultName;
    std::wstring comment;
};

struct OpenFileRequestedEvent {
    using ReturnType = void;
    std::wstring filePath;
};

struct RefreshRequestedEvent {
    using ReturnType = void;
};

struct EntryRenamedEvent {
    using ReturnType = void;
    std::wstring oldPath;
    std::wstring newPath;
    std::wstring newName;
};

// Forward declaration
class IExplorerViewModelObserver;

struct NavigationState {
    std::wstring path;
    std::vector<std::wstring> selectedItems;
};

class FileList;
class TreeView;
struct IconWorkItem;

class ExplorerViewModel : public IAsyncTaskCallback {
public:
    ExplorerViewModel(std::shared_ptr<ExplorerModel> model, Settings* settings, IDispatcher* dispatcher);
    virtual ~ExplorerViewModel();
    void SetSettings(Settings* settings) { _settings = settings; }
    Settings* GetSettings() const { return _settings; }

    void OpenFile(const std::wstring& filePath);
    void NavigateOrExecute(const std::wstring& input);
    std::optional<std::wstring> ResolveAndValidateDirectory(const std::wstring& input);
    void InitModel();
    void UpdateDirectory(std::shared_ptr<ExplorerEntry> entry, const std::wstring& path);
    void StopWorkerThread();

    void AddObserver(IExplorerViewModelObserver* observer);
    void RemoveObserver(IExplorerViewModelObserver* observer);

    // Navigation history methods
    void NavigateTo(const std::wstring& path, bool recordHistory = true);
    void NavigateBack();
    void NavigateForward();
    bool CanNavigateBack() const;
    bool CanNavigateForward() const;
    void OnParentDirectoryRenamed(const std::wstring& oldPath, const std::wstring& newPath);

    // History getters for toolbar dropdown menus
    INT GetBackHistory(LPTSTR* pszPathes) const;
    INT GetForwardHistory(LPTSTR* pszPathes) const;
    void NavigateToHistoryOffset(int offset);

    // Active Selection tracking (updates the current navigation state's selectedItems list)
    void UpdateSelection(const std::vector<std::wstring>& selectedItems);
    std::vector<std::wstring> GetCurrentSelection() const;

    // View data getters
    std::wstring GetCurrentDir() const;
    const std::vector<FileSystemEntry>& GetCurrentDirEntries() const;

    // Directory loading results
    void OnEntriesLoaded(const std::wstring& currentDir, std::vector<FileSystemEntry> entries);

    // Commands
    void Refresh();
    void SetFilter(const std::wstring& filter);
    std::wstring GetFilter() const;
    bool CreateFolder(const std::wstring& parentPath, std::wstring& errorMsg);
    bool CreateFile(const std::wstring& parentPath, std::wstring& errorMsg);
    bool RenameEntry(const std::wstring& oldPath, std::wstring& errorMsg);

    template<typename EventType>
    typename EventType::ReturnType emit(const EventType& ev)
    {
        if constexpr (std::is_same_v<typename EventType::ReturnType, void>) {
            std::vector<IExplorerViewModelObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                observersCopy = _observers;
            }
            for (auto* observer : observersCopy) {
                observer->handle(ev);
            }
        } else {
            std::vector<IExplorerViewModelObserver*> observersCopy;
            {
                std::lock_guard<std::mutex> lock(_mutex);
                observersCopy = _observers;
            }
            for (auto* observer : observersCopy) {
                auto res = observer->handle(ev);
                if (res.has_value()) {
                    return res;
                }
            }
            return typename EventType::ReturnType{};
        }
    }

    // Async Task requests from View
    void CheckFolderChildren(HTREEITEM hItem, const std::wstring& path);
    void FetchFileListIcons(FileList* fileList, HWND hListWnd, const std::wstring& workDir, std::vector<IconWorkItem>&& workItems, std::shared_ptr<std::atomic<bool>> cancelToken, uint64_t generation);
    void FetchTreeViewIcons(TreeView* treeCtrl, HTREEITEM hItem, const std::wstring& path, DevType devType);

    // IAsyncTaskCallback implementation
    void OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task) override;

    // Asynchronous callbacks from task completions
    void OnFolderChildrenChecked(HTREEITEM hItem, const std::wstring& path, bool hasChildren);

private:
    void EnqueueAsyncTask(std::unique_ptr<IAsyncTask> task);
    void ClearPendingTasks(std::optional<TaskCategory> category = std::nullopt);
    void NotifyCurrentDirectoryChanged();
    void NotifyEntriesLoaded();
    void NotifyNavigationStateChanged();
    std::wstring PreprocessInput(const std::wstring& input) const;
    std::wstring ResolveToAbsolutePath(const std::wstring& expandedInput) const;

    std::shared_ptr<ExplorerModel> _model;
    Settings* _settings;
    WorkerThread _workerThread;

    std::wstring _currentDir;
    std::wstring _filter{ L"*.*" };
    std::vector<FileSystemEntry> _currentDirEntries;

    // Navigation Stack
    std::vector<NavigationState> _history;
    std::vector<NavigationState>::iterator _historyItr;

    std::vector<IExplorerViewModelObserver*> _observers;
    mutable std::mutex _mutex;

    IDispatcher* _dispatcher{ nullptr };

    uint64_t _currentGeneration{ 0 };
    std::shared_ptr<std::atomic<bool>> _cancelToken;
};

class IExplorerViewModelObserver {
public:
    virtual ~IExplorerViewModelObserver() = default;
    virtual void OnCurrentDirectoryChanged(const std::wstring& path) = 0;
    virtual void OnDirectoryEntriesLoaded(const std::wstring& path, const std::vector<FileSystemEntry>& entries) = 0;
    virtual void OnNavigationStateChanged() = 0;
    virtual void OnCommandExecutionFailed(const std::wstring& command) = 0;
    virtual void OnToggleWorkspaceModeRequested() = 0;
    virtual void OnFolderChildrenChecked(HTREEITEM hItem, const std::wstring& path, bool hasChildren) {}

    // Event handler overloads
    virtual std::optional<std::wstring> handle(const PromptForNameEvent& ev) { return std::nullopt; }
    virtual void handle(const OpenFileRequestedEvent& ev) {}
    virtual void handle(const RefreshRequestedEvent& ev) {}
    virtual void handle(const EntryRenamedEvent& ev) {}
};
