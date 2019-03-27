// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "storage.h"

#include "settings.h"

#include <primitives/sw/cl.h>

#include <boost/algorithm/string.hpp>

static cl::opt<path> storage_dir_override("storage-dir");

namespace sw
{

static void checkPath(const path &p, const String &msg)
{
    const auto s = p.string();
    for (auto &c : s)
    {
        if (isspace(c))
            throw SW_RUNTIME_ERROR("You have spaces in the " + msg + " path. SW could not work in this directory: '" + s + "'");
    }
}

void Storage::set_storage_dir(const path &p)
{
    auto make_canonical = [](const path &p)
    {
        auto a = fs::absolute(p);
        if (!fs::exists(a))
            fs::create_directories(a);
        return fs::canonical(a);
    };

    path ap;
    if (storage_dir_override.empty())
        ap = make_canonical(p);
    else
        ap = make_canonical(storage_dir_override);
    checkPath(ap, "storage directory");

#ifdef _WIN32
    storage_dir = normalize_path_windows(ap);
#else
    storage_dir = ap.string();
#endif

#define DIR(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x);
#include "storage_directories.inl"
#undef SET
}

void Storage::set_build_dir(const path &p)
{
    checkPath(p, "build directory");
    build_dir = p;
}

void Storage::update(const Storage &dirs, SettingsType t)
{
    if (t > type)
        return;
    auto dirs2 = dirs;
    std::swap(*this, dirs2);
    type = t;
}

path Storage::get_static_files_dir() const
{
    return storage_dir_etc / "static";
}

Storage &getStorageUnsafe()
{
    static Storage directories;
    return directories;
}

const Storage &getStorage()
{
    auto &directories = getStorageUnsafe();
    if (directories.empty())
        directories.set_storage_dir(Settings::get_user_settings().storage_dir);
    return directories;
}

}
