// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2020 Egor Pugin <egor.pugin@gmail.com>

#include "version.h"

#include <fmt/format.h>

namespace sw
{

static bool is_branch(const std::string &s)
{
    return 1
        && (std::isalpha(s[0]) || s[0] == '_')
        && std::all_of(s.begin(), s.end(), [](auto &&c)
    {
        return std::isalnum(c) || c == '_';
    });
}

PackageVersion::PackageVersion()
{
    checkAndSetFirstVersion();
}

PackageVersion::PackageVersion(const char *s)
{
    if (!s)
        throw SW_RUNTIME_ERROR("Empty package version");
    *this = PackageVersion{ std::string{s} };
}

PackageVersion::PackageVersion(const std::string &s)
{
    if (s.empty())
        throw SW_RUNTIME_ERROR("Empty package version");

    if (is_branch(s))
        value = s;
    else
        value = Version(s);
    checkAndSetFirstVersion();
}

PackageVersion::PackageVersion(const Version &v)
{
    value = v;
    checkAndSetFirstVersion();
}

bool PackageVersion::isBranch() const
{
    return std::holds_alternative<Branch>(value);
}

bool PackageVersion::isVersion() const
{
    return std::holds_alternative<Version>(value);
}

PackageVersion::Version &PackageVersion::getVersionInternal()
{
    return std::get<Version>(value);
}

const PackageVersion::Version &PackageVersion::getVersion() const
{
    return std::get<Version>(value);
}

const PackageVersion::Branch &PackageVersion::getBranch() const
{
    return std::get<Branch>(value);
}

bool PackageVersion::isRelease() const
{
    if (isBranch())
        return false;
    return getVersion().isRelease();
}

bool PackageVersion::isPreRelease() const
{
    return !isRelease();
}

std::string PackageVersion::format(const std::string &s) const
{
    if (isBranch())
    {
        return fmt::format(s,
            fmt::arg("b", getBranch()),
            fmt::arg("v", toString())
        );
    }
    else
        return getVersion().format(s);
}

std::string PackageVersion::toString(const String &delim) const
{
    if (isBranch())
        return getBranch();
    return getVersion().toString(delim);
}

void PackageVersion::checkAndSetFirstVersion()
{
    if (!isBranch())
        getVersionInternal().setFirstVersion();
    check();
}

void PackageVersion::check() const
{
    if (isBranch())
    {
        if (getBranch().size() > 200)
            throw SW_RUNTIME_ERROR("Invalid version: " + getBranch() + ", branch must have size <= 200");
    }
    else
    {
        getVersion().checkValidity();
    }
}

#define BRANCH_COMPARE(op)                          \
    if (isBranch() && rhs.isBranch())               \
        return getBranch() op rhs.getBranch();      \
    if (isBranch())                                 \
        return !(getBranch() op std::string{});     \
    if (rhs.isBranch())                             \
        return !(std::string{} op rhs.getBranch()); \
    return value op rhs.value

bool PackageVersion::operator<(const PackageVersion &rhs) const
{
    BRANCH_COMPARE(<);
}

bool PackageVersion::operator<=(const PackageVersion &rhs) const
{
    BRANCH_COMPARE(<=);
}

bool PackageVersion::operator>(const PackageVersion &rhs) const
{
    BRANCH_COMPARE(>);
}

bool PackageVersion::operator>=(const PackageVersion &rhs) const
{
    BRANCH_COMPARE(>=);
}

bool PackageVersion::operator==(const PackageVersion &rhs) const
{
    return value == rhs.value;
}

PackageVersion::Number PackageVersion::getMajor() const
{
    if (isBranch())
        return 0;
    return getVersion().getMajor();
}

PackageVersion::Number PackageVersion::getMinor() const
{
    if (isBranch())
        return 0;
    return getVersion().getMinor();
}

PackageVersion::Number PackageVersion::getPatch() const
{
    if (isBranch())
        return 1;
    return getVersion().getPatch();
}

PackageVersion::Number PackageVersion::getTweak() const
{
    if (isBranch())
        return 0;
    return getVersion().getTweak();
}

PackageVersionRange::PackageVersionRange()
{
    value = VersionRange(Version::min(), Version::max());
}

PackageVersionRange::PackageVersionRange(const char *s)
{
    if (!s)
        throw SW_RUNTIME_ERROR("Empty package version range");
    *this = PackageVersionRange{ std::string{s} };
}

PackageVersionRange::PackageVersionRange(const std::string &s)
{
    if (s.empty())
        throw SW_RUNTIME_ERROR("Empty package version range");

    if (is_branch(s))
        value = s;
    else
        value = VersionRange(s);
}

PackageVersionRange::PackageVersionRange(const PackageVersion &v)
{
    if (v.isBranch())
        value = v.getBranch();
    else
        value = VersionRange{v.getVersion(), v.getVersion()};
}

bool PackageVersionRange::isBranch() const
{
    return std::holds_alternative<Branch>(value);
}

bool PackageVersionRange::isRange() const
{
    return std::holds_alternative<VersionRange>(value);
}

const PackageVersionRange::VersionRange &PackageVersionRange::getRange() const
{
    return std::get<VersionRange>(value);
}

PackageVersionRange::VersionRange &PackageVersionRange::getRange()
{
    return std::get<VersionRange>(value);
}

const PackageVersion::Branch &PackageVersionRange::getBranch() const
{
    return std::get<Branch>(value);
}

PackageVersion::Branch &PackageVersionRange::getBranch()
{
    return std::get<Branch>(value);
}

std::string PackageVersionRange::toString() const
{
    if (isBranch())
        return getBranch();
    return getRange().toString();
}

std::optional<PackageVersion> PackageVersionRange::toVersion() const
{
    if (isBranch())
        return getBranch();
    return getRange().toVersion();
}

bool PackageVersionRange::contains(const PackageVersion &rhs) const
{
    if (isBranch() ^ rhs.isBranch())
        return false;
    if (isBranch())
        return getBranch() == rhs.getBranch();
    return getRange().contains(rhs.getVersion());
}

bool PackageVersionRange::contains(const PackageVersionRange &rhs) const
{
    if (isBranch() ^ rhs.isBranch())
        return false;
    if (isBranch())
        return getBranch() == rhs.getBranch();
    return getRange().contains(rhs.getRange());
}

bool PackageVersionRange::operator==(const PackageVersionRange &rhs) const
{
    return value == rhs.value;
}

PackageVersionRange &PackageVersionRange::operator|=(const PackageVersionRange &r)
{
    if (isBranch() || r.isBranch())
        throw SW_RUNTIME_ERROR("Cannot unite branch package versions");
    getRange() |= r.getRange();
    return *this;
}

PackageVersionRange &PackageVersionRange::operator&=(const PackageVersionRange &r)
{
    if (isBranch() || r.isBranch())
        throw SW_RUNTIME_ERROR("Cannot intersect branch package versions");
    getRange() &= r.getRange();
    return *this;
}

PackageVersionRange PackageVersionRange::operator|(const PackageVersionRange &r) const
{
    auto l = *this;
    l |= r;
    return l;
}

PackageVersionRange PackageVersionRange::operator&(const PackageVersionRange &r) const
{
    auto l = *this;
    l &= r;
    return l;
}

size_t PackageVersionRange::getHash() const
{
    if (isBranch())
        return std::hash<sw::PackageVersionRange::Branch>()(getBranch());
    return std::hash<primitives::version::VersionRange>()(getRange());
}

std::optional<PackageVersion> getMinSatisfyingVersion(const PackageVersionRange &r, const VersionSet &s)
{
    // do we really need this 'min' version?
    SW_UNIMPLEMENTED;

    // add policies?

    if (!s.empty_releases())
    {
        for (auto &v : s.releases())
        {
            if (r.contains(v))
                return v;
        }
    }

    // from old version range
    /*for (auto &v : s)
    {
        if (r.getRange().contains(v.getVersion()))
            return v;
    }*/
    return {};
}

std::optional<PackageVersion> getMaxSatisfyingVersion(const PackageVersionRange &r, const VersionSet &s)
{
    // add policies?

    if (!s.empty_releases())
    {
        for (auto i = s.rbegin_releases(); i != s.rend_releases(); ++i)
        {
            if (r.contains(*i))
                return *i;
        }
    }

    SW_UNIMPLEMENTED;

    // from old version range
    /*for (auto i = s.rbegin(); i != s.rend(); ++i)
    {
        if (r.getRange().contains(i->getVersion()))
            return *i;
    }*/
    return {};
}

}
