#pragma once

#include <string>
#include <vector>
#include <optional>
#include <windows.h>
#include <filesystem>

struct FileSystemEntry {
    std::wstring name;
    DWORD attributes;
    bool isDirectory;

    FileSystemEntry(const std::wstring& name, DWORD attributes)
        : name(name)
        , attributes(attributes)
        , isDirectory((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {}
};

class FileSystemService {
public:
    static FileSystemService& Instance();

    std::vector<std::wstring> GetLogicalDrives();
    std::optional<std::wstring> GetVolumeName(const std::wstring& drivePath);

    bool HaveChildren(const std::wstring& folderPath, bool useFullTree, bool showHidden);
    std::vector<FileSystemEntry> GetDirectoryEntries(const std::wstring& path, bool useFullTree, bool showHidden);

    HRESULT CreateNewFile(const std::wstring& filePath);
    bool CreateNewDirectory(const std::wstring& directoryPath);

    int DeleteFiles(HWND hWnd, const std::vector<std::wstring>& paths, bool immediate);
    int CopyFiles(HWND hWnd, const std::vector<std::wstring>& from, const std::wstring& to);
    int MoveFiles(HWND hWnd, const std::vector<std::wstring>& from, const std::wstring& to);

    BOOL ConvertNetPathName(const std::wstring& pathName, std::wstring& remotePath);
    HRESULT ResolveShortCut(const std::wstring& shortcutPath, std::wstring& resolvedPath);

private:
    FileSystemService() = default;
    ~FileSystemService() = default;
    FileSystemService(const FileSystemService&) = delete;
    FileSystemService& operator=(const FileSystemService&) = delete;

    std::wstring ToDoubleNullTerminatedString(const std::vector<std::wstring>& paths);
};
