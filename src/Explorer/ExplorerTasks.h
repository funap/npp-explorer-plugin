#pragma once

#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "Settings.h"
#include "FileSystemService.h"
#include "Explorer.h"
#include <string>
#include <vector>
#include <memory>
#include <windows.h>
#include <commctrl.h>
#include <atomic>

class ExplorerDialog;

class TaskCheckFolderChildren : public IAsyncTask {
public:
    TaskCheckFolderChildren(ExplorerDialog* dialog, HTREEITEM hItem, const std::wstring& path, Settings* settings);

    void Execute() override;
    void OnCompleted() override;
    TaskCategory GetCategory() const override { return TaskCategory::TreeView; }

private:
    ExplorerDialog* _dialog;
    HTREEITEM _hItem;
    std::wstring _path;
    Settings* _settings;
    bool _hasChildren{false};
};


class TaskInit : public IAsyncTask {
public:
    TaskInit(std::shared_ptr<ExplorerModel> model, Settings* settings);

    void Execute() override;
    void OnCompleted() override;
    TaskPriority GetPriority() const override { return TaskPriority::High; }
    TaskCategory GetCategory() const override { return TaskCategory::TreeView; }

private:
    std::shared_ptr<ExplorerModel> _model;
    Settings* _settings;
    std::shared_ptr<ExplorerEntry> _root;
};

class TaskUpdateDirectory : public IAsyncTask {
public:
    // Takes path and entry as separate arguments.
    // _path is used in Execute() (worker thread) and is an immutable value copy.
    // _entry is only accessed in OnCompleted() (UI thread) to apply the result.
    TaskUpdateDirectory(std::shared_ptr<ExplorerModel> model, std::shared_ptr<ExplorerEntry> entry, const std::wstring& path, Settings* settings);

    void Execute() override;
    void OnCompleted() override;
    TaskPriority GetPriority() const override { return TaskPriority::High; }
    TaskCategory GetCategory() const override { return TaskCategory::TreeView; }

private:
    std::shared_ptr<ExplorerModel> _model;
    std::shared_ptr<ExplorerEntry> _entry;
    std::wstring _path;  // immutable value-copy, safe to read from worker thread
    Settings* _settings;
    std::vector<std::shared_ptr<ExplorerEntry>> _children;
};

class FileList;
class ExplorerViewModel;
struct IconWorkItem;

class TaskLoadFileList : public IAsyncTask {
public:
    TaskLoadFileList(const std::wstring& currentDir, Settings* settings, ExplorerViewModel* viewModel);

    void Execute() override;
    void OnCompleted() override;
    TaskPriority GetPriority() const override { return TaskPriority::High; }
    TaskCategory GetCategory() const override { return TaskCategory::FileList; }

private:
    std::wstring _currentDir;
    Settings* _settings;
    ExplorerViewModel* _viewModel;
    std::vector<FileSystemEntry> _entries;
};

class TaskFetchIcons : public IAsyncTask {
public:
    TaskFetchIcons(FileList* fileList, HWND hListWnd, const std::wstring& workDir, std::vector<IconWorkItem>&& workItems, std::shared_ptr<std::atomic<bool>> cancelToken, uint64_t generation);

    void Execute() override;
    void OnCompleted() override;
    TaskCategory GetCategory() const override { return TaskCategory::FileList; }

private:
    FileList* _fileList;
    HWND _hListWnd;
    std::wstring _workDir;
    std::vector<IconWorkItem> _vWorkItems;
    std::shared_ptr<std::atomic<bool>> _cancelToken;
    uint64_t _generation;
};

class TreeView;

class TaskTreeViewFetchIcons : public IAsyncTask {
public:
    TaskTreeViewFetchIcons(TreeView* treeCtrl, HTREEITEM hItem, const std::wstring& path, DevType devType);

    void Execute() override;
    void OnCompleted() override;
    TaskCategory GetCategory() const override { return TaskCategory::TreeView; }

private:
    TreeView* _treeCtrl;
    HTREEITEM _hItem;
    std::wstring _path;
    DevType _devType;
    int _icon{0};
    int _iconSelected{0};
    int _overlay{0};
};
