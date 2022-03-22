// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package_name.h"
#include "package_id.h"
#include "unresolved_package_name.h"

#include <nlohmann/json_fwd.hpp>
#include <primitives/filesystem.h>

#include <memory>
#include <optional>
#include <variant>

namespace sw
{

struct Directories;

using PackageSettingKey = String;
using PackageSettingValue = String;
struct PackageSetting;
struct PackageSettings;
struct PackageId;
struct UnresolvedPackageId;
struct ResolveRequest;
struct Resolver;
struct settings_hash;

struct SW_SUPPORT_API PackageSettings
{
    enum StringType : int
    {
        KeyValue = 0,

        Json,
        // yml

        Simple = KeyValue,
    };

    PackageSettings() = default;
    PackageSettings(const PackageSettings &) = default;
    PackageSettings &operator=(const PackageSettings &) = default;
    ~PackageSettings();

    PackageSetting &operator[](const PackageSettingKey &);
    const PackageSetting &operator[](const PackageSettingKey &) const;

    void mergeMissing(const PackageSettings &);
    void mergeAndAssign(const PackageSettings &);
    void erase(const PackageSettingKey &);

    // other merges
    void mergeFromString(const String &s, int type = Json);
    void mergeFromJson(const nlohmann::json &);

    settings_hash getHash() const;
    String getHashString() const;
    static String getHashString(const String &);
    String toString(int type = Json) const;

    bool operator==(const PackageSettings &) const;
    bool isSubsetOf(const PackageSettings &) const;

    auto begin() { return settings.begin(); }
    auto end() { return settings.end(); }
    auto begin() const { return settings.begin(); }
    auto end() const { return settings.end(); }

    bool empty() const;

private:
    std::map<PackageSettingKey, PackageSetting> settings;

    //String toStringKeyValue() const;
    nlohmann::json toJson() const;
    settings_hash getHash1() const;

    friend struct PackageSetting;

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;
    template <class Ar>
    void serialize(Ar &ar, unsigned)
    {
        ar &settings;
    }
#endif
};

struct SW_SUPPORT_API PackageId
{
    PackageName n;
    settings_hash h;

    const auto &getName() const { return n; }
    const auto &getHash() const { return h; }

    // maybe also print settings hash
    String toString() const { return n.toString(); }

    // does not work with SW_SUPPORT_API
    //auto operator<=>(const PackageId &) const = default;

    //bool operator==(const PackageId &rhs) const { return std::tie(n, s) == std::tie(rhs.n, rhs.s); }
    bool operator==(const PackageId &rhs) const { return std::tie(n, h) == std::tie(rhs.n, rhs.h); }
};

struct SW_SUPPORT_API PackageIdFull
{
    PackageName n;
    PackageSettings s;

    const auto &getName() const { return n; }
    const auto &getSettings() const { return s; }

    // maybe also print settings hash
    String toString() const { return n.toString(); }

    // does not work with SW_SUPPORT_API
    //auto operator<=>(const PackageId &) const = default;

    bool operator==(const PackageIdFull &rhs) const { return std::tie(n, s) == std::tie(rhs.n, rhs.s); }
    //bool operator==(const PackageId &rhs) const { return std::tie(n, h) == std::tie(rhs.n, rhs.h); }
};

} // namespace sw

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackageName>()(p.getName());
        return hash_combine(h, (uint64_t)p.getHash());
    }
};

template<> struct hash<::sw::PackageIdFull>
{
    size_t operator()(const ::sw::PackageIdFull &p) const
    {
        auto h = std::hash<::sw::PackageName>()(p.getName());
        return hash_combine(h, (uint64_t)p.getSettings().getHash());
    }
};

template<> struct hash<::sw::PackageSettings>
{
    size_t operator()(const ::sw::PackageSettings &p) const
    {
        return p.getHash();
    }
};

}

namespace sw
{

struct SW_SUPPORT_API PackageSetting
{
    struct nulltag_t
    {
        bool operator==(const nulltag_t &) const { return true; }
    };

    struct abs_path : path
    {
        abs_path(const path &);
    };

