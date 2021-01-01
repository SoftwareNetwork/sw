// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package_name.h"
#include "settings.h"

namespace sw
{

struct SW_SUPPORT_API PackageId
{
    PackageName n;
    PackageSettings s;

    const auto &getName() const { return n; }
    const auto &getSettings() const { return s; }

    // maybe also print settings hash
    String toString() const { return n.toString(); }
};

}

namespace std
{

template<> struct hash<::sw::PackageId>
{
    size_t operator()(const ::sw::PackageId &p) const
    {
        auto h = std::hash<::sw::PackageName>()(p.getName());
        return hash_combine(h, p.getSettings().getHash());
    }
};

}
