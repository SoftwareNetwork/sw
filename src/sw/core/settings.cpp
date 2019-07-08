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
    default:
        SW_UNIMPLEMENTED;
    }
    s["os"]["version"] = o.Version.toString();

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

    // no version at the moment
    // it is not clear if it's needed

    return s;
}

TargetSetting::TargetSetting(const TargetSettingKey &k) : key(k)
{
}

TargetSetting::TargetSetting(const TargetSetting &rhs)
{
    operator=(rhs);
}

TargetSetting &TargetSetting::operator=(const TargetSetting &rhs)
{
    key = rhs.key;
    value = rhs.value;
    array = rhs.array;
    if (rhs.settings)
        settings = std::make_unique<TargetSettings>(*rhs.settings);
    return *this;
}

TargetSetting &TargetSetting::operator=(const TargetSettings &u)
{
    if (!settings)
        settings = std::make_unique<TargetSettings>();
    *settings = u;
    return *this;
}

TargetSetting &TargetSetting::operator[](const TargetSettingKey &k)
{
    if (!settings)
        settings = std::make_unique<TargetSettings>();
    return (*settings)[k];
}

const TargetSetting &TargetSetting::operator[](const TargetSettingKey &k) const
{
    if (!settings)
        settings = std::make_unique<TargetSettings>();
    return (*settings)[k];
}

bool TargetSetting::operator<(const TargetSetting &rhs) const
{
    if (settings && rhs.settings)
        return std::tie(value, *settings) < std::tie(rhs.value, *rhs.settings);
    return std::tie(value, array) < std::tie(rhs.value, rhs.array);
}

const String &TargetSetting::getValue() const
{
    if (!value)
        throw SW_RUNTIME_ERROR("empty value for key: " + key);
    return *value;
}

const std::vector<TargetSettingValue> &TargetSetting::getArray() const
{
    if (!array)
        throw SW_RUNTIME_ERROR("empty array for key: " + key);
    return *array;
}

const TargetSettings &TargetSetting::getSettings() const
{
    if (!settings)
    {
        static const TargetSettings ts;
        return ts;
    }
    return *settings;
}

bool TargetSetting::operator==(const TargetSetting &rhs) const
{
    if ((!value || !rhs.value) && (!array || !rhs.array))
        return false;
    if (value && rhs.value)
        return value == rhs.value;
    if (array && rhs.array)
        return array == rhs.array;
    SW_UNIMPLEMENTED;
}

bool TargetSetting::operator!=(const TargetSetting &rhs) const
{
    return !operator==(rhs);
}

void TargetSetting::merge(const TargetSetting &rhs)
{
    value = rhs.value;
    array = rhs.array;
    if (!rhs.settings)
        return;
    if (!settings)
        settings = std::make_unique<TargetSettings>();
    settings->merge(*rhs.settings);
}

void TargetSetting::push_back(const TargetSettingValue &v)
{
    if (!array)
        array.emplace();
    array->push_back(v);
}

TargetSetting::operator bool() const
{
    return !!value || !!array;// || settings;
}

/*bool TargetSetting::hasValue() const
{
    return !!value;
}*/

String TargetSettings::getConfig() const
{
    return toString();
}

String TargetSettings::getHash() const
{
    return shorten_hash(blake2b_512(getConfig()), 6);
}

String TargetSettings::toString(int type) const
{
    switch (type)
    {
    //case Simple:
        //return toStringKeyValue();
    case Json:
        return toJson().dump();
    default:
        SW_UNIMPLEMENTED;
    }
}

nlohmann::json TargetSetting::toJson() const
{
    nlohmann::json j;
    if (value)
        return getValue();
    else if (array)
    {
        for (auto &v2 : *array)
            j.push_back(v2);
    }
    if (settings)
        return settings->toJson();
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

/*String TargetSettings::toStringKeyValue() const
{
    String c;
    for (auto &[k, v] : *this)
    {
        if (v)
            c += k + ": " + v.getValue() + "\n";
    }
    return c;
}*/

TargetSetting &TargetSettings::operator[](const TargetSettingKey &k)
{
    return settings.try_emplace(k, k).first->second;
}

const TargetSetting &TargetSettings::operator[](const TargetSettingKey &k) const
{
    return settings.try_emplace(k, k).first->second;
}

bool TargetSettings::operator==(const TargetSettings &rhs) const
{
    const auto &main = settings.size() < rhs.settings.size() ? *this : rhs;
    const auto &other = settings.size() >= rhs.settings.size() ? *this : rhs;
    return std::all_of(main.settings.begin(), main.settings.end(), [&other](const auto &p)
    {
        if (other[p.first] && p.second)
            return other[p.first] == p.second;
        return true;
    });
}

bool TargetSettings::operator<(const TargetSettings &rhs) const
{
    return settings < rhs.settings;
}

void TargetSettings::merge(const TargetSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].merge(v);
}

void TargetSettings::erase(const TargetSettingKey &k)
{
    settings.erase(k);
}

} // namespace sw
