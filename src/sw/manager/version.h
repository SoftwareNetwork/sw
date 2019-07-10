// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/support/hash.h>

#include <primitives/version_range.h>

namespace sw
{

namespace db
{

using PackageId = int64_t;
using PackageVersionId = int64_t;

}

// return back to just int64_t
using PackageVersionGroupNumber = int64_t;

using primitives::version::Version;
using primitives::version::VersionSet;
using primitives::version::VersionMap;
using primitives::version::UnorderedVersionMap;

struct SW_MANAGER_API VersionRange : primitives::version::VersionRange
{
#pragma warning(pop)
    using Base = primitives::version::VersionRange;

    using Base::Base;

    std::optional<Version> getMinSatisfyingVersion(const VersionSet &) const;
    std::optional<Version> getMaxSatisfyingVersion(const VersionSet &) const;
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
