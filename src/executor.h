/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
    };

    using Threads = std::vector<ThreadData>;

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
    size_t nThreads = 5;
    std::atomic_size_t index{ 0 };
    bool done = false;

    void run(size_t i);
};
