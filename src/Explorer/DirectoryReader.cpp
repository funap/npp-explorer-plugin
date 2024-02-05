/*
  The MIT License (MIT)

  Copyright (c) 2019 funap

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "DirectoryReader.h"

#include <utility>

DirectoryReader::DirectoryReader()
    : _needsStop(false)
    , _reading(false)
{

}

DirectoryReader::~DirectoryReader()
{
    Cancel();
}

void DirectoryReader::ReadDir(const std::filesystem::path& rootPath, ReadDirCallback readDirCallback, ReadDirFinCallback readDirFinCallback)
{
    if (_workerThread.joinable()) {
        Cancel();
    }

    _readDirCallback = std::move(readDirCallback);
    _readDirFinCallback = std::move(readDirFinCallback);
    _rootPath = rootPath;
    _needsStop = false;
    _reading = true;
    _workerThread = std::thread([this](DirectoryReader* self) {
        self->ReadDirRecursive(self->GetRootPath());
        _reading = false;
        _readDirFinCallback();
    }, this);
}

bool DirectoryReader::ReadDirRecursive(const std::filesystem::path& path)
{
    namespace fs = std::filesystem;

    bool result = true;
    fs::directory_iterator it(path, fs::directory_options::skip_permission_denied);
    fs::directory_iterator end;
    while (it != end) {
        if (_needsStop) {
            result = false;
            break;
        }
        try {
            if (it->is_directory()) {
                const std::wstring name = it->path().filename().wstring();
                if ('.' == name[0]) {
                    // skip hidden name directory
                }
                else if ('$' == name[0]) {
                    // skip system directory
                }
                else {
                    ReadDirRecursive(it->path());
                }
            }
            else if (it->is_regular_file()) {
                _readDirCallback(it->path());
            }
        }
        catch (.../* fs::filesystem_error& err*/) {
            //OutputDebugStringA(err.what());
        }
        ++it;
    }

    return result;
}

void DirectoryReader::Cancel()
{
    _needsStop = true;
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
}

const std::filesystem::path& DirectoryReader::GetRootPath() const
{
    return _rootPath;
}

bool DirectoryReader::IsReading() const
{
    return _reading;
}