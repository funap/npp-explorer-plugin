#include "ExplorerTasks.h"
#include "FileList.h"

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

TaskLoadFileList::TaskLoadFileList(const std::wstring& currentDir, Settings* settings, FileList* fileList)
    : _currentDir(currentDir), _settings(settings), _fileList(fileList) {}

void TaskLoadFileList::Execute() {
    _entries = FileSystemService::GetDirectoryEntries(_currentDir, _settings->IsShowHidden(), true);
}

void TaskLoadFileList::OnCompleted() {
    _fileList->OnEntriesLoaded(_currentDir, std::move(_entries));
}
