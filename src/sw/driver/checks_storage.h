// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "checks.h"

#include <shared_mutex>

namespace sw
{

struct ChecksStorage
{
    std::unordered_map<size_t /* hash */, CheckValue> all_checks;
    std::unordered_map<size_t /* hash */, const Check *> manual_checks;
    bool loaded = false;
    bool new_manual_checks_loaded = false;

    void load(const path &fn);
    void load_manual(const path &fn);
    void save(const path &fn) const;

    void add(const Check &c);
};

}
