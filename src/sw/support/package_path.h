// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/hash.h>

#include <primitives/string.h>

namespace sw
{

// case insensitive
struct SW_SUPPORT_API PackagePath
{
    using PathElement = String;
    using Base = std::vector<PathElement>;
    using value_type = PathElement;
    using element_type = typename PathElement::value_type;
    using iterator = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;

    using CheckSymbol = bool(*)(int);

    PackagePath() = default;
    PackagePath(const char *s);
    PackagePath(String s);
    ~PackagePath() = default;

    PathElement toString(const PathElement &delim = ".") const;
    PathElement toStringLower(const PathElement &delim = ".") const;
    PackagePath parent() const;
    PackagePath slice(int start, int end = -1) const;

    auto empty() const { return data.empty(); }
    auto size() const { return data.size(); }
    auto back() const { return data.back(); }
    auto front() const { return data.front(); }
    auto clear() { return data.clear(); }

    bool operator==(const PackagePath &rhs) const
    {
        return std::equal(begin(), end(), rhs.begin(), rhs.end(), [](const auto &s1, const auto &s2) {
            return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(), [](const auto &c1, const auto &c2) {
                return tolower(c1) == tolower(c2);
            });
        });
    }

    PackagePath &operator=(const PackagePath &s)
    {
        data.operator=(s.data);
        return (PackagePath &)*this;
    }

    PackagePath operator/(const PackagePath &e) const
    {
        PackagePath tmp = (PackagePath&)*this;
        tmp.insert(tmp.end(), e.begin(), e.end());
        return tmp;
    }

    PackagePath &operator/=(const PackagePath &e)
    {
        return *this = *this / e;
    }

    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    size_t getHash() const;

private:
    std::vector<PathElement> data;

    PackagePath(PathElement s, CheckSymbol check_symbol);
    PackagePath(const_iterator b, const_iterator e) : data(b, e) {}

    void insert(const_iterator w, const_iterator b, const_iterator e) { data.insert(w, b, e); }
    void assign(const_iterator b, const_iterator e) { data.assign(b, e); }
    void push_back(const value_type &t) { data.push_back(t); }
    value_type &operator[](size_t i) { return data[i]; }
    const value_type &operator[](size_t i) const { return data[i]; }

public:
    enum class ElementType : uint8_t
    {
        Namespace,
        Owner,
        Tail,
    };

    String toPath() const;
    path toFileSystemPath() const;

    bool hasNamespace() const;
    bool isAbsolute(const String &username = String()) const;
    bool isRelative(const String &username = String()) const;
    bool isRootOf(const PackagePath &rhs) const;
    bool hasSameParent(const PackagePath &rhs) const;

    PathElement getNamespace() const;
    PathElement getOwner() const;
    PathElement getName() const;

    PackagePath back(const PackagePath &root) const;

    PackagePath operator[](ElementType e) const;
    bool operator<(const PackagePath &rhs) const;

#define PACKAGE_PATH(name)          \
    static PackagePath name()       \
    {                               \
        return PackagePath(#name);  \
    }                               \
    bool is_##name() const          \
    {                               \
        if (empty())                \
            return false;           \
        return (*this)[0] == #name; \
    }
#include "package_path.inl"
#undef PACKAGE_PATH
};

}

namespace std
{

template<> struct hash<sw::PackagePath>
{
    size_t operator()(const sw::PackagePath& path) const
    {
        return path.getHash();
    }
};

}
