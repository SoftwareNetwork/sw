// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "package_path.h"

#include <boost/algorithm/string.hpp>
#include <primitives/templates.h>

namespace sw
{

static const PackagePath::Replacements repls{
    { '-', '_' }
};

bool is_valid_path_symbol(int c)
{
    return c > 0 && c <= 127 &&
        (isalnum(c) || c == '.' || c == '_' || c == '-');
}

PackagePath::PackagePath(const char *s)
    : PackagePath(String(s))
{
}

PackagePath::PackagePath(String s)
    : Base(s, is_valid_path_symbol, repls)
{
    if (s.size() > 4096)
        throw SW_RUNTIME_EXCEPTION("Too long project path (must be <= 4096)");
}

PackagePath::PackagePath(const PackagePath &p)
    : Base(p, is_valid_path_symbol)
{
}

PackagePath::Base::value_type PackagePath::getName() const
{
    return back();
}

String PackagePath::toPath() const
{
    String p = toStringLower();
    std::replace(p.begin(), p.end(), '.', '/');
    return p;
}

path PackagePath::toFileSystemPath() const
{
    path p;
    if (empty())
        return p;
    for (const auto &[i, e] : enumerate(*this))
    {
        if (i == int(ElementType::Owner))
        {
            p /= e.substr(0, 1);
            p /= e.substr(0, 2);
        }
        p /= e;
    }
    return p;
}

bool PackagePath::operator<(const PackagePath &p) const
{
    if (empty() && p.empty())
        return false;
    if (empty())
        return true;
    if (p.empty())
        return false;
    auto &p0 = (*this)[0];
    auto &pp0 = p[0];
    if (boost::iequals(p0, pp0))
        return Base::operator<(p);

    // namespace order
#define PACKAGE_PATH(n)          \
    if (boost::iequals(p0, #n))  \
        return true;             \
    if (boost::iequals(pp0, #n)) \
        return false;
#include "package_path.inl"
#undef PACKAGE_PATH

    return false;
}

bool PackagePath::hasNamespace() const
{
    if (empty())
        return false;
    if (
#define PACKAGE_PATH(n) \
    (*this)[0] == n()[0] ||
#include "package_path.inl"
#undef PACKAGE_PATH
        0)
        return true;
    return false;
}

PackagePath::Base::value_type PackagePath::getOwner() const
{
    if (size() < 2)
        return Base::value_type();
    return (*this)[1];
}

bool PackagePath::isAbsolute(const String &username) const
{
    if (!hasNamespace())
        return false;
    if (username.empty())
    {
        if (size() > 1)
            return true;
        return false;
    }
    if (size() > 2 && (*this)[1] == username)
        return true;
    return false;
}

bool PackagePath::isRelative(const String &username) const
{
    return !isAbsolute(username);
}

bool PackagePath::hasSameParent(const PackagePath &rhs) const
{
    if (empty() || rhs.empty())
        return false;
    if (operator==(rhs))
        return true;

    PackagePath p1 = *this;
    while (!p1.empty())
    {
        if (p1.isRootOf(*this) && p1.isRootOf(rhs))
            return true;
        p1 = p1.parent();
    }
    return false;
}

PackagePath PackagePath::operator[](ElementType e) const
{
    if (empty())
        return *this;
    switch (e)
    {
    case ElementType::Namespace:
        return PackagePath((*this)[0]);
    case ElementType::Owner:
        return PackagePath(getOwner());
    case ElementType::Tail:
        if (size() < 2)
            return PackagePath();
        return { begin() + 2, end() };
    }
    return *this;
}

bool PackagePath::isRootOf(const PackagePath &rhs) const
{
    if (size() >= rhs.size())
        return false;
    for (size_t i = 0; i < size(); i++)
    {
        if ((*this)[i] != rhs[i])
            return false;
    }
    return true;
}

PackagePath PackagePath::back(const PackagePath &root) const
{
    PackagePath p;
    if (!root.isRootOf(*this))
        return p;
    for (size_t i = 0; i < root.size(); i++)
    {
        if ((*this)[i] != root[i])
        {
            p.assign(begin() + i, end());
            break;
        }
    }
    if (p.empty())
        p.assign(end() - (size() - root.size()), end());
    return p;
}

String PackagePath::getHash() const
{
    return blake2b_512(toStringLower());
}

}
