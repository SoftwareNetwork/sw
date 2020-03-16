/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "settings.h"

#include <sw/builder/os.h>
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
    case OSType::Cygwin:
        s["os"]["kernel"] = "org.cygwin";
        break;
    case OSType::Mingw:
        s["os"]["kernel"] = "org.mingw";
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

TargetSetting::Map &TargetSetting::getSettings()
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

const TargetSetting::Map &TargetSetting::getSettings() const
{
    auto s = std::get_if<Map>(&value);
    if (!s)
    {
        thread_local const Map ts;
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
            if (e.is_string())
                v->push_back(e);
            else if (e.is_object())
            {
                Map m;
                m.mergeFromJson(e);
                v->push_back(m);
            }
            else
                SW_UNIMPLEMENTED;
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

void TargetSetting::push_back(const Value &v)
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
        {
            if (v2.index() == 0)
                j.push_back(std::get<Value>(v2));
            else
                j.push_back(std::get<Map>(v2).toJson());
        }
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
        {
            if (v2.index() == 0)
                hash_combine(h, std::get<Value>(v2));
            else
                hash_combine(h, std::get<Map>(v2).getHash1());
        }
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
