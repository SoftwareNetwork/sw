// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/hash.h>

// object path
// users:
// 1. package path
// 2. setting path? (double check)

// pp - case insensitive
// sp - case sensitive

namespace sw
{

SW_SUPPORT_API
bool isValidPackagePathSymbol(int c);

template <class ThisType, class PathElement = std::string, bool CaseSensitive = false>
struct PathBase
{
    using Base = std::vector<PathElement>;
    using value_type = PathElement;
    using element_type = typename PathElement::value_type;
    using iterator = typename Base::iterator;
    using const_iterator = typename Base::const_iterator;

    using CheckSymbol = bool(*)(int);

    PathBase() = default;
    ~PathBase() = default;

    PathBase(const element_type *s, CheckSymbol check_symbol = nullptr)
        : PathBase(PathElement(s), check_symbol)
    {
    }

    PathBase(PathElement s, CheckSymbol check_symbol = nullptr)
    {
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

    PathBase(const PathBase &p)
        : data(p.data)
    {
    }

    PathElement toString(const PathElement &delim = ".") const
    {
        PathElement p;
        if (empty())
            return p;
        for (auto &e : *this)
            p += e + delim;
        p.resize(p.size() - delim.size());
        return p;
    }

    PathElement toStringLower(const PathElement &delim = ".") const
    {
        auto s = toString(delim);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }

    ThisType parent() const
    {
        if (empty())
            return {};
        return { begin(), end() - 1 };
    }

    ThisType slice(int start, int end = -1) const
    {
        if (end == -1)
            return ThisType(PathBase{ begin() + start, this->end() });
        else
            return ThisType(PathBase{ begin() + start, begin() + end });
    }

    auto empty() const { return data.empty(); }
    auto size() const { return data.size(); }
    auto back() const { return data.back(); }
    auto front() const { return data.front(); }
    auto clear() { return data.clear(); }

    bool operator==(const PathBase &rhs) const
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

    bool operator<(const PathBase &rhs) const
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
        data.operator=(s.data);
        return (ThisType &)*this;
    }

    ThisType operator/(const ThisType &e) const
    {
        ThisType tmp = (ThisType&)*this;
        tmp.insert(tmp.end(), e.begin(), e.end());
        return tmp;
    }

    ThisType &operator/=(const ThisType &e)
    {
        return *this = *this / e;
    }

    operator PathElement() const
    {
        return toString();
    }

    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }

    size_t hash() const
    {
        size_t h = 0;
        for (const auto &e : *this)
        {
            auto lower = e;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            hash_combine(h, std::hash<PathElement>()(lower));
        }
        return h;
    }

protected:
    PathBase(const_iterator b, const_iterator e) : data(b, e) {}
    void insert(const_iterator w, const_iterator b, const_iterator e) { data.insert(w, b, e); }
    void assign(const_iterator b, const_iterator e) { data.assign(b, e); }
    void push_back(const value_type &t) { data.push_back(t); }
    value_type &operator[](int i) { return data[i]; }
    const value_type &operator[](int i) const { return data[i]; }

private:
    std::vector<PathElement> data;
};

// able to split input on addition operations
template <class ThisType>
struct SecureSplitablePath : PathBase<ThisType>
{
    using Base = PathBase<ThisType>;
    using Base::Base;

    ThisType operator/(const String &e) const
    {
        return Base::operator/(ThisType(e));
    }

    ThisType &operator/=(const String &e)
    {
        return Base::operator/=(ThisType(e));
    }
};

struct SW_SUPPORT_API PackagePath : SecureSplitablePath<PackagePath>
{
    using Base = SecureSplitablePath<PackagePath>;

    enum class ElementType : uint8_t
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

    String getHash() const;

    Base::value_type getNamespace() const;
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
    value_type &operator[](int i) { return Base::operator[](i); }
    const value_type &operator[](int i) const { return Base::operator[](i); }
};

#if defined(_WIN32)// || defined(__APPLE__)
#if defined(__APPLE__)
SW_MANAGER_API_EXTERN
#endif
template struct SW_SUPPORT_API PathBase<PackagePath>;
#elif defined(__APPLE__)
#else
template struct PathBase<PackagePath>;
#endif

}

namespace std
{

template<> struct hash<sw::PackagePath>
{
    size_t operator()(const sw::PackagePath& path) const
    {
        return path.hash();
    }
};

}
