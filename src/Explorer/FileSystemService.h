#pragma once

#include <string>
#include <vector>
#include <optional>
#include <ctime>

class FileSystemEntry {
public:
    FileSystemEntry(const std::wstring& name, unsigned int attributes, size_t fileSize, time_t lastWriteTime, bool isParent = false);

    const std::wstring& Name() const;
    void SetName(const std::wstring& name);
    unsigned int Attributes() const;
    size_t FileSize() const;
    time_t LastWriteTime() const;

    bool IsDirectory() const;
    bool IsHidden() const;
    bool IsParent() const;

private:
    std::wstring _name;
    unsigned int _attributes;
    size_t _fileSize;
    time_t _lastWriteTime;
    bool _isParent;
};

class FileSystemService {
public:
    static std::vector<std::wstring> GetLogicalDrives();
    static std::optional<std::wstring> GetVolumeName(const std::wstring& drivePath);

    static bool HaveChildren(const std::wstring& folderPath, bool useFullTree, bool showHidden);
    static std::vector<FileSystemEntry> GetDirectoryEntries(const std::wstring& path, bool showHidden, bool includeParent = false);
    static std::wstring CombinePath(const std::wstring& parent, const std::wstring& child);

    static bool CreateNewFile(const std::wstring& filePath);
    static bool CreateNewDirectory(const std::wstring& directoryPath);

    static bool DeleteFiles(void* hWnd, const std::vector<std::wstring>& paths, bool immediate);
    static bool CopyFiles(void* hWnd, const std::vector<std::wstring>& from, const std::wstring& to);
    static bool MoveFiles(void* hWnd, const std::vector<std::wstring>& from, const std::wstring& to);

    static bool ConvertNetPathName(const std::wstring& pathName, std::wstring& remotePath);
    static bool ResolveShortCut(const std::wstring& shortcutPath, std::wstring& resolvedPath);

private:
    static std::wstring ToDoubleNullTerminatedString(const std::vector<std::wstring>& paths);
};
