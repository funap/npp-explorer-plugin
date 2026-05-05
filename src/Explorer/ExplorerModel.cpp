#include "ExplorerModel.h"
#include <algorithm>

ExplorerEntry::ExplorerEntry(const std::wstring& path, const FileSystemEntry& fsEntry)
    : _path(path)
    , _fsEntry(fsEntry)
    , _hasLoadedChildren(false)
{
}

const std::wstring& ExplorerEntry::Path() const {
    return _path;
}

const FileSystemEntry& ExplorerEntry::FSEntry() const {
    return _fsEntry;
}

void ExplorerEntry::SetChildren(std::vector<std::shared_ptr<ExplorerEntry>> children) {
    _children = std::move(children);
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
