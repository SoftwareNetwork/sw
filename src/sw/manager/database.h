/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2019 Egor Pugin
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

#include <sw/support/filesystem.h>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace sqlpp::sqlite3 { class connection; }

namespace sw
{

struct SW_MANAGER_API Database
{
    std::unique_ptr<sqlpp::sqlite3::connection> db;
    path fn;

    Database(const path &db_name, const String &schema);
    ~Database();

    void open(bool read_only = false, bool in_memory = false);

    int getIntValue(const String &key);
    void setIntValue(const String &key, int v);

protected:
    //
    template <typename T>
    std::optional<T> getValue(const String &key) const;

    template <typename T>
    T getValue(const String &key, const T &default_) const;

    template <typename T>
    void setValue(const String &key, const T &v) const;
};

}
