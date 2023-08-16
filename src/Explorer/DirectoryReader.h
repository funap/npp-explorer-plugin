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
#pragma once

#include <filesystem>
#include <thread>
#include <functional>

class DirectoryReader
{
public:
    DirectoryReader();
    ~DirectoryReader();
    using ReadDirCallback = std::function<void(const std::filesystem::path& path)>;
    using ReadDirFinCallback = std::function<void()>;
    void ReadDir(const std::filesystem::path& rootPath, ReadDirCallback readDirCallback, ReadDirFinCallback readDirFinCallback);
    void Cancel();
    const std::filesystem::path& GetRootPath() const;
    bool IsReading() const;
private:
    bool                    _needsStop;
    bool                    _reading;
    std::filesystem::path   _rootPath;
    std::thread             _workerThread;
    ReadDirCallback         _readDirCallback;
    ReadDirFinCallback      _readDirFinCallback;

    bool ReadDirRecursive(const std::filesystem::path& path);
};
