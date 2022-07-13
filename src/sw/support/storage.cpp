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

static path getDatabaseRootDir1(const path &root)
{
    return root / "sw" / "database";
}

path Directories::getDatabaseRootDir() const
{
    static const Strings upgrade_from
    {
        // push new values to front
        // "1"
    };

    auto p = getDatabaseRootDir1(storage_dir_etc) / "1";

    [[maybe_unused]]
    static bool once = [this, &new_root = p]()
    {
        for (auto &u : upgrade_from)
        {
            auto old = getDatabaseRootDir1(storage_dir_etc) / u;
            if (!fs::exists(old))
                continue;
            fs::copy(old, new_root, fs::copy_options::recursive);
            break;
        }
        return true;
    }();

    return p;
}

size_t ResolveResultWithDependencies::getHash(const UnresolvedPackage &u)
{
    auto hi = h.find(u);
    if (hi != h.end())
        return hi->second;
    auto mi = m.find(u);
    if (mi == m.end())
        return h[u] = 0;
    size_t hash = 0;
    for (auto &d : mi->second->getData().dependencies)
    {
        if (u != d)
            hash_combine(hash, getHash(d));
    }
    return hash;
}

ResolveResultWithDependencies IStorage::resolveWithDependencies(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    auto r = resolve(pkgs, unresolved_pkgs);
    while (1)
    {
        std::unordered_map<UnresolvedPackage, Package*> r2;
        for (auto &[u, p] : r)
            r2.emplace(u, p.get());
        auto sz = r.size();
        for (auto &[u, p] : r2)
            r.merge(resolve(p->getData().dependencies, unresolved_pkgs));
        if (r.size() == sz)
            break;
    }
    return ResolveResultWithDependencies{ std::move(r) };
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
