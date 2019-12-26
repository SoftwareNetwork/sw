// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <nlohmann/json_fwd.hpp>
#include <primitives/string.h>

#include <memory>
#include <optional>
#include <variant>

namespace sw
{

/*struct SettingValue
{
    //String value;
    bool used = false;

    // int inherit?
    // -1 - infinite depth
    // 0 - false
    // >0 - depth
    bool inherit = false;
};*/

using TargetSettingKey = String;
using TargetSettingValue = String;
struct TargetSetting;
struct TargetSettings;

struct SW_CORE_API TargetSettings
{
    enum StringType : int
    {
        KeyValue    = 0,

        Json,
        // yml

        Simple      = KeyValue,
    };

    TargetSetting &operator[](const TargetSettingKey &);
    const TargetSetting &operator[](const TargetSettingKey &) const;

    void merge(const TargetSettings &);
    void erase(const TargetSettingKey &);

    // other merges
    void merge(const String &s, int type = Json);
    void mergeFromJson(const nlohmann::json &);

    String getConfig() const; // getShortConfig()?
    String toString(int type = Json) const;
    String getHash() const;

    bool operator==(const TargetSettings &) const;
    bool operator<(const TargetSettings &) const;
    bool isSubsetOf(const TargetSettings &) const;

    auto begin() { return settings.begin(); }
    auto end() { return settings.end(); }
    auto begin() const { return settings.begin(); }
    auto end() const { return settings.end(); }

    bool empty() const;

private:
    std::map<TargetSettingKey, TargetSetting> settings;

    //String toStringKeyValue() const;
    nlohmann::json toJson() const;

    friend struct TargetSetting;
};

struct SW_CORE_API TargetSetting
{
    TargetSetting(const TargetSettingKey &k);
    TargetSetting(const TargetSetting &);
    TargetSetting &operator=(const TargetSetting &);

    TargetSetting &operator[](const TargetSettingKey &k);
    const TargetSetting &operator[](const TargetSettingKey &k) const;

    TargetSetting &operator=(const TargetSettings &u);

    template <class U>
    TargetSetting &operator=(const U &u)
    {
        value = u;
        return *this;
    }

    bool operator==(const TargetSetting &) const;
    bool operator!=(const TargetSetting &) const;
    bool operator<(const TargetSetting &) const;

    template <class U>
    bool operator==(const U &u) const
    {
        auto v = std::get_if<TargetSettingValue>(&value);
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

    //String getHash() const;

    const String &getValue() const;
    const std::vector<TargetSettingValue> &getArray() const;
    TargetSettings &getSettings();
    const TargetSettings &getSettings() const;

    void push_back(const TargetSettingValue &);
    void reset();

    void use();
    void setUseCount(int);

    void setRequired(bool = true);
    bool isRequired() const;

    void useInHash(bool);
    bool useInHash() const { return used_in_hash; }

    void merge(const TargetSetting &);
    void mergeFromJson(const nlohmann::json &);

    bool isValue() const;
    bool isArray() const;
    bool isObject() const;

private:
    int use_count = 1;
    bool required = false;
    bool used_in_hash = true;
    TargetSettingKey key;
    std::variant<std::monostate, TargetSettingValue, std::vector<TargetSettingValue>, TargetSettings> value;

    nlohmann::json toJson() const;

    friend struct TargetSettings;
};

SW_CORE_API
TargetSettings toTargetSettings(const struct OS &);

} // namespace sw
