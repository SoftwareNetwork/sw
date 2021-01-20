// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settings.h"

#include "hash.h"
#include "resolver.h"
#include "storage.h"

#include <primitives/overload.h>

#include <nlohmann/json.hpp>
#include <pystring.h>

namespace sw
{

bool PackageSetting::isEmpty() const
{
    return std::get_if<Empty>(&value);
}

bool PackageSetting::isNull() const
{
    return std::get_if<Null>(&value);
}

void PackageSetting::setNull()
{
    //reset();
    value = Null{};
}

PackageSetting &PackageSetting::operator[](const PackageSettingKey &k)
{
    if (isEmpty())
        *this = Map();
    return std::get<Map>(value)[k];
}

const PackageSetting &PackageSetting::operator[](const PackageSettingKey &k) const
{
    auto v = std::get_if<Map>(&value);
    if (!v)
    {
        thread_local PackageSetting s;
        return s;
    }
    return (*v)[k];
}

const String &PackageSetting::getValue() const
{
    auto v = std::get_if<Value>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty value");
    return *v;
}

const PackageSetting::Array &PackageSetting::getArray() const
{
    if (isEmpty())
    {
        thread_local Array s;
        return s;
    }
    auto v = std::get_if<Array>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty array");
    return *v;
}

PackageSetting::Map &PackageSetting::getMap()
{
    auto v = std::get_if<Map>(&value);
    if (!v)
    {
        if (!isEmpty())
            throw SW_RUNTIME_ERROR("Not settings");
        *this = Map();
        v = std::get_if<Map>(&value);
    }
    return *v;
}

const PackageSetting::Map &PackageSetting::getMap() const
{
    auto s = std::get_if<Map>(&value);
    if (!s)
    {
        thread_local const Map ts;
        return ts;
    }
    return *s;
}

static const path &get_root_dir(const Directories &d)
{
    return d.storage_dir;
}

path PackageSetting::getPathValue(const Directories &d) const
{
    return getPathValue(get_root_dir(d));
}

path PackageSetting::getPathValue(const path &root) const
{
    return normalize_path(root / getAbsolutePathValue());
}

void PackageSetting::setPathValue(const Directories &d, const path &value)
{
    setPathValue(get_root_dir(d), value);
}

void PackageSetting::setPathValue(const path &root, const path &value)
{
    if (is_under_root_by_prefix_path(value, root))
        *this = normalize_path(value.lexically_relative(root));
    else
        setAbsolutePathValue(value);
}

path PackageSetting::getAbsolutePathValue() const
{
    return get<path>();
}

void PackageSetting::setAbsolutePathValue(const path &value)
{
    *this = normalize_path(value);
}

::sw::Resolver &PackageSetting::getResolver() const
{
    return *get<Resolver>();
}

bool PackageSetting::resolve(ResolveRequest &rr) const
{
    return getResolver().resolve(rr);
}

bool PackageSetting::operator==(const PackageSetting &rhs) const
{
    if (ignore_in_comparison)
        return true;
    return value == rhs.value;
}

void PackageSetting::ignoreInComparison(bool b)
{
    ignore_in_comparison = b;
}

void PackageSetting::mergeMissing(const PackageSetting &rhs)
{
    auto s = std::get_if<Map>(&value);
    if (s)
    {
        s->mergeMissing(std::get<Map>(rhs.value));
        return;
    }
    if (isEmpty())
        *this = rhs;
}

void PackageSetting::mergeAndAssign(const PackageSetting &rhs)
{
    auto s = std::get_if<Map>(&value);
    if (s)
    {
        s->mergeAndAssign(std::get<Map>(rhs.value));
        return;
    }
    *this = rhs;
}

void PackageSetting::mergeFromJson(const nlohmann::json &j)
{
    if (!j.is_object())
        throw SW_RUNTIME_ERROR("Not an object");

    if (j.contains("properties"))
        SW_UNIMPLEMENTED;

    int i = j["index"];
    switch (i)
    {
    case 1:
        return;
#define GET_RAW(n)                                                        \
    case n:                                                               \
        *this = j["value"].get<std::variant_alternative_t<n, Variant>>(); \
        return
    GET_RAW(2);
    GET_RAW(3);
    GET_RAW(4);
    GET_RAW(5);
#undef GET_RAW
    case 6:
        *this = path((const char8_t *)j["value"].get<String>().c_str());
        return;
    case 7:
    {
        Array a;
        for (auto &e : j["value"])
        {
            PackageSetting s;
            s.mergeFromJson(e);
            a.push_back(s);
        }
        *this = a;
    }
        return;
    case 8:
        getMap().mergeFromJson(j["value"]);
        return;
    case 9:
        //SW_UNIMPLEMENTED;
        return;
#define GET_RAW(n)                                                                \
    case n:                                                                       \
        *this = std::variant_alternative_t<n, Variant>(j["value"].get<String>()); \
        return
    GET_RAW(10);
    GET_RAW(11);
    GET_RAW(12);
    GET_RAW(13);
    GET_RAW(14);
#undef GET_RAW
    default:
        SW_UNIMPLEMENTED;
    }

    if (j.is_array())
    {
        auto v = std::get_if<Array>(&value);
        if (!v)
        {
            *this = Array();
            v = std::get_if<Array>(&value);
        }
        v->clear();
        return;
    }

    /*if (pystring::endswith(it.key(), "_ignore_in_comparison"))
    {
        if (it.value().get<String>() == "true")
            (*this)[it.key().substr(0, it.key().size() - strlen("_ignore_in_comparison"))].ignore_in_comparison = true;
        continue;
    }*/

    throw SW_RUNTIME_ERROR("Bad json value. Only objects, arrays and strings are currently accepted.");
}

bool PackageSetting::isValue() const
{
    return std::get_if<Value>(&value);
}

bool PackageSetting::isArray() const
{
    return std::get_if<Array>(&value);
}

bool PackageSetting::isObject() const
{
    return std::get_if<Map>(&value);
}

bool PackageSetting::isResolver() const
{
    SW_UNIMPLEMENTED;
    //return !!resolver;
}

void PackageSetting::push_back(const ArrayValue &v)
{
    if (isEmpty())
        *this = Array();
    return std::get<Array>(value).push_back(v);
}

void PackageSetting::reset()
{
    PackageSetting s;
    *this = s;
}

PackageSetting::operator bool() const
{
    return !isEmpty() || *this == true;
}

bool PackageSetting::hasValue() const
{
    return !isEmpty();
}

PackageSettings::~PackageSettings() {}

size_t PackageSettings::getHash() const
{
    return getHash1();
}

String PackageSettings::getHashString(const String &s)
{
    return shorten_hash(s, 6);
}

String PackageSettings::getHashString() const
{
    return getHashString(std::to_string(getHash()));
}

void PackageSettings::mergeFromString(const String &s, int type)
{
    switch (type)
    {
    case Json:
    {
        auto j = nlohmann::json::parse(s);
        mergeFromJson(j);
    }
        break;
    default:
        SW_UNIMPLEMENTED;
    }
}

String PackageSettings::toString(int type) const
{
    switch (type)
    {
    case Json:
        return toJson().dump();
    default:
        SW_UNIMPLEMENTED;
    }
}

nlohmann::json PackageSetting::toJson() const
{
    nlohmann::json j;
    j["index"] = value.index();
    auto o = overload(
        [](const Empty &) { return nlohmann::json{}; },
        [](const Null &) -> nlohmann::json { return nullptr; },
        [](const Array &a) {
            nlohmann::json j;
            for (auto &v2 : a)
                j.push_back(v2.toJson());
            return j;
        },
        [](const Map &m) { return m.toJson(); },
        [](const path &p) -> nlohmann::json { return to_printable_string(p); },
        [](const Resolver &r) -> nlohmann::json { return {}; },
        [](auto &&v) -> nlohmann::json {
            if constexpr (0
                || std::is_same_v<std::decay_t<decltype(v)>, PackagePath>
                || std::is_same_v<std::decay_t<decltype(v)>, PackageVersion>
                || std::is_same_v<std::decay_t<decltype(v)>, PackageName>
                || std::is_same_v<std::decay_t<decltype(v)>, PackageVersionRange>
                || std::is_same_v<std::decay_t<decltype(v)>, UnresolvedPackageName>
                )
                return v.toString();
            else
                return v;
        });
    j["value"] = std::visit(o, value);
    if (ignore_in_comparison)
        j["protperties"]["ignore_in_comparison"] = true;
    return j;
}

nlohmann::json PackageSettings::toJson() const
{
    nlohmann::json j;
    for (auto &[k, v] : *this)
    {
        if (v.ignoreInComparison() || v.isEmpty())
            continue;
        j[k] = v.toJson();
    }
    return j;
}

size_t PackageSetting::getHash1() const
{
    auto o = overload(
        [](const Empty &) -> size_t { return 0; },
        [](const Null &) {
            size_t h = 0;
            return hash_combine(h, h); // combine 0 and 0
        },
        [](const Array &a) {
            size_t h = 0;
            for (auto &v2 : a)
                hash_combine(h, v2.getHash1());
            return h;
        },
        [](const Map &m) {
            size_t h = 0;
            return hash_combine(h, m.getHash1());
        },
        [](const path &p) -> size_t { SW_UNIMPLEMENTED; },
        [](const Resolver &r) -> size_t { return {}; },
        [](auto &&v) -> size_t {
            size_t h = 0;
            return hash_combine(h, std::hash<std::decay_t<decltype(v)>>()(v));
        });
    return std::visit(o, value);
}

size_t PackageSettings::getHash1() const
{
    size_t h = 0;
    for (auto &[k, v] : *this)
    {
        if (v.ignoreInComparison())
            continue;
        auto h2 = v.getHash1();
        if (h2 == 0)
            continue;
        hash_combine(h, k);
        hash_combine(h, h2);
    }
    return h;
}

PackageSetting &PackageSettings::operator[](const PackageSettingKey &k)
{
    return settings.try_emplace(k, PackageSetting{}).first->second;
}

const PackageSetting &PackageSettings::operator[](const PackageSettingKey &k) const
{
    auto i = settings.find(k);
    if (i == settings.end())
    {
        thread_local PackageSetting s;
        return s;
    }
    return i->second;
}

bool PackageSettings::operator==(const PackageSettings &rhs) const
{
    for (auto &[k, v] : rhs.settings)
    {
        if (v.ignoreInComparison())
            continue;
        auto i = settings.find(k);
        if (i == settings.end())
        {
            if (!v)
                continue;
            return false;
        }
        if (i->second != v)
            return false;
    }

    // check the rest of this settings
    for (auto &[k, v] : settings)
    {
        if (v.ignoreInComparison())
            continue;
        auto i = rhs.settings.find(k);
        if (i == rhs.settings.end())
        {
            if (!v)
                continue;
            return false;
        }
    }
    return true;
}

bool PackageSettings::isSubsetOf(const PackageSettings &s) const
{
    for (auto &[k, v] : settings)
    {
        // value is missing -> ok
        if (!v)
            continue;
        // ignore -> ok
        if (v.ignoreInComparison())
            continue;

        auto i = s.settings.find(k);
        if (i == s.settings.end() || !i->second)
            return false;

        auto lv = std::get_if<PackageSettings>(&v.value);
        auto rv = std::get_if<PackageSettings>(&i->second.value);
        if (lv && rv)
        {
            if (!lv->isSubsetOf(*rv))
                return false;
            continue;
        }

        if (i->second != v)
            return false;
    }
    return true;
}

void PackageSettings::mergeMissing(const PackageSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].mergeMissing(v);
}

void PackageSettings::mergeAndAssign(const PackageSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].mergeAndAssign(v);
}

void PackageSettings::mergeFromJson(const nlohmann::json &j)
{
    if (!j.is_object())
        throw SW_RUNTIME_ERROR("Not an object");
    for (auto it = j.begin(); it != j.end(); ++it)
        (*this)[it.key()].mergeFromJson(it.value());
}

void PackageSettings::erase(const PackageSettingKey &k)
{
    settings.erase(k);
}

bool PackageSettings::empty() const
{
    return settings.empty();
}

} // namespace sw
