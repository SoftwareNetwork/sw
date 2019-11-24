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
    required = rhs.required;
    use_count = rhs.use_count;
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
    if (value.index() == 0)
    {
        static std::vector<TargetSettingValue> s;
        return s;
    }
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
        static const TargetSettings ts;
        return ts;
    }
    return *s;
}

bool TargetSetting::operator==(const TargetSetting &rhs) const
{
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

int TargetSetting::compareEqualKeys(const TargetSetting &lhs, const TargetSetting &rhs)
{
    auto lv = std::get_if<TargetSettings>(&lhs.value);
    auto rv = std::get_if<TargetSettings>(&rhs.value);
    if (lv && rv)
        return TargetSettings::compareEqualKeys(*lv, *rv);
    bool r = lhs.value == rhs.value;
    if (r)
        return 0;
    return 1; // -1 is not implemented at the moment
}

void TargetSetting::merge(const TargetSetting &rhs)
{
    auto s = std::get_if<TargetSettings>(&value);
    if (s)
    {
        s->merge(std::get<TargetSettings>(rhs.value));
        return;
    }
    value = rhs.value;
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
    decltype(value) v;
    value.swap(v);
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

void TargetSettings::merge(const String &s, int type)
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
    for (auto &[k, v] : rhs.settings)
    {
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
    return true;
}

bool TargetSettings::operator<(const TargetSettings &rhs) const
{
    return settings < rhs.settings;
}

int TargetSettings::compareEqualKeys(const TargetSettings &lhs, const TargetSettings &rhs)
{
    // at the moment we check if smaller set is subset of bigger one
    const auto &main = lhs.settings.size() <= rhs.settings.size() ? lhs : rhs;
    const auto &other = lhs.settings.size() > rhs.settings.size() ? lhs : rhs;
    bool r = std::all_of(main.settings.begin(), main.settings.end(), [&other](const auto &p)
    {
        if (
            // compare if both is present
            (other[p.first] && p.second)

            // or one is present and required
            || (other[p.first] && other[p.first].isRequired())
            || (p.second && p.second.isRequired())
            )
        {
            return TargetSetting::compareEqualKeys(other[p.first], p.second) == 0;
        }
        return true;
    });
    // check required settings in other
    r &= std::all_of(other.settings.begin(), other.settings.end(), [&main](const auto &p)
    {
        if (
            // compare if any is required
            (p.second && p.second.isRequired()) ||
            (main[p.first] && main[p.first].isRequired())
            )
        {
            return TargetSetting::compareEqualKeys(main[p.first], p.second) == 0;
        }
        return true;
    });
    if (r)
        return 0;
    return 1; // -1 is not implemented at the moment
}

void TargetSettings::merge(const TargetSettings &rhs)
{
    for (auto &[k, v] : rhs)
        (*this)[k].merge(v);
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
