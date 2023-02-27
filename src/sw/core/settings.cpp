// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "settings.h"

#include <sw/builder/os.h>
#include <sw/manager/storage.h>
#include <sw/support/hash.h>

#include <nlohmann/json.hpp>
#include <pystring.h>

namespace sw
{

TargetSettings toTargetSettings(const OS &o)
{
    TargetSettings s;
    switch (o.Type)
    {
    case OSType::Windows:
        s["os"]["kernel"] = "com.Microsoft.Windows.NT";
        break;
    case OSType::Linux:
        s["os"]["kernel"] = "org.torvalds.linux";
        break;
    case OSType::Macos:
        s["os"]["kernel"] = "com.Apple.Macos";
        break;
    case OSType::Darwin:
        s["os"]["kernel"] = "com.Apple.Darwin";
        break;
    case OSType::IOS:
        s["os"]["kernel"] = "com.Apple.Ios";
        break;
    case OSType::Cygwin:
        s["os"]["kernel"] = "org.cygwin";
        break;
    case OSType::Mingw:
        s["os"]["kernel"] = "org.mingw";
        break;
    case OSType::Wasm:
        s["os"]["kernel"] = "org.emscripten";
        break;
    default:
        SW_UNIMPLEMENTED;
    }
    // do not specify, just takes max available
    //s["os"]["version"] = o.Version.toString();

    if (o.Android) {
        s["os"]["kernel"] = "com.google.android";
    }
    if (o.Mingw) {
        s["os"]["kernel"] = "org.mingw";
    }

    switch (o.Arch)
    {
    case ArchType::x86:
        s["os"]["arch"] = "x86";
        break;
    case ArchType::x86_64:
        s["os"]["arch"] = "x86_64";
        break;
    case ArchType::arm:
        s["os"]["arch"] = "arm";
        break;
    case ArchType::aarch64:
        s["os"]["arch"] = "aarch64";
        break;
    case ArchType::wasm64:
        s["os"]["arch"] = "wasm64";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    /*switch (o.SubArch)
    {
    case SubArchType:::
    s["os"]["subarch"] = "";
    break;
    default:
    SW_UNIMPLEMENTED;
    }*/

    switch (o.EnvType)
    {
    case EnvironmentType::GNUEABI:
        s["os"]["environment"] = "gnueabi";
        break;
    case EnvironmentType::GNUEABIHF:
        s["os"]["environment"] = "gnueabihf";
        break;
    }

    // we might not have sdk version installed
    //s["os"]["version"] = o.Version.toString();

    return s;
}

TargetSetting::TargetSetting(const TargetSetting &rhs)
{
    operator=(rhs);
}

void TargetSetting::copy_fields(const TargetSetting &rhs)
{
    required = rhs.required;
    use_count = rhs.use_count;
    used_in_hash = rhs.used_in_hash;
    ignore_in_comparison = rhs.ignore_in_comparison;
    serializable_ = rhs.serializable_;
}

bool TargetSetting::isEmpty() const
{
    return value.index() == 0;
}

bool TargetSetting::isNull() const
{
    return value.index() == 4;
}

void TargetSetting::setNull()
{
    *this = NullType{};
}

TargetSetting &TargetSetting::operator=(const TargetSetting &rhs)
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

TargetSetting &TargetSetting::operator[](const TargetSettingKey &k)
{
    if (value.index() == 0)
    {
        if (!isEmpty())
            throw SW_RUNTIME_ERROR("key is not a map (null)");
        *this = Map();
    }
    return std::get<Map>(value)[k];
}

const TargetSetting &TargetSetting::operator[](const TargetSettingKey &k) const
{
    if (value.index() != 3)
    {
        thread_local TargetSetting s;
        return s;
    }
    return std::get<Map>(value)[k];
}

const String &TargetSetting::getValue() const
{
    auto v = std::get_if<Value>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty value");
    return *v;
}

const TargetSetting::Array &TargetSetting::getArray() const
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

TargetSetting::Map &TargetSetting::getMap()
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

const TargetSetting::Map &TargetSetting::getMap() const
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

path TargetSetting::getPathValue(const Directories &d) const
{
    return getPathValue(get_root_dir(d));
}

path TargetSetting::getPathValue(const path &root) const
{
    return normalize_path(root / getAbsolutePathValue());
}

void TargetSetting::setPathValue(const Directories &d, const path &value)
{
    setPathValue(get_root_dir(d), value);
}

void TargetSetting::setPathValue(const path &root, const path &value)
{
    if (is_under_root_by_prefix_path(value, root))
        *this = to_string(normalize_path(value.lexically_relative(root)));
    else
        setAbsolutePathValue(value);
}

path TargetSetting::getAbsolutePathValue() const
{
    return fs::u8path(getValue());
}

void TargetSetting::setAbsolutePathValue(const path &value)
{
    *this = to_string(normalize_path(value));
}

bool TargetSetting::operator==(const TargetSetting &rhs) const
{
    if (ignore_in_comparison)
        return true;
    return value == rhs.value;
}

/*bool TargetSetting::operator!=(const TargetSetting &rhs) const
{
    return !operator==(rhs);
}*/

bool TargetSetting::operator<(const TargetSetting &rhs) const
{
    return value < rhs.value;
}

/*String TargetSetting::getHash() const
{
}*/

void TargetSetting::useInHash(bool b)
{
    used_in_hash = b;
}

void TargetSetting::ignoreInComparison(bool b)
{
    ignore_in_comparison = b;
}

// rename to serializable?
void TargetSetting::serializable(bool b)
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

void TargetSetting::mergeMissing(const TargetSetting &rhs)
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

void TargetSetting::mergeAndAssign(const TargetSetting &rhs)
{
    auto s = std::get_if<Map>(&value);
    if (s)
    {
        s->mergeAndAssign(std::get<Map>(rhs.value));
        return;
    }
    *this = rhs;
}

void TargetSetting::mergeFromJson(const nlohmann::json &j)
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
            TargetSetting s;
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

bool TargetSetting::isValue() const
{
    return std::get_if<Value>(&value);
}

bool TargetSetting::isArray() const
{
    return std::get_if<Array>(&value);
}

bool TargetSetting::isObject() const
{
    return std::get_if<Map>(&value);
}

void TargetSetting::push_back(const ArrayValue &v)
{
    if (value.index() == 0)
    {
        if (!isEmpty())
            throw SW_RUNTIME_ERROR("key is not an array (null)");
        *this = Array();
    }
    return std::get<Array>(value).push_back(v);
}

void TargetSetting::reset()
{
    TargetSetting s;
    *this = s;
}

void TargetSetting::use()
{
    if (use_count > 0)
        use_count--;
    if (use_count == 0)
        reset();
}

void TargetSetting::setUseCount(int c)
{
    use_count = c;
}

void TargetSetting::setRequired(bool b)
{
    required = b;
}

bool TargetSetting::isRequired() const
{
    return required;
}

TargetSetting::operator bool() const
{
    return !isEmpty();
}

String TargetSettings::getHash() const
{
    return shorten_hash(std::to_string(getHash1()), 6);
}

void TargetSettings::mergeFromString(const String &s, int type)
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

String TargetSettings::toString(int type) const
{
    switch (type)
    {
    case Json:
        return toJson().dump();
    default:
        SW_UNIMPLEMENTED;
    }
}

nlohmann::json TargetSetting::toJson() const
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

nlohmann::json TargetSettings::toJson() const
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

size_t TargetSetting::getHash1() const
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

size_t TargetSettings::getHash1() const
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

TargetSetting &TargetSettings::operator[](const TargetSettingKey &k)
{
    return settings.try_emplace(k, TargetSetting{}).first->second;
}

const TargetSetting &TargetSettings::operator[](const TargetSettingKey &k) const
{
    auto i = settings.find(k);
    if (i == settings.end())
    {
        thread_local TargetSetting s;
        return s;
    }
    return i->second;
}

bool TargetSettings::operator==(const TargetSettings &rhs) const
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

bool TargetSettings::operator<(const TargetSettings &rhs) const
{
    return settings < rhs.settings;
}

bool TargetSettings::isSubsetOf(const TargetSettings &s) const
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

        auto lv = std::get_if<TargetSettings>(&v.value);
        auto rv = std::get_if<TargetSettings>(&i->second.value);
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

void TargetSettings::mergeMissing(const TargetSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].mergeMissing(v);
}

void TargetSettings::mergeAndAssign(const TargetSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].mergeAndAssign(v);
}

void TargetSettings::mergeFromJson(const nlohmann::json &j)
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

void TargetSettings::erase(const TargetSettingKey &k)
{
    settings.erase(k);
}

bool TargetSettings::empty() const
{
    return settings.empty();
}

} // namespace sw
