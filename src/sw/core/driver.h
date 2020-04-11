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

#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct Input;
struct SwContext;
enum class InputType : uint8_t;

struct SW_CORE_API Specification
{
    void addFile(const path &relative_path, const String &contents);
    int64_t getHash() const;

//private:
    std::map<path, String> files;
};

struct SW_CORE_API IDriver
{
    virtual ~IDriver();

    /// Detect available inputs on path.
    virtual std::vector<std::unique_ptr<Input>> detectInputs(const path &abspath, InputType) const = 0;

    /// Optimized input loading in a batch.
    /// Inputs are unique and non null.
    /// Inputs will receive their entry points.
    virtual void loadInputsBatch(SwContext &, const std::set<Input*> &) const = 0;

    // get features()?
};

} // namespace sw
