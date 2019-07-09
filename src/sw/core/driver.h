// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/manager/package.h>

namespace sw
{

struct Input;

struct SW_CORE_API IDriver
{
    virtual ~IDriver() = 0;

    virtual PackageId getPackageId() const = 0;
    //virtual PackageId getPackage() const = 0; // no
    //virtual PackageId getName() const = 0; // ?

    virtual bool canLoad(const Input &) const = 0;
    virtual void load(const std::set<Input> &) = 0;

    //virtual void execute() = 0;
    //virtual bool prepareStep() = 0;

    // get features()?
};

//struct INativeDriver : IDriver {}; // ?

} // namespace sw
