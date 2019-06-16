// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "settings.h"

#include <sw/support/hash.h>

#include <nlohmann/json.hpp>

namespace sw
{

String TargetSettings::getConfig() const
{
    String c;
    for (auto &[k, v] : *this)
        c += k + v;
    return c;
}

String TargetSettings::getHash() const
{
    return shorten_hash(blake2b_512(getConfig()), 6);
}

String TargetSettings::toString(int type) const
{
    switch (type)
    {
    case Simple:
        return toStringKeyValue();
    case Json:
        return toJsonString();
    default:
        SW_UNIMPLEMENTED;
    }
}

String TargetSettings::toJsonString() const
{
    nlohmann::json j;
    for (auto &[k, v] : *this)
        j[k] = v;
    return j.dump();
}

String TargetSettings::toStringKeyValue() const
{
    String c;
    for (auto &[k, v] : *this)
        c += k + ": " + v + "\n";
    return c;
}

bool TargetSettings::operator==(const TargetSettings &rhs) const
{
    const auto &main = size() < rhs.size() ? *this : rhs;
    const auto &other = size() >= rhs.size() ? *this : rhs;
    return std::all_of(main.begin(), main.end(), [&other](const auto &p)
    {
        auto i = other.find(p.first);
        return i != other.end() && i->second == p.second;
    });
}

} // namespace sw
