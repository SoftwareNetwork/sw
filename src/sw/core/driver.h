// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/manager/package.h>

namespace sw
{

struct RawInput;
struct SwContext;
struct TargetEntryPoint;
using TargetEntryPointPtr = std::shared_ptr<TargetEntryPoint>;

struct SW_CORE_API IDriver
{
    using EntryPointsVector1 = std::vector<TargetEntryPointPtr>;
    using EntryPointsVector = std::vector<EntryPointsVector1>;

    virtual ~IDriver() = 0;

    // get driver pkg id
    virtual PackageId getPackageId() const = 0;
    //virtual PackageId getPackage() const = 0; // no
    //virtual PackageId getName() const = 0; // ?

    /// test if driver is able to load this input
    virtual bool canLoad(const RawInput &) const = 0;

    // load entry points for inputs
    // inputs are unique and non null
    // inputs will receive their entry points
    // result is number of vectors of entry points equal to inputs, in the same order
    // one input may provide several entry points (yml)
    // we return shared points because we cannot load them into context because package ids is not known in advance
    // (in case of loading not installed package)
    // if entry points were already loaded (like for installed packages), internal vector may be empty
    virtual EntryPointsVector load(SwContext &, const std::vector<RawInput> &) const = 0;

    // get raw spec
    // complex return value?
    // for example set of files
    virtual String getSpecification(const RawInput &) const = 0;

    // get features()?
};

} // namespace sw
