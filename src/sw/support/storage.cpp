// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "storage.h"

//#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "storage");

namespace sw
{

IResolver::~IResolver() = default;

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

void ResolveRequestResult::setPackageForce(PackagePtr in)
{
    r = std::move(in);
}

bool ResolveRequestResult::setPackage(PackagePtr in)
{
    // check in version rr.u.contains(in)?
    // no, we might force set complete different package as resolve result
    // even with other package path
    // we just select the best version here

    // version acceptance algorithm

    SW_CHECK(in);

    // always accept first package
    if (!r)
    {
        setPackageForce(std::move(in));
        return true;
    }

    // 1. we already have a branch, nothing to do
    // (we can't resolve for more suitable branch)
    // 2. we already have a version, nothing to do
    // (version is more preferred than branch)
    if (in->getVersion().isBranch())
        return false;

    // always prefer releases over pre-releases
    if (r->getVersion().isPreRelease() && in->getVersion().isRelease())
    {
        setPackageForce(std::move(in));
        return true;
    }

    // do not accept any pre-release over release
    if (r->getVersion().isRelease() && in->getVersion().isPreRelease())
        return false;

    // now simple less than check
    if (r->getVersion() < in->getVersion())
    {
        setPackageForce(std::move(in));
        return true;
    }
    return false;
}

bool ResolveRequest::setPackage(PackagePtr in)
{
    SW_CHECK(in);

    if (!u.getRange().contains(in->getVersion()))
        return false;
    return ResolveRequestResult::setPackage(std::move(in));
}

bool Resolver::resolve(ResolveRequest &rr) const
{
    // select the best candidate from all storages
    for (auto &&s : storages)
    {
        if (!s->resolve(rr))
            continue;

        if (0
            // when we found a branch, we stop, because following storages cannot give us more preferable branch
            || rr.getPackage().getVersion().isBranch()
            )
        {
            break;
        }
    }
    return rr.isResolved();
}

void Resolver::addStorage(IResolver &s)
{
    storages.push_back(&s);
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
