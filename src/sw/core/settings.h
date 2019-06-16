// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/string.h>

namespace sw
{

struct SettingValue
{
    //String value;
    bool used = false;

    // int inherit?
    // -1 - infinite depth
    // 0 - false
    // >0 - depth
    bool inherit = false;
};

using TargetSettingKey = String;
using TargetSettingValue = String;

// make map internal?
struct SW_CORE_API TargetSettings : std::map<TargetSettingKey, TargetSettingValue>
{
    enum StringType : int
    {
        KeyValue    = 0,

        Json,
        // yml

        Simple      = KeyValue,
    };

    String getConfig() const; // getShortConfig()?
    String getHash() const;
    String toString(int type = Simple) const;

    bool operator==(const TargetSettings &) const;

private:
    String toStringKeyValue() const;
    String toJsonString() const;
};

} // namespace sw
