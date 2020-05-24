// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/hash.h>

#include <primitives/version_range.h>

namespace sw
{

namespace db
{

using PackageId = int64_t;
using PackageVersionId = int64_t;
using FileId = int64_t;

}

using primitives::version::Version;
using primitives::version::VersionSet;
using primitives::version::VersionMap;
using primitives::version::UnorderedVersionMap;

struct SW_SUPPORT_API VersionRange : primitives::version::VersionRange
{
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
