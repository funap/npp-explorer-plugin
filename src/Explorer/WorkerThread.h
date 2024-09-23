// The MIT License (MIT)
//
// Copyright (c) 2024 funap
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

#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class IAsyncTask {
public:
    virtual ~IAsyncTask() {}
    virtual void Execute() = 0;
    virtual void OnCompleted() = 0;
};

class IAsyncTaskCallback {
public:
    virtual ~IAsyncTaskCallback() {}
    virtual void OnAsyncTaskCompleted(std::unique_ptr<IAsyncTask> task) = 0;
};


class WorkerThread {
public:
    WorkerThread();
    ~WorkerThread();
    
    void Start(IAsyncTaskCallback* callback);
    void Stop();
    void Enqueue(std::unique_ptr<IAsyncTask> task);

private:
    std::queue<std::unique_ptr<IAsyncTask>> _taskQueue;
    std::mutex              _taskQueueMutex;
    std::condition_variable _taskQueueCv;
    std::thread             _thread;
    IAsyncTaskCallback*     _callback{nullptr};
    bool                    _running{false};

    void Run();
};
