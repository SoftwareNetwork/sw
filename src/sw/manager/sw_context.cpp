// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "resolver.h"  // remove when vs bug is fixed
#include "storage.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "icontext");

namespace sw
{

SwManagerContext::SwManagerContext(const path &local_storage_root_dir)
{
    //store = std::make_unique<PackageStore>(*this);

    auto p = local_storage_root_dir;
    p += "2";
    storages.emplace_back(std::make_unique<LocalStorage>(p));
    storages.emplace_back(std::make_unique<RemoteStorage>(getLocalStorage(), "software-network", getLocalStorage().getDatabaseRootDir()));
}

SwManagerContext::~SwManagerContext() = default;

LocalStorage &SwManagerContext::getLocalStorage()
{
    return static_cast<LocalStorage&>(*storages[0]);
}

const LocalStorage &SwManagerContext::getLocalStorage() const
{
    return static_cast<const LocalStorage&>(*storages[0]);
}

std::unordered_map<UnresolvedPackage, Package> SwManagerContext::resolve(const UnresolvedPackages &pkgs)
{
    UnresolvedPackages resolved;
    return resolve1(pkgs);
}

std::unordered_map<UnresolvedPackage, Package> SwManagerContext::resolve1(const UnresolvedPackages &pkgs)
{
    if (pkgs.empty())
        return {};

    auto pkgs2 = pkgs;
    std::unordered_map<UnresolvedPackage, Package> resolved;
    for (auto &s : storages)
    {
        UnresolvedPackages unresolved;
        auto rpkgs = s->resolve(pkgs2, unresolved);
        resolved.merge(rpkgs);
        pkgs2 = std::move(unresolved);
    }
    if (!pkgs2.empty())
    {
        String s;
        for (auto &d : pkgs2)
            s += d.toString() + ", ";
        if (!s.empty())
            s.resize(s.size() - 2);
        throw SW_RUNTIME_ERROR("Some packages (" + std::to_string(pkgs2.size()) + ") are unresolved: " + s);
    }

    // resolve children deps
    UnresolvedPackages unresolved;
    for (auto &[u, p] : resolved)
    {
        auto &deps = p.getData().dependencies;
        unresolved.insert(deps.begin(), deps.end());
        resolved_packages.insert(u);
    }
    for (auto &u : resolved_packages)
        unresolved.erase(u);
    resolved.merge(resolve1(unresolved));

    return resolved;
}

}

