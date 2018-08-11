// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <../support/hash.h>

namespace sw
{

SW_MANAGER_API
bool is_valid_path_symbol(int c);

template <class ThisType, class PathElement = std::string, bool CaseSensitive = false>
struct PathBase : protected std::vector<PathElement>
{
    using Base = std::vector<PathElement>;
    using value_type = PathElement;
    using element_type = typename PathElement::value_type;
    using const_iterator = typename Base::const_iterator;

    using Replacements = std::unordered_map<element_type, element_type>;

    PathBase() = default;
    ~PathBase() = default;

    PathBase(const char *s, const Replacements &repl = Replacements())
        : PathBase(String(s), repl)
    {
    }

    PathBase(String s, const Replacements &repl = Replacements())
    {
        auto prev = s.begin();
        for (auto i = s.begin(); i != s.end(); ++i)
        {
            auto &c = *i;
            if (!is_valid_path_symbol(c))
                throw std::runtime_error("Bad symbol '"s + c + "' in path: '" + s + "'");
            auto it = repl.find(c);
            if (it != repl.end())
                c = it->second;
            if (c == '.')
            {
                Base::emplace_back(prev, i);
                prev = std::next(i);
            }
        }
        if (!s.empty())
            Base::emplace_back(prev, s.end());
    }

    PathBase(const PathBase &p)
        : Base(p)
    {
    }

    //String toString(char delim = '.') const { return toString(String() += delim); }
    String toString(const String &delim = ".") const
    {
        String p;
        if (empty())
            return p;
        for (auto &e : *this)
            p += e + delim;
        p.resize(p.size() - delim.size());
        return p;
    }

    String toStringLower(const String &delim = ".") const
    {
        auto s = toString(delim);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    ThisType parent() const
    {
        return { begin(), end() - 1 };
    }

    ThisType slice(int start, int end = -1) const
    {
        if (end == -1)
            return ThisType{ begin() + start, this->end() };
        else
            return ThisType{ begin() + start, begin() + end };
    }

    using Base::empty;
    using Base::size;
    using Base::back;
    using Base::front;
    using Base::clear;

    bool operator==(const ThisType &rhs) const
    {
        if constexpr (!CaseSensitive)
        {
            return std::equal(begin(), end(), rhs.begin(), rhs.end(), [](const auto &s1, const auto &s2) {
                return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(), [](const auto &c1, const auto &c2) {
                    return tolower(c1) == tolower(c2);
                });
            });
        }
        else
            return std::operator==(*this, rhs);
    }

    bool operator!=(const ThisType &rhs) const
    {
        return !operator==(rhs);
    }

    bool operator<(const ThisType &rhs) const
    {
        if constexpr (!CaseSensitive)
        {
            return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end(), [](const auto &s1, const auto &s2) {
                return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), [](const auto &c1, const auto &c2) {
                    return tolower(c1) < tolower(c2);
                });
            });
        }
        else
            return std::operator<(*this, rhs);
    }

    ThisType &operator=(const ThisType &s)
    {
        Base::operator=(s);
        return (ThisType &)*this;
    }

    ThisType operator/(const String &e) const
    {
        if (e.empty())
            return ThisType((ThisType&)*this);
        ThisType tmp = (ThisType&)*this;
        tmp.push_back(e);
        return tmp;
    }

    ThisType operator/(const ThisType &e) const
    {
        ThisType tmp = (ThisType&)*this;
        tmp.insert(tmp.end(), e.begin(), e.end());
        return tmp;
    }

    ThisType &operator/=(const String &e)
    {
        return *this = *this / e;
    }

    ThisType &operator/=(const ThisType &e)
    {
        return *this = *this / e;
    }

    operator String() const
    {
        return toString();
    }

    const_iterator begin() const { return Base::begin(); }
    const_iterator end() const { return Base::end(); }

    size_t hash() const
    {
        size_t h = 0;
        for (const auto &e : *this)
        {
            auto lower = e;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            hash_combine(h, std::hash<String>()(lower));
        }
        return h;
    }

protected:
    using Base::Base;
    using Base::operator[];
};

struct SW_MANAGER_API Path : PathBase<Path>
{
    using Base = PathBase<Path>;
    using Base::Base;
};
template struct PathBase<Path>;

struct SW_MANAGER_API PackagePath : PathBase<PackagePath>
{
    using Base = PathBase<PackagePath>;

    enum class ElementType
    {
        Namespace,
        Owner,
        Tail,
    };

    using Base::Base;
    PackagePath() = default;
    PackagePath(const char *s);
    PackagePath(String s);
    PackagePath(const PackagePath &p);
    ~PackagePath() = default;

    String toPath() const;
    path toFileSystemPath() const;

    bool hasNamespace() const;
    bool isAbsolute(const String &username = String()) const;
    bool isRelative(const String &username = String()) const;
    bool isRootOf(const PackagePath &rhs) const;
    bool hasSameParent(const PackagePath &rhs) const;

    Base::value_type getOwner() const;
    Base::value_type getName() const;

    using Base::back;
    using Base::front;
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

private:
    using Base::operator[];
};
template struct PathBase<PackagePath>;

}

namespace std
{

template<> struct hash<sw::Path>
{
    size_t operator()(const sw::Path& path) const
    {
        return path.hash();
    }
};

template<> struct hash<sw::PackagePath>
{
    size_t operator()(const sw::PackagePath& path) const
    {
        return path.hash();
    }
};

}
