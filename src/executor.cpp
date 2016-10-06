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

#include "executor.h"

#include <string>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "executor");

Executor::Executor(size_t nThreads)
    : nThreads(nThreads)
{
    taskQueues.resize(nThreads);
    for (size_t i = 0; i < nThreads; i++)
        thread_pool.emplace_back([this, i] { run(i); });
}

Executor::~Executor()
{
    stop();
    for (auto &t : thread_pool)
        t.join();
}

void Executor::run(size_t i)
{
    while (!done)
    {
        std::string error;
        try
        {
                Task t;
                const size_t spin_count = nThreads * 4;
                for (auto n = 0; n != spin_count; ++n)
                {
                    if (taskQueues[(i + n) % nThreads].try_pop(t))
                        break;
                }
                // no task popped, probably shutdown command was issues
                if (!t && !taskQueues[i].pop(t))
                    break;
                t();
        }
        catch (const std::exception &e)
        {
            error = e.what();
        }
        catch (...)
        {
            error = "unknown exception";
        }
        if (!error.empty())
        {
            LOG_ERROR(logger, "executor: " << this << ", thread #" << i + 1 << ", error: " << error);
        }
    }
}

Executor &getTaskExecutor(Executor *e)
{
    static Executor *executor;
    if (e)
        executor = e;
    return *executor;
}

Executor &getMailExecutor()
{
    static Executor executor;
    return executor;
}
