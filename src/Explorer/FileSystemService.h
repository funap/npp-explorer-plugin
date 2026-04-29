#pragma once

#include <string>
#include <vector>
#include <optional>

class FileSystemEntry {
public:
    FileSystemEntry(const std::wstring& name, unsigned int attributes, unsigned __int64 fileSize, unsigned __int64 lastWriteTime, bool isParent = false)
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
    unsigned int Attributes() const { return _attributes; }
    unsigned __int64 FileSize() const { return _fileSize; }
    unsigned __int64 LastWriteTime() const { return _lastWriteTime; }

    bool IsDirectory() const { return (_attributes & 0x00000010) != 0; } // FILE_ATTRIBUTE_DIRECTORY
    bool IsHidden() const { return (_attributes & 0x00000002) != 0; }    // FILE_ATTRIBUTE_HIDDEN
    bool IsParent() const { return _isParent; }

    // UI specific state
    int Icon() const { return _iIcon; }
    void SetIcon(int icon) const { _iIcon = icon; }
    int Overlay() const { return _iOverlay; }
    void SetOverlay(int overlay) const { _iOverlay = overlay; }
    unsigned int State() const { return _state; }
    void SetState(unsigned int state) const { _state = state; }

private:
    std::wstring _name;
    unsigned int _attributes;
    unsigned __int64 _fileSize;
    unsigned __int64 _lastWriteTime;
    bool _isParent;

    mutable int _iIcon;
    mutable int _iOverlay;
    mutable unsigned int _state;
};

class FileSystemService {
public:
    static FileSystemService& Instance();

    std::vector<std::wstring> GetLogicalDrives();
    std::optional<std::wstring> GetVolumeName(const std::wstring& drivePath);

    bool HaveChildren(const std::wstring& folderPath, bool useFullTree, bool showHidden);
    std::vector<FileSystemEntry> GetDirectoryEntries(const std::wstring& path, bool showHidden, bool includeParent = false);

    bool CreateNewFile(const std::wstring& filePath);
    bool CreateNewDirectory(const std::wstring& directoryPath);

    bool DeleteFiles(void* hWnd, const std::vector<std::wstring>& paths, bool immediate);
    bool CopyFiles(void* hWnd, const std::vector<std::wstring>& from, const std::wstring& to);
    bool MoveFiles(void* hWnd, const std::vector<std::wstring>& from, const std::wstring& to);

    bool ConvertNetPathName(const std::wstring& pathName, std::wstring& remotePath);
    bool ResolveShortCut(const std::wstring& shortcutPath, std::wstring& resolvedPath);

private:
    FileSystemService() = default;
    ~FileSystemService() = default;
    FileSystemService(const FileSystemService&) = delete;
    FileSystemService& operator=(const FileSystemService&) = delete;

    std::wstring ToDoubleNullTerminatedString(const std::vector<std::wstring>& paths);
};
