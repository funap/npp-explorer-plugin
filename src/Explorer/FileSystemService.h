#pragma once

#include <string>
#include <vector>
#include <optional>
#include <windows.h>
#include <filesystem>

class FileSystemEntry {
public:
    FileSystemEntry(const std::wstring& name, DWORD attributes, unsigned __int64 fileSize, FILETIME lastWriteTime, bool isParent = false)
        : _name(name)
        , _attributes(attributes)
        , _fileSize(fileSize)
        , _lastWriteTime(lastWriteTime)
        , _isParent(isParent)
        , _iIcon(-1)
        , _iOverlay(0)
        , _state(0)
    {}

    const std::wstring& Name() const { return _name; }
    DWORD Attributes() const { return _attributes; }
    unsigned __int64 FileSize() const { return _fileSize; }
    FILETIME LastWriteTime() const { return _lastWriteTime; }

    bool IsDirectory() const { return (_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    bool IsHidden() const { return (_attributes & FILE_ATTRIBUTE_HIDDEN) != 0; }
    bool IsParent() const { return _isParent; }

    // UI specific state
    int Icon() const { return _iIcon; }
    void SetIcon(int icon) const { _iIcon = icon; }
    int Overlay() const { return _iOverlay; }
    void SetOverlay(int overlay) const { _iOverlay = overlay; }
    UINT State() const { return _state; }
    void SetState(UINT state) const { _state = state; }

private:
    std::wstring _name;
    DWORD _attributes;
    unsigned __int64 _fileSize;
    FILETIME _lastWriteTime;
    bool _isParent;

    mutable int _iIcon;
    mutable int _iOverlay;
    mutable UINT _state;
};

class FileSystemService {
public:
    static FileSystemService& Instance();

    std::vector<std::wstring> GetLogicalDrives();
    std::optional<std::wstring> GetVolumeName(const std::wstring& drivePath);

    bool HaveChildren(const std::wstring& folderPath, bool useFullTree, bool showHidden);
    std::vector<FileSystemEntry> GetDirectoryEntries(const std::wstring& path, bool showHidden, bool includeParent = false);

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
