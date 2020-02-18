// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    /// One input may provide several entry points (yml).
    [[nodiscard]]
    virtual void loadInputsBatch(SwContext &, const std::set<Input*> &) const = 0;

    // get features()?
};

} // namespace sw
