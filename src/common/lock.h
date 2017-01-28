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

#include "filesystem.h"

#include <boost/interprocess/sync/named_mutex.hpp>

#include <memory>
#include <mutex>
#include <shared_mutex>

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

using shared_mutex = std::shared_timed_mutex;

path get_lock(const path &fn);

class ScopedFileLock
{
public:
    ScopedFileLock(const path &fn);
    ScopedFileLock(const path &fn, std::defer_lock_t);
    ~ScopedFileLock();

    bool try_lock();
    bool is_locked() const { return locked; }
    void lock();

private:
    FileLockPtr lock_;
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
