// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settings.h"

#include "hash.h"
#include "storage.h"

#include <nlohmann/json.hpp>
#include <pystring.h>

namespace sw
{

PackageSetting::PackageSetting(const PackageSetting &rhs)
{
    operator=(rhs);
}

void PackageSetting::copy_fields(const PackageSetting &rhs)
{
    required = rhs.required;
    use_count = rhs.use_count;
    used_in_hash = rhs.used_in_hash;
    ignore_in_comparison = rhs.ignore_in_comparison;
    serializable_ = rhs.serializable_;
}

bool PackageSetting::isEmpty() const
{
    return value.index() == 0;
}

bool PackageSetting::isNull() const
{
    return value.index() == 4;
}

void PackageSetting::setNull()
{
    *this = NullType{};
}

PackageSetting &PackageSetting::operator=(const PackageSetting &rhs)
{
    // if we see an option which was consumed, we do not copy, just reset this
    if (rhs.use_count == 0)
    {
        reset();
        return *this;
    }
    value = rhs.value;
    copy_fields(rhs);
    return *this;
}

PackageSetting &PackageSetting::operator[](const PackageSettingKey &k)
{
    if (value.index() == 0)
    {
        if (!isEmpty())
            throw SW_RUNTIME_ERROR("key is not a map (null)");
        *this = Map();
    }
    return std::get<Map>(value)[k];
}

const PackageSetting &PackageSetting::operator[](const PackageSettingKey &k) const
{
    if (value.index() != 3)
    {
        thread_local PackageSetting s;
        return s;
    }
    return std::get<Map>(value)[k];
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
    if (value.index() == 0)
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
    auto s = std::get_if<Map>(&value);
    if (!s)
    {
        if (value.index() != 0)
            throw SW_RUNTIME_ERROR("Not settings");
        *this = Map();
        s = std::get_if<Map>(&value);
    }
    return *s;
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
        *this = to_string(normalize_path(value.lexically_relative(root)));
    else
        setAbsolutePathValue(value);
}

path PackageSetting::getAbsolutePathValue() const
{
    return (const char8_t *)getValue().c_str();
}

void PackageSetting::setAbsolutePathValue(const path &value)
{
    *this = to_string(normalize_path(value));
}

bool PackageSetting::operator==(const PackageSetting &rhs) const
{
    if (ignore_in_comparison)
        return true;
    return value == rhs.value;
}

/*bool PackageSetting::operator!=(const PackageSetting &rhs) const
{
    return !operator==(rhs);
}*/

bool PackageSetting::operator<(const PackageSetting &rhs) const
{
    return value < rhs.value;
}

/*String PackageSetting::getHash() const
{
}*/

void PackageSetting::useInHash(bool b)
{
    used_in_hash = b;
}

void PackageSetting::ignoreInComparison(bool b)
{
    ignore_in_comparison = b;
}

// rename to serializable?
void PackageSetting::serializable(bool b)
{
    serializable_ = b;

    // not serializing means no round trip,
    // so it cannot be used in hash and
    // must be ignored in comparisons
    if (!serializable())
    {
        useInHash(false);
        ignoreInComparison(true);
    }
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
    if (j.is_object())
    {
        auto v = std::get_if<Map>(&value);
        if (!v)
        {
            *this = Map();
            v = std::get_if<Map>(&value);
        }
        v->mergeFromJson(j);
        return;
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
        for (auto &e : j)
        {
            PackageSetting s;
            s.mergeFromJson(e);
            v->push_back(s);
        }
        return;
    }

    if (j.is_string())
    {
        *this = j.get<String>();
        return;
    }

    if (j.is_null())
    {
        setNull();
        return;
    }

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

void PackageSetting::push_back(const ArrayValue &v)
{
    if (value.index() == 0)
    {
        if (!isEmpty())
            throw SW_RUNTIME_ERROR("key is not an array (null)");
        *this = Array();
    }
    return std::get<Array>(value).push_back(v);
}

void PackageSetting::reset()
{
    PackageSetting s;
    *this = s;
}

void PackageSetting::use()
{
    if (use_count > 0)
        use_count--;
    if (use_count == 0)
        reset();
}

void PackageSetting::setUseCount(int c)
{
    use_count = c;
}

void PackageSetting::setRequired(bool b)
{
    required = b;
}

bool PackageSetting::isRequired() const
{
    return required;
}

PackageSetting::operator bool() const
{
    return !isEmpty();
}

String PackageSettings::getHash() const
{
    return shorten_hash(std::to_string(getHash1()), 6);
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
    switch (value.index())
    {
    case 0:
        return j;
    case 1:
        return getValue();
    case 2:
        for (auto &v2 : std::get<Array>(value))
            j.push_back(v2.toJson());
        break;
    case 3:
        return std::get<Map>(value).toJson();
    case 4:
        return nullptr;
    default:
        SW_UNREACHABLE;
    }
    return j;
}

nlohmann::json PackageSettings::toJson() const
{
    nlohmann::json j;
    for (auto &[k, v] : *this)
    {
        if (!v.serializable())
            continue;
        auto j2 = v.toJson();
        if (j2.is_null() && !v.isNull())
            continue;
        j[k] = j2;
        if (!v.used_in_hash)
            j[k + "_used_in_hash"] = "false";
        if (v.ignore_in_comparison)
            j[k + "_ignore_in_comparison"] = "true";
    }
    return j;
}

size_t PackageSetting::getHash1() const
{
    size_t h = 0;
    switch (value.index())
    {
    case 0:
        return h;
    case 1:
        return hash_combine(h, getValue());
    case 2:
        for (auto &v2 : std::get<Array>(value))
            hash_combine(h, v2.getHash1());
        break;
    case 3:
        return hash_combine(h, std::get<Map>(value).getHash1());
    case 4:
        return hash_combine(h, h); // combine 0 and 0
    default:
        SW_UNREACHABLE;
    }
    return h;
}

size_t PackageSettings::getHash1() const
{
    size_t h = 0;
    for (auto &[k, v] : *this)
    {
        if (!v.used_in_hash)
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

bool PackageSettings::operator<(const PackageSettings &rhs) const
{
    return settings < rhs.settings;
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
    {
        if (pystring::endswith(it.key(), "_used_in_hash"))
        {
            if (it.value().get<String>() == "false")
                (*this)[it.key().substr(0, it.key().size() - strlen("_used_in_hash"))].used_in_hash = false;
            continue;
        }
        if (pystring::endswith(it.key(), "_ignore_in_comparison"))
        {
            if (it.value().get<String>() == "true")
                (*this)[it.key().substr(0, it.key().size() - strlen("_ignore_in_comparison"))].ignore_in_comparison = true;
            continue;
        }
        (*this)[it.key()].mergeFromJson(it.value());
    }
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
