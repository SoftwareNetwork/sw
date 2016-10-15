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

#include "filesystem.h"

#include <boost/interprocess/sync/named_mutex.hpp>

#include <memory>
#include <mutex>

#define CPPAN_INTERPROCESS_MUTEX(m) ("cppan." m ".m")
#define CPPAN_STATIC_INTERPROCESS_MUTEX(m) \
    static InterprocessMutex cppan_mutex(Interprocess::open_or_create, CPPAN_INTERPROCESS_MUTEX(m)); \
    return cppan_mutex

namespace boost
{
    namespace interprocess
    {
        class file_lock;
    }
}

namespace Interprocess = boost::interprocess;
using FileLock = Interprocess::file_lock;
using FileLockPtr = std::unique_ptr<FileLock>;

using InterprocessMutex = Interprocess::named_mutex;

class ScopedFileLock
{
public:
    ScopedFileLock(const path &fn);
    ScopedFileLock(const path &fn, std::defer_lock_t);
    ~ScopedFileLock();

    bool try_lock();
    bool is_locked() const { return locked; }

private:
    FileLockPtr lock;
    bool locked = false;
};

class ScopedShareableFileLock
{
public:
    ScopedShareableFileLock(const path &fn);
    ~ScopedShareableFileLock();

private:
    FileLockPtr lock;
};
