#include "ExplorerTasks.h"

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

TaskUpdateDirectory::TaskUpdateDirectory(std::shared_ptr<ExplorerModel> model, std::shared_ptr<ExplorerEntry> entry, Settings* settings)
    : _model(model), _entry(entry), _settings(settings) {}

void TaskUpdateDirectory::Execute() {
    auto path = _entry->Path();
    auto entries = FileSystemService::GetDirectoryEntries(path, _settings->IsShowHidden());

    for (const auto& fsEntry : entries) {
        std::wstring childPath = path;
        if (!childPath.empty() && childPath.back() != L'\\') {
            childPath += L"\\";
        }
        childPath += fsEntry.Name();
        _children.push_back(std::make_shared<ExplorerEntry>(childPath, fsEntry));
    }
}

void TaskUpdateDirectory::OnCompleted() {
    _entry->SetChildren(_children);
    _model->NotifyEntryUpdated(_entry);
}

#include "ExplorerDialog.h"
#include "FileList.h"
#include "Explorer.h"

TaskGetCompleteIconTree::TaskGetCompleteIconTree(HWND hDlg, ExplorerDialog* dialog, HTREEITEM hItem, const std::wstring& path, DevType type)
    : _hDlg(hDlg), _dialog(dialog), _hItem(hItem), _path(path), _type(type) {}

void TaskGetCompleteIconTree::Execute() {
    GetCompleteIcon(_path.c_str(), nullptr, _type, &_iconNormal, &_iconSelected, &_overlay);
}

void TaskGetCompleteIconTree::OnCompleted() {
    if (::IsWindow(_hDlg) && _dialog) {
        _dialog->UpdateTreeItemCompleteIcon(_hItem, _iconNormal, _iconSelected, _overlay);
    }
}

TaskGetCompleteIconFileList::TaskGetCompleteIconFileList(HWND hList, FileList* fileList, UINT iItem, const std::wstring& path, const std::wstring& name, DevType type)
    : _hList(hList), _fileList(fileList), _iItem(iItem), _path(path), _name(name), _type(type) {}

void TaskGetCompleteIconFileList::Execute() {
    GetCompleteIcon(_path.c_str(), _name.c_str(), _type, &_iconNormal, &_iconSelected, &_overlay);
}

void TaskGetCompleteIconFileList::OnCompleted() {
    if (::IsWindow(_hList) && _fileList) {
        _fileList->SetCompleteIconAsync(_iItem, _name, _iconNormal, _overlay);
    }
}
