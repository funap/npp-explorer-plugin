#pragma once

#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <memory>
#include <windows.h>
#include <commctrl.h>

class ExplorerDialog;

class TaskCheckFolderChildren : public IAsyncTask {
public:
    TaskCheckFolderChildren(ExplorerDialog* dialog, HTREEITEM hItem, const std::wstring& path, Settings* settings);

    void Execute() override;
    void OnCompleted() override;

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

private:
    std::shared_ptr<ExplorerModel> _model;
    Settings* _settings;
    std::shared_ptr<ExplorerEntry> _root;
};

class TaskUpdateDirectory : public IAsyncTask {
public:
    TaskUpdateDirectory(std::shared_ptr<ExplorerModel> model, std::shared_ptr<ExplorerEntry> entry, Settings* settings);

    void Execute() override;
    void OnCompleted() override;

private:
    std::shared_ptr<ExplorerModel> _model;
    std::shared_ptr<ExplorerEntry> _entry;
    Settings* _settings;
    std::vector<std::shared_ptr<ExplorerEntry>> _children;
};

class FileList;
struct IconWorkItem;

class TaskLoadFileList : public IAsyncTask {
public:
    TaskLoadFileList(const std::wstring& currentDir, Settings* settings, FileList* fileList);

    void Execute() override;
    void OnCompleted() override;

private:
    std::wstring _currentDir;
    Settings* _settings;
    FileList* _fileList;
    std::vector<FileSystemEntry> _entries;
};

class TaskExtractIcons : public IAsyncTask {
public:
    TaskExtractIcons(FileList* fileList, HWND hListWnd, const std::wstring& workDir, std::vector<IconWorkItem>&& workItems, std::shared_ptr<bool> cancelToken);

    void Execute() override;
    void OnCompleted() override;

private:
    FileList* _fileList;
    HWND _hListWnd;
    std::wstring _workDir;
    std::vector<IconWorkItem> _vWorkItems;
    std::shared_ptr<bool> _cancelToken;
};
