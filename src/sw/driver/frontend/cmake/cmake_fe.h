// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/driver/entry_point.h>

class cmake;
class cmTarget;
class cmMakefile;

namespace sw
{
struct CheckSet;
}

namespace sw::driver::cpp
{

struct CmakeTargetEntryPoint : NativeTargetEntryPoint
{
    using Base = NativeTargetEntryPoint;

    mutable std::unique_ptr<cmake> cm;
    mutable SwBuild *b = nullptr;
    mutable TargetSettings ts;
    mutable NativeCompiledTarget *t = nullptr;
    mutable CheckSet *cs = nullptr;

    CmakeTargetEntryPoint(const path &fn);
    ~CmakeTargetEntryPoint();

    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const AllowedPackages &pkgs, const PackagePath &prefix) const override;

private:
    path rootfn;

    void init() const;
    void loadPackages1(Build &) const override;

    static NativeCompiledTarget *addTarget(Build &, cmTarget &);
    void setupTarget(cmMakefile &, cmTarget &, NativeCompiledTarget &, const StringSet &list_of_targets) const;
};

}
