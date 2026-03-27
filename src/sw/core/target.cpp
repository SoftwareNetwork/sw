// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "target.h"

#include "input.h"
#include "rule.h"

namespace sw
{

IDependency::~IDependency() = default;
ITarget::~ITarget() {}
TargetEntryPoint::~TargetEntryPoint() = default;

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

std::unique_ptr<IRule> ITarget::getRule() const { return nullptr; }

TargetData::~TargetData()
{
}

TargetContainer::TargetContainer()
{
}

TargetContainer::TargetContainer(const TargetContainer &rhs)
{
    operator=(rhs);
}

TargetContainer &TargetContainer::operator=(const TargetContainer &rhs)
{
    if (this == &rhs)
        return *this;
    targets = rhs.targets;
    if (rhs.input)
        input = std::make_unique<BuildInput>(rhs.getInput());
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

const BuildInput &TargetContainer::getInput() const
{
    if (!input)
        throw SW_RUNTIME_ERROR("No input was set");
    return *input;
}

void TargetContainer::setInput(const BuildInput &i)
{
    if (input && i != *input)
        throw SW_RUNTIME_ERROR("Setting input twice: " + i.getInput().getName());
    input = std::make_unique<BuildInput>(i);
}

std::vector<ITargetPtr> TargetContainer::loadPackages(SwBuild &b, const TargetSettings &s, const PackageIdSet &allowed_packages) const
{
    return getInput().loadPackages(b, s, allowed_packages);
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

}
