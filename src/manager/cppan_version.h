// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <../support/hash.h>

#define LOCAL_BRANCH_NAME "local"

#include <primitives/version.h>

namespace sw
{

namespace db
{

using PackageId = int64_t;
using PackageVersionId = int64_t;

}

using PackageVersionGroupNumber = int64_t;

using primitives::version::Version;

#pragma warning(push)
#pragma warning(disable:4275) // warning C4275: non dll-interface struct 'primitives::Command' used as base for dll-interface struct 'sw::builder::Command'

struct SW_MANAGER_API VersionRange : primitives::version::VersionRange
{
#pragma warning(pop)
    using base = primitives::version::VersionRange;

    using base::base;

    using base::getMinSatisfyingVersion;
    using base::getMaxSatisfyingVersion;

    /// resolve from db
    std::optional<Version> getMinSatisfyingVersion() const;

    /// resolve from db
    std::optional<Version> getMaxSatisfyingVersion() const;
};

}

namespace std
{

template<> struct hash<sw::VersionRange>
{
    size_t operator()(const sw::VersionRange &v) const
    {
        return hash<primitives::version::VersionRange>()(v);
    }
};

}
