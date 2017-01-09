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

#include "directories.h"

#include "cppan_string.h"
#include "config.h"
#include "settings.h"

Directories directories;

void checkPath(const path &p, const String &msg)
{
    const auto s = p.string();
    for (auto &c : s)
    {
        if (isspace(c))
            throw std::runtime_error("You have spaces in the " + msg + " path. CPPAN could not work in this directory: '" + s + "'");
    }
}

void Directories::set_storage_dir(const path &p)
{
    auto ap = fs::absolute(p);
    checkPath(ap, "storage directory");

    storage_dir = ap;

#define SET(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x)

    SET(bin);
    SET(cfg);
    SET(etc);
    SET(exp);
    SET(lib);
#ifdef _WIN32
    SET(lnk);
#endif
    SET(obj);
    SET(src);
    SET(tmp);
    SET(usr);
#undef SET
}

void Directories::set_build_dir(const path &p)
{
    checkPath(p, "build directory");
    build_dir = p;
}

void Directories::update(const Directories &dirs, SettingsType t)
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
        dirs.set_storage_dir(Settings::get_user_settings().storage_dir);
    return dirs;
}
