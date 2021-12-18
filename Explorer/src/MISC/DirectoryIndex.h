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

#include <vector>
#include <filesystem>
#include <thread>

class DirectoryIndexListener
{
public:
	virtual void onIndexBuildCompleted() const = 0;
	virtual void onIndexBuildCanceled() const = 0;
};

class DirectoryIndex
{
public:
	DirectoryIndex();
	~DirectoryIndex();
	void setListener(const DirectoryIndexListener* listener);
	void init(const std::filesystem::path& basePath);
	void build();
	bool buildTaskRecursive(const std::filesystem::path& path);
	void cancel();

	const std::vector<std::filesystem::path>& GetFileIndex();
	const std::filesystem::path& GetCurrentDir();
	bool isIndexing();


private:
	bool								_needsStop;
	bool								_indexed;
	const DirectoryIndexListener*		_listener;
	std::vector<std::filesystem::path>	_fileIndex;
	std::filesystem::path				_currentDirectory;
	std::thread							_workerThread;
};
