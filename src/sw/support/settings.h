// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

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
struct Resolver;
struct ResolveRequest;

struct SW_SUPPORT_API PackageSettings
{
    enum StringType : int
    {
        KeyValue    = 0,

        Json,
        // yml

        Simple      = KeyValue,
    };

    ~PackageSettings();

    PackageSetting &operator[](const PackageSettingKey &);
    const PackageSetting &operator[](const PackageSettingKey &) const;

    void mergeMissing(const PackageSettings &);
    void mergeAndAssign(const PackageSettings &);
    void erase(const PackageSettingKey &);

    // other merges
    void mergeFromString(const String &s, int type = Json);
    void mergeFromJson(const nlohmann::json &);

    size_t getHash() const;
    String getHashString() const;
    String toString(int type = Json) const;

    bool operator==(const PackageSettings &) const;
    bool operator<(const PackageSettings &) const;
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
    size_t getHash1() const;

    friend struct PackageSetting;

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;
    template <class Ar>
    void serialize(Ar &ar, unsigned)
    {
        ar & settings;
    }
#endif
};

struct SW_SUPPORT_API PackageSetting
{
    struct nulltag_t
    {
        bool operator==(const nulltag_t &) const { return true; }
        bool operator<(const nulltag_t &) const { return false; }
    };

    using Value = PackageSettingValue;
    using Map = PackageSettings;
    using ArrayValue = PackageSetting;
    using Array = std::vector<ArrayValue>;
    //using ResolverType = Resolver*;
    using ResolverType = std::unique_ptr<Resolver>;
    using ResolverPtr = ResolverType;
    using NullType = nulltag_t;
    // append only
    using Variant = std::variant<std::monostate, Value, Array, Map, NullType/*, ResolverPtr*/>;

    PackageSetting();
    PackageSetting(const Value &);
    /*PackageSetting(const Array &);
    PackageSetting(const Map &);*/
    PackageSetting(const path &);
    PackageSetting(ResolverType);
    PackageSetting(const PackageSetting &);
    PackageSetting &operator=(const PackageSetting &);
    PackageSetting(PackageSetting &&);
    PackageSetting &operator=(PackageSetting &&);
    ~PackageSetting();

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
    //bool operator!=(const PackageSetting &) const;
    bool operator<(const PackageSetting &) const;

    template <class U>
    bool operator==(const U &u) const
    {
        auto v = std::get_if<Value>(&value);
        if (!v)
            return false;
        return *v == u;
    }

    template <class U>
    bool operator!=(const U &u) const
    {
        return !operator==(u);
    }

    explicit operator bool() const;
    //bool hasValue() const;
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
    bool resolve(ResolveRequest &) const;

    void push_back(const ArrayValue &);
    void reset();

    void use();
    void setUseCount(int);

    void setRequired(bool = true);
    bool isRequired() const;

    void useInHash(bool);
    bool useInHash() const { return used_in_hash; }

    void ignoreInComparison(bool);
    bool ignoreInComparison() const { return ignore_in_comparison; }

    void serializable(bool);
    bool serializable() const { return serializable_; }

    void mergeAndAssign(const PackageSetting &);
    void mergeMissing(const PackageSetting &);
    void mergeFromJson(const nlohmann::json &);

    bool isValue() const;
    bool isArray() const;
    bool isObject() const;
    bool isResolver() const;

private:
    int use_count = 1;
    bool required = false;
    bool used_in_hash = true;
    bool ignore_in_comparison = false;
    bool serializable_ = true;
    // when adding new member, add it to copy_fields()!
    Variant value;
    ResolverType resolver;

    nlohmann::json toJson() const;
    size_t getHash1() const;
    void copy_fields(const PackageSetting &);

    friend struct PackageSettings;

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;

    template <class Ar>
    void load(Ar &ar, unsigned)
    {
        ar & used_in_hash;
        ar & ignore_in_comparison;
        //
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
        if (!serializable_)
            return;
        ar & used_in_hash;
        ar & ignore_in_comparison;
        //
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

namespace std
{

template<> struct hash<::sw::PackageSettings>
{
    size_t operator()(const ::sw::PackageSettings &p) const
    {
        return p.getHash();
    }
};

}
