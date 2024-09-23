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

#include "WorkerThread.h"

WorkerThread::WorkerThread()
{
}

WorkerThread::~WorkerThread()
{
    Stop();
}

void WorkerThread::Start(IAsyncTaskCallback* callback)
{
    _running = true;
    _callback = callback;
    _thread = std::thread(&WorkerThread::Run, this);
}

void WorkerThread::Stop()
{
    _running = false;
    _taskQueueCv.notify_one();

    if (_thread.joinable()) {
        _thread.join();
    }
}

void WorkerThread::Enqueue(std::unique_ptr<IAsyncTask> task) {
    {
        std::unique_lock<std::mutex> lock(_taskQueueMutex);
        _taskQueue.push(std::move(task));
    }
    _taskQueueCv.notify_one();
}

void WorkerThread::Run() {
    while (_running) {
        std::unique_ptr<IAsyncTask> task;
        {
            std::unique_lock<std::mutex> lock(_taskQueueMutex);
            _taskQueueCv.wait(lock, [this] { return !_taskQueue.empty() || !_running; });
            if (!_running) {
                break;
            }
            task = std::move(_taskQueue.front());
            _taskQueue.pop();
        }

        task->Execute();

        if (_callback) {
            _callback->OnAsyncTaskCompleted(std::move(task));
        }
    }
}
