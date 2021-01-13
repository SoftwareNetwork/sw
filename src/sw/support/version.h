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

struct SW_SUPPORT_API PackageVersion
{
    using Branch = String;
    using Version = primitives::version::Version;
    using Number = Version::Number;

    PackageVersion();
    PackageVersion(const char *);
    PackageVersion(const std::string &);
    PackageVersion(const Version &);
    //PackageVersion(const PackageVersion &) = default;

    // checkers
    bool isBranch() const;
    bool isVersion() const;
    //bool isTag() const; todo - add tags?

    const Version &getVersion() const;
    const Branch &getBranch() const;

    bool isRelease() const;
    bool isPreRelease() const;

    // if branch, returns 0.0.1.0
    Number getMajor() const;
    Number getMinor() const;
    Number getPatch() const;
    Number getTweak() const;

    [[nodiscard]]
    std::string format(const std::string &s) const;
    [[nodiscard]]
    std::string toString(const String &delim = ".") const;
    [[nodiscard]]
    std::string toRangeString() const;

    // operators
    bool operator<(const PackageVersion &) const;
    bool operator>(const PackageVersion &) const;
    bool operator<=(const PackageVersion &) const;
    bool operator>=(const PackageVersion &) const;
    bool operator==(const PackageVersion &) const;

    //static Version min();

private:
    std::variant<Version, Branch> value;

    void checkAndSetFirstVersion();
    void check() const;
    Version &getVersionInternal();
};

struct SW_SUPPORT_API PackageVersionRange
{
    using Branch = PackageVersion::Branch;
    using Branches = std::unordered_set<Branch>;
    using Version = primitives::version::Version;
    using VersionRange = primitives::version::VersionRange;

    /// default is any version or * or Version::min() - Version::max()
    PackageVersionRange();

    /// from string
    PackageVersionRange(const char *);
    PackageVersionRange(const std::string &);
    PackageVersionRange(const PackageVersion &);

    std::string toString() const;
    std::optional<PackageVersion> toVersion() const;

    bool contains(const PackageVersion &) const;
    bool contains(const PackageVersionRange &) const;

    bool operator==(const PackageVersionRange &) const;

    PackageVersionRange &operator|=(const PackageVersionRange &);
    PackageVersionRange &operator&=(const PackageVersionRange &);
    PackageVersionRange operator|(const PackageVersionRange &) const;
    PackageVersionRange operator&(const PackageVersionRange &) const;

    size_t getHash() const;

    // checkers
    bool isBranch() const;
    bool isRange() const;

private:
    std::variant<VersionRange, Branch> value;

    VersionRange &getRange();
    const VersionRange &getRange() const;
    Branch &getBranch();
    const Branch &getBranch() const;
};

using VersionSet = primitives::version::detail::ReverseVersionContainer<PackageVersion, std::set>;

template <class ... Args>
using VersionSetCustom = primitives::version::detail::ReverseVersionContainer<PackageVersion, std::set, Args...>;

template <class ... Args>
using VersionMap = primitives::version::detail::ReverseVersionContainer<PackageVersion, std::map, Args...>;

template <class ... Args>
using UnorderedVersionMap = primitives::version::detail::VersionContainer<PackageVersion, std::unordered_map, Args...>;

SW_SUPPORT_API
std::optional<PackageVersion> getMinSatisfyingVersion(const PackageVersionRange &, const VersionSet &);

SW_SUPPORT_API
std::optional<PackageVersion> getMaxSatisfyingVersion(const PackageVersionRange &, const VersionSet &);

}

namespace std
{

template<> struct hash<sw::PackageVersion>
{
    size_t operator()(const sw::PackageVersion &v) const
    {
        if (v.isBranch())
            return std::hash<sw::PackageVersion::Branch>()(v.getBranch());
        return std::hash<primitives::version::Version>()(v.getVersion());
    }
};

template<> struct hash<sw::PackageVersionRange>
{
    size_t operator()(const sw::PackageVersionRange &v) const
    {
        return v.getHash();
    }
};

}
