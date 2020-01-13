// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "settings.h"

#include <sw/builder/os.h>
#include <sw/support/hash.h>

#include <nlohmann/json.hpp>

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
    default:
        SW_UNIMPLEMENTED;
    }
    // do not specify, just takes max available
    //s["os"]["version"] = o.Version.toString();

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
}

TargetSetting &TargetSetting::operator=(const TargetSetting &rhs)
{
    value = rhs.value;
    copy_fields(rhs);
    return *this;
}

TargetSetting &TargetSetting::operator=(const TargetSettings &u)
{
    value = u;
    return *this;
}

TargetSetting &TargetSetting::operator[](const TargetSettingKey &k)
{
    if (value.index() == 0)
        value = TargetSettings();
    return std::get<TargetSettings>(value)[k];
}

const TargetSetting &TargetSetting::operator[](const TargetSettingKey &k) const
{
    if (value.index() != 3)
    {
        thread_local TargetSetting s;
        return s;
    }
    return std::get<TargetSettings>(value)[k];
}

const String &TargetSetting::getValue() const
{
    auto v = std::get_if<TargetSettingValue>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty value");
    return *v;
}

const std::vector<TargetSettingValue> &TargetSetting::getArray() const
{
    if (value.index() == 0)
    {
        thread_local std::vector<TargetSettingValue> s;
        return s;
    }
    auto v = std::get_if<std::vector<TargetSettingValue>>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty array");
    return *v;
}

TargetSettings &TargetSetting::getSettings()
{
    auto s = std::get_if<TargetSettings>(&value);
    if (!s)
    {
        if (value.index() != 0)
            throw SW_RUNTIME_ERROR("Not settings");
        *this = TargetSettings();
        s = std::get_if<TargetSettings>(&value);
    }
    return *s;
}

const TargetSettings &TargetSetting::getSettings() const
{
    auto s = std::get_if<TargetSettings>(&value);
    if (!s)
    {
        thread_local const TargetSettings ts;
        return ts;
    }
    return *s;
}

bool TargetSetting::operator==(const TargetSetting &rhs) const
{
    if (ignore_in_comparison)
        return true;
    return value == rhs.value;
}

bool TargetSetting::operator!=(const TargetSetting &rhs) const
{
    return !operator==(rhs);
}

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

void TargetSetting::mergeMissing(const TargetSetting &rhs)
{
    auto s = std::get_if<TargetSettings>(&value);
    if (s)
    {
        s->mergeMissing(std::get<TargetSettings>(rhs.value));
        return;
    }
    if (value.index() == 0)
    {
        value = rhs.value;
        copy_fields(rhs);
    }
}

void TargetSetting::mergeAndAssign(const TargetSetting &rhs)
{
    auto s = std::get_if<TargetSettings>(&value);
    if (s)
    {
        s->mergeAndAssign(std::get<TargetSettings>(rhs.value));
        return;
    }
    value = rhs.value;
    copy_fields(rhs);
}

void TargetSetting::mergeFromJson(const nlohmann::json &j)
{
    if (j.is_object())
    {
        auto v = std::get_if<TargetSettings>(&value);
        if (!v)
        {
            operator=(TargetSettings());
            v = std::get_if<TargetSettings>(&value);
        }
        v->mergeFromJson(j);
        return;
    }

    if (j.is_array())
    {
        auto v = std::get_if<std::vector<TargetSettingValue>>(&value);
        if (!v)
        {
            operator=(std::vector<TargetSettingValue>());
            v = std::get_if<std::vector<TargetSettingValue>>(&value);
        }
        v->clear();
        for (auto &e : j)
            v->push_back(e);
        return;
    }

    if (j.is_string())
    {
        operator=(j.get<String>());
        return;
    }

    if (j.is_null())
    {
        reset();
        return;
    }

    throw SW_RUNTIME_ERROR("Bad json value. Only objects, arrays and strings are currently accepted.");
}

bool TargetSetting::isValue() const
{
    return std::get_if<TargetSettingValue>(&value);
}

bool TargetSetting::isArray() const
{
    return std::get_if<std::vector<TargetSettingValue>>(&value);
}

bool TargetSetting::isObject() const
{
    return std::get_if<TargetSettings>(&value);
}

void TargetSetting::push_back(const TargetSettingValue &v)
{
    if (value.index() == 0)
        value = std::vector<TargetSettingValue>();
    return std::get<std::vector<TargetSettingValue>>(value).push_back(v);
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
    return value.index() != 0;
}

String TargetSettings::getConfig() const
{
    return toString();
}

String TargetSettings::getHash() const
{
    return shorten_hash(blake2b_512(getConfig()), 6);
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
    if (!used_in_hash)
        return j;
    switch (value.index())
    {
    case 0:
        return j;
    case 1:
        return getValue();
    case 2:
        for (auto &v2 : std::get<std::vector<TargetSettingValue>>(value))
            j.push_back(v2);
        break;
    case 3:
        return std::get<TargetSettings>(value).toJson();
    }
    return j;
}

nlohmann::json TargetSettings::toJson() const
{
    nlohmann::json j;
    for (auto &[k, v] : *this)
    {
        auto j2 = v.toJson();
        if (!j2.is_null())
            j[k] = j2;
    }
    return j;
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
            if (v.value.index() == 0)
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
            if (v.value.index() == 0)
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
        if (v.value.index() == 0)
            continue;
        // ignore -> ok
        if (v.ignoreInComparison())
            continue;

        auto i = s.settings.find(k);
        if (i == s.settings.end() || i->second.value.index() == 0)
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
