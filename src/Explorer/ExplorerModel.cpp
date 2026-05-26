#include "ExplorerModel.h"
#include "FileSystemService.h"
#include <algorithm>

ExplorerEntry::ExplorerEntry(const std::wstring& path, const FileSystemEntry& fsEntry)
    : _path(path)
    , _fsEntry(fsEntry)
    , _hasLoadedChildren(false)
{
}

std::wstring ExplorerEntry::Path() const {
    if (!_path.empty()) {
        return _path;
    }
    if (auto p = _parent.lock()) {
        return FileSystemService::CombinePath(p->Path(), _fsEntry.Name());
    }
    return _path;
}

const FileSystemEntry& ExplorerEntry::FSEntry() const {
    return _fsEntry;
}

void ExplorerEntry::SetChildren(std::vector<std::shared_ptr<ExplorerEntry>> children) {
    _children = std::move(children);
    for (auto& child : _children) {
        child->_parent = const_cast<ExplorerEntry*>(this)->shared_from_this();
    }
    _hasLoadedChildren = true;
}

std::vector<std::shared_ptr<ExplorerEntry>> ExplorerEntry::Children() const {
    return _children;
}

bool ExplorerEntry::HasLoadedChildren() const {
    return _hasLoadedChildren;
}

ExplorerModel::ExplorerModel() {}

void ExplorerModel::SetRoot(std::shared_ptr<ExplorerEntry> root) {
    std::lock_guard<std::mutex> lock(_mutex);
    _root = root;
}

std::shared_ptr<ExplorerEntry> ExplorerModel::Root() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _root;
}

void ExplorerModel::AddObserver(IExplorerModelObserver* observer) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (std::find(_observers.begin(), _observers.end(), observer) == _observers.end()) {
        _observers.push_back(observer);
    }
}

void ExplorerModel::RemoveObserver(IExplorerModelObserver* observer) {
    std::lock_guard<std::mutex> lock(_mutex);
    _observers.erase(std::remove(_observers.begin(), _observers.end(), observer), _observers.end());
}

void ExplorerModel::NotifyEntryUpdated(std::shared_ptr<ExplorerEntry> entry) {
    std::vector<IExplorerModelObserver*> observersCopy;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        observersCopy = _observers;
    }
    for (auto* observer : observersCopy) {
        observer->OnEntryUpdated(entry);
    }
}
