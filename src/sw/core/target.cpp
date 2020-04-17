/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "target.h"

#include "input.h"

namespace sw
{

IDependency::~IDependency() = default;
ITarget::~ITarget() = default;
TargetEntryPoint::~TargetEntryPoint() = default;

TargetData::~TargetData()
{
}

void TargetContainer::push_back(const ITargetPtr &t)
{
    // on the same settings, we take input target and overwrite old one

    auto i = findEqual(t->getSettings());
    if (i == end())
    {
        targets.push_back(t);
        return;
    }
    *i = t;
}

void TargetContainer::clear()
{
    targets.clear();
}

TargetContainer::Base::iterator TargetContainer::findEqual(const TargetSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings() == s;
    });
}

TargetContainer::Base::const_iterator TargetContainer::findEqual(const TargetSettings &s) const
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings() == s;
    });
}

TargetContainer::Base::iterator TargetContainer::findSuitable(const TargetSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings().isSubsetOf(s);
    });
}

TargetContainer::Base::const_iterator TargetContainer::findSuitable(const TargetSettings &s) const
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings().isSubsetOf(s);
    });
}

bool TargetContainer::empty() const
{
    return targets.empty();
}

TargetContainer::Base::iterator TargetContainer::erase(Base::iterator begin, Base::iterator end)
{
    return targets.erase(begin, end);
}

const Input &TargetContainer::getInput() const
{
    if (!input)
        throw SW_RUNTIME_ERROR("No input was set");
    return *input;
}

void TargetContainer::setInput(const Input &i)
{
    if (input && &i != input)
        throw SW_RUNTIME_ERROR("Setting input twice: " + i.getName());
    input = &i;
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
    auto j = i->second.findSuitable(ts);
    if (j == i->second.end())
        return std::pair<Version, ITarget*>{ i->first, nullptr };
    return std::pair<Version, ITarget*>{ i->first, j->get() };
}

ITarget *TargetMap::find(const PackageId &pkg, const TargetSettings &ts) const
{
    auto i = find(pkg);
    if (i == end())
        return {};
    auto k = i->second.findSuitable(ts);
    if (k == i->second.end())
        return {};
    return k->get();
}

ITarget *TargetMap::find(const UnresolvedPackage &pkg, const TargetSettings &ts) const
{
    auto i = find(pkg);
    if (i == end())
        return {};
    auto k = i->second.findSuitable(ts);
    if (k == i->second.end())
        return {};
    return k->get();
}

PredefinedTarget::PredefinedTarget(const PackageId &id, const TargetSettings &ts)
    : pkg(id), ts(ts)
{
}

PredefinedTarget::~PredefinedTarget()
{
}

struct PredefinedDependency : IDependency
{
    PredefinedDependency(const PackageId &unresolved_pkg, const TargetSettings &ts) : unresolved_pkg(unresolved_pkg), ts(ts) {}
    virtual ~PredefinedDependency() {}

    const TargetSettings &getSettings() const override { return ts; }
    UnresolvedPackage getUnresolvedPackage() const override { return unresolved_pkg; }
    bool isResolved() const override { return t; }
    void setTarget(const ITarget &t) override { this->t = &t; }
    const ITarget &getTarget() const override
    {
        if (!t)
            throw SW_RUNTIME_ERROR("not resolved");
        return *t;
    }

private:
    PackageId unresolved_pkg;
    TargetSettings ts;
    const ITarget *t = nullptr;
};

std::vector<IDependency *> PredefinedTarget::getDependencies() const
{
    if (!deps_set)
    {
        //for (auto &[k, v] : public_ts["dependencies"]["link"].getSettings())
            //deps.push_back(std::make_shared<PredefinedDependency>(k, v.getSettings()));

        // we take only link libs now, is it correct?
        for (auto &[k, v] : public_ts["properties"].getSettings())
        {
            for (auto &[k2, v2] : v["dependencies"].getSettings())
            {
                deps.push_back(std::make_shared<PredefinedDependency>(k2, v2.getSettings()));
            }
        }

        deps_set = true;
    }
    std::vector<IDependency *> deps;
    for (auto &d : this->deps)
        deps.push_back(d.get());
    return deps;
}

}
