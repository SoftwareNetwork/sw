// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "directories.h"

#include "settings.h"

#include <boost/algorithm/string.hpp>

namespace sw
{

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

#ifdef _WIN32
    storage_dir = boost::replace_all_copy(ap.string(), "/", "\\");
#else
    storage_dir = ap.string();
#endif

#define SET(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x)

    SET(bin);
    SET(cfg);
    SET(etc);
    //SET(exp);
    SET(lib);
#ifdef _WIN32
    SET(lnk);
#endif
    //SET(obj);
    SET(pkg);
    //SET(src);
    SET(tmp);
    //SET(usr);
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

/*path Directories::get_include_dir() const
{
    return storage_dir_usr / "include";
}

path Directories::get_local_dir() const
{
    return storage_dir_usr / "local";
}*/

path Directories::get_static_files_dir() const
{
    return storage_dir_etc / "static";
}

Directories &getDirectoriesUnsafe()
{
    static Directories directories;
    return directories;
}

const Directories &getDirectories()
{
    return getDirectoriesUnsafe();
}

const Directories &getUserDirectories()
{
    static Directories directories;
    if (directories.empty())
        directories.set_storage_dir(Settings::get_user_settings().storage_dir);
    return directories;
}

}
