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

private:
    std::weak_ptr<ExplorerEntry> _parent;
    std::wstring _path;
    FileSystemEntry _fsEntry;
    std::vector<std::shared_ptr<ExplorerEntry>> _children;
    bool _hasLoadedChildren;
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
