// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "package_path.h"

#include <boost/algorithm/string.hpp>
#include <primitives/templates.h>

namespace sw
{

static bool isValidPackagePathSymbol(int c)
{
    return
        c > 0 && c <= 127 // this prevents isalnum() errors
        &&
        (isalnum(c) || c == '.' || c == '_'/* || c == '-'*/);
}

PackagePath::PackagePath(const char *s)
    : PackagePath(String(s))
{
}

PackagePath::PackagePath(String s)
    : PackagePath(s, isValidPackagePathSymbol)
{
    if (s.size() > 4096)
        throw SW_RUNTIME_ERROR("Too long project path (must be <= 4096)");
}

PackagePath::PackagePath(PathElement s, CheckSymbol check_symbol)
{
    data.reserve(s.size());

    auto prev = s.begin();
    for (auto i = s.begin(); i != s.end(); ++i)
    {
        auto &c = *i;
        if (check_symbol && !check_symbol(c))
            throw SW_RUNTIME_ERROR("Bad symbol '"s + c + "' in path: '" + s + "'");
        if (c == '.')
        {
            data.emplace_back(prev, i);
            prev = std::next(i);
        }
    }
    if (!s.empty())
        data.emplace_back(prev, s.end());
}

PackagePath::PathElement PackagePath::toString(const PackagePath::PathElement &delim) const
{
    PathElement p;
    if (empty())
        return p;
    for (auto &e : *this)
        p += e + delim;
    p.resize(p.size() - delim.size());
    return p;
}

PackagePath::PathElement PackagePath::toStringLower(const PathElement &delim) const
{
    auto s = toString(delim);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

PackagePath PackagePath::parent() const
{
    if (empty())
        return {};
    return { begin(), end() - 1 };
}

PackagePath PackagePath::slice(int start, int end) const
{
    if (end == -1)
        return PackagePath(begin() + start, this->end());
    else
        return PackagePath(begin() + start, begin() + end);
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
    {
        return std::lexicographical_compare(begin(), end(), p.begin(), p.end(), [](const auto &s1, const auto &s2) {
            return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), [](const auto &c1, const auto &c2) {
                return tolower(c1) < tolower(c2);
            });
        });
    }

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

PackagePath::Base::value_type PackagePath::getNamespace() const
{
    if (size() == 0)
        return Base::value_type();
    return (*this)[0];
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

size_t PackagePath::getHash() const
{
    size_t h = 0;
    for (const auto &e : *this)
    {
        for (auto c : e)
            hash_combine(h, std::hash<decltype(c)>()(tolower(c)));
    }
    return h;
}

}
