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
