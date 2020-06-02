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
{
    // first goes resolve cache
    cache_storage_id = storages.size();
    storages.emplace_back(std::make_unique<CachedStorage>());

    local_storage_id = storages.size();
    storages.emplace_back(std::make_unique<LocalStorage>(local_storage_root_dir));

    first_remote_storage_id = storages.size();
    for (auto &r : Settings::get_user_settings().getRemotes(allow_network))
    {
        if (r->isDisabled())
            continue;
        storages.emplace_back(
            std::make_unique<RemoteStorageWithFallbackToRemoteResolving>(
                getLocalStorage(), *r, allow_network));
    }
}

SwManagerContext::~SwManagerContext() = default;

CachedStorage &SwManagerContext::getCachedStorage() const
{
    return dynamic_cast<CachedStorage&>(*storages[cache_storage_id]);
}

LocalStorage &SwManagerContext::getLocalStorage()
{
    return static_cast<LocalStorage&>(*storages[local_storage_id]);
}

const LocalStorage &SwManagerContext::getLocalStorage() const
{
    return static_cast<const LocalStorage&>(*storages[local_storage_id]);
}

std::vector<IStorage *> SwManagerContext::getRemoteStorages() const
{
    std::vector<IStorage *> r;
    for (int i = first_remote_storage_id; i < storages.size(); i++)
        r.push_back(storages[i].get());
    return r;
}

ResolveResultWithDependencies SwManagerContext::resolve(const UnresolvedPackages &in_pkgs, bool use_cache) const
{
    if (in_pkgs.empty())
        return {};

    std::vector<IStorage *> s2;
    for (const auto &[i, s] : enumerate(storages))
    {
        if (i != cache_storage_id || use_cache)
            s2.push_back(s.get());
    }
    return resolve(in_pkgs, s2);
}

ResolveResultWithDependencies SwManagerContext::resolve(const UnresolvedPackages &in_pkgs, const std::vector<IStorage*> &storages) const
{
    std::lock_guard lk(resolve_mutex);

    ResolveResultWithDependencies resolved;
    auto upkgs = in_pkgs;
    while (1)
    {
        ResolveResultWithDependencies resolved_step;
        for (auto &p : upkgs)
        {
            if (resolved.find(p) != resolved.end())
                continue;

            // select the best candidate from all storages first
            // (later we'll have security selector also - what signature matches)

            PackagePtr pkg;
            for (const auto &[i, s] : enumerate(storages))
            {
                UnresolvedPackages unresolved;
                auto r = s->resolve({ p }, unresolved);
                if (r.empty())
                    continue; // not found in this storage
                if (p.getRange().isBranch())
                {
                    // when we found a branch, we stop, because following storages cannot give us more preferable branch
                    // TODO: change this when security is on
                    // (following storages cold give us suitable (signed) branch)
                    pkg = std::move(r.begin()->second);
                    break;
                }
                if (!pkg || r.begin()->second->getVersion() > pkg->getVersion())
                {
                    pkg = std::move(r.begin()->second);
                }
                if (pkg && i == cache_storage_id)
                {
                    // cache hit, we stop immediately
                    break;
                }
            }
            if (!pkg)
                throw SW_RUNTIME_ERROR("Package '" + p.toString() + "' is not resolved");

            resolved_step[p] = std::move(pkg);
        }

        if (resolved_step.empty())
            break;

        // gather deps
        upkgs.clear(); // clear current unresolved pkgs
        for (auto &[u, p] : resolved_step)
            upkgs.insert(p->getData().dependencies.begin(), p->getData().dependencies.end());

        resolved.merge(resolved_step);
    }

    // save existing results
    getCachedStorage().storePackages(resolved.m);

    return resolved;
}

std::unordered_map<UnresolvedPackage, LocalPackage> SwManagerContext::install(const UnresolvedPackages &pkgs, bool use_cache) const
{
    auto m = resolve(pkgs, use_cache);

    // two unresolved pkgs may point to single pkg,
    // so make pkgs unique
    std::unordered_map<PackageId, Package*> pkgs2;
    for (auto &[u, p] : m)
        pkgs2.emplace(*p, p.get());

    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &p : pkgs2)
    {
        fs.push_back(e.push([this, &p] { install(*p.second); }));
    }
    waitAndGet(fs);

    // install should be fast enough here
    std::unordered_map<UnresolvedPackage, LocalPackage> pkgs3;
    for (auto &[u, p] : m)
        pkgs3.emplace(u, install(*p));

    return pkgs3;
}

LocalPackage SwManagerContext::install(const Package &p) const
{
    return getLocalStorage().install(p);
}

LocalPackage SwManagerContext::resolve(const UnresolvedPackage &pkg) const
{
    return install(*resolve(UnresolvedPackages{ pkg }).find(pkg)->second);
}

void SwManagerContext::setCachedPackages(const std::unordered_map<UnresolvedPackage, PackageId> &pkgs) const
{
    auto &s = getCachedStorage();
    ResolveResult pkgs2;
    for (auto &[u, p] : pkgs)
        pkgs2.emplace(u, std::make_unique<LocalPackage>(getLocalStorage(), p));
    s.storePackages(pkgs2);
}

}