    using Value = PackageSettingValue;
    using Map = PackageSettings;
    using ArrayValue = PackageSetting;
    using Array = std::vector<ArrayValue>;
    using ResolverType = std::unordered_map<UnresolvedPackageName, std::unordered_map<PackageSettings, PackageIdFull>>;
    using Empty = std::monostate;
    using Null = nulltag_t;
    using Path = path;

    using Variant = std::variant<
        Empty, Null,
        bool, int64_t, double, String,
        Path,
        Array, Map, ResolverType,
        PackagePath, PackageVersion, PackageName, PackageVersionRange, UnresolvedPackageName
    >;

    PackageSetting() = default;
    PackageSetting(const PackageSetting &) = default;
    PackageSetting &operator=(const PackageSetting &) = default;

    template <class U>
    PackageSetting(const U &u)
    {
        operator=(u);
    }

    template <class U>
    PackageSetting &operator=(const U &u)
    {
        if constexpr (std::is_same_v<U, std::nullptr_t>)
        {
            setNull();
            return *this;
        }
        reset();
        value = u;
        return *this;
    }

    PackageSetting &operator[](const PackageSettingKey &k);
    const PackageSetting &operator[](const PackageSettingKey &k) const;

    bool operator==(const PackageSetting &) const;

    template <class U>
    bool operator==(U &&u) const
    {
        auto v = std::get_if<U>(&value);
        return v && *v == u;
    }

    template <class T>
    bool is() const
    {
        return std::get_if<T>(&value);
    }

    template <class T>
    T &get()
    {
        return std::get<T>(value);
    }

    template <class T>
    const T &get() const
    {
        return std::get<T>(value);
    }

    explicit operator bool() const;
    bool hasValue() const;

    bool isEmpty() const;
    bool isNull() const;
    void setNull();

    const String &getValue() const;
    const Array &getArray() const;
    Map &getMap();
    const Map &getMap() const;

    // path helpers
    path getPathValue(const Directories &) const;
    path getPathValue(const path &root) const;
    void setPathValue(const Directories &, const path &value);
    void setPathValue(const path &root, const path &value);
    path getAbsolutePathValue() const;
    void setAbsolutePathValue(const path &value);
    std::optional<PackageIdFull> resolve(const ResolveRequest &) const;
    void addResolvedPackage(const UnresolvedPackageName &, const PackageSettings &, const PackageIdFull &);
    void setResolver();
    //auto &getResolver() { return std::get<ResolverType>(value); }
    //const auto &getResolver() const { return std::get<ResolverType>(value); }

    void push_back(const ArrayValue &);
    void reset();

    void ignoreInComparison(bool);
    bool ignoreInComparison() const { return ignore_in_comparison; }

    void mergeAndAssign(const PackageSetting &);
    void mergeMissing(const PackageSetting &);
    void mergeFromJson(const nlohmann::json &);

    bool isValue() const;
    bool isArray() const;
    bool isObject() const;
    bool isResolver() const;

private:
    bool ignore_in_comparison = false;
    Variant value;

    nlohmann::json toJson() const;
    settings_hash getHash1() const;

    friend struct PackageSettings;

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;

    template <class Ar>
    void load(Ar &ar, unsigned)
    {
        size_t idx;
        ar & idx;
        switch (idx)
        {
        case 0:
            break;
        case 1:
        {
            Value v;
            ar & v;
            value = v;
        }
            break;
        case 2:
        {
            Array v1;
            ar & v1;
            value = v1;
        }
            break;
        case 3:
        {
            Map v;
            ar & v;
            value = v;
        }
            break;
        }
    }
    template <class Ar>
    void save(Ar &ar, unsigned) const
    {
        if (ignore_in_comparison)
            return;
        ar & value.index();
        switch (value.index())
        {
        case 0:
            break;
        case 1:
            ar & std::get<Value>(value);
            break;
        case 2:
            ar & std::get<Array>(value);
            break;
        case 3:
            ar & std::get<Map>(value);
            break;
        }
    }
    BOOST_SERIALIZATION_SPLIT_MEMBER()
#endif
};

// serialization

SW_SUPPORT_API
PackageSettings loadSettings(const path &archive_fn, int type = 0);

SW_SUPPORT_API
void saveSettings(const path &archive_fn, const PackageSettings &, int type = 0);

} // namespace sw
