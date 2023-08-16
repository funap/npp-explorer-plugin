#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <filesystem>
#include <thread>
#include <mutex>

class FileSystemWatcher {
public:
    FileSystemWatcher();
    ~FileSystemWatcher();

    void Reset(const std::wstring& directoryPath);
    void Stop();

    using CreatedCallback = std::function<void(const std::filesystem::path&)>;
    void Created(CreatedCallback callback);

    using DeletedCallback = std::function<void(const std::filesystem::path&)>;
    void Deleted(DeletedCallback callback);
 
    using RenamedCallback = std::function<void(const std::filesystem::path&, const std::filesystem::path&)>;
    void Renamed(RenamedCallback callback);

private:
    void Run(std::wstring directory);

    std::thread                 m_thread;
    bool                        m_stop;

    CreatedCallback             m_createdCallback;
    DeletedCallback             m_deletedCallback;
    RenamedCallback             m_renamedCallback;

};