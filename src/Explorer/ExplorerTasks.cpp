#include "ExplorerTasks.h"
#include "FileList.h"
#include "ExplorerDialog.h"

TaskInit::TaskInit(std::shared_ptr<ExplorerModel> model, Settings* settings)
    : _model(model), _settings(settings) {}

void TaskInit::Execute() {
    auto drives = FileSystemService::GetLogicalDrives();
    std::vector<std::shared_ptr<ExplorerEntry>> children;

    for (const auto& drivePath : drives) {
        auto volumeName = FileSystemService::GetVolumeName(drivePath);
        std::wstring name = volumeName ? std::format(L"{}: [{}]", drivePath[0], *volumeName) : std::format(L"{}:", drivePath[0]);

        FileSystemEntry fsEntry(name, FILE_ATTRIBUTE_DIRECTORY, 0, 0, false);

        int iIconNormal = 0, iIconSelected = 0, iIconOverlayed = 0;
        // ExtractIcons needs to be handled differently or skipped here if it uses UI thread,
        // but typically ExtractIcons just reads from system so it might be safe.
        // We'll leave it to the View to resolve icons later if needed, but for now we keep it simple.

        children.push_back(std::make_shared<ExplorerEntry>(drivePath, fsEntry));
    }

    _root = std::make_shared<ExplorerEntry>(L"This PC", FileSystemEntry(L"This PC", FILE_ATTRIBUTE_DIRECTORY, 0, 0, false));
    _root->SetChildren(children);
}

void TaskInit::OnCompleted() {
    _model->SetRoot(_root);
    _model->NotifyEntryUpdated(_root);
}

TaskUpdateDirectory::TaskUpdateDirectory(std::shared_ptr<ExplorerModel> model, std::shared_ptr<ExplorerEntry> entry, const std::wstring& path, Settings* settings)
    : _model(model), _entry(entry), _path(path), _settings(settings) {}

void TaskUpdateDirectory::Execute() {
    // Use _path (value-copied in constructor, immutable on this thread) -- do NOT call
    // _entry->Path() here, as _entry may be modified concurrently from the UI thread.
    auto entries = FileSystemService::GetDirectoryEntries(_path, _settings->IsShowHidden());

    std::wstring basePath = _path;
    if (!basePath.empty() && basePath.back() != L'\\') {
        basePath += L'\\';
    }

    for (const auto& fsEntry : entries) {
        std::wstring childPath = basePath + fsEntry.Name();
        _children.push_back(std::make_shared<ExplorerEntry>(childPath, fsEntry));
    }
}

void TaskUpdateDirectory::OnCompleted() {
    _entry->SetChildren(_children);
    _model->NotifyEntryUpdated(_entry);
}

TaskLoadFileList::TaskLoadFileList(const std::wstring& currentDir, Settings* settings, FileList* fileList)
    : _currentDir(currentDir), _settings(settings), _fileList(fileList) {}

void TaskLoadFileList::Execute() {
    _entries = FileSystemService::GetDirectoryEntries(_currentDir, _settings->IsShowHidden(), true);
}

void TaskLoadFileList::OnCompleted() {
    _fileList->OnEntriesLoaded(_currentDir, std::move(_entries));
}

TaskCheckFolderChildren::TaskCheckFolderChildren(ExplorerDialog* dialog, HTREEITEM hItem, const std::wstring& path, Settings* settings)
    : _dialog(dialog), _hItem(hItem), _path(path), _settings(settings) {}

void TaskCheckFolderChildren::Execute() {
    _hasChildren = FileSystemService::HaveChildren(_path, _settings->IsUseFullTree(), _settings->IsShowHidden());
}

void TaskCheckFolderChildren::OnCompleted() {
    _dialog->OnFolderChildrenChecked(_hItem, _path, _hasChildren);
}

TaskExtractIcons::TaskExtractIcons(FileList* fileList, HWND hListWnd, const std::wstring& workDir, std::vector<IconWorkItem>&& workItems, std::shared_ptr<std::atomic<bool>> cancelToken, uint64_t generation)
    : _fileList(fileList), _hListWnd(hListWnd), _workDir(workDir), _vWorkItems(std::move(workItems)), _cancelToken(cancelToken), _generation(generation) {}

void TaskExtractIcons::Execute() {
    for (const auto& item : _vWorkItems) {
        if (_cancelToken && _cancelToken->load()) {
            break;
        }

        int icon = 0;
        int iconSelected = 0;
        int overlay = 0;

        ExtractIcons(_workDir.c_str(), item.name.c_str(), item.type, &icon, &iconSelected, &overlay);

        if (_cancelToken && _cancelToken->load()) {
            break;
        }

        IconResult* result = new IconResult{ _workDir, item.index, icon, overlay, _generation, item.name };
        if (!::PostMessage(_hListWnd, EXM_UPDATE_ICON_RESULT, 0, (LPARAM)result)) {
            delete result;
        }
    }
}

void TaskExtractIcons::OnCompleted() {
    // Incrementally posted results to FileList, nothing to do on complete.
}

#include "TreeView.h"

TaskTreeViewExtractIcons::TaskTreeViewExtractIcons(TreeView* treeCtrl, HTREEITEM hItem, const std::wstring& path, DevType devType)
    : _treeCtrl(treeCtrl), _hItem(hItem), _path(path), _devType(devType) {}

void TaskTreeViewExtractIcons::Execute() {
    ExtractIcons(_path.c_str(), nullptr, _devType, &_icon, &_iconSelected, &_overlay);
}

void TaskTreeViewExtractIcons::OnCompleted() {
    if (_treeCtrl && _hItem) {
        _treeCtrl->SetItemIcons(_hItem, _icon, _iconSelected, _overlay);
    }
}

