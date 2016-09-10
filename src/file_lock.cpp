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

#include "file_lock.h"

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

////////////////////////////////////////

ScopedFileLock::ScopedFileLock(const path &fn)
{
    lock = std::make_unique<FileLock>(prepare_lock_file(fn).c_str());
    lock->lock();
    locked = true;
}

ScopedFileLock::ScopedFileLock(const path &fn, std::defer_lock_t)
{
    lock = std::make_unique<FileLock>(prepare_lock_file(fn).c_str());
}

ScopedFileLock::~ScopedFileLock()
{
    if (locked)
        lock->unlock();
}

bool ScopedFileLock::try_lock()
{
    return locked = lock->try_lock();
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
