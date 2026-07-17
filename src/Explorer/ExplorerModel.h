#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include "FileSystemService.h"

class ExplorerEntry : public std::enable_shared_from_this<ExplorerEntry> {
public:
    ExplorerEntry(const std::wstring& path, const FileSystemEntry& fsEntry);

    std::wstring Path() const;
    const FileSystemEntry& FSEntry() const;
    void Rename(const std::wstring& newPath, const std::wstring& newName);

    void SetChildren(std::vector<std::shared_ptr<ExplorerEntry>> children);
    std::vector<std::shared_ptr<ExplorerEntry>> Children() const;

    bool HasLoadedChildren() const;

    // View state (mutable - used by UI for caching display info)
    int Icon() const { return _icon; }
    void SetIcon(int icon) const { _icon = icon; }
    int Overlay() const { return _overlay; }
    void SetOverlay(int overlay) const { _overlay = overlay; }
    unsigned int ViewState() const { return _viewState; }
    void SetViewState(unsigned int state) const { _viewState = state; }
    void ResetViewCache() const { _icon = -1; _overlay = 0; _viewState = 0; }

    // Convenience accessors (delegate to FSEntry)
    const std::wstring& Name() const { return _fsEntry.Name(); }
    bool IsDirectory() const { return _fsEntry.IsDirectory(); }
    bool IsHidden() const { return _fsEntry.IsHidden(); }
    bool IsParent() const { return _fsEntry.IsParent(); }
    unsigned int Attributes() const { return _fsEntry.Attributes(); }
    size_t FileSize() const { return _fsEntry.FileSize(); }
    time_t LastWriteTime() const { return _fsEntry.LastWriteTime(); }

private:
    std::weak_ptr<ExplorerEntry> _parent;
    std::wstring _path;
    FileSystemEntry _fsEntry;
    std::vector<std::shared_ptr<ExplorerEntry>> _children;
    bool _hasLoadedChildren;
    mutable int _icon{-1};
    mutable int _overlay{0};
    mutable unsigned int _viewState{0};
};

class IExplorerModelObserver {
public:
    virtual ~IExplorerModelObserver() = default;
    virtual void OnEntryUpdated(std::shared_ptr<ExplorerEntry> entry) = 0;
};

class ExplorerModel {
public:
    ExplorerModel();

    void SetRoot(std::shared_ptr<ExplorerEntry> root);
    std::shared_ptr<ExplorerEntry> Root() const;

    void AddObserver(IExplorerModelObserver* observer);
    void RemoveObserver(IExplorerModelObserver* observer);

    void NotifyEntryUpdated(std::shared_ptr<ExplorerEntry> entry);

private:
    std::shared_ptr<ExplorerEntry> _root;
    std::vector<IExplorerModelObserver*> _observers;
    mutable std::mutex _mutex;
};
