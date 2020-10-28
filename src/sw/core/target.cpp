// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "target.h"

#include "input.h"
//#include "rule.h"

namespace sw
{

IDependency::~IDependency() = default;
ITarget::~ITarget() {}
TargetEntryPoint::~TargetEntryPoint() = default;
TargetData::~TargetData() = default;
IRule::~IRule() = default;

AllowedPackages::AllowedPackages(const UnresolvedPackages &in)
{
    for (auto &u : in)
    {
        if (u.getRange().isBranch())
            branches.insert(u);
        else
        {
            if (pkgs.contains(u.getPath()))
                pkgs[u.getPath()] |= u.getRange();
            else
                pkgs.emplace(u.getPath(), u.getRange());
        }
    }
}

AllowedPackages::AllowedPackages(const PackageIdSet &in)
{
    for (auto &u : in)
    {
        if (u.getVersion().isBranch())
            branches.insert(u);
        else
        {
            if (pkgs.contains(u.getPath()))
                pkgs[u.getPath()] |= u.getVersion();
            else
                pkgs.emplace(u.getPath(), u.getVersion());
        }
    }
}

bool AllowedPackages::contains(const PackageId &p) const
{
    auto i = pkgs.find(p.getPath());
    return (i != pkgs.end() && i->second.hasVersion(p.getVersion()))
        || branches.contains(p);
        ;
}

bool AllowedPackages::empty() const
{
    return pkgs.empty();
}

TargetFile::TargetFile(const path &p, bool is_generated, bool is_from_other_target)
    : fn(p), is_generated(is_generated), is_from_other_target(is_from_other_target)
{
    //if (p.is_absolute() && !is_generated && !is_from_other_target)
        //throw SW_RUNTIME_ERROR("Only generated/other target absolute files are allowed: " + normalize_path(p));
    if (!p.is_absolute())
        throw SW_RUNTIME_ERROR("Only absolute paths accepted");
}

/*TargetFile::TargetFile(const path &inp, const path &rootdir, bool is_generated, bool is_from_other_target)
    : fn(inp), is_generated(is_generated), is_from_other_target(is_from_other_target)
{
    if (fn.is_relative())
        throw SW_RUNTIME_ERROR("path must be absolute: " + normalize_path(fn));
    if (!is_generated)
    {
        if (is_under_root(fn, rootdir))
            fn = fn.lexically_relative(rootdir);
        else
            is_from_other_target = true; // hack! sdir files from this target for bdir package will be from other tgt
    }
    if (fn.is_absolute() && !is_generated && !is_from_other_target)
        throw SW_RUNTIME_ERROR("Only generated/other target absolute files are allowed: " + normalize_path(fn));
}*/

//std::unique_ptr<IRule> ITarget::getRule() const { return nullptr; }

InputLoader::InputLoader() = default;
InputLoader::~InputLoader() = default;

InputLoader::InputLoader(const InputLoader &rhs)
{
    operator=(rhs);
}

InputLoader &InputLoader::operator=(const InputLoader &rhs)
{
    if (this == &rhs)
        return *this;
    if (rhs.input)
        input = std::make_unique<BuildInput>(rhs.getInput());
    return *this;
}

const BuildInput &InputLoader::getInput() const
{
    if (!input)
        throw SW_RUNTIME_ERROR("No input was set");
    return *input;
}

void InputLoader::setInput(const BuildInput &i)
{
    if (input && i != *input)
        throw SW_RUNTIME_ERROR("Setting input twice: " + i.getInput().getName());
    input = std::make_unique<BuildInput>(i);
}

std::vector<ITargetPtr> InputLoader::loadPackages(SwBuild &b, const TargetSettings &s, const AllowedPackages &allowed_packages) const
{
    return getInput().loadPackages(b, s, allowed_packages);
}

TargetContainer::TargetContainer() = default;

TargetContainer::TargetContainer(const TargetContainer &rhs)
    : InputLoader(rhs)
{
    operator=(rhs);
}

TargetContainer &TargetContainer::operator=(const TargetContainer &rhs)
{
    if (this == &rhs)
        return *this;
    targets = rhs.targets;
    return *this;
}

TargetContainer::~TargetContainer()
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

PredefinedTarget::PredefinedTarget(const LocalPackage &id, const TargetSettings &ts)
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
        for (auto &[k, v] : public_ts["properties"].getMap())
        {
            for (auto &v : v["dependencies"].getArray())
            {
                for (auto &[k2, v2] : v.getMap())
                    deps.push_back(std::make_shared<PredefinedDependency>(k2, v2.getMap()));
            }
        }

        deps_set = true;
    }
    std::vector<IDependency *> deps;
    for (auto &d : this->deps)
        deps.push_back(d.get());
    return deps;
}

IRulePtr PredefinedTarget::getRule(const String &name) const
{
    auto i = rules.find(name);
    if (i == rules.end())
        throw SW_RUNTIME_ERROR(getPackage().toString() + ": No such rule: " + name);
    return i->second->clone();
}

}
