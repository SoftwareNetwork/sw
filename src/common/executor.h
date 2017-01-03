/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using Task = std::function<void()>;

class TaskQueue
{
    using Tasks = std::deque<Task>;
    using Lock = std::unique_lock<std::mutex>;

public:
    TaskQueue() {}
    TaskQueue(TaskQueue &&rhs)
    {
        Lock lock(rhs.m);
        q = std::move(rhs.q);
    }

    bool try_push(Task &t)
    {
        {
            Lock lock(m, std::try_to_lock);
            if (!lock)
                return false;
            q.emplace_back(std::move(t));
        }
        cv.notify_one();
        return true;
    }

    bool try_pop(Task &t)
    {
        Lock lock(m, std::try_to_lock);
        if (!lock || q.empty() || done_)
            return false;
        t = std::move(q.front());
        q.pop_front();
        return true;
    }

    void push(Task &t)
    {
        {
            Lock lock(m);
            q.emplace_back(std::move(t));
        }
        cv.notify_one();
    }

    bool pop(Task &t)
    {
        Lock lock(m);
        while (q.empty() && !done_)
            cv.wait(lock);
        if (q.empty() || done_)
            return false;
        t = std::move(q.front());
        q.pop_front();
        return true;
    }

    void done()
    {
        {
            Lock lock(m);
            done_ = true;
        }
        cv.notify_all();
    }

    bool empty() const
    {
        return q.empty();
    }

private:
    Tasks q;
    std::mutex m;
    std::condition_variable cv;
    bool done_ = false;
};

class Executor
{
    struct ThreadData
    {
        std::thread t;
        TaskQueue q;
        volatile bool busy = false;
        std::exception_ptr eptr;
    };

    using Threads = std::vector<ThreadData>;

public:
    bool throw_exceptions = false;

public:
    Executor(size_t nThreads = std::thread::hardware_concurrency());
    ~Executor();

    size_t numberOfThreads() const { return nThreads; }

    template <class T>
    void push(T &&t)
    {
        static_assert(!std::is_lvalue_reference<T>::value, "Argument cannot be an lvalue");

        Task task([t = std::move(t)]{ t(); });
        auto i = index++;
        for (auto n = 0; n != nThreads; ++n)
        {
            if (thread_pool[(i + n) % nThreads].q.try_push(task))
                return;
        }
        thread_pool[i % nThreads].q.push(task);
    }

    void stop();
    void wait();

private:
    Threads thread_pool;
    size_t nThreads;
    std::atomic_size_t index{ 0 };
    bool done = false;

    void run(size_t i);
};

size_t get_max_threads(size_t N = 4);
