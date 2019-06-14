// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target.h"

namespace sw
{

ITarget::~ITarget() = default;

TargetLoader::~TargetLoader() = default;

void TargetData::loadPackages(const TargetSettings &s, const PackageIdSet &whitelist)
{
    if (!ep)
        throw SW_RUNTIME_ERROR("No entry point provided");
    ep->loadPackages(s, whitelist);
}

void TargetData::setEntryPoint(const std::shared_ptr<TargetLoader> &e)
{
    ep = std::move(e);
}

TargetData::Base::iterator TargetData::find(const TargetSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return *t == s;
    });
}

TargetData::Base::const_iterator TargetData::find(const TargetSettings &s) const
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return *t == s;
    });
}

detail::SimpleExpected<TargetMap::Base::version_map_type::iterator> TargetMap::find_and_select_version(const PackagePath &pp)
{
    auto i = find(pp);
    if (i == end(pp))
        return PackagePathNotFound;
    auto vo = select_version(i->second);
    if (!vo)
        return PackageNotFound;
    return i->second.find(*vo);
}

detail::SimpleExpected<TargetMap::Base::version_map_type::const_iterator> TargetMap::find_and_select_version(const PackagePath &pp) const
{
    auto i = find(pp);
    if (i == end(pp))
        return PackagePathNotFound;
    auto vo = select_version(i->second);
    if (!vo)
        return PackageNotFound;
    return i->second.find(*vo);
}

detail::SimpleExpected<std::pair<Version, ITarget*>> TargetMap::find(const PackagePath &pp, const TargetSettings &ts) const
{
    auto i = find_and_select_version(pp);
    if (!i)
        return i.ec();
    auto j = i->second.find(ts);
    if (j == i->second.end())
        return std::pair<Version, ITarget*>{ i->first, nullptr };
    return std::pair<Version, ITarget*>{ i->first, j->get() };
}

ITarget *TargetMap::find(const PackageId &pkg, const TargetSettings &ts) const
{
    auto i = find(pkg);
    if (i == end())
        return {};
    auto k = i->second.find(ts);
    if (k == i->second.end())
        return {};
    return k->get();
}

ITarget *TargetMap::find(const UnresolvedPackage &pkg, const TargetSettings &ts) const
{
    // TODO: consider provided resolving into find()
    auto i = find(pkg);
    if (i == end())
        return {};
    auto k = i->second.find(ts);
    if (k == i->second.end())
        return {};
    return k->get();
}

}
