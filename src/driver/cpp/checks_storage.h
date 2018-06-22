// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "checks.h"

#include <shared_mutex>

namespace sw
{

using ChecksContainer = std::unordered_map<String, int>;

struct ChecksStorage
{
    ChecksContainer checks;
    mutable std::shared_mutex m;
    bool loaded = false;

    ChecksStorage();
    ChecksStorage(const ChecksStorage &rhs);
    ~ChecksStorage();

    void load(const path &fn);
    void save(const path &fn) const;

    bool isChecked(const String &d) const;
    bool isChecked(const String &d, int &v) const;
    void add(const String &d, int v);
};

}
