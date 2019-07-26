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
    /*array = rhs.array;
    if (rhs.settings)
        settings = std::make_unique<TargetSettings>(*rhs.settings);*/
    return *this;
}

TargetSetting &TargetSetting::operator=(const TargetSettings &u)
{
    /*if (!settings)
        settings = std::make_unique<TargetSettings>();
    *settings = u;*/
    value = u;
    return *this;
}

TargetSetting &TargetSetting::operator[](const TargetSettingKey &k)
{
    /*if (!settings)
        settings = std::make_unique<TargetSettings>();
    return (*settings)[k];*/
    if (value.index() == 0)
        value = TargetSettings();
    return std::get<TargetSettings>(value)[k];
}

const TargetSetting &TargetSetting::operator[](const TargetSettingKey &k) const
{
    /*if (!settings)
    {
        static TargetSetting s("");
        return s;
    }
    return (*settings)[k];*/
    if (value.index() != 3)
    {
        static TargetSetting s("");
        return s;
    }
    return std::get<TargetSettings>(value)[k];
}

const String &TargetSetting::getValue() const
{
    auto v = std::get_if<TargetSettingValue>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty value for key: " + key);
    return *v;
}

const std::vector<TargetSettingValue> &TargetSetting::getArray() const
{
    auto v = std::get_if<std::vector<TargetSettingValue>>(&value);
    if (!v)
        throw SW_RUNTIME_ERROR("empty array for key: " + key);
    return *v;
}

TargetSettings &TargetSetting::getSettings()
{
    auto s = std::get_if<TargetSettings>(&value);
    if (!s)
    {
        static TargetSettings ts;
        return ts;
    }
    return *s;
}

const TargetSettings &TargetSetting::getSettings() const
{
    auto s = std::get_if<TargetSettings>(&value);
    if (!s)
    {
        static const TargetSettings ts;
        return ts;
    }
    return *s;
}

bool TargetSetting::operator<(const TargetSetting &rhs) const
{
    /*if (settings && rhs.settings)
    return std::tie(value, *settings) < std::tie(rhs.value, *rhs.settings);
    return std::tie(value, array) < std::tie(rhs.value, rhs.array);*/
    return value < rhs.value;
}

bool TargetSetting::operator==(const TargetSetting &rhs) const
{
    /*if ((!value || !rhs.value) && (!array || !rhs.array))
        return false;
    if (value && rhs.value)
        return value == rhs.value;
    if (array && rhs.array)
        return array == rhs.array;*/
    return value == rhs.value;
}

bool TargetSetting::operator!=(const TargetSetting &rhs) const
{
    return !operator==(rhs);
}

void TargetSetting::merge(const TargetSetting &rhs)
{
    /*value = rhs.value;
    array = rhs.array;
    if (!rhs.settings)
        return;
    if (!settings)
        settings = std::make_unique<TargetSettings>();
    settings->merge(*rhs.settings);*/
    auto s = std::get_if<TargetSettings>(&value);
    if (s)
    {
        s->merge(std::get<TargetSettings>(rhs.value));
        return;
    }
    value = rhs.value;
}

void TargetSetting::push_back(const TargetSettingValue &v)
{
    /*if (!array)
        array.emplace();
    array->push_back(v);*/
    if (value.index() == 0)
        value = std::vector<TargetSettingValue>();
    return std::get<std::vector<TargetSettingValue>>(value).push_back(v);
}

void TargetSetting::reset()
{
    decltype(value) v;
    value.swap(v);
}

TargetSetting::operator bool() const
{
    //return !!value || !!array;// || settings;
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

void TargetSettings::mergeFromString(const String &s, int type = Json) const
{
    SW_UNIMPLEMENTED;
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
    return settings.try_emplace(k, k).first->second;
}

const TargetSetting &TargetSettings::operator[](const TargetSettingKey &k) const
{
    auto i = settings.find(k);
    if (i == settings.end())
    {
        static TargetSetting s("");
        return s;
    }
    return i->second;
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

bool TargetSettings::empty() const
{
    return settings.empty();
}

} // namespace sw
