#include "FileSystemService.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <winnetwk.h>
#include <wrl/client.h>
#include <algorithm>
#include <format>

FileSystemService& FileSystemService::Instance()
{
    static FileSystemService instance;
    return instance;
}

std::vector<std::wstring> FileSystemService::GetLogicalDrives()
{
    std::vector<std::wstring> drives;
    DWORD driveList = ::GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (0x01 & (driveList >> i)) {
            std::wstring drivePath = L"A:\\";
            drivePath[0] = static_cast<wchar_t>(L'A' + i);
            drives.push_back(drivePath);
        }
    }
    return drives;
}

std::optional<std::wstring> FileSystemService::GetVolumeName(const std::wstring& drivePath)
{
    DWORD volumeNameSize = MAX_PATH;
    std::wstring volumeName(volumeNameSize, L'\0');

    if (::GetVolumeInformation(drivePath.c_str(), &volumeName[0], volumeNameSize, nullptr, nullptr, nullptr, nullptr, 0)) {
        volumeName.resize(wcslen(volumeName.c_str()));
        return volumeName;
    }
    return std::nullopt;
}

bool FileSystemService::HaveChildren(const std::wstring& folderPath, bool useFullTree, bool showHidden)
{
    std::wstring searchPath = folderPath;
    if (searchPath.empty()) return false;
    if (searchPath.back() != L'\\') {
        searchPath.append(L"\\");
    }
    searchPath.append(L"*");

    WIN32_FIND_DATA findData{};
    HANDLE hFind = ::FindFirstFile(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool hasChildren = false;
    do {
        bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        bool isDot = (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0);

        if (isDot || (isHidden && !showHidden) || findData.cFileName[0] == L'?') {
            continue;
        }

        if (isDirectory || useFullTree) {
            hasChildren = true;
            break;
        }
    } while (::FindNextFile(hFind, &findData));

    ::FindClose(hFind);
    return hasChildren;
}

std::vector<FileSystemEntry> FileSystemService::GetDirectoryEntries(const std::wstring& path, bool showHidden, bool includeParent)
{
    std::vector<FileSystemEntry> entries;
    std::wstring findPath = path;
    if (findPath.empty()) return entries;
    if (findPath.back() != L'\\') {
        findPath.push_back(L'\\');
    }
    findPath.push_back(L'*');

    WIN32_FIND_DATA findData{};
    HANDLE hFind = ::FindFirstFile(findPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
            bool isParent = (wcscmp(findData.cFileName, L"..") == 0);
            bool isCurrent = (wcscmp(findData.cFileName, L".") == 0);

            if (isCurrent || (isHidden && !showHidden) || findData.cFileName[0] == L'?') {
                continue;
            }

            if (isParent && !includeParent) {
                continue;
            }

            unsigned __int64 fileSize = (static_cast<unsigned __int64>(findData.nFileSizeHigh) << 32) + findData.nFileSizeLow;
            entries.emplace_back(findData.cFileName, findData.dwFileAttributes, fileSize, findData.ftLastWriteTime, isParent);
        } while (::FindNextFile(hFind, &findData));
        ::FindClose(hFind);
    }
    return entries;
}

HRESULT FileSystemService::CreateNewFile(const std::wstring& filePath)
{
    HANDLE hFile = ::CreateFile(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return HRESULT_FROM_WIN32(::GetLastError());
    }
    ::CloseHandle(hFile);
    return S_OK;
}

bool FileSystemService::CreateNewDirectory(const std::wstring& directoryPath)
{
    return ::CreateDirectory(directoryPath.c_str(), nullptr) != FALSE;
}

int FileSystemService::DeleteFiles(HWND hWnd, const std::vector<std::wstring>& paths, bool immediate)
{
    std::wstring from = ToDoubleNullTerminatedString(paths);
    SHFILEOPSTRUCT fileOp = {
        .hwnd = hWnd,
        .wFunc = FO_DELETE,
        .pFrom = from.c_str(),
        .pTo = nullptr,
        .fFlags = static_cast<FILEOP_FLAGS>(immediate ? 0 : FOF_ALLOWUNDO),
    };
    return ::SHFileOperation(&fileOp);
}

int FileSystemService::CopyFiles(HWND hWnd, const std::vector<std::wstring>& fromPaths, const std::wstring& toPath)
{
    std::wstring from = ToDoubleNullTerminatedString(fromPaths);
    std::wstring to = toPath;
    to.push_back(L'\0'); // double null termination for SHFileOperation

    SHFILEOPSTRUCT fileOp = {
        .hwnd = hWnd,
        .wFunc = FO_COPY,
        .pFrom = from.c_str(),
        .pTo = to.c_str(),
        .fFlags = FOF_RENAMEONCOLLISION,
    };
    return ::SHFileOperation(&fileOp);
}

int FileSystemService::MoveFiles(HWND hWnd, const std::vector<std::wstring>& fromPaths, const std::wstring& toPath)
{
    std::wstring from = ToDoubleNullTerminatedString(fromPaths);
    std::wstring to = toPath;
    to.push_back(L'\0'); // double null termination for SHFileOperation

    SHFILEOPSTRUCT fileOp = {
        .hwnd = hWnd,
        .wFunc = FO_MOVE,
        .pFrom = from.c_str(),
        .pTo = to.c_str(),
        .fFlags = FOF_RENAMEONCOLLISION,
    };
    return ::SHFileOperation(&fileOp);
}

BOOL FileSystemService::ConvertNetPathName(const std::wstring& pathName, std::wstring& remotePath)
{
    DWORD driveList = ::GetLogicalDrives();
    WCHAR volumeName[3] = L" :";
    WCHAR remoteName[MAX_PATH];

    for (INT i = 0; i < 26; ++i) {
        if (0x01 & (driveList >> i)) {
            volumeName[0] = static_cast<wchar_t>(L'A' + i);

            DWORD dwRemoteLength = MAX_PATH;
            if (NO_ERROR == WNetGetConnection(volumeName, remoteName, &dwRemoteLength)) {
                if (pathName.find(remoteName) == 0) {
                    remotePath = volumeName;
                    std::wstring subPath = pathName.substr(wcslen(remoteName));
                    if (!subPath.empty() && subPath.front() != L'\\') {
                        remotePath += L"\\";
                    }
                    remotePath += subPath;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

HRESULT FileSystemService::ResolveShortCut(const std::wstring& shortcutPath, std::wstring& resolvedPath)
{
    Microsoft::WRL::ComPtr<IShellLink> shellLink;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IPersistFile> persistFile;
    hr = shellLink.As(&persistFile);
    if (FAILED(hr)) return hr;

    hr = persistFile->Load(shortcutPath.c_str(), STGM_READ);
    if (FAILED(hr)) return hr;

    hr = shellLink->Resolve(nullptr, SLR_UPDATE);
    if (FAILED(hr)) return hr;

    WCHAR szResolvedPath[MAX_PATH];
    hr = shellLink->GetPath(szResolvedPath, MAX_PATH, nullptr, SLGP_RAWPATH);
    if (FAILED(hr)) return hr;

    resolvedPath = szResolvedPath;
    if (::PathIsDirectory(szResolvedPath) && resolvedPath.back() != L'\\') {
        resolvedPath.push_back(L'\\');
    }

    return S_OK;
}

std::wstring FileSystemService::ToDoubleNullTerminatedString(const std::vector<std::wstring>& paths)
{
    std::wstring result;
    for (const auto& path : paths) {
        result += path;
        result.push_back(L'\0');
    }
    result.push_back(L'\0');
    return result;
}
