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

#include "lock.h"

#include "directories.h"

#include <boost/interprocess/sync/file_lock.hpp>

#include <iostream>

std::string prepare_lock_file(const path &fn)
{
    fs::create_directories(fn.parent_path());
    auto lock_file = fn.parent_path() / (fn.filename().string() + ".lock");
    if (!fs::exists(lock_file))
        std::ofstream(lock_file.string());
    return lock_file.string();
}

path get_lock(const path &fn)
{
    return directories.storage_dir_etc / "locks" / fn.filename();
}

////////////////////////////////////////

ScopedFileLock::ScopedFileLock(const path &fn)
{
    lock_ = std::make_unique<FileLock>(prepare_lock_file(fn).c_str());
    lock_->lock();
    locked = true;
}

ScopedFileLock::ScopedFileLock(const path &fn, std::defer_lock_t)
{
    lock_ = std::make_unique<FileLock>(prepare_lock_file(fn).c_str());
}

ScopedFileLock::~ScopedFileLock()
{
    if (locked)
        lock_->unlock();
}

bool ScopedFileLock::try_lock()
{
    return locked = lock_->try_lock();
}

void ScopedFileLock::lock()
{
    lock_->lock();
    locked = true;
}

////////////////////////////////////////

ScopedShareableFileLock::ScopedShareableFileLock(const path &fn)
{
    lock = std::make_unique<FileLock>(prepare_lock_file(fn).c_str());
    lock->lock_sharable();
}

ScopedShareableFileLock::~ScopedShareableFileLock()
{
    lock->unlock_sharable();
}
