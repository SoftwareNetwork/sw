// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "sw_context.h"

#include "remote.h"
#include "settings.h"
#include "storage.h"
#include "storage_remote.h"

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "icontext");

namespace sw
{

SwManagerContext::SwManagerContext(const path &local_storage_root_dir, bool allow_network)
    : cache_storage(std::make_unique<CachedStorage>())
    , cr(std::make_unique<CachingResolver>(*cache_storage))
{
    local_storage = std::make_unique<LocalStorage>(local_storage_root_dir);

    for (auto &r : Settings::get_user_settings().getRemotes(allow_network))
    {
        if (r->isDisabled())
            continue;
        addStorage(
            std::make_unique<RemoteStorageWithFallbackToRemoteResolving>(
                getLocalStorage(), *r, allow_network));
    }

    cr->addStorage(*local_storage); // provides faster resolving (smaller set of packages)?
    for (auto &&s : remote_storages)
        cr->addStorage(*s);
}

SwManagerContext::~SwManagerContext() = default;

void SwManagerContext::addStorage(std::unique_ptr<IStorage> s)
{
    remote_storages.emplace_back(std::move(s));
}

CachedStorage &SwManagerContext::getCachedStorage() const
{
    return *cache_storage;
}

LocalStorage &SwManagerContext::getLocalStorage()
{
    return *local_storage;
}

const LocalStorage &SwManagerContext::getLocalStorage() const
{
    return *local_storage;
}

std::vector<IStorage *> SwManagerContext::getRemoteStorages() const
{
    std::vector<IStorage *> r;
    for (auto &&s : remote_storages)
        r.push_back(s.get());
    return r;
}

bool SwManagerContext::resolve(ResolveRequest &rr, bool use_cache) const
{
    return use_cache
        ? cr->resolve(rr)
        : cr->Resolver::resolve(rr)
        ;
}

void SwManagerContext::install(ResolveRequest &rr) const
{
    // true for now
    if (!rr.isResolved() && !resolve(rr, true))
        throw SW_RUNTIME_ERROR("Not resolved: " + rr.u.toString());
    auto lp = install(rr.getPackage());
    rr.r = lp.clone(); // force overwrite with local package
}

void SwManagerContext::install(std::vector<ResolveRequest> &rrs) const
{
    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &rr : rrs)
    {
        fs.push_back(e.push([this, &rr]
        {
            //if (!rr.hasTarget())
            install(rr);
        }));
    }
    waitAndGet(fs);
}

LocalPackage SwManagerContext::install(const Package &p) const
{
    return getLocalStorage().install(p);
}

void SwManagerContext::setCachedPackages(const std::unordered_map<UnresolvedPackage, PackageId> &pkgs) const
{
    SW_UNIMPLEMENTED;
    /*ResolveResult pkgs2;
    for (auto &[u, p] : pkgs)
        pkgs2.emplace(u, std::make_unique<LocalPackage>(getLocalStorage(), p));
    getCachedStorage().storePackages(pkgs2);*/
}

}

