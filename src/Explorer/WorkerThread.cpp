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
#include <objbase.h>

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
        if (task->GetPriority() == TaskPriority::High) {
            _highPriorityQueue.push_back(std::move(task));
        } else {
            _lowPriorityQueue.push_back(std::move(task));
        }
    }
    _taskQueueCv.notify_one();
}

void WorkerThread::ClearPendingTasks(std::optional<TaskCategory> category) {
    std::unique_lock<std::mutex> lock(_taskQueueMutex);
    if (!category.has_value()) {
        // No category filter: clear all tasks
        _highPriorityQueue.clear();
        _lowPriorityQueue.clear();
    } else {
        // Category filter: remove only tasks matching the given category
        auto removeMatching = [&](std::deque<std::unique_ptr<IAsyncTask>>& queue) {
            queue.erase(
                std::remove_if(queue.begin(), queue.end(), [&](const std::unique_ptr<IAsyncTask>& t) {
                    return t->GetCategory() == *category;
                }),
                queue.end()
            );
        };
        removeMatching(_highPriorityQueue);
        removeMatching(_lowPriorityQueue);
    }
}

void WorkerThread::Run() {
    // Use MTA (Multi-Threaded Apartment) for background worker thread.
    // STA requires a message loop which this thread does not run, and could
    // cause deadlocks when shell extensions internally use COM marshalling.
    ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (_running) {
        std::unique_ptr<IAsyncTask> task;
        {
            std::unique_lock<std::mutex> lock(_taskQueueMutex);
            _taskQueueCv.wait(lock, [this] { 
                return !_highPriorityQueue.empty() || !_lowPriorityQueue.empty() || !_running; 
            });
            if (!_running) {
                break;
            }
            
            if (!_highPriorityQueue.empty()) {
                task = std::move(_highPriorityQueue.front());
                _highPriorityQueue.pop_front();
            } else if (!_lowPriorityQueue.empty()) {
                task = std::move(_lowPriorityQueue.front());
                _lowPriorityQueue.pop_front();
            }
        }

        if (task) {
            task->Execute();

            if (_callback) {
                _callback->OnAsyncTaskCompleted(std::move(task));
            }
        }
    }

    ::CoUninitialize();
}

