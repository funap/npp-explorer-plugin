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

void ExplorerEntry::SetHaveChildren(bool haveChildren) {
    _haveChildren = haveChildren;
}

bool ExplorerEntry::HaveChildren() const {
    return _haveChildren;
}

void ExplorerEntry::SetSelectedIcon(int icon) {
    _iIconSelected = icon;
}

int ExplorerEntry::SelectedIcon() const {
    return _iIconSelected;
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

std::shared_ptr<ExplorerEntry> ExplorerModel::FindEntry(const std::wstring& path) const {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_root) return nullptr;

    // Simplistic breadth-first or depth-first search for the path.
    // Given paths match exactly, we can just do a DFS.
    std::vector<std::shared_ptr<ExplorerEntry>> stack = { _root };
    while (!stack.empty()) {
        auto current = stack.back();
        stack.pop_back();

        if (current->Path() == path) {
            return current;
        }

        for (const auto& child : current->Children()) {
            stack.push_back(child);
        }
    }
    return nullptr;
}
