#pragma once

#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <memory>

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

class ExplorerDialog;
class FileList;

class TaskGetCompleteIconTree : public IAsyncTask {
public:
    TaskGetCompleteIconTree(HWND hDlg, ExplorerDialog* dialog, HTREEITEM hItem, const std::wstring& path, DevType type);

    void Execute() override;
    void OnCompleted() override;

private:
    HWND _hDlg;
    ExplorerDialog* _dialog;
    HTREEITEM _hItem;
    std::wstring _path;
    DevType _type;
    int _iconNormal{0};
    int _iconSelected{0};
    int _overlay{0};
};

class TaskGetCompleteIconFileList : public IAsyncTask {
public:
    TaskGetCompleteIconFileList(HWND hList, FileList* fileList, UINT iItem, const std::wstring& path, const std::wstring& name, DevType type);

    void Execute() override;
    void OnCompleted() override;

private:
    HWND _hList;
    FileList* _fileList;
    UINT _iItem;
    std::wstring _path;
    std::wstring _name;
    DevType _type;
    int _iconNormal{0};
    int _iconSelected{0};
    int _overlay{0};
};
