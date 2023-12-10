// The MIT License (MIT)
//
// Copyright (c) 2023 funap
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include "FileSystemWatcher.h"

#include <Windows.h>
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>

namespace {
constexpr DWORD operator "" _KB(unsigned long long value) {
    return static_cast<DWORD>(value * 1024);
}
}

FileSystemWatcher::FileSystemWatcher()
    : m_stop(false)
{
}

FileSystemWatcher::~FileSystemWatcher()
{
    Stop();
}
void FileSystemWatcher::Reset(const std::wstring& directory)
{
    Stop();
    m_stop = false;
    m_thread = std::thread(&FileSystemWatcher::Run, this, directory);
}

void FileSystemWatcher::Stop()
{
    m_stop = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void FileSystemWatcher::Created(CreatedCallback callback)
{
    m_createdCallback = callback;
}

void FileSystemWatcher::Deleted(DeletedCallback callback)
{
    m_deletedCallback = callback;
}

void FileSystemWatcher::Renamed(RenamedCallback callback)
{
    m_renamedCallback = callback;
}

void FileSystemWatcher::Run(std::wstring directory)
{
    HANDLE hDir = ::CreateFileW(
        directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        return;
    }

    constexpr DWORD NOTIFY_FILTER   = FILE_NOTIFY_CHANGE_FILE_NAME
                                    | FILE_NOTIFY_CHANGE_DIR_NAME
                                    | FILE_NOTIFY_CHANGE_CREATION;

    DWORD bufferSize = 16_KB;
    auto buffer = std::make_unique<BYTE[]>(bufferSize);

    HANDLE hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (nullptr == hEvent) {
        return;
    }

    while (!m_stop) {
        ResetEvent(hEvent);
        OVERLAPPED olp{
            .hEvent = hEvent
        };

        DWORD bytesReturned = 0;
        BOOL result = ReadDirectoryChangesW(
            hDir,           // Directory
            buffer.get(),   // Buffer
            bufferSize,     // Buffer Length
            TRUE,           // Watch Subtree
            NOTIFY_FILTER,  // Notify Filter
            nullptr,        // Bytes Returned
            &olp,           // Overlapped
            nullptr         // Completion Routine
        );

        if (!result) {
            DWORD lastError = GetLastError();
            if (lastError == ERROR_INSUFFICIENT_BUFFER) {
                bufferSize *= 2;
                buffer = std::make_unique<BYTE[]>(bufferSize);
                continue;
            }
            break;
        }

        while (!m_stop) {
            DWORD waitResult = WaitForSingleObject(hEvent, 500);
            if (waitResult != WAIT_TIMEOUT) {
                break;
            }
        }

        if (m_stop) {
            CancelIo(hDir);
            WaitForSingleObject(hEvent, INFINITE);
            break;
        }

        // get the results of asynchronous I/O
        DWORD retsize = 0;
        if (!GetOverlappedResult(hDir, &olp, &retsize, FALSE)) {
            // failed to get result
            break;
        }

        if (retsize == 0) {
            // buffer over, retry.
            continue;
        }

        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.get());
        std::wstring oldName;
        while (true) {
            std::wstring filename(info->FileName, info->FileNameLength / sizeof(WCHAR));
            if (info->Action == FILE_ACTION_ADDED) {
                if (m_createdCallback) {
                    m_createdCallback(directory + filename);
                }
            }
            else if (info->Action == FILE_ACTION_REMOVED) {
                if (m_deletedCallback) {
                    m_deletedCallback(directory + filename);
                }
            }
            else if (info->Action == FILE_ACTION_RENAMED_OLD_NAME) {
                oldName = directory + filename;
            }
            else if (info->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                if (!oldName.empty()) {
                    std::wstring newName = directory + filename;
                    if (m_renamedCallback) {
                        m_renamedCallback(oldName, newName);
                    }
                    oldName.clear();
                }
            }
            else {

            }

            if (info->NextEntryOffset == 0) {
                break;
            }

            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
        }
    }

    CloseHandle(hEvent);
    CloseHandle(hDir);
}
