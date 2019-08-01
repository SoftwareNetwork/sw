// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target.h"

namespace sw
{

ITarget::~ITarget() = default;
IDependency::~IDependency() = default;

TargetEntryPoint::~TargetEntryPoint() = default;

void TargetEntryPoint::addKnownPackage(const PackageId &pkg)
{
    known_packages.insert(pkg);
}

const PackageIdSet &TargetEntryPoint::getKnownPackages() const
{
    return known_packages;
}

TargetData::~TargetData()
{
}

void TargetData::loadPackages(SwBuild &b, const TargetSettings &s, const PackageIdSet &whitelist) const
{
    if (!ep)
        throw SW_RUNTIME_ERROR("No entry point provided");
    ep->loadPackages(b, s, whitelist);
}

TargetEntryPointPtr TargetData::getEntryPoint() const
{
    if (!ep)
        throw SW_RUNTIME_ERROR("No entry point provided");
    return ep;
}

void TargetData::setEntryPoint(const std::shared_ptr<TargetEntryPoint> &e)
{
    if (ep && ep != e)
        throw SW_RUNTIME_ERROR("Setting entry point twice");
    ep = e;
}

const ITarget *TargetContainer::getAnyTarget() const
{
    if (!targets.empty())
        return targets.begin()->get();
    if (!targets_inactive.empty())
        return targets_inactive.begin()->get();
    return nullptr;
}

void TargetContainer::push_back(const ITargetPtr &t)
{
    targets.push_back(t);
}

void TargetContainer::push_back_inactive(const ITargetPtr &t)
{
    targets_inactive.push_back(t);
}

void TargetContainer::clear()
{
    targets.clear();
}

TargetContainer::Base::iterator TargetContainer::find(const TargetSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return *t == s;
    });
}

TargetContainer::Base::const_iterator TargetContainer::find(const TargetSettings &s) const
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return *t == s;
    });
}

bool TargetContainer::empty() const
{
    return targets.empty();
}

TargetMap::~TargetMap()
{
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
