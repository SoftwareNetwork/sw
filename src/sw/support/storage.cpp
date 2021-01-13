// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "storage.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "storage");

namespace sw
{

static void checkPath(const path &p)
{
    const auto s = p.string();
    for (auto &c : s)
    {
        if (isspace(c))
            throw SW_RUNTIME_ERROR("You have spaces in the storage directory path. SW cannot work in this directory: '" + s + "'");
    }
}

Directories::Directories(const path &p)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("empty path");

    auto make_canonical = [](const path &p)
    {
        auto a = fs::absolute(p);
        if (!fs::exists(a))
            fs::create_directories(a);
        return primitives::filesystem::canonical(a);
    };

    auto ap = make_canonical(p);
    checkPath(ap);

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

int getPackagesDatabaseSchemaVersion()
{
    return 4;
}

String getPackagesDatabaseSchemaVersionFileName()
{
    return "schema.version";
}

String getPackagesDatabaseVersionFileName()
{
    return "db.version";
}

int readPackagesDatabaseVersion(const path &dir)
{
    auto p = dir / getPackagesDatabaseVersionFileName();
    if (!fs::exists(p))
        return 0;
    return std::stoi(read_file(p));
}

}
