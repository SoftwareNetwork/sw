// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "resolver.h"

#include "package.h"

namespace sw
{

IResolver::~IResolver() = default;

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

}
