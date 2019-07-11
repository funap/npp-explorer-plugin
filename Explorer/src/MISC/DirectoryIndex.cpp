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

#include "DirectoryIndex.h"

DirectoryIndex::DirectoryIndex() :
	_needsStop(false),
	_indexed(false),
	_listener(nullptr),
	_fileIndex(),
	_currentDirectory(),
	_workerThread()
{

}

DirectoryIndex::~DirectoryIndex()
{
	cancel();
}

void DirectoryIndex::setListener(const DirectoryIndexListener* listener)
{
	_listener = listener;
}

void DirectoryIndex::init(const std::filesystem::path& basePath)
{
	_currentDirectory = basePath;
	_fileIndex.clear();
}

void DirectoryIndex::build()
{
	if (_workerThread.joinable()) {
		// already building, do nothing
	}
	else {
		_needsStop = false;
		_indexed = false;

		_workerThread = std::thread([](DirectoryIndex *arg) {
			bool complete = arg->buildTaskRecursive(arg->_currentDirectory);
			arg->_indexed = true;
			if (arg->_listener) {
				if (complete) {
					arg->_listener->onIndexBuildCompleted();
				}
				else {
					arg->_listener->onIndexBuildCanceled();
				}
			}

		}, this);
	}

}

bool DirectoryIndex::buildTaskRecursive(const std::filesystem::path& path)
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
				else {
					buildTaskRecursive(it->path());
				}
			}
			else if (it->is_regular_file()) {
				_fileIndex.emplace_back(it->path());
			}
		}
		catch (.../* fs::filesystem_error& err*/) {
			//OutputDebugStringA(err.what());
		}
		++it;
	}
	return result;
}

void DirectoryIndex::cancel()
{
	_needsStop = true;
	if (_workerThread.joinable()) {
		_workerThread.join();
	}
}

const std::filesystem::path& DirectoryIndex::GetCurrentDir()
{
	return _currentDirectory;
}


const std::vector<std::filesystem::path>& DirectoryIndex::GetFileIndex()
{
	if (_workerThread.joinable()) {
		_workerThread.join();
	}
	return _fileIndex;
}

bool DirectoryIndex::isIndexing()
{
	return !_indexed;
}