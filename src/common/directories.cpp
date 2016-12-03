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

#include "directories.h"

#include "cppan_string.h"
#include "config.h"

Directories directories;

void Directories::set_storage_dir(const path &p)
{
    storage_dir = fs::absolute(p);

#define SET(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x)

    SET(bin);
    SET(cfg);
    SET(etc);
    SET(lib);
    //SET(lnk);
    SET(obj);
    SET(src);
    SET(tmp);
    SET(usr);
#undef SET

}

void Directories::set_build_dir(const path &p)
{
    build_dir = p;
}

void Directories::update(const Directories &dirs, ConfigType t)
{
    if (t > type)
        return;
    auto dirs2 = dirs;
    std::swap(*this, dirs2);
    type = t;
}

path Directories::get_include_dir() const
{
    return storage_dir_usr / "include";
}

path Directories::get_local_dir() const
{
    return storage_dir_usr / "local";
}

path Directories::get_static_files_dir() const
{
    return storage_dir_etc / "static";
}

const Directories &get_user_directories()
{
    static Directories dirs;
    if (dirs.empty())
        dirs.set_storage_dir(Config::get_user_config().settings.storage_dir);
    return dirs;
}
