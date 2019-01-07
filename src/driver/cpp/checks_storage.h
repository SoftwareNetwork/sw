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

struct ChecksStorage
{
    std::unordered_map<size_t /* hash */, CheckValue> all_checks;
    bool loaded = false;

    void load(const path &fn);
    void save(const path &fn) const;

    void add(const Check &c);
};

}
