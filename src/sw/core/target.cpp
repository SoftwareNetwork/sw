// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "target.h"

#include <sw/manager/storage.h>
#include <sw/support/unresolved_package_id.h>

namespace sw
{

//IDependency::~IDependency() = default;
ITarget::~ITarget() {}
//TargetEntryPoint::~TargetEntryPoint() = default;
TargetData::~TargetData() = default;

//PackageSettings &IDependency::getSettings() { return getUnresolvedPackageId().getSettings(); }
//const PackageSettings &IDependency::getSettings() const { return getUnresolvedPackageId().getSettings(); }

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

void TargetContainer::push_back(ITarget &t, Input &i)
{
    setInput(i);
    push_back(t);
}

void TargetContainer::push_back(ITarget &t)
{
    //if (!input)
        //throw SW_LOGIC_ERROR("Provide input first");
    targets.insert(&t);

    // on the same settings, we take input target and overwrite old one

    /*auto i = findEqual(t.getSettings());
    if (i == end())
    {
        targets.insert(&t);
        return;
    }
    *i = &t;*/
}

void TargetContainer::setInput(Input &i)
{
    if (input && input != &i)
        throw SW_RUNTIME_ERROR("Different input provided");
    input = &i;
}

Input &TargetContainer::getInput() const
{
    if (!input)
        throw SW_RUNTIME_ERROR("No input");
    return *input;
}

void TargetContainer::clear()
{
    targets.clear();
}

TargetContainer::Base::iterator TargetContainer::findEqual(const PackageSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings() == s;
    });
}

TargetContainer::Base::const_iterator TargetContainer::findEqual(const PackageSettings &s) const
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings() == s;
    });
}

TargetContainer::Base::iterator TargetContainer::findSuitable(const PackageSettings &s)
{
    return std::find_if(begin(), end(), [&s](const auto &t)
    {
        return t->getSettings().isSubsetOf(s);
    });
}

TargetContainer::Base::const_iterator TargetContainer::findSuitable(const PackageSettings &s) const
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

ITarget *TargetMap::find(const PackageId &pkg) const
{
    SW_UNIMPLEMENTED;
    /*auto i = find(pkg.getName());
    if (i == end())
        return {};
    auto k = i->second.findSuitable(pkg.getSettings());
    if (k == i->second.end())
        return {};
    return *k;*/
}

ITarget *TargetMap::find(const ResolveRequest &rr) const
{
    SW_UNIMPLEMENTED;
    /*auto i = find(rr.getUnresolvedPackageName());
    if (i == end())
        return {};
    auto k = i->second.findSuitable(rr.getSettings());
    if (k == i->second.end())
        return {};
    return *k;*/
}

PredefinedTarget::PredefinedTarget(const PackageIdFull &id)
    : pkg(id)
{
}

PredefinedTarget::~PredefinedTarget()
{
}

/*struct PredefinedDependency : IDependency
{
    PredefinedDependency(const PackageId &p) : pkg(p) {}
    virtual ~PredefinedDependency() {}

    const PackageSettings &getSettings() const override { return pkg.getSettings(); }
    const UnresolvedPackage &getUnresolvedPackage() const override { return pkg.getName(); }
    bool isResolved() const override { return t; }
    void setTarget(const ITarget &t) override { this->t = &t; }
    const ITarget &getTarget() const override
    {
        if (!t)
            throw SW_RUNTIME_ERROR("not resolved");
        return *t;
    }

private:
    PackageId pkg;
    const ITarget *t = nullptr;
};*/

/*std::vector<IDependency *> PredefinedTarget::getDependencies() const
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
}*/

}
